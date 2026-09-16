#ifndef BSP_I2C_H
#define BSP_I2C_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* 两条 I2C 总线都走软件模拟（GPIO 翻转），不用 STM32 的 I2C 外设。
 *
 * 为什么：Proteus 的 STM32 模型不认 I2C 的复用功能配置，硬件外设驱动的总线在
 * 仿真里收不到任何应答——OLED 全黑、存储读写全失败。软件模拟在仿真和实物上
 * 都可用，代价是速率由 GPIO 翻转速度决定（当前约 80 kHz），比硬件外设慢。
 *
 * 下面三个函数仍然收一个 I2C_HandleTypeDef *，但它不再驱动硬件，只当总线编号：
 *   &hi2c1 → PB6/PB7（OLED），&hi2c2 → PB10/PB11（存储）。这样上层的 OLED、
 *   EEPROM、配置和历史代码一行都不用改。
 *
 * timeout 参数保留是为了不改动调用方；软件模拟是同步的，它不再被使用。 */

/* 配置两条总线的引脚。必须在使用任何读写之前调用一次。 */
void bsp_i2c_init(void);

/* offset 为设备内部地址，address_bits 选择 8 位或 16 位地址格式。 */
bool bsp_i2c_read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset, uint16_t address_bits,
                  uint8_t *data, uint16_t size, uint32_t timeout);
bool bsp_i2c_write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t offset, uint16_t address_bits,
                   const uint8_t *data, uint16_t size, uint32_t timeout);
/* 探测器件是否应答：发一次地址，看从机有没有拉低 SDA。 */
bool bsp_i2c_ready(I2C_HandleTypeDef *bus, uint16_t address, uint32_t timeout);
#endif
