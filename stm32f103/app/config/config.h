#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GAS_COUNT 3u

/* 设置解码器从 EEPROM 读入时所接受的取值范围。 */
#define GAS_THRESHOLD_MIN 200u
#define GAS_THRESHOLD_MAX 4000u
#define GAS_THRESHOLD_STEP 50u
/* 与按键可选的周期列表中的最小值一致：按键永远到不了的最小值，只会在从
 * EEPROM 载入时才被认可。 */
#define GAS_PERIOD_MIN_MS 100u
#define GAS_PERIOD_MAX_MS 5000u
/* 应用层用来调度采样的 TIM2 节拍。不是整节拍数的周期会被调度成与它声称的
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

/* 报警蜂鸣器响多久。三个取值排在同一个数轴上——「不响」最短，「一直响」最长
 * ——所以按键就是加减两端夹取，不需要单独一套档位表。 */
#define GAS_BUZZER_OFF 0u
#define GAS_BUZZER_MAX_S 60u
#define GAS_BUZZER_ALWAYS (GAS_BUZZER_MAX_S + 1u)
/* 持续鸣响的毫秒哨兵值，由 alarm 模块解释。 */
#define GAS_BUZZER_FOREVER_MS 0xffffu
/* 最长的秒数换算成毫秒之后必须仍然小于哨兵值，否则「70 秒」这种值会撞上
 * 「一直响」的编码。日后把上限调大时，这里先编译不过，而不是让两个含义悄悄
 * 重合。 */
_Static_assert(GAS_BUZZER_MAX_S * 1000u < GAS_BUZZER_FOREVER_MS,
               "the longest buzzer duration must stay below the forever sentinel");

typedef struct {
    uint16_t alarm[GAS_COUNT];
    uint16_t sample_period_ms;
    /* GAS_BUZZER_OFF / 1..GAS_BUZZER_MAX_S 秒 / GAS_BUZZER_ALWAYS。 */
    uint8_t buzzer;
    bool lockout;
} gas_config_t;

/* 后端必须在返回前完成写入（包括 EEPROM 的 ACK polling）。 */
typedef bool (*config_read_fn)(void *, uint16_t, uint8_t *, size_t);
typedef bool (*config_write_fn)(void *, uint16_t, const uint8_t *, size_t);
typedef struct {
    void *context;
    config_read_fn read;
    config_write_fn write;
} config_io_t;

/* 0x00 和 0x10 处的两份 16 字节副本；取最新且有效的那份。 */
bool config_load(const config_io_t *io, gas_config_t *config);
bool config_save(const config_io_t *io, const gas_config_t *config);
void config_defaults(gas_config_t *config);
bool config_valid(const gas_config_t *config);
void config_buzzer_name(uint8_t value, char *out, size_t size);
uint16_t config_buzzer_duration_ms(const gas_config_t *config);
#endif
