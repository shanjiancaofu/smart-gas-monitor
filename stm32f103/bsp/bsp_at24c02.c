#include "bsp_at24c02.h"
#include "bsp_i2c.h"

void bsp_at24c02_init(bsp_at24c02_t *eeprom, I2C_HandleTypeDef *i2c)
{
    eeprom->i2c = i2c;
    eeprom->address = AT24C02_ADDRESS;
}

bool bsp_at24c02_read(void *context, uint16_t offset, uint8_t *data, size_t size)
{
    bsp_at24c02_t *eeprom = context;
    if (offset >= AT24C02_SIZE || size > (size_t)(AT24C02_SIZE - offset)) {
        return false;
    }
    return bsp_i2c_read(eeprom->i2c, eeprom->address, (uint8_t)offset, data, (uint16_t)size, 10);
}

bool bsp_at24c02_write(void *context, uint16_t offset, const uint8_t *data, size_t size)
{
    bsp_at24c02_t *eeprom = context;
    if (offset >= AT24C02_SIZE || size > (size_t)(AT24C02_SIZE - offset)) {
        return false;
    }
    while (size != 0) {
        /* AT24C02 页大小为 8 字节；一次写入不能在页内回绕。 */
        size_t count = 8u - (offset % 8u);
        if (count > size) {
            count = size;
        }
        if (!bsp_i2c_write(eeprom->i2c, eeprom->address, (uint8_t)offset, data, (uint16_t)count,
                           10) ||
            !bsp_i2c_ready(eeprom->i2c, eeprom->address, 1)) {
            return false;
        }
        offset = (uint16_t)(offset + count);
        data += count;
        size -= count;
    }
    return true;
}
