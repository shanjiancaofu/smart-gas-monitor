#ifndef GAS_MONITOR_H
#define GAS_MONITOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 通道顺序与 mq_sensor_read() 一致；app.c 用断言保证两者相同。 */
typedef enum {
    GAS_MQ4 = 0,
    GAS_MQ7,
    GAS_MQ8,
    GAS_COUNT
} gas_channel_t;

/* 反复按 KEY1 到达的选中项。前三项与通道编号对齐，使 alarm[selected - 1]
 * 取到正确的阈值，gas_monitor_key() 依赖这个范围来约束下标。 */
typedef enum {
    GAS_SEL_MAIN = 0,
    GAS_SEL_MQ4,
    GAS_SEL_MQ7,
    GAS_SEL_MQ8,
    GAS_SEL_PERIOD,
    /* 只供浏览的界面。这里没有可调项，KEY2/KEY3 留给调用者解释为翻阅历史。 */
    GAS_SEL_HISTORY,
    GAS_SEL_COUNT
} gas_setting_t;

/* 设置解码器从 EEPROM 读入时所接受的取值范围。 */
#define GAS_THRESHOLD_MIN 200u
#define GAS_THRESHOLD_MAX 4000u
#define GAS_THRESHOLD_STEP 50u
/* 与按键可选的周期列表中的最小值一致：按键永远到不了的最小值，只会在从
 * EEPROM 载入时才被认可。 */
#define GAS_PERIOD_MIN_MS 100u
#define GAS_PERIOD_MAX_MS 5000u
/* 合成根用来调度采样的 TIM2 节拍。不是整节拍数的周期会被调度成与它声称的
 * 值不同的另一种周期，所以这样的值直接拒绝，而不是悄悄按四舍五入后的相邻
 * 值执行。 */
#define GAS_TICK_MS 10u

#define GAS_WARNING_PERCENT 80u
/* 恢复到预警带以下。按比例取值，使接近范围下端的阈值也仍留有能达到的
 * 安全区。 */
#define GAS_SAFE_PERCENT 70u

#define GAS_SAFE_HOLD_MS 3000u
#define GAS_WARMUP_MS 60000u
#define GAS_SAVE_DELAY_MS 2000u
#define GAS_SAMPLE_TIMEOUT_PERIODS 3u

typedef struct {
    uint16_t alarm[GAS_COUNT];
    uint16_t sample_period_ms;
    bool lockout;
} gas_config_t;

typedef enum { GAS_WARMUP, GAS_NORMAL, GAS_WARNING, GAS_ALARM,
               GAS_SAFE_WAIT, GAS_FAULT, GAS_STATE_COUNT } gas_state_t;

typedef struct {
    gas_config_t config;
    gas_state_t state;
    uint16_t adc[GAS_COUNT];
    uint32_t started_ms, sample_ms, safe_since_ms, changed_ms;
    uint32_t alarm_count;
    uint8_t alarm_mask, selected;
    bool sample_valid, sample_attempted, latched, safe_timing, reset_ready, dirty;
} gas_monitor_t;

/* 显示和串口协议都要给出通道名和掩码名，所以这套拼写只在这里放一份，
 * 而不是各写一份。 */
const char *gas_channel_name(gas_channel_t channel);
/* 短到能放进面板那 16 个字符的一行。 */
const char *gas_state_name(gas_state_t state);
/* "MQ4" / "MQ4+MQ7" / "MQ4+MQ7+MQ8"，空掩码时为 "NONE"。 */
void gas_alarm_mask_name(uint8_t mask, char *out, size_t size);
void gas_config_defaults(gas_config_t *config);
bool gas_config_valid(const gas_config_t *config);
uint16_t gas_warning_threshold(uint16_t alarm);
uint16_t gas_safe_threshold(uint16_t alarm);
/* 采样器落后自己的周期到这个程度，就算停摆。 */
uint32_t gas_sample_timeout_ms(const gas_monitor_t *m);
void gas_monitor_init(gas_monitor_t *m, const gas_config_t *config, uint32_t now);
/* 只传入完整且新鲜的三通道样本。ADC 失败会使全部数据失效。 */
void gas_monitor_sample(gas_monitor_t *m, const uint16_t adc[GAS_COUNT], bool valid, uint32_t now);
void gas_monitor_tick(gas_monitor_t *m, uint32_t now);
/* 消抖后的按下沿，按键 1..4。没有远程开阀的接口。 */
void gas_monitor_key(gas_monitor_t *m, unsigned key, uint32_t now);
/* 来自按键路径之外的配置修改，目前只有串口协议。两者都拒绝设置解码器会
 * 拒绝的值，所以远程写入不可能造出一份开机能存但读不回来的配置。 */
bool gas_monitor_set_threshold(gas_monitor_t *m, gas_channel_t channel,
                               uint16_t value, uint32_t now);
bool gas_monitor_set_period(gas_monitor_t *m, uint16_t ms, uint32_t now);
/* 关闭阀门并锁存。只有 KEY4 能清除锁存，所以这个动作无法远程撤销：故意
 * 不提供远程开阀。 */
void gas_monitor_close_valve(gas_monitor_t *m, uint32_t now);
bool gas_monitor_valve_open(const gas_monitor_t *m);
bool gas_monitor_save_due(const gas_monitor_t *m, uint32_t now);
#endif
