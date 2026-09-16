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
/* 多久没采到有效样本就判采样中断，单位是采样周期。
 *
 * 取 10 而不是 3：这个窗口必须大于任何一次合法的阻塞。最大的一笔是显示刷新，
 * 整屏 1024 字节走软件 I2C 约 210 ms，慢一档的构建能到 750 ms。取 3（默认周期
 * 下 300 ms）时，一次整屏刷新就会被当成采样故障，把「环境持续安全 3 秒」的窗
 * 口清零，KEY4 永远解不开锁。取 10 给足余量，采样器真死了也仍在一秒内报出来。 */
#define GAS_SAMPLE_TIMEOUT_PERIODS 10u

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


