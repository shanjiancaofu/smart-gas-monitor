#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bsp_eeprom_config.h"

#define GAS_COUNT 3u

/* Comment normalized for portability. */
#define GAS_THRESHOLD_MIN 200u
#define GAS_THRESHOLD_MAX 4000u
#define GAS_THRESHOLD_STEP 50u
/* Comment normalized for portability. */
#define GAS_PERIOD_MIN_MS 100u
#define GAS_PERIOD_MAX_MS 5000u
/* Comment normalized for portability. */
#define GAS_TICK_MS 10u

#define GAS_WARNING_PERCENT 80u
/* Comment normalized for portability. */
#define GAS_SAFE_PERCENT 70u

#define GAS_SAFE_HOLD_MS 3000u
/* 预热期保持关阀且不判阈值。取 3 秒：真实 MQ 传感器要热机几分钟以上才稳定，
 * 那个时长对课设演示没有意义。它和上一条的 GAS_SAFE_HOLD_MS 数值相同但含义
 * 无关——一个是开机等待，一个是恢复判定要求的连续安全时长。 */
#define GAS_WARMUP_MS 3000u
#define GAS_SAVE_DELAY_MS 2000u
/* 多久没采到有效样本就判采样中断：窗口 = 采样周期 + 这个固定余量。
 *
 * 用「周期 + 1 秒」而不是「周期 × 倍数」：这个窗口要盖住的是**单次合法阻塞**，
 * 最大的一笔是显示刷新（软件 I2C 整屏 1024 字节，实测 200~750 ms，视编译结果
 * 而定）。余量是个绝对值，跟采样周期无关；用倍数的话，周期配到 5000 ms 时窗口
 * 会涨到 50 秒，采样器真死了也要 50 秒才报出来。
 *
 * 取 1 秒：默认 100 ms 周期下窗口 1.1 s，比最慢的一次整屏刷新（约 750 ms）还
 * 宽出三分之一；周期调到 5000 ms 时窗口 6 s，也没有失控。 */
#define GAS_SAMPLE_TIMEOUT_MARGIN_MS 1000u

/* Comment normalized for portability. */
#define GAS_BUZZER_OFF 0u
#define GAS_BUZZER_MAX_S 60u
#define GAS_BUZZER_ALWAYS (GAS_BUZZER_MAX_S + 1u)
/* Comment normalized for portability. */
#define GAS_BUZZER_FOREVER_MS 0xffffu
/* Comment normalized for portability. */
#if !defined(__ARMCC_VERSION)
_Static_assert(GAS_BUZZER_MAX_S * 1000u < GAS_BUZZER_FOREVER_MS,
               "the longest buzzer duration must stay below the forever sentinel");
#endif

typedef struct {
    uint16_t alarm[GAS_COUNT];
    uint16_t sample_period_ms;
    /* GAS_BUZZER_OFF, 1..GAS_BUZZER_MAX_S seconds, or GAS_BUZZER_ALWAYS. */
    uint8_t buzzer;
    bool lockout;
} gas_config_t;

/* Comment normalized for portability. */
typedef bool (*config_read_fn)(void *, uint16_t, uint8_t *, size_t);
typedef bool (*config_write_fn)(void *, uint16_t, const uint8_t *, size_t);
typedef struct {
    void *context;
    config_read_fn read;
    config_write_fn write;
} config_io_t;

typedef enum {
    CONFIG_LOAD_OK = 0,
    CONFIG_LOAD_EMPTY,
    CONFIG_LOAD_IO_ERROR
} config_load_status_t;

/* Comment normalized for portability. */
bool config_load(const config_io_t *io, gas_config_t *config);
config_load_status_t config_load_status(const config_io_t *io, gas_config_t *config);
bool config_save(const config_io_t *io, const gas_config_t *config);
void config_defaults(gas_config_t *config);
bool config_valid(const gas_config_t *config);
void config_buzzer_name(uint8_t value, char *out, size_t size);
uint16_t config_buzzer_duration_ms(const gas_config_t *config);
#endif


