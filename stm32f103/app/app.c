#include "app.h"
#include <string.h>

#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "alarm/alarm.h"
#include "bsp_adc.h"
#include "bsp_eeprom.h"
#include "bsp_key.h"
#include "bsp_uart.h"
#include "display/display.h"
#include "protocol/protocol.h"

#define APP_TICK_MS GAS_TICK_MS

#if !defined(__ARMCC_VERSION)
_Static_assert((unsigned)KEY_PAGE + 1u == (unsigned)GAS_KEY_PAGE &&
                   (unsigned)KEY_UP + 1u == (unsigned)GAS_KEY_UP &&
                   (unsigned)KEY_DOWN + 1u == (unsigned)GAS_KEY_DOWN &&
                   (unsigned)KEY_CONFIRM + 1u == (unsigned)GAS_KEY_CONFIRM &&
                   (unsigned)KEY_SELECT + 1u == (unsigned)GAS_KEY_SELECT &&
                   (unsigned)KEY_EVENT_COUNT == (unsigned)GAS_KEY_SELECT,
               "BSP and application key roles must stay aligned");
#endif

typedef struct {
    gas_t monitor;
    bsp_adc_t sensor;
    alarm_t alarm;
    bsp_key_t keys;
    bsp_eeprom_t eeprom;
    config_io_t store;
    history_t history;
    bsp_oled_t oled;
    display_t display;
    protocol_t protocol_usb, protocol_radio;

    bsp_uart_t link_usb, link_radio;
    gas_state_t last_state;
    uint32_t last_tick, last_save_attempt;
    bool sensor_ready, storage_ok, last_lockout;
} app_t;

static app_t system_app;

static volatile uint32_t app_ticks;

void app_tick_isr(void)
{

    ++app_ticks;
}

static void pump_reply(protocol_t *protocol, bsp_uart_t *uart)
{
    const char *line;
    if (bsp_uart_busy(uart)) {
        return;
    }
    line = protocol_next(protocol);
    if (line == NULL) {
        return;
    }
    (void)bsp_uart_write(uart, line);
    (void)bsp_uart_write(uart, "\r\n");
}

static void poll_links(app_t *app, uint32_t now)
{
    char line[SERIAL_LINE_MAX];
    bsp_uart_poll(&app->link_usb);
    bsp_uart_poll(&app->link_radio);
    if (bsp_uart_read_line(&app->link_usb, line, sizeof(line))) {
        protocol_command(&app->protocol_usb, line, now);
    }
    if (bsp_uart_read_line(&app->link_radio, line, sizeof(line))) {
        protocol_command(&app->protocol_radio, line, now);
    }
    pump_reply(&app->protocol_usb, &app->link_usb);
    pump_reply(&app->protocol_radio, &app->link_radio);
}

static void persist_lockout_edge(app_t *app)
{
    bool lockout = app->monitor.config.lockout;
    if (lockout == app->last_lockout) {
        return;
    }
    app->last_lockout = lockout;
    app->storage_ok = config_save(&app->store, &app->monitor.config);
    if (app->storage_ok) {
        app->monitor.dirty = false;
    }
}

