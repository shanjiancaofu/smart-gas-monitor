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
#if USE_SOFT_I2C
/* Proteus runs the 72 MHz MCU well below real time; keep demos responsive. */
#define GAS_SAFE_HOLD_MS 500u
#define GAS_WARMUP_MS 500u
#else
#define GAS_SAFE_HOLD_MS 3000u
#define GAS_WARMUP_MS 3000u
#endif
/* 棰勭儹鏈熶繚鎸佸叧闃€涓斾笉鍒ら槇鍊笺€傚彇 3 绉掞細鐪熷疄 MQ 浼犳劅鍣ㄨ鐑満鍑犲垎閽熶互涓婃墠绋冲畾锛?
 * 閭ｄ釜鏃堕暱瀵硅璁炬紨绀烘病鏈夋剰涔夈€傚畠鍜屼笂涓€鏉＄殑 GAS_SAFE_HOLD_MS 鏁板€肩浉鍚屼絾鍚箟
 * 鏃犲叧鈥斺€斾竴涓槸寮€鏈虹瓑寰咃紝涓€涓槸鎭㈠鍒ゅ畾瑕佹眰鐨勮繛缁畨鍏ㄦ椂闀裤€?*/
#define GAS_SAVE_DELAY_MS 2000u
/* 澶氫箙娌￠噰鍒版湁鏁堟牱鏈氨鍒ら噰鏍蜂腑鏂細绐楀彛 = 閲囨牱鍛ㄦ湡 + 杩欎釜鍥哄畾浣欓噺銆?
 *
 * 鐢ㄣ€屽懆鏈?+ 1 绉掋€嶈€屼笉鏄€屽懆鏈?脳 鍊嶆暟銆嶏細杩欎釜绐楀彛瑕佺洊浣忕殑鏄?*鍗曟鍚堟硶闃诲**锛?
 * 鏈€澶х殑涓€绗旀槸鏄剧ず鍒锋柊锛堣蒋浠?I2C 鏁村睆 1024 瀛楄妭锛屽疄娴?200~750 ms锛岃缂栬瘧缁撴灉
 * 鑰屽畾锛夈€備綑閲忔槸涓粷瀵瑰€硷紝璺熼噰鏍峰懆鏈熸棤鍏筹紱鐢ㄥ€嶆暟鐨勮瘽锛屽懆鏈熼厤鍒?5000 ms 鏃剁獥鍙?
 * 浼氭定鍒?50 绉掞紝閲囨牱鍣ㄧ湡姝讳簡涔熻 50 绉掓墠鎶ュ嚭鏉ャ€?
 *
 * 鍙?1 绉掞細榛樿 100 ms 鍛ㄦ湡涓嬬獥鍙?1.1 s锛屾瘮鏈€鎱㈢殑涓€娆℃暣灞忓埛鏂帮紙绾?750 ms锛夎繕
 * 瀹藉嚭涓夊垎涔嬩竴锛涘懆鏈熻皟鍒?5000 ms 鏃剁獥鍙?6 s锛屼篃娌℃湁澶辨帶銆?*/
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


