#ifndef APP_H
#define APP_H
#include "alarm_output.h"
#include "at24c02.h"
#include "communication/protocol.h"
#include "display/display.h"
#include "gas/gas_monitor.h"
#include "history/history.h"
#include "key.h"
#include "mq_sensor.h"
#include "serial.h"
#include "settings/settings.h"

/* Composition root: owns every module and the scheduling between them. */
typedef struct {
    gas_monitor_t monitor;
    mq_sensor_t sensor;
    key_t keys;
    at24c02_t eeprom;
    settings_io_t store;
    history_t history;
    display_t display;
    protocol_t protocol;
    /* The USB-TTL adapter and the HC-05 radio carry the same protocol, so both
     * are driven from the one parser. */
    serial_t link_usb, link_radio;
    gas_state_t last_state;
    uint32_t last_tick, last_save_attempt;
    bool sensor_ready, storage_ok;
} app_t;

/* The EEPROM and the panel are on separate I2C buses, so they are passed
 * separately rather than as one "i2c" handle. */
void app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom,
              I2C_HandleTypeDef *oled, TIM_HandleTypeDef *tick,
              UART_HandleTypeDef *usb, UART_HandleTypeDef *radio);
/* One pass of the bare-metal superloop. */
void app_run(app_t *app);
/* Called from TIM2_IRQHandler every 10 ms. Counts ticks and nothing else. */
void app_tick_isr(void);
#endif
