#include "runtime/gas_app.h"
#include <string.h>

static void apply_outputs(const gas_monitor_t *m)
{
    bool alarm = m->state == GAS_ALARM || m->state == GAS_FAULT;
    gas_board_outputs(gas_monitor_valve_open(m), m->state == GAS_NORMAL,
                      !alarm && m->state != GAS_NORMAL, alarm);
}

void gas_app_init(gas_app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom)
{
    gas_config_t config;
    memset(app, 0, sizeof(*app));
    app->adc_ready = gas_board_init(&app->board, adc, eeprom);
    app->store.context = &app->board;
    app->store.read = gas_board_eeprom_read;
    app->store.write = gas_board_eeprom_write;
    gas_config_defaults(&config);
    app->storage_ok = gas_store_load(&app->store, &config);
    gas_monitor_init(&app->monitor, &config, HAL_GetTick());
    app->last_sample = HAL_GetTick() - 100u;
    apply_outputs(&app->monitor);
}
void gas_app_poll(gas_app_t *app)
{
    uint32_t now = HAL_GetTick();
    uint8_t keys;
    unsigned i;
    if ((uint32_t)(now - app->last_sample) >= 100u) {
        uint16_t values[3] = {0};
        bool valid = app->adc_ready && gas_board_sample(&app->board, values);
        now = HAL_GetTick(); app->last_sample = now;
        gas_monitor_sample(&app->monitor, values, valid, now);
    }
    gas_monitor_tick(&app->monitor, now);
    keys = gas_board_keys(&app->board, now);
    for (i = 0; i < 4; ++i)
        if (keys & (1u << i)) gas_monitor_key(&app->monitor, i + 1u, now);
    apply_outputs(&app->monitor);
    if (gas_monitor_save_due(&app->monitor, now) &&
        (uint32_t)(now - app->last_save_attempt) >= GAS_SAVE_DELAY_MS) {
        app->last_save_attempt = now;
        app->storage_ok = gas_store_save(&app->store, &app->monitor.config);
        if (app->storage_ok) app->monitor.dirty = false;
        /* Re-evaluate freshness after bounded, blocking EEPROM operations. */
        gas_monitor_tick(&app->monitor, HAL_GetTick());
        apply_outputs(&app->monitor);
    }
}
