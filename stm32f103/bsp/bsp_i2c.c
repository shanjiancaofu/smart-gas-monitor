#include "bsp_i2c.h"
#include "i2c.h"

/* 软件模拟 I2C。两条总线的引脚和理由见 bsp_i2c.h。 */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t scl;
    uint16_t sda;
} soft_bus_t;

static const soft_bus_t soft_buses[2] = {
    {GPIOB, GPIO_PIN_6, GPIO_PIN_7},     /* &hi2c1：OLED */
    {GPIOB, GPIO_PIN_10, GPIO_PIN_11},   /* &hi2c2：存储 */
};

/* 半个时钟周期的延时，单位是循环次数——**不是 CPU 周期**：每次迭代还有变量
 * 访问、比较、递增、分支，实测一次迭代约 5~16 个周期，视编译结果而定。所以
 * 改优化等级或换编译器都会改变实际速率，判断标准看仿真日志和逻辑分析仪。
 *
 * 为什么不用空循环：原来 for (volatile i < N) {} 的写法被 Keil 的 ARMCC 整个
 * 优化掉了——Proteus 里测到的 STOP 建立时间在 30、60、140 三种循环次数下都是
 * 3.110913 us，一模一样，说明循环压根没跑。加 __NOP__ 之后编译器无从下手。
 *
 * 100 次的选择依据：Proteus 的 I2CMEM 模型对 SCL 的**每一个**高、低电平都要
 * 下限（TD_CLK_HIGH = 4 us、TD_CLK_LOW = 4.7 us），低于它按位拒绝通信；而
 * OLED 是另一个模型，要求宽松照样能亮，所以"屏幕正常"证明不了"存储也正常"。 */
#define I2C_DELAY_ITERATIONS 100u

static void i2c_delay(void)
{
    for (volatile unsigned i = 0; i < I2C_DELAY_ITERATIONS; ++i) {
        __NOP();
    }
}

/* 句柄只用来选总线，不驱动硬件。 */
static const soft_bus_t *bus_of(I2C_HandleTypeDef *handle)
{
    return handle == &hi2c2 ? &soft_buses[1] : &soft_buses[0];
}

static void scl_set(const soft_bus_t *bus, bool high)
{
    HAL_GPIO_WritePin(bus->port, bus->scl, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void sda_set(const soft_bus_t *bus, bool high)
{
    HAL_GPIO_WritePin(bus->port, bus->sda, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 开漏引脚的输入缓冲一直有效，所以输出模式下读得到真实线电平。 */
static bool sda_get(const soft_bus_t *bus)
{
    return HAL_GPIO_ReadPin(bus->port, bus->sda) == GPIO_PIN_SET;
}

/* 把 SDA 真正放成高阻输入。
 *
 * 这一步是必须的，不能只写 SET 了事：开漏输出即使写 1，引脚仍被推挽级的输出
 * 缓冲监视着，从机拉低时 Proteus 会判定成两个驱动源争用，报
 * "Logic contention on net"。发送阶段才是输出，等应答和读数据时都要让开。 */
static void sda_input(const soft_bus_t *bus)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = bus->sda;
    gpio.Mode = GPIO_MODE_INPUT;
    /* 必须带内部上拉。存储没接时这条线没有任何外部上拉，浮空输入会被读成
     * 忽高忽低的随机值——读成低就被当成从机应答，于是 history_init() 以为
     * 存储在线，把 510 个槽位全扫一遍，光初始化就花掉 7.7 秒。带上拉之后
     * 空闲即读作高（I2C 的空闲电平），真器件拉低应答时能盖过这个弱上拉。 */
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(bus->port, &gpio);
}

/* 主机要驱动 SDA 时切回开漏输出。 */
static void sda_output(const soft_bus_t *bus)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = bus->sda;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    /* 输出模式刻意不加片内上拉。Proteus 的 CM3 模型把开漏引脚上的上拉当成一个
     * 驱动源，从机拉低应答时就报 "Logic contention on net"。这里不需要它：所有
     * 读线的动作（sda_get）都发生在 sda_input() 之后，而那条路径上拉是留着的，
     * 输出阶段主机只写不读。实物的正规做法也是靠模块自带的 4.7k 外部上拉。 */
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->port, &gpio);
}

void bsp_i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    unsigned i;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;   /* 理由见 sda_output() */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = soft_buses[0].scl | soft_buses[0].sda | soft_buses[1].scl | soft_buses[1].sda;
    HAL_GPIO_Init(GPIOB, &gpio);

    for (i = 0; i < 2u; ++i) {
        scl_set(&soft_buses[i], true);
        sda_set(&soft_buses[i], true);
    }
}

static void i2c_start(const soft_bus_t *bus)
{
    sda_output(bus);
    sda_set(bus, true);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    sda_set(bus, false);   /* SCL 高时 SDA 下降 = 起始条件 */
    i2c_delay();
    scl_set(bus, false);
    i2c_delay();
}

