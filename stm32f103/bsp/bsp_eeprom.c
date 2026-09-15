#include "bsp_eeprom.h"
#include "bsp_i2c.h"

void bsp_eeprom_init(bsp_eeprom_t *eeprom, I2C_HandleTypeDef *i2c)
{
    eeprom->i2c = i2c;
    eeprom->address = EEPROM_I2C_ADDRESS;
}

bool bsp_eeprom_read(void *context, uint16_t offset, uint8_t *data, size_t size)
{
    bsp_eeprom_t *eeprom = context;
    if (offset >= EEPROM_CAPACITY || size > (size_t)(EEPROM_CAPACITY - offset)) {
        return false;
    }
    return bsp_i2c_read(eeprom->i2c, eeprom->address, offset, EEPROM_ADDRESS_BITS, data,
                        (uint16_t)size, 10);
}

bool bsp_eeprom_write(void *context, uint16_t offset, const uint8_t *data, size_t size)
{
    bsp_eeprom_t *eeprom = context;
    if (offset >= EEPROM_CAPACITY || size > (size_t)(EEPROM_CAPACITY - offset)) {
        return false;
    }
    while (size != 0) {

        size_t count = EEPROM_PAGE_SIZE - (offset % EEPROM_PAGE_SIZE);
        if (count > size) {
            count = size;
        }
        if (!bsp_i2c_write(eeprom->i2c, eeprom->address, offset, EEPROM_ADDRESS_BITS, data,
                           (uint16_t)count, 10) ||
            !bsp_i2c_ready(eeprom->i2c, eeprom->address, 1)) {
            return false;
        }
        offset = (uint16_t)(offset + count);
        data += count;
        size -= count;
    }
    return true;
}
