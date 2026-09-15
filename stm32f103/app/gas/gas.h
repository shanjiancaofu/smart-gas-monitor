#ifndef GAS_H
#define GAS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config/config.h"

/* 气体编号顺序固定为 MQ4、MQ7、MQ8，与 gas_read_samples 的映射一致。 */
typedef enum { GAS_MQ4 = 0, GAS_MQ7, GAS_MQ8 } gas_channel_t;

/* 反复按 KEY1 到达的选中项。前三项与通道编号对齐，使 alarm[selected - 1]
 * 取到正确的阈值，gas_key() 依赖这个范围来约束下标。 */
typedef enum {
    GAS_SEL_MAIN = 0,
    GAS_SEL_MQ4,
    GAS_SEL_MQ7,
    GAS_SEL_MQ8,
    GAS_SEL_PERIOD,
    GAS_SEL_BUZZER,
    /* 只供浏览的界面。这里没有可调项，KEY2/KEY3 留给调用者解释为翻阅历史。 */
    GAS_SEL_HISTORY,
    GAS_SEL_COUNT
} gas_setting_t;

typedef enum {
    GAS_WARMUP,
    GAS_NORMAL,
    GAS_WARNING,
    GAS_ALARM,
    GAS_SAFE_WAIT,
    GAS_FAULT,
    GAS_STATE_COUNT
} gas_state_t;

typedef struct {
    gas_config_t config;
    gas_state_t state;
    uint16_t adc[GAS_COUNT];
    uint32_t started_ms, sample_ms, safe_since_ms, changed_ms;
    uint32_t alarm_count;
    uint8_t alarm_mask, selected;
    bool sample_valid, sample_attempted, latched, safe_timing, reset_ready, dirty;
} gas_t;

/* 显示和串口协议都要给出通道名和掩码名，所以这套拼写只在这里放一份，
 * 而不是各写一份。 */
const char *gas_channel_name(gas_channel_t channel);
/* 短到能放进面板那 16 个字符的一行。 */
const char *gas_state_name(gas_state_t state);
/* "MQ4" / "MQ4+MQ7" / "MQ4+MQ7+MQ8"，空掩码时为 "NONE"。 */
void gas_alarm_mask_name(uint8_t mask, char *out, size_t size);
uint16_t gas_warning_threshold(uint16_t alarm);
uint16_t gas_safe_threshold(uint16_t alarm);
/* 采样器落后自己的周期到这个程度，就算停摆。 */
uint32_t gas_sample_timeout_ms(const gas_t *m);
void gas_init(gas_t *m, const gas_config_t *config, uint32_t now);
/* 只传入完整且新鲜的三通道样本。ADC 失败会使全部数据失效。 */
void gas_sample(gas_t *m, const uint16_t adc[GAS_COUNT], bool valid, uint32_t now);
void gas_update(gas_t *m, uint32_t now);
/* 消抖后的按下沿，按键 1..4。没有远程开阀的接口。 */
void gas_key(gas_t *m, unsigned key, uint32_t now);
/* 来自按键路径之外的配置修改，目前只有串口协议。两者都拒绝设置解码器会
 * 拒绝的值，所以远程写入不可能造出一份开机能存但读不回来的配置。 */
bool gas_set_threshold(gas_t *m, gas_channel_t channel, uint16_t value, uint32_t now);
bool gas_set_period(gas_t *m, uint16_t ms, uint32_t now);
/* 取 uint16 而不是字段本身的 uint8：超范围的值必须在收窄之前就被拒绝，否则
 * 256 会截断成 0，静默变成「不响」。 */
bool gas_set_buzzer(gas_t *m, uint16_t value, uint32_t now);
/* 关闭阀门并锁存。只有 KEY4 能清除锁存，所以这个动作无法远程撤销：故意
 * 不提供远程开阀。 */
void gas_close_valve(gas_t *m, uint32_t now);
bool gas_valve_open(const gas_t *m);
bool gas_save_due(const gas_t *m, uint32_t now);
/* 应用层规定 MQ 对应的 ADC 通道和八次平均规则；读取接口可由 HAL 或测试桩提供。 */
typedef bool (*gas_adc_read_fn)(void *, uint8_t, uint16_t *);
bool gas_read_samples(gas_adc_read_fn read, void *context, uint16_t values[GAS_COUNT]);
uint16_t gas_get_value(const gas_t *gas, uint8_t id);
gas_state_t gas_get_state(const gas_t *gas);
#endif
