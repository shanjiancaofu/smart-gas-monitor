#include "app.h"
#include <string.h>

#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "alarm/alarm.h"
#include "bsp_adc.h"
#include "bsp_at24c02.h"
#include "bsp_key.h"
#include "bsp_uart.h"
#include "display/display.h"
#include "protocol/protocol.h"

#define APP_TICK_MS GAS_TICK_MS

typedef struct {
    gas_t monitor;
    bsp_adc_t sensor;
    alarm_t alarm;
    bsp_key_t keys;
    bsp_at24c02_t eeprom;
    config_io_t store;
    history_t history;
    display_t display;
    protocol_t protocol;
    /* USB-TTL 适配器和 HC-05 无线链路走同一套协议，因此由同一个解析器驱动。 */
    bsp_uart_t link_usb, link_radio;
    gas_state_t last_state;
    uint32_t last_tick, last_save_attempt;
    bool sensor_ready, storage_ok, last_lockout;
} app_t;

static app_t system_app;

static volatile uint32_t app_ticks;

void app_tick_isr(void)
{
    /* 只对计数加一。转换、显示和 EEPROM 全部留在主循环，因此这个处理函数
     * 不会阻塞在任何一个外设上。 */
    ++app_ticks;
}

/* 给每一路各发一行待发应答，且只在两路都空闲时才发。这样限速，14 条记录
 * 的转储才不会阻塞主循环——在 9600 baud 下，那要花的时间足以看起来像一次
 * 采样器故障。 */
static void pump_replies(app_t *app)
{
    const char *line;
    if (bsp_uart_busy(&app->link_usb) || bsp_uart_busy(&app->link_radio)) {
        return;
    }
    line = protocol_next(&app->protocol);
    if (line == NULL) {
        return;
    }
    (void)bsp_uart_write(&app->link_usb, line);
    (void)bsp_uart_write(&app->link_radio, line);
    (void)bsp_uart_write(&app->link_usb, "\r\n");
    (void)bsp_uart_write(&app->link_radio, "\r\n");
}

/* 两路链接承载相同的命令，所以应答只入队一次，两个口上的流量完全一样。 */
static void poll_links(app_t *app, uint32_t now)
{
    char line[SERIAL_LINE_MAX];
    bsp_uart_poll(&app->link_usb);
    bsp_uart_poll(&app->link_radio);
    if (bsp_uart_read_line(&app->link_usb, line, sizeof(line)) ||
        bsp_uart_read_line(&app->link_radio, line, sizeof(line))) {
        protocol_command(&app->protocol, line, now);
    }
    pump_replies(app);
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
    /* 在别的东西可能失败之前，先把阀门放到它该在的位置。 */
    alarm_init(&app->alarm);
    /* 采样时钟从这里开始运行；采样本身在下面才开始，要等 gas 对象建立
     * 之后。 */
    if (HAL_TIM_Base_Start_IT(tick) != HAL_OK) {
        return false;
    }
    app->last_tick = app_ticks;
    bsp_key_init(&app->keys);
    bsp_at24c02_init(&app->eeprom, eeprom);
    app->sensor_ready = bsp_adc_init(&app->sensor, adc);
    app->store.context = &app->eeprom;
    app->store.read = bsp_at24c02_read;
    app->store.write = bsp_at24c02_write;
    config_defaults(&config);
    app->storage_ok = config_load(&app->store, &config);
    /* 历史区与配置共用同一片 EEPROM，所以放在配置之后打开，且不上报结果：
     * 读不出来的记录就等于空记录。 */
    (void)history_init(&app->history, &app->store);
    now = HAL_GetTick();
    gas_init(&app->monitor, &config, now);
    display_init(&app->display, oled);
    protocol_init(&app->protocol, &app->monitor, &app->history);
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
    /* 对齐的 32 位读在 Cortex-M3 上是原子的，因此这里不需要临界区。所有
     * 可选的周期都是 APP_TICK_MS 的整数倍，所以这个除法是精确的。 */
    uint32_t ticks = app_ticks;
    uint32_t periods = app->monitor.config.sample_period_ms / APP_TICK_MS;
    if ((uint32_t)(ticks - app->last_tick) >= periods) {
        uint16_t values[GAS_COUNT] = {0};
        bool valid;
        /* 按整周期推进，使调度在转换耗时变化时仍保持相位；但主循环停顿超过
         * 一个整周期时重新对齐。 */
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
    uint8_t keys;
    unsigned i;
    keys = bsp_key_poll(&app->keys, now);
    for (i = 0; i < KEY_COUNT; ++i) {
        unsigned key;
        if ((keys & (1u << i)) == 0u) {
            continue;
        }
        key = i + 1u;
        /* 历史界面没有可调项，那里的 KEY2/KEY3 改为翻阅记录，不会传到参数处理逻辑里。 */
        if (app->monitor.selected == GAS_SEL_HISTORY &&
            display_history_key(&app->display, &app->history, key)) {
            continue;
        }
        gas_key(&app->monitor, key, now);
    }
}

static void history_update(app_t *app, uint32_t now)
{
    unsigned i;
    /* 每次报警只写一条记录。报警何时开始由状态机判定，上一次的状态则用来
     * 区分「新报警」和「报警还在持续」，后者不会每个采样周期都写。 */
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
        /* 这个界面正在显示的记录列表刚刚变了。 */
        if (app->monitor.selected == GAS_SEL_HISTORY) {
            app->display.history_index = 0u;
        }
        /* 不等人问就主动推送，这样守着无线链路的手机不必轮询就能知道阀门
         * 已关。 */
        protocol_alarm(&app->protocol);
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
        /* EEPROM 操作是有超时的阻塞事务，做完之后重新评估数据的新鲜度。 */
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
