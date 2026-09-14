#ifndef GAS_STORE_H
#define GAS_STORE_H
#include "monitor/gas_monitor.h"
#include <stddef.h>
/* Backend must complete writes (including EEPROM ACK polling) before returning. */
typedef bool (*gas_store_read_fn)(void *, uint16_t, uint8_t *, size_t);
typedef bool (*gas_store_write_fn)(void *, uint16_t, const uint8_t *, size_t);
typedef struct {
    void *context;
    gas_store_read_fn read;
    gas_store_write_fn write;
} gas_store_io_t;
bool gas_store_load(const gas_store_io_t *io, gas_config_t *config);
bool gas_store_save(const gas_store_io_t *io, const gas_config_t *config);
#endif
