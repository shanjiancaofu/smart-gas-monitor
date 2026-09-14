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

static void apply_outputs(const gas_monitor_t *m, uint32_t tick)
{
    bool alarm = m->state == GAS_ALARM || m->state == GAS_FAULT;
    alarm_output_apply(gas_monitor_valve_open(m), m->state == GAS_NORMAL,
                       !alarm && m->state != GAS_NORMAL, alarm, tick);
}

/* Sends one line of the pending response to every link, and only when all of
 * them are free. Pacing it this way is what keeps a fourteen-record dump from
 * blocking the main loop, which at 9600 baud would otherwise take long enough
 * to look like a sampler fault. */
static void pump_replies(app_t *app)
{
    const char *line;
    if (serial_busy(&app->link_usb) || serial_busy(&app->link_radio)) return;
    line = protocol_next(&app->protocol);
    if (line == NULL) return;
    (void)serial_write(&app->link_usb, line);
    (void)serial_write(&app->link_radio, line);
    (void)serial_write(&app->link_usb, "\r\n");
    (void)serial_write(&app->link_radio, "\r\n");
}

/* Both links carry the same commands, so a reply is only ever queued once and
 * the two ports see identical traffic. */
static void poll_links(app_t *app, uint32_t now)
{
    char line[SERIAL_LINE_MAX];
    serial_poll(&app->link_usb);
    serial_poll(&app->link_radio);
    if (serial_read_line(&app->link_usb, line, sizeof(line)) ||
        serial_read_line(&app->link_radio, line, sizeof(line)))
        protocol_command(&app->protocol, line, now);
    pump_replies(app);
}

bool app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom,
              I2C_HandleTypeDef *oled, TIM_HandleTypeDef *tick,
              UART_HandleTypeDef *usb, UART_HandleTypeDef *radio)
{
    gas_config_t config;
    uint32_t now;
    memset(app, 0, sizeof(*app));
    /* Put the valve where it belongs before anything else can fail. */
    alarm_output_init();
    /* The sample clock runs from here on; sampling itself starts further down,
     * once the monitor exists. */
    if (HAL_TIM_Base_Start_IT(tick) != HAL_OK) return false;
    app->last_tick = app_ticks;
    key_init(&app->keys);
    at24c02_init(&app->eeprom, eeprom);
    app->sensor_ready = mq_sensor_init(&app->sensor, adc);
    app->store.context = &app->eeprom;
    app->store.read = at24c02_read;
    app->store.write = at24c02_write;
    gas_config_defaults(&config);
    app->storage_ok = settings_load(&app->store, &config);
    /* The log shares the EEPROM with the settings, so it is opened after them
     * and reports nothing: an unreadable log is an empty log. */
    (void)history_init(&app->history, &app->store);
    now = HAL_GetTick();
    gas_monitor_init(&app->monitor, &config, now);
    display_init(&app->display, oled);
    protocol_init(&app->protocol, &app->monitor, &app->history);
    serial_init(&app->link_usb, usb);
    serial_init(&app->link_radio, radio);
    app->last_state = app->monitor.state;
    apply_outputs(&app->monitor, app_ticks);
    return true;
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
    for (i = 0; i < KEY_COUNT; ++i) {
        unsigned key;
        if ((keys & (1u << i)) == 0u) continue;
        key = i + 1u;
        /* The history page has nothing adjustable, so KEY2/KEY3 browse the log
         * there instead of reaching the settings handler. */
        if (app->monitor.selected == GAS_SEL_HISTORY &&
            display_history_key(&app->display, &app->history, key)) continue;
        gas_monitor_key(&app->monitor, key, now);
    }
    apply_outputs(&app->monitor, app_ticks);
    /* One record per alarm episode. The state machine is the authority on when
     * an alarm starts, and the previous state is what makes a persisting alarm
     * write once rather than on every sample. */
    if (app->monitor.state == GAS_ALARM && app->last_state != GAS_ALARM) {
        history_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.uptime_s = now / 1000u;
        for (i = 0; i < GAS_COUNT; ++i) entry.adc[i] = app->monitor.adc[i];
        entry.alarm_mask = app->monitor.alarm_mask;
        if (!history_append(&app->history, &entry)) app->storage_ok = false;
        if (!settings_save(&app->store, &app->monitor.config)) app->storage_ok = false;
        else app->monitor.dirty = false;
        /* The page is showing a record list that just changed underneath it. */
        if (app->monitor.selected == GAS_SEL_HISTORY) app->display.history_index = 0u;
        /* Told without being asked, so a phone watching the radio link does not
         * have to poll to find out the valve has shut. */
        protocol_alarm(&app->protocol);
    }
    app->last_state = app->monitor.state;
    display_update(&app->display, &app->monitor, &app->history, app->storage_ok, now);
    poll_links(app, HAL_GetTick());
    if (gas_monitor_save_due(&app->monitor, now) &&
        (uint32_t)(now - app->last_save_attempt) >= GAS_SAVE_DELAY_MS) {
        app->last_save_attempt = now;
        app->storage_ok = settings_save(&app->store, &app->monitor.config);
        if (app->storage_ok) app->monitor.dirty = false;
        /* Re-evaluate freshness after bounded, blocking EEPROM operations. */
        now = HAL_GetTick();
        gas_monitor_tick(&app->monitor, now);
        apply_outputs(&app->monitor, app_ticks);
    }
}