bool app_init(void)
{
    app_t *app = &system_app;
    ADC_HandleTypeDef *adc = &hadc1;
    I2C_HandleTypeDef *eeprom = &hi2c2;
    I2C_HandleTypeDef *oled = &hi2c1;
    TIM_HandleTypeDef *tick = &htim2;
    UART_HandleTypeDef *usb = &huart1;
    UART_HandleTypeDef *radio = &huart2;
    gas_config_t config;
    uint32_t now;
    memset(app, 0, sizeof(*app));

    alarm_init(&app->alarm);

    if (HAL_TIM_Base_Start_IT(tick) != HAL_OK) {
        return false;
    }
    app->last_tick = app_ticks;
    /* 面板先点起来。后面读 EEPROM 要走一串可能超时的 I2C，屏幕不该跟着
     * 黑着等——EEPROM 没接时那段能到十几秒。 */
    (void)bsp_oled_init(&app->oled, oled);
    display_init(&app->display, &app->oled);
    bsp_key_init(&app->keys);
    bsp_eeprom_init(&app->eeprom, eeprom);
    app->sensor_ready = bsp_adc_init(&app->sensor, adc);
    app->store.context = &app->eeprom;
    app->store.read = bsp_eeprom_read;
    app->store.write = bsp_eeprom_write;
    config_defaults(&config);
    config_load_status_t load_status = config_load_status(&app->store, &config);
    app->storage_ok = load_status == CONFIG_LOAD_OK;
    if (load_status == CONFIG_LOAD_EMPTY) {
        /* A blank EEPROM is a first boot, not a hardware failure. Persist defaults. */
        app->storage_ok = config_save(&app->store, &config);
    }

    (void)history_init(&app->history, &app->store);
    now = HAL_GetTick();
    gas_init(&app->monitor, &config, now);

    protocol_init(&app->protocol_usb, &app->monitor, &app->history);
    protocol_init(&app->protocol_radio, &app->monitor, &app->history);
    bsp_uart_init(&app->link_usb, usb);
    bsp_uart_init(&app->link_radio, radio);
    app->last_state = app->monitor.state;
    app->last_lockout = app->monitor.config.lockout;
    alarm_update(&app->alarm, &app->monitor, app_ticks);
    return true;
}

static uint32_t sample_update(app_t *app)
{
    uint32_t now = HAL_GetTick();

    uint32_t ticks = app_ticks;
    uint32_t periods = app->monitor.config.sample_period_ms / APP_TICK_MS;
    if ((uint32_t)(ticks - app->last_tick) >= periods) {
        uint16_t values[GAS_COUNT] = {0};
        bool valid;

        app->last_tick += periods;
        if ((uint32_t)(ticks - app->last_tick) >= periods) {
            app->last_tick = ticks;
        }
        valid = app->sensor_ready && gas_read_samples(bsp_adc_read, &app->sensor, values);
        now = HAL_GetTick();
        gas_sample(&app->monitor, values, valid, now);
    }
    gas_update(&app->monitor, now);
    return now;
}

static void keys_update(app_t *app, uint32_t now)
{
    uint8_t keys = bsp_key_poll(&app->keys, now);
    unsigned i;

    for (i = 0; i < KEY_COUNT; ++i) {
        if (keys & (1u << i)) {
            display_key(&app->display, &app->monitor, &app->history, i + 1u, now);
        }
    }
}

static void history_update(app_t *app, uint32_t now)
{
    unsigned i;

    if (app->monitor.state == GAS_ALARM && app->last_state != GAS_ALARM) {
        history_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.uptime_s = now / 1000u;
        for (i = 0; i < GAS_COUNT; ++i) {
            entry.adc[i] = app->monitor.adc[i];
        }
        entry.alarm_mask = app->monitor.alarm_mask;
        if (!history_add(&app->history, &entry)) {
            app->storage_ok = false;
        }

        if (app->display.page == DISPLAY_HISTORY) {
            app->display.history_index = 0u;
        }

        protocol_alarm(&app->protocol_usb);
        protocol_alarm(&app->protocol_radio);
    }
    app->last_state = app->monitor.state;
}

static void config_update(app_t *app, uint32_t now)
{
    if (gas_save_due(&app->monitor, now) &&
        (uint32_t)(now - app->last_save_attempt) >= GAS_SAVE_DELAY_MS) {
        app->last_save_attempt = now;
        app->storage_ok = config_save(&app->store, &app->monitor.config);
        if (app->storage_ok) {
            app->monitor.dirty = false;
        }

        now = HAL_GetTick();
        gas_update(&app->monitor, now);
        alarm_update(&app->alarm, &app->monitor, app_ticks);
    }
}

void app_update(void)
{
    app_t *app = &system_app;
    uint32_t now = sample_update(app);

    keys_update(app, now);
    poll_links(app, now);
    alarm_update(&app->alarm, &app->monitor, app_ticks);
    persist_lockout_edge(app);
    history_update(app, now);
    display_update(&app->display, &app->monitor, &app->history, app->storage_ok, now);
    config_update(app, now);
}

