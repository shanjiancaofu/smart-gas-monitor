#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "bsp_eeprom_config.h"

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint16_t address;
} bsp_eeprom_t;

void bsp_eeprom_init(bsp_eeprom_t *eeprom, I2C_HandleTypeDef *i2c);
bool bsp_eeprom_read(void *context, uint16_t offset, uint8_t *data, size_t size);
bool bsp_eeprom_write(void *context, uint16_t offset, const uint8_t *data, size_t size);
#endif
