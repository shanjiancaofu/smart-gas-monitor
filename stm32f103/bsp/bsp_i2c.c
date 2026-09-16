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

/* 半个时钟周期的延时。30 次循环在实物上（72 MHz、GCC -Og）约 5.7 us，SCL 一个
 * 完整周期约 11 us、总线速率约 88 kHz；整屏 1024 字节刷新因此从 747 ms 降到
 * 约 155 ms，稳稳落在 300 ms 采样超时线以内。
 *
 * 这个数是实测出来的，不是一个"理论值"：同样的循环次数在不同编译结果下长度能
 * 差五倍。140 次循环在 Proteus 用的 Keil 构建里约 5.4 us，在实物的 GCC -Og 构
 * 建里却有 26 us——总线因此只跑到 12.6 kHz，整屏刷新要 747 ms，把采样器挤过了
 * 超时线，KEY4 就再也解不开锁。以实物为准取 30。
 *
 * 代价：Proteus 会重新报 STOP 建立时间低于 4.7 us。那只是模型的时序警告，不影
 * 响通信（80 次循环那版就是如此）。真嫌吵就在仿真日志里忽略它。 */
#define I2C_DELAY_LOOPS 30u

static void i2c_delay(void)
{
    for (volatile unsigned i = 0; i < I2C_DELAY_LOOPS; ++i) {
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
    /* 同 sda_input()：没有外部上拉时，开漏写 1 只是把线放开，带上拉才是高。 */
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->port, &gpio);
}

void bsp_i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    unsigned i;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
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
    i2c_delay();
    /* SCL 已经是高，再把 SDA 放开就是停止条件。先抬高 SCL、等满一个半周期再动
     * SDA，这段就是 tSU;STO。 */
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
    bool ack;

    (void)timeout;
    i2c_start(bus);
    ack = i2c_write_byte(bus, (uint8_t)address);
    i2c_stop(bus);
    return ack;
}
