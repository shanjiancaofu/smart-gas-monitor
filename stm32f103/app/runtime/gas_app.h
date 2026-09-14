#ifndef GAS_APP_H
#define GAS_APP_H
#include "gas_board.h"
#include "monitor/gas_monitor.h"
#include "parameters/gas_store.h"
typedef struct {
    gas_monitor_t monitor;
    gas_board_t board;
    gas_store_io_t store;
    uint32_t last_sample, last_save_attempt;
    bool adc_ready, storage_ok;
} gas_app_t;
void gas_app_init(gas_app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom);
void gas_app_poll(gas_app_t *app);
#endif
