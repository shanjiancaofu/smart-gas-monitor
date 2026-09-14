#include "app.h"
#include <string.h>

/* BSP 按 MQ4、MQ7、MQ8 的顺序读取通道，gas_channel_t 也用同样的顺序编号；
 * 两者不一致时，阈值会被悄悄换到别的传感器上。 */
_Static_assert(GAS_COUNT == MQ_SENSOR_CHANNELS, "gas channel order must match the BSP");

/* TIM2 每 APP_TICK_MS 触发一次。它只是采样调度用的时钟；状态机的时间基准
 * 仍是 HAL_GetTick()，超时和运行时长都用它。 */
#define APP_TICK_MS 10u

/* 蜂鸣器窗口把毫秒换算成节拍，用的是 BSP 的 BUZZER_TICK_MS，所以那个常量
 * 必须和这里的实际节拍一致；「一直响」的哨兵值两层各定义了一份。这些都是
 * 分层要求（BSP 不能反向依赖 app），一致性只能在同时看得见两边的这里钉住。 */
_Static_assert(BUZZER_TICK_MS == APP_TICK_MS,
               "the buzzer window assumes the TIM2 tick period");
_Static_assert(BUZZER_FOREVER == GAS_BUZZER_FOREVER_MS,
               "gas_monitor and alarm_output disagree on the forever sentinel");

static volatile uint32_t app_ticks;

void app_tick_isr(void)
{
    /* 只对计数加一。转换、显示和 EEPROM 全部留在主循环，因此这个处理函数
     * 不会阻塞在任何一个外设上。 */
    ++app_ticks;
}

static void apply_outputs(const gas_monitor_t *m, uint32_t tick)
{
    bool alarm = m->state == GAS_ALARM || m->state == GAS_FAULT;
    alarm_output_apply(gas_monitor_valve_open(m), m->state == GAS_NORMAL,
                       !alarm && m->state != GAS_NORMAL, alarm,
                       gas_buzzer_duration_ms(&m->config), tick);
}

/* 给每一路各发一行待发应答，且只在两路都空闲时才发。这样限速，14 条记录
 * 的转储才不会阻塞主循环——在 9600 baud 下，那要花的时间足以看起来像一次
 * 采样器故障。 */
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

/* 两路链接承载相同的命令，所以应答只入队一次，两个口上的流量完全一样。 */
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

static void persist_lockout_edge(app_t *app)
{
    bool lockout = app->monitor.config.lockout;
    if (lockout == app->last_lockout) return;
    app->last_lockout = lockout;
    app->storage_ok = settings_save(&app->store, &app->monitor.config);
    if (app->storage_ok) app->monitor.dirty = false;
}

bool app_init(app_t *app, ADC_HandleTypeDef *adc, I2C_HandleTypeDef *eeprom,
              I2C_HandleTypeDef *oled, TIM_HandleTypeDef *tick,
              UART_HandleTypeDef *usb, UART_HandleTypeDef *radio)
{
    gas_config_t config;
    uint32_t now;
    memset(app, 0, sizeof(*app));
    /* 在别的东西可能失败之前，先把阀门放到它该在的位置。 */
    alarm_output_init();
    /* 采样时钟从这里开始运行；采样本身在下面才开始，要等 gas_monitor 对象
     * 建立之后。 */
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
    /* 历史区与配置共用同一片 EEPROM，所以放在配置之后打开，且不上报结果：
     * 读不出来的记录就等于空记录。 */
    (void)history_init(&app->history, &app->store);
    now = HAL_GetTick();
    gas_monitor_init(&app->monitor, &config, now);
    display_init(&app->display, oled);
    protocol_init(&app->protocol, &app->monitor, &app->history);
    serial_init(&app->link_usb, usb);
    serial_init(&app->link_radio, radio);
    app->last_state = app->monitor.state;
    app->last_lockout = app->monitor.config.lockout;
    apply_outputs(&app->monitor, app_ticks);
    return true;
}

void app_run(app_t *app)
{
    uint32_t now = HAL_GetTick();
    /* 对齐的 32 位读在 Cortex-M3 上是原子的，因此这里不需要临界区。所有
     * 可选的周期都是 APP_TICK_MS 的整数倍，所以这个除法是精确的。 */
    uint32_t ticks = app_ticks;
    uint32_t periods = app->monitor.config.sample_period_ms / APP_TICK_MS;
    uint8_t keys;
    unsigned i;
    if ((uint32_t)(ticks - app->last_tick) >= periods) {
        uint16_t values[MQ_SENSOR_CHANNELS] = {0};
        bool valid;
        /* 按整周期推进，使调度在转换耗时变化时仍保持相位；但主循环停顿超过
         * 一个整周期时重新对齐。 */
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
        /* 历史界面没有可调项，那里的 KEY2/KEY3 改为翻阅记录，不会传到参数处理逻辑里。 */
        if (app->monitor.selected == GAS_SEL_HISTORY &&
            display_history_key(&app->display, &app->history, key)) continue;
        gas_monitor_key(&app->monitor, key, now);
    }
    /* 先处理远程 CLOSE，再应用输出并持久化状态变化沿。 */
    poll_links(app, now);
    persist_lockout_edge(app);
    apply_outputs(&app->monitor, app_ticks);
    /* 每次报警只写一条记录。报警何时开始由状态机判定，上一次的状态则用来
     * 区分「新报警」和「报警还在持续」，后者不会每个采样周期都写。 */
    if (app->monitor.state == GAS_ALARM && app->last_state != GAS_ALARM) {
        history_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.uptime_s = now / 1000u;
        for (i = 0; i < GAS_COUNT; ++i) entry.adc[i] = app->monitor.adc[i];
        entry.alarm_mask = app->monitor.alarm_mask;
        if (!history_append(&app->history, &entry)) app->storage_ok = false;
        /* 这个界面正在显示的记录列表刚刚变了。 */
        if (app->monitor.selected == GAS_SEL_HISTORY) app->display.history_index = 0u;
        /* 不等人问就主动推送，这样守着无线链路的手机不必轮询就能知道阀门
         * 已关。 */
        protocol_alarm(&app->protocol);
    }
    app->last_state = app->monitor.state;
    display_update(&app->display, &app->monitor, &app->history, app->storage_ok, now);
    if (gas_monitor_save_due(&app->monitor, now) &&
        (uint32_t)(now - app->last_save_attempt) >= GAS_SAVE_DELAY_MS) {
        app->last_save_attempt = now;
        app->storage_ok = settings_save(&app->store, &app->monitor.config);
        if (app->storage_ok) app->monitor.dirty = false;
        /* EEPROM 操作是有超时的阻塞事务，做完之后重新评估数据的新鲜度。 */
        now = HAL_GetTick();
        gas_monitor_tick(&app->monitor, now);
        apply_outputs(&app->monitor, app_ticks);
    }
}
