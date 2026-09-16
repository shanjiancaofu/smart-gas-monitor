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

/* 半个时钟周期的延时。72 MHz 下 i2c_delay() 一次调用约 432 个周期 = 6 us，
 * 于是 SCL 一个完整周期约 12 us，总线速率约 83 kHz。count 是循环次数，靠实测
 * 标定——软件延时本来就是近似值，这里只需要满足 I2C 的时序下限：Proteus 的
 * SSD1306 和存储模型都要求 STOP 的建立时间 tSU;STO >= 4.7 us。 */
#define I2C_DELAY_LOOPS 80u

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

/* 开漏输出：写 1 是释放这条线，由上拉拉高，所以两条线不会互相顶。 */
static void scl_set(const soft_bus_t *bus, bool high)
{
    HAL_GPIO_WritePin(bus->port, bus->scl, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void sda_set(const soft_bus_t *bus, bool high)
{
    HAL_GPIO_WritePin(bus->port, bus->sda, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 开漏引脚的输入缓冲一直有效，所以读得到真实线电平。 */
static bool sda_get(const soft_bus_t *bus)
{
    return HAL_GPIO_ReadPin(bus->port, bus->sda) == GPIO_PIN_SET;
}

void bsp_i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    unsigned i;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
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
    sda_set(bus, false);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    /* SCL 已经是高，再把 SDA 放开就是停止条件。先抬高 SCL 再等一个周期才动
     * SDA，这段保持时间要盖过模型的 tSU;STO 下限。 */
    sda_set(bus, true);
    i2c_delay();
}

/* 发一个字节，返回从机有没有应答。 */
static bool i2c_write_byte(const soft_bus_t *bus, uint8_t byte)
{
    unsigned i;
    bool ack;

    for (i = 0; i < 8u; ++i) {
        sda_set(bus, (byte & 0x80u) != 0u);
        i2c_delay();
        scl_set(bus, true);
        i2c_delay();
        scl_set(bus, false);
        i2c_delay();
        byte = (uint8_t)(byte << 1);
    }
    /* 释放 SDA 把第九个时钟让给从机，低电平就是应答。 */
    sda_set(bus, true);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    ack = !sda_get(bus);
    scl_set(bus, false);
    i2c_delay();
    return ack;
}

/* 收一个字节。ack 为真时主机回应答（还有后续字节），为假时回非应答（最后一个）。 */
static uint8_t i2c_read_byte(const soft_bus_t *bus, bool ack)
{
    uint8_t byte = 0u;
    unsigned i;

    sda_set(bus, true);    /* 主机释放 SDA，交给从机驱动 */
    i2c_delay();
    for (i = 0; i < 8u; ++i) {
        scl_set(bus, true);
        i2c_delay();
        byte = (uint8_t)((byte << 1) | (sda_get(bus) ? 1u : 0u));
        scl_set(bus, false);
        i2c_delay();
    }
    sda_set(bus, !ack);
    i2c_delay();
    scl_set(bus, true);
    i2c_delay();
    scl_set(bus, false);
    i2c_delay();
    sda_set(bus, true);
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
