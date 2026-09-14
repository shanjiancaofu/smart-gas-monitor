#ifndef AT24C02_H
#define AT24C02_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stddef.h>

#ifndef AT24C02_ADDRESS
#define AT24C02_ADDRESS (0x50u << 1)
#endif
#define AT24C02_SIZE 256u

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint16_t address;
} at24c02_t;

void at24c02_init(at24c02_t *eeprom, I2C_HandleTypeDef *i2c);
/* 这两个签名与 settings_io_t 一致，配置存储可以直接指向它们。一次写要等器件
 * 对每一页都应答之后才返回。 */
bool at24c02_read(void *context, uint16_t offset, uint8_t *data, size_t size);
bool at24c02_write(void *context, uint16_t offset, const uint8_t *data, size_t size);
#endif
