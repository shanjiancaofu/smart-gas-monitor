#ifndef APP_H
#define APP_H
#include "alarm_output.h"
#include "at24c02.h"
#include "gas/gas_monitor.h"
#include "key.h"
#include "mq_sensor.h"
#include "settings/settings.h"

/* Composition root: owns every module and the scheduling between them. */
typedef struct {
    gas_monitor_t monitor;
    mq_sensor_t sensor;
    key_t keys;
    at24c02_t eeprom;
    settings_io_t store;
    uint32_t last_sample, last_save_attempt;
    bool sensor_ready, storage_ok;
} app_t;

void app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom);
/* One pass of the bare-metal superloop. */
void app_run(app_t *app);
#endif
