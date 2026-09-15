#ifndef BSP_I2C_H
#define BSP_I2C_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* 地址按 HAL 格式左移一位，offset 为设备的一字节寄存器/控制地址。timeout 的
 * 单位是毫秒；ready() 用它作为每次 ACK 探测的时限，共探测 10 次。 */
bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint8_t offset, uint8_t *data,
                  uint16_t size, uint32_t timeout);
bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint8_t offset, const uint8_t *data,
                   uint16_t size, uint32_t timeout);
bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout);
#endif
