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

/* 合成根：持有所有模块以及模块之间的调度。 */
typedef struct {
    gas_monitor_t monitor;
    mq_sensor_t sensor;
    key_t keys;
    at24c02_t eeprom;
    settings_io_t store;
    history_t history;
    display_t display;
    protocol_t protocol;
    /* USB-TTL 适配器和 HC-05 无线链路走同一套协议，因此由同一个解析器驱动。 */
    serial_t link_usb, link_radio;
    gas_state_t last_state;
    uint32_t last_tick, last_save_attempt;
    bool sensor_ready, storage_ok, last_lockout;
} app_t;

/* EEPROM 和面板挂在不同的 I2C 总线上，所以分开传入，而不是合并成一个
 * 「i2c」句柄。 */
bool app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom,
              I2C_HandleTypeDef *oled, TIM_HandleTypeDef *tick,
              UART_HandleTypeDef *usb, UART_HandleTypeDef *radio);
/* 裸机超级循环的一轮。 */
void app_run(app_t *app);
/* 每 10 ms 由 TIM2_IRQHandler 调用一次。只累加节拍，不做别的。 */
void app_tick_isr(void);
#endif
