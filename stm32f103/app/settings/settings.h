#ifndef SETTINGS_H
#define SETTINGS_H
#include "gas/gas_monitor.h"
#include <stdbool.h>
#include <stddef.h>

/* Backend must complete writes (including EEPROM ACK polling) before returning. */
typedef bool (*settings_read_fn)(void *, uint16_t, uint8_t *, size_t);
typedef bool (*settings_write_fn)(void *, uint16_t, const uint8_t *, size_t);
typedef struct {
    void *context;
    settings_read_fn read;
    settings_write_fn write;
} settings_io_t;

/* Two 16-byte copies at 0x00 and 0x10; the newest valid one wins. */
bool settings_load(const settings_io_t *io, gas_config_t *config);
bool settings_save(const settings_io_t *io, const gas_config_t *config);
#endif
