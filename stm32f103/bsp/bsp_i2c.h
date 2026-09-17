#ifndef BSP_I2C_H
#define BSP_I2C_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

/*
 * One API supports both backends:
 *   USE_SOFT_I2C=0: CubeMX initialized hardware I2C (real hardware default).
 *   USE_SOFT_I2C=1: GPIO bit-bang I2C (Proteus default).
 * In soft mode bsp_i2c_init() deinitializes I2C1/I2C2 before reclaiming pins.
 * timeout is expressed in milliseconds; ready() retries until that deadline.
 */
void bsp_i2c_init(void);
bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset,
                  uint16_t address_bits, uint8_t *data, uint16_t size, uint32_t timeout);
bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset,
                   uint16_t address_bits, const uint8_t *data, uint16_t size, uint32_t timeout);
bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout);
#endif