static void i2c_stop(const soft_bus_t *bus)
{
    sda_output(bus);
    sda_set(bus, false);
    i2c_delay();
    scl_set(bus, true);
    /* 这一等就是 tSU;STO：SCL 抬起来之后要停够时间才允许动 SDA。 */
    i2c_delay();
    sda_set(bus, true);
    i2c_delay();
}

/* 发一个字节，返回从机有没有应答。 */
static bool i2c_write_byte(const soft_bus_t *bus, uint8_t byte)
{
    unsigned i;
    bool ack;

    sda_output(bus);
    for (i = 0; i < 8u; ++i) {
        sda_set(bus, (byte & 0x80u) != 0u);
        i2c_delay();
        scl_set(bus, true);
        i2c_delay();
        scl_set(bus, false);
        i2c_delay();
        byte = (uint8_t)(byte << 1);
    }
    /* 第九个时钟让给从机：先把 SDA 放成高阻，再抬高 SCL 读应答。 */
    sda_input(bus);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    ack = !sda_get(bus);   /* 低电平 = 应答 */
    scl_set(bus, false);
    i2c_delay();
    sda_output(bus);
    return ack;
}

/* 收一个字节。ack 为真时主机回应答（还有后续字节），为假时回非应答（最后一个）。 */
static uint8_t i2c_read_byte(const soft_bus_t *bus, bool ack)
{
    uint8_t byte = 0u;
    unsigned i;

    sda_input(bus);        /* 整段读取期间 SDA 都归从机驱动 */
    i2c_delay();
    for (i = 0; i < 8u; ++i) {
        scl_set(bus, true);
        i2c_delay();
        byte = (uint8_t)((byte << 1) | (sda_get(bus) ? 1u : 0u));
        scl_set(bus, false);
        i2c_delay();
    }
    sda_output(bus);       /* 这一位由主机驱动 */
    sda_set(bus, !ack);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    scl_set(bus, false);
    i2c_delay();
    return byte;
}

/* 发设备地址 + 内部地址，"写"方向。返回从机是否应答。 */
static bool i2c_send_address(const soft_bus_t *bus, uint16_t address, uint16_t offset,
                             uint16_t address_bits)
{
    if (!i2c_write_byte(bus, (uint8_t)address)) {
        return false;
    }
    if (address_bits == 16u) {
        return i2c_write_byte(bus, (uint8_t)(offset >> 8)) &&
               i2c_write_byte(bus, (uint8_t)offset);
    }
    return i2c_write_byte(bus, (uint8_t)offset);
}

bool bsp_i2c_write(I2C_HandleTypeDef *bus_handle, uint16_t address, uint16_t offset,
                   uint16_t address_bits, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    const soft_bus_t *bus = bus_of(bus_handle);
    uint16_t i;
    bool ok;

    (void)timeout;
    i2c_start(bus);
    ok = i2c_send_address(bus, address, offset, address_bits);
    for (i = 0; ok && i < size; ++i) {
        ok = i2c_write_byte(bus, data[i]);
    }
    i2c_stop(bus);
    return ok;
}

bool bsp_i2c_read(I2C_HandleTypeDef *bus_handle, uint16_t address, uint16_t offset,
                  uint16_t address_bits, uint8_t *data, uint16_t size, uint32_t timeout)
{
    const soft_bus_t *bus = bus_of(bus_handle);
    uint16_t i;

    (void)timeout;
    if (size == 0u) {
        return true;
    }
    i2c_start(bus);
    if (!i2c_send_address(bus, address, offset, address_bits)) {
        i2c_stop(bus);
        return false;
    }
    /* 重新起始，这次是读方向。 */
    i2c_start(bus);
    if (!i2c_write_byte(bus, (uint8_t)(address | 1u))) {
        i2c_stop(bus);
        return false;
    }
    for (i = 0; i < size; ++i) {
        /* 最后一个字节回非应答，之后主机才能发停止条件。 */
        data[i] = i2c_read_byte(bus, (uint16_t)(i + 1u) < size);
    }
    i2c_stop(bus);
    return true;
}

bool bsp_i2c_ready(I2C_HandleTypeDef *bus_handle, uint16_t address, uint32_t timeout)
{
    const soft_bus_t *bus = bus_of(bus_handle);
    uint32_t waited;

    /* 探测器件在不在，timeout 单位是毫秒。
     *
     * 必须真的重试：EEPROM 写完一个页之后会忙 TD_WRITE 时间（24C64 典型 5 ms、
     * Proteus 模型 6 ms）才重新应答。写完之后立刻探一次、NACK 就当失败，是
     * 把「器件正在忙」误判成「器件不在」——表现就是参数存不住、历史一直是
     * 0/510，而 OLED 因为从不走这条路径照常显示，屏幕正常反而掩盖了它。 */
    for (waited = 0u;; ++waited) {
        bool ack;

        i2c_start(bus);
        ack = i2c_write_byte(bus, (uint8_t)address);
        i2c_stop(bus);
        if (ack) {
            return true;
        }
        if (waited >= timeout) {
            return false;
        }
        HAL_Delay(1);
    }
}
