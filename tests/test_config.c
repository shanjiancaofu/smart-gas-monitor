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
    assert(config_load(&io, &loaded) && loaded.buzzer == 5u);
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
    b = a; b.buzzer = GAS_BUZZER_ALWAYS + 1u;
    assert(!config_save(&io, &b));
}
static void test_version_mismatch_falls_back(void)
{
    fake_t f;
    config_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, loaded;
    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    config_defaults(&a);
    assert(config_save(&io, &a));
    /* 旧载荷布局留下的记录必须被忽略，而不是被误读。 */
    f.data[1] = 1;
    assert(!config_load(&io, &loaded));

    /* 版本 4 尤其要挡住：v5 只挪动了 sequence 和 buzzer 两个字节，v4 记录的
     * 其余偏移全都对得上，少了版本检查它会「几乎读对」——而 byte 3 在 v4 里
     * 是序号的高字节，读成蜂鸣器时长恰好是 0，也就是把蜂鸣器静默关掉。 */
    assert(config_save(&io, &a));
    f.data[1] = 4;
    assert(!config_load(&io, &loaded));
}
int main(void)
{
    test_store();
    test_version_mismatch_falls_back();
    puts("PASS: config load/save/power-loss/crc/version/range");
    return 0;
}
