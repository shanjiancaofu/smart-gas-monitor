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

/* 三级判定用的三个百分比，都是「报警阈值的百分之多少」：
 *
 *   >= 80%   预警（WARNING），阀门仍然开
 *   >= 100%  报警（ALARM），关阀并锁存
 *   <  70%   才允许开始计恢复时间
 *   >= 75%   恢复计时中途作废，退回锁存
 *
 * 进出用两条线（70/75）而不是一条：单线判定时，浓度正好压在线上来回抖，会让
 * LOCKED/READY 跟着闪。75 高于 70 是必须的——反过来的话这两个判断会互相打架，
 * 永远恢复不了。 */
#define GAS_WARNING_PERCENT 80u
#define GAS_SAFE_PERCENT 70u
#define GAS_SAFE_RELEASE_PERCENT 75u

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif
/* 预热期保持关阀且不判阈值。取 3 秒：真实 MQ 传感器要热机几分钟以上才稳定，
 * 那个时长对课设演示没有意义。它和 GAS_SAFE_HOLD_MS 数值相同但含义无关——
 * 一个是开机等待，一个是恢复判定要求的连续安全时长。
 *
 * 软件 I2C 分支（也就是 Proteus）两个都缩到 500 ms：仿真跑 72 MHz 的 MCU 远慢
 * 于实时，3 秒在演示里会等成十来秒。 */
#if USE_SOFT_I2C
#define GAS_SAFE_HOLD_MS 500u
#define GAS_WARMUP_MS 500u
#else
#define GAS_SAFE_HOLD_MS 3000u
#define GAS_WARMUP_MS 3000u
#endif
#define GAS_SAVE_DELAY_MS 2000u
/* 多久没采到有效样本就判采样中断：窗口 = 采样周期 + 这个固定余量。
 *
 * 用「周期 + 余量」而不是「周期 × 倍数」：这个窗口要盖住的是**单次合法阻塞**，
 * 最大的一笔是显示刷新（软件 I2C 整屏 1024 字节，实测 200~750 ms，视编译结果
 * 而定）。余量是个绝对值，跟采样周期无关；用倍数的话，周期配到 5000 ms 时窗口
 * 会涨到 50 秒，采样器真死了也要 50 秒才报出来。
 *
 * 两套取值：软件 I2C 一次整屏刷新就要 700 ms 上下，1000 ms 的余量贴着上限，
 * 取 5000 ms 留够余裕；硬件 I2C 快得多，取 1000 ms 即可——默认 100 ms 周期下
 * 窗口 1.1 s，比最慢的一次整屏刷新还宽出三分之一。 */
#if USE_SOFT_I2C
#define GAS_SAMPLE_TIMEOUT_MARGIN_MS 5000u
#else
#define GAS_SAMPLE_TIMEOUT_MARGIN_MS 1000u
#endif

/* 蜂鸣器档位。用有符号类型，好让「一直响」排在实际秒数之外：
 *
 *   -1      一直响到报警解除
 *    0      不响
 *    1..60  响这么多秒
 *
 * 按键在这根轴上走：OFF → 1S → 2S → … → 60S → ALWAYS，「加」永远意味着响得更久。
 * 轴首尾相接成一个环，到顶再按绕回 OFF、到底再按绕到 ALWAYS，两个方向都能一直
 * 走下去。EEPROM 里按补码存，-1 就是 0xFF。 */
#define GAS_BUZZER_ALWAYS (-1)
#define GAS_BUZZER_OFF 0
#define GAS_BUZZER_MAX_S 60
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
    /* -1=ALWAYS, 0=OFF, 1..60=最长报警时长（秒）。 */
    int8_t buzzer;
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
void config_buzzer_name(int8_t value, char *out, size_t size);
uint16_t config_buzzer_duration_ms(const gas_config_t *config);
#endif


