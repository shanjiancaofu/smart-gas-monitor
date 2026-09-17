#ifndef BSP_EEPROM_CONFIG_H
#define BSP_EEPROM_CONFIG_H
/* 默认 AT24C64；编译时 -DEEPROM_MODEL=2 切换为 AT24C02。 */
#ifndef EEPROM_MODEL
#define EEPROM_MODEL 64
#endif
/* 写完一个页之后等器件结束内部写周期，单位毫秒。AT24C64/24C02 都是最大 5 ms，
 * Proteus 的 I2CMEM 模型给的是 6 ms，取 10 留一倍余量。这个值直接决定
 * bsp_i2c_ready() 重试多久，给小了会把「器件忙」当成「器件不在」。 */
#define EEPROM_WRITE_TIMEOUT_MS 10u

#if EEPROM_MODEL == 64
#define EEPROM_CAPACITY 8192u
#define EEPROM_PAGE_SIZE 32u
#define EEPROM_ADDRESS_BITS 16u
#elif EEPROM_MODEL == 2
#define EEPROM_CAPACITY 256u
#define EEPROM_PAGE_SIZE 8u
#define EEPROM_ADDRESS_BITS 8u
#else
#error Unsupported EEPROM_MODEL
#endif

#ifndef EEPROM_I2C_ADDRESS
#define EEPROM_I2C_ADDRESS (0x50u << 1)
#endif
#endif
