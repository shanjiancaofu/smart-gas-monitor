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
    /* 每个字段都取一个和默认值不同的值，这样落盘再读回来时，少写或多读任何
     * 一个字段都会在这条链路上露出来。 */
    b = a; b.alarm[GAS_MQ6] = 2500; b.sample_period_ms = 500; b.lockout = true;
    b.buzzer = GAS_BUZZER_ALWAYS;
    assert(!config_load(&io, &loaded));
    assert(config_save(&io, &a));
    baseline = f;
    /* 在替换非活动槽的每一个字节处各模拟一次掉电。 */
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
    /* 下面几处按偏移直接改字节。槽 1 从 0x10 开始，所以它的 byte n 就是
     * data[16 + n]；槽内布局见 settings.c 顶部。 */
    /* 较新槽的 lockout 字节（byte 12）也在 CRC 覆盖范围内；改坏它应当使该槽
     * 作废，回退到较旧的、未锁存的那份。 */
    f.data[16 + 12] ^= 1u;
    assert(config_load(&io, &loaded) && !loaded.lockout);
    /* 较新槽的蜂鸣器字节（byte 3）同样在 CRC 里。改坏它，该槽作废、回退到
     * 仍是默认 5 秒的那一份，而不是照着一个坏字节去决定响多久。 */
    f = baseline;
    f.data[16 + 3] ^= 1u;
    assert(config_load(&io, &loaded) && loaded.buzzer == GAS_BUZZER_ALWAYS);
    f = baseline;
    /* 较新槽损坏，不能把好的那份也拖下水。 */
    f.data[16 + 4] ^= 1;
    assert(config_load(&io, &loaded));
    assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    /* 越界值在写进 EEPROM 之前就被拒绝。 */
    b = a; b.alarm[GAS_MQ4] = GAS_THRESHOLD_MAX + 1u;
    assert(!config_save(&io, &b));
    b = a; b.sample_period_ms = GAS_PERIOD_MAX_MS + 1u;
    assert(!config_save(&io, &b));
    b = a; b.buzzer = GAS_BUZZER_MAX_S + 1u;
    assert(!config_save(&io, &b));
}
/* 把某个槽伪造成「上一版固件写下的记录」：改版本号和蜂鸣器字节，再重算 CRC。
 * 要改的字段必须先改完再调它——CRC 覆盖 0..12，改在它后面就白改了。
 *
 * 槽位必须点名：config_save() 是在两个槽之间轮换写的，伪造哪个槽决定了这次
 * 读回的是不是它。伪造成旧的那个槽，读回来的是新槽，断言会因为这个错误的
 * 原因通过。 */
static void seal(fake_t *f, unsigned slot, uint8_t version, uint8_t buzzer_byte)
{
    uint8_t *p = f->data + slot * 16u;
    uint16_t crc;
    p[1] = version;
    p[3] = buzzer_byte;
    crc = test_crc16(p, 13u);
    p[13] = (uint8_t)crc;
    p[14] = (uint8_t)(crc >> 8);
}

static void test_version_mismatch_falls_back(void)
{
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, loaded;

    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    config_defaults(&a);
    a.lockout = true;
    assert(config_save(&io, &a));

    /* A truly old layout remains invalid. */
    seal(&f, 0u, 1u, 0u);
    assert(!config_load(&io, &loaded));

    /* v6 及更早：那时还没有蜂鸣器档位这个概念，给默认值。阈值、周期，尤其是
     * 落盘的安全锁存都要原样保留——升级固件不能顺手把阀门解锁。 */
    assert(config_save(&io, &a));
    f.data[12] = 1u;
    seal(&f, 0u, 6u, 61u);
    assert(config_load(&io, &loaded));
    assert(loaded.buzzer == GAS_BUZZER_ALWAYS && loaded.lockout);

    /* 槽 0 现在是一份合法的 v6 记录，所以这一笔会写到槽 1、序号 +1。下面伪造
     * 的就是槽 1——它才是「较新的记录」，不伪造它就读不回来。 */
    assert(config_save(&io, &a));
    /* v7 的编码是 0=OFF / 1=ALWAYS / 2..60=秒，只有 "1" 的含义变了（ALWAYS
     * 让位给「1 秒」），其余档位按数值一一对应。 */
    seal(&f, 1u, 7u, 1u);
    assert(config_load(&io, &loaded));
    assert(loaded.buzzer == GAS_BUZZER_ALWAYS && loaded.lockout);

    seal(&f, 1u, 7u, 5u);
    assert(config_load(&io, &loaded));
    assert(loaded.buzzer == 5);

    seal(&f, 1u, 7u, 0u);
    assert(config_load(&io, &loaded));
    assert(loaded.buzzer == GAS_BUZZER_OFF);
}

int main(void)
{
    test_store();
    test_version_mismatch_falls_back();
    puts("PASS: config load/save/power-loss/crc/version/range");
    return 0;
}
