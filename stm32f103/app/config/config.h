#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bsp_eeprom_config.h"

#define GAS_COUNT 3u

/* 璁剧疆瑙ｇ爜鍣ㄤ粠 EEPROM 璇诲叆鏃舵墍鎺ュ彈鐨勫彇鍊艰寖鍥淬€?*/
#define GAS_THRESHOLD_MIN 200u
#define GAS_THRESHOLD_MAX 4000u
#define GAS_THRESHOLD_STEP 50u
/* 涓庢寜閿彲閫夌殑鍛ㄦ湡鍒楄〃涓殑鏈€灏忓€间竴鑷达細鎸夐敭姘歌繙鍒颁笉浜嗙殑鏈€灏忓€硷紝鍙細鍦ㄤ粠
 * EEPROM 杞藉叆鏃舵墠琚鍙€?*/
#define GAS_PERIOD_MIN_MS 100u
#define GAS_PERIOD_MAX_MS 5000u
/* 搴旂敤灞傜敤鏉ヨ皟搴﹂噰鏍风殑 TIM2 鑺傛媿銆備笉鏄暣鑺傛媿鏁扮殑鍛ㄦ湡浼氳璋冨害鎴愪笌瀹冨０绉扮殑
 * 鍊间笉鍚岀殑鍙︿竴绉嶅懆鏈燂紝鎵€浠ヨ繖鏍风殑鍊肩洿鎺ユ嫆缁濓紝鑰屼笉鏄倓鎮勬寜鍥涜垗浜斿叆鍚庣殑鐩搁偦
 * 鍊兼墽琛屻€?*/
#define GAS_TICK_MS 10u

#define GAS_WARNING_PERCENT 80u
/* 鎭㈠鍒伴璀﹀甫浠ヤ笅銆傛寜姣斾緥鍙栧€硷紝浣挎帴杩戣寖鍥翠笅绔殑闃堝€间篃浠嶇暀鏈夎兘杈惧埌鐨?
 * 瀹夊叏鍖恒€?*/
#define GAS_SAFE_PERCENT 70u

#define GAS_SAFE_HOLD_MS 3000u
#define GAS_WARMUP_MS 60000u
#define GAS_SAVE_DELAY_MS 2000u
#define GAS_SAMPLE_TIMEOUT_PERIODS 3u

/* 鎶ヨ铚傞福鍣ㄥ搷澶氫箙銆備笁涓彇鍊兼帓鍦ㄥ悓涓€涓暟杞翠笂鈥斺€斻€屼笉鍝嶃€嶆渶鐭紝銆屼竴鐩村搷銆嶆渶闀?
 * 鈥斺€旀墍浠ユ寜閿氨鏄姞鍑忎袱绔す鍙栵紝涓嶉渶瑕佸崟鐙竴濂楁。浣嶈〃銆?*/
#define GAS_BUZZER_OFF 0u
#define GAS_BUZZER_MAX_S 60u
#define GAS_BUZZER_ALWAYS (GAS_BUZZER_MAX_S + 1u)
/* 鎸佺画楦ｅ搷鐨勬绉掑摠鍏靛€硷紝鐢?alarm 妯″潡瑙ｉ噴銆?*/
#define GAS_BUZZER_FOREVER_MS 0xffffu
/* 鏈€闀跨殑绉掓暟鎹㈢畻鎴愭绉掍箣鍚庡繀椤讳粛鐒跺皬浜庡摠鍏靛€硷紝鍚﹀垯銆?0 绉掋€嶈繖绉嶅€间細鎾炰笂
 * 銆屼竴鐩村搷銆嶇殑缂栫爜銆傛棩鍚庢妸涓婇檺璋冨ぇ鏃讹紝杩欓噷鍏堢紪璇戜笉杩囷紝鑰屼笉鏄涓や釜鍚箟鎮勬倓
 * 閲嶅悎銆?*/
#if !defined(__ARMCC_VERSION)
_Static_assert(GAS_BUZZER_MAX_S * 1000u < GAS_BUZZER_FOREVER_MS,
               "the longest buzzer duration must stay below the forever sentinel");
#endif

typedef struct {
    uint16_t alarm[GAS_COUNT];
    uint16_t sample_period_ms;
    /* GAS_BUZZER_OFF / 1..GAS_BUZZER_MAX_S 绉?/ GAS_BUZZER_ALWAYS銆?*/
    uint8_t buzzer;
    bool lockout;
} gas_config_t;

/* 鍚庣蹇呴』鍦ㄨ繑鍥炲墠瀹屾垚鍐欏叆锛堝寘鎷?EEPROM 鐨?ACK polling锛夈€?*/
typedef bool (*config_read_fn)(void *, uint16_t, uint8_t *, size_t);
typedef bool (*config_write_fn)(void *, uint16_t, const uint8_t *, size_t);
typedef struct {
    void *context;
    config_read_fn read;
    config_write_fn write;
} config_io_t;

/* 0x00 鍜?0x10 澶勭殑涓や唤 16 瀛楄妭鍓湰锛涘彇鏈€鏂颁笖鏈夋晥鐨勯偅浠姐€?*/
bool config_load(const config_io_t *io, gas_config_t *config);
bool config_save(const config_io_t *io, const gas_config_t *config);
void config_defaults(gas_config_t *config);
bool config_valid(const gas_config_t *config);
void config_buzzer_name(uint8_t value, char *out, size_t size);
uint16_t config_buzzer_duration_ms(const gas_config_t *config);
#endif

