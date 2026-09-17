#include "bsp_eeprom.h"
#include "bsp_i2c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned reads, writes, ready;
    uint32_t ready_timeout; /* 最近一次 bsp_i2c_ready 的等待上限 */
    uint16_t offsets[4], sizes[4], address_bits[4];
} fake_bus_t;

bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset,
                  uint16_t address_bits, uint8_t *data, uint16_t size, uint32_t timeout)
{
    fake_bus_t *fake = (fake_bus_t *)bus;
    (void)address; (void)timeout;
    fake->offsets[fake->reads] = offset;
    fake->sizes[fake->reads] = size;
    fake->address_bits[fake->reads++] = address_bits;
    memset(data, 0x5a, size);
    return true;
}

bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset,
                   uint16_t address_bits, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    fake_bus_t *fake = (fake_bus_t *)bus;
    (void)address; (void)data; (void)timeout;
    fake->offsets[fake->writes] = offset;
    fake->sizes[fake->writes] = size;
    fake->address_bits[fake->writes++] = address_bits;
    return true;
}

bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout)
{
    fake_bus_t *fake = (fake_bus_t *)bus;
    (void)address;
    ++fake->ready;
    /* 记下来给断言用：这个参数曾经被 (void) 掉、只探测一次，于是 EEPROM 写完
     * 一个页还在忙（24C64 最长 5 ms）时被错判成写失败。 */
    fake->ready_timeout = timeout;
    return true;
}

int main(void)
{
    fake_bus_t fake = {0};
    bsp_eeprom_t eeprom;
    uint8_t data[5] = {0};
    uint16_t start = EEPROM_PAGE_SIZE - 2u;

    bsp_eeprom_init(&eeprom, (I2C_HandleTypeDef *)&fake);
    assert(bsp_eeprom_read(&eeprom, start, data, sizeof(data)));
    assert(fake.reads == 1 && fake.address_bits[0] == EEPROM_ADDRESS_BITS);
    assert(bsp_eeprom_write(&eeprom, start, data, sizeof(data)));
    assert(fake.writes == 2 && fake.ready == 2);
    /* 写完之后必须留足等器件结束内部写周期的时间，不能退化成"探一次就判失败"。 */
    assert(fake.ready_timeout >= EEPROM_WRITE_TIMEOUT_MS);
    assert(fake.ready_timeout >= 5u);
    assert(fake.offsets[0] == start && fake.sizes[0] == 2);
    assert(fake.offsets[1] == start + 2u && fake.sizes[1] == 3);
    assert(fake.address_bits[0] == EEPROM_ADDRESS_BITS &&
           fake.address_bits[1] == EEPROM_ADDRESS_BITS);
    assert(!bsp_eeprom_read(&eeprom, EEPROM_CAPACITY, data, 1));
    assert(!bsp_eeprom_write(&eeprom, EEPROM_CAPACITY - 1u, data, 2));
#if EEPROM_MODEL == 64
    assert(EEPROM_CAPACITY == 8192u && EEPROM_PAGE_SIZE == 32u && EEPROM_ADDRESS_BITS == 16u);
#else
    assert(EEPROM_CAPACITY == 256u && EEPROM_PAGE_SIZE == 8u && EEPROM_ADDRESS_BITS == 8u);
#endif
    puts("PASS: eeprom capacity/address-width/page-split/bounds");
    return 0;
}
