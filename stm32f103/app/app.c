#include "app.h"
#include <string.h>

/* The BSP reads channels in MQ4, MQ7, MQ8 order and gas_channel_t numbers them
 * the same way; a mismatch would silently swap thresholds between sensors. */
_Static_assert(GAS_COUNT == MQ_SENSOR_CHANNELS, "gas channel order must match the BSP");

static void apply_outputs(const gas_monitor_t *m)
{
    bool alarm = m->state == GAS_ALARM || m->state == GAS_FAULT;
    alarm_output_apply(gas_monitor_valve_open(m), m->state == GAS_NORMAL,
                       !alarm && m->state != GAS_NORMAL, alarm);
}

void app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom)
{
    gas_config_t config;
    uint32_t now;
    memset(app, 0, sizeof(*app));
    /* Put the valve where it belongs before anything else can fail. */
    alarm_output_init();
    key_init(&app->keys);
    at24c02_init(&app->eeprom, eeprom);
    app->sensor_ready = mq_sensor_init(&app->sensor, adc);
    app->store.context = &app->eeprom;
    app->store.read = at24c02_read;
    app->store.write = at24c02_write;
    gas_config_defaults(&config);
    app->storage_ok = settings_load(&app->store, &config);
    now = HAL_GetTick();
    gas_monitor_init(&app->monitor, &config, now);
    app->last_sample = now - app->monitor.config.sample_period_ms;
    apply_outputs(&app->monitor);
}

void app_run(app_t *app)
{
    uint32_t now = HAL_GetTick();
    uint8_t keys;
    unsigned i;
    if ((uint32_t)(now - app->last_sample) >= app->monitor.config.sample_period_ms) {
        uint16_t values[MQ_SENSOR_CHANNELS] = {0};
        bool valid = app->sensor_ready && mq_sensor_read(&app->sensor, values);
        now = HAL_GetTick(); app->last_sample = now;
        gas_monitor_sample(&app->monitor, values, valid, now);
    }
    gas_monitor_tick(&app->monitor, now);
    keys = key_poll(&app->keys, now);
    for (i = 0; i < KEY_COUNT; ++i)
        if (keys & (1u << i)) gas_monitor_key(&app->monitor, i + 1u, now);
    apply_outputs(&app->monitor);
    if (gas_monitor_save_due(&app->monitor, now) &&
        (uint32_t)(now - app->last_save_attempt) >= GAS_SAVE_DELAY_MS) {
        app->last_save_attempt = now;
        app->storage_ok = settings_save(&app->store, &app->monitor.config);
        if (app->storage_ok) app->monitor.dirty = false;
        /* Re-evaluate freshness after bounded, blocking EEPROM operations. */
        gas_monitor_tick(&app->monitor, HAL_GetTick());
        apply_outputs(&app->monitor);
    }
}
