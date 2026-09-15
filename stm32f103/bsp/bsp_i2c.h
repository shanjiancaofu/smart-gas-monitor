#ifndef BSP_I2C_H
#define BSP_I2C_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* offset 为设备内部地址，address_bits 选择 8 位或 16 位地址格式。 */
bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset, uint16_t address_bits,
                  uint8_t *data, uint16_t size, uint32_t timeout);
bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset, uint16_t address_bits,
                   const uint8_t *data, uint16_t size, uint32_t timeout);
bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout);
#endif
