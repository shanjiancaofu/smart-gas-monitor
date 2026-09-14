#include "app.h"
#include <string.h>

/* The BSP reads channels in MQ4, MQ7, MQ8 order and gas_channel_t numbers them
 * the same way; a mismatch would silently swap thresholds between sensors. */
_Static_assert(GAS_COUNT == MQ_SENSOR_CHANNELS, "gas channel order must match the BSP");

/* TIM2 fires every APP_TICK_MS. It is the scheduling clock for sampling only;
 * HAL_GetTick() remains the time base for the state machine, for timeouts and
 * for how long the unit has been running. */
#define APP_TICK_MS 10u

static volatile uint32_t app_ticks;

void app_tick_isr(void)
{
    /* Only a counter. The conversion, the display and the EEPROM all stay in the
     * main loop, so this handler never blocks on a peripheral. */
    ++app_ticks;
}

static void apply_outputs(const gas_monitor_t *m)
{
    bool alarm = m->state == GAS_ALARM || m->state == GAS_FAULT;
    alarm_output_apply(gas_monitor_valve_open(m), m->state == GAS_NORMAL,
                       !alarm && m->state != GAS_NORMAL, alarm);
}

void app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom,
              TIM_HandleTypeDef *tick)
{
    gas_config_t config;
    uint32_t now;
    memset(app, 0, sizeof(*app));
    /* Put the valve where it belongs before anything else can fail. */
    alarm_output_init();
    /* The sample clock runs from here on; sampling itself starts further down,
     * once the monitor exists. */
    HAL_TIM_Base_Start_IT(tick);
    app->last_tick = app_ticks;
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
    apply_outputs(&app->monitor);
}

void app_run(app_t *app)
{
    uint32_t now = HAL_GetTick();
    /* An aligned 32-bit read is atomic on Cortex-M3, so this needs no critical
     * section. Every selectable period is a multiple of APP_TICK_MS, so the
     * division is exact. */
    uint32_t ticks = app_ticks;
    uint32_t periods = app->monitor.config.sample_period_ms / APP_TICK_MS;
    uint8_t keys;
    unsigned i;
    if ((uint32_t)(ticks - app->last_tick) >= periods) {
        uint16_t values[MQ_SENSOR_CHANNELS] = {0};
        bool valid;
        /* Advance by whole periods so the schedule keeps its phase across the
         * variable time a conversion takes, but resync if the main loop stalled
         * for longer than one whole period. */
        app->last_tick += periods;
        if ((uint32_t)(ticks - app->last_tick) >= periods) app->last_tick = ticks;
        valid = app->sensor_ready && mq_sensor_read(&app->sensor, values);
        now = HAL_GetTick();
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
