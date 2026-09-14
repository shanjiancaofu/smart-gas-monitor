#ifndef SETTINGS_H
#define SETTINGS_H
#include "gas/gas_monitor.h"
#include <stdbool.h>
#include <stddef.h>

/* 后端必须在返回前完成写入（包括 EEPROM 的 ACK polling）。 */
typedef bool (*settings_read_fn)(void *, uint16_t, uint8_t *, size_t);
typedef bool (*settings_write_fn)(void *, uint16_t, const uint8_t *, size_t);
typedef struct {
    void *context;
    settings_read_fn read;
    settings_write_fn write;
} settings_io_t;

/* 0x00 和 0x10 处的两份 16 字节副本；取最新且有效的那份。 */
bool settings_load(const settings_io_t *io, gas_config_t *config);
bool settings_save(const settings_io_t *io, const gas_config_t *config);
#endif
