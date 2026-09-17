#include "gas/gas.h"
#include "config/config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t data[256]; int budget; } fake_t;

static bool read_mem(void *ctx, uint16_t addr, uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert((size_t)addr + size <= 256);
    memcpy(data, f->data + addr, size);
    return true;
}
static bool write_mem(void *ctx, uint16_t addr, const uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    size_t i;
    assert((size_t)addr + size <= 256);
    for (i = 0; i < size; ++i) {
        if (f->budget == 0) return false;
        if (f->budget > 0) --f->budget;
        f->data[addr + i] = data[i];
    }
    return true;
}
static uint16_t test_crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0xffffu;
    unsigned i;
    while (n--) {
        crc ^= (uint16_t)*p++ << 8;
        for (i = 0; i < 8u; ++i) {
            crc = (uint16_t)((crc & 0x8000u) ? (crc << 1) ^ 0x1021u : crc << 1);
        }
    }
    return crc;
}

static void test_store(void)
{
    fake_t f, baseline;
    config_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, b, loaded;
    int cut;
    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    config_defaults(&a);
    /* 姣忎釜瀛楁閮藉彇涓€涓拰榛樿鍊间笉鍚岀殑鍊硷紝杩欐牱钀界洏鍐嶈鍥炴潵鏃讹紝灏戝啓鎴栧璇讳换浣?
     * 涓€涓瓧娈甸兘浼氬湪杩欐潯閾捐矾涓婇湶鍑烘潵銆?*/
    b = a; b.alarm[GAS_MQ6] = 2500; b.sample_period_ms = 500; b.lockout = true;
    b.buzzer = GAS_BUZZER_ALWAYS;
    assert(!config_load(&io, &loaded));
    assert(config_save(&io, &a));
    baseline = f;
    /* 鍦ㄦ浛鎹㈤潪娲诲姩妲界殑姣忎竴涓瓧鑺傚鍚勬ā鎷熶竴娆℃帀鐢点€?*/
    for (cut = 0; cut < 17; ++cut) {
        f = baseline; f.budget = cut;
        assert(!config_save(&io, &b));
        assert(config_load(&io, &loaded));
        assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    }
    f = baseline; f.budget = -1;
    assert(config_save(&io, &b));
    assert(config_load(&io, &loaded));
    assert(loaded.alarm[GAS_MQ6] == 2500 && loaded.sample_period_ms == 500);
    assert(loaded.lockout && loaded.buzzer == GAS_BUZZER_ALWAYS);
    /* 涓嬮潰鍑犲鎸夊亸绉荤洿鎺ユ敼瀛楄妭銆傛Ы 1 浠?0x10 寮€濮嬶紝鎵€浠ュ畠鐨?byte n 灏辨槸
     * data[16 + n]锛涙Ы鍐呭竷灞€瑙?settings.c 椤堕儴銆?*/
    /* 杈冩柊妲界殑 lockout 瀛楄妭锛坆yte 12锛変篃鍦?CRC 瑕嗙洊鑼冨洿鍐咃紱鏀瑰潖瀹冨簲褰撲娇璇ユЫ
     * 浣滃簾锛屽洖閫€鍒拌緝鏃х殑銆佹湭閿佸瓨鐨勯偅浠姐€?*/
    f.data[16 + 12] ^= 1u;
    assert(config_load(&io, &loaded) && !loaded.lockout);
    /* 杈冩柊妲界殑铚傞福鍣ㄥ瓧鑺傦紙byte 3锛夊悓鏍峰湪 CRC 閲屻€傛敼鍧忓畠锛岃妲戒綔搴熴€佸洖閫€鍒?
     * 浠嶆槸榛樿 5 绉掔殑閭ｄ竴浠斤紝鑰屼笉鏄収鐫€涓€涓潖瀛楄妭鍘诲喅瀹氬搷澶氫箙銆?*/
    f = baseline;
    f.data[16 + 3] ^= 1u;
    assert(config_load(&io, &loaded) && loaded.buzzer == GAS_BUZZER_ALWAYS);
    f = baseline;
    /* 杈冩柊妲芥崯鍧忥紝涓嶈兘鎶婂ソ鐨勯偅浠戒篃鎷栦笅姘淬€?*/
    f.data[16 + 4] ^= 1;
    assert(config_load(&io, &loaded));
    assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    /* 瓒婄晫鍊煎湪鍐欒繘 EEPROM 涔嬪墠灏辫鎷掔粷銆?*/
    b = a; b.alarm[GAS_MQ4] = GAS_THRESHOLD_MAX + 1u;
    assert(!config_save(&io, &b));
    b = a; b.sample_period_ms = GAS_PERIOD_MAX_MS + 1u;
    assert(!config_save(&io, &b));
    b = a; b.buzzer = GAS_BUZZER_MAX_S + 1u;
    assert(!config_save(&io, &b));
}
static void test_version_mismatch_falls_back(void)
{
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, loaded;
    uint16_t crc;

    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    config_defaults(&a);
    a.lockout = true;
    assert(config_save(&io, &a));

    /* A truly old layout remains invalid. */
    f.data[1] = 1u;
    crc = test_crc16(f.data, 13u);
    f.data[13] = (uint8_t)crc;
    f.data[14] = (uint8_t)(crc >> 8);
    assert(!config_load(&io, &loaded));

    /* v6 is layout-compatible. Apply v7's new buzzer default and,
       critically, retain the persisted safety lockout. */
    assert(config_save(&io, &a));
    f.data[1] = 6u;
    f.data[3] = 61u;
    f.data[12] = 1u;
    crc = test_crc16(f.data, 13u);
    f.data[13] = (uint8_t)crc;
    f.data[14] = (uint8_t)(crc >> 8);
    assert(config_load(&io, &loaded));
    assert(loaded.buzzer == GAS_BUZZER_ALWAYS && loaded.lockout);
}

int main(void)
{
    test_store();
    test_version_mismatch_falls_back();
    puts("PASS: config load/save/power-loss/crc/version/range");
    return 0;
}
