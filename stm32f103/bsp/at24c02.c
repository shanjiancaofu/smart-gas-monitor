#include "at24c02.h"

void at24c02_init(at24c02_t *eeprom, I2C_HandleTypeDef *i2c)
{
    eeprom->i2c = i2c;
    eeprom->address = AT24C02_ADDRESS;
}

bool at24c02_read(void *context, uint16_t offset, uint8_t *data, size_t size)
{
    at24c02_t *eeprom = context;
    if (offset >= AT24C02_SIZE || size > (size_t)(AT24C02_SIZE - offset)) return false;
    return HAL_I2C_Mem_Read(eeprom->i2c, eeprom->address, offset,
        I2C_MEMADD_SIZE_8BIT, data, (uint16_t)size, 10) == HAL_OK;
}

bool at24c02_write(void *context, uint16_t offset, const uint8_t *data, size_t size)
{
    at24c02_t *eeprom = context;
    if (offset >= AT24C02_SIZE || size > (size_t)(AT24C02_SIZE - offset)) return false;
    while (size != 0) {
        /* AT24C02 page size is 8 bytes; never wrap inside a page. */
        size_t count = 8u - (offset % 8u);
        if (count > size) count = size;
        if (HAL_I2C_Mem_Write(eeprom->i2c, eeprom->address, offset,
            I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, (uint16_t)count, 10) != HAL_OK ||
            HAL_I2C_IsDeviceReady(eeprom->i2c, eeprom->address, 10, 1) != HAL_OK) return false;
        offset = (uint16_t)(offset + count); data += count; size -= count;
    }
    return true;
}
