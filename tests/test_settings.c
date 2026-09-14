#include "gas/gas_monitor.h"
#include "settings/settings.h"
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
    settings_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, b, loaded;
    int cut;
    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    gas_config_defaults(&a);
    b = a; b.alarm[GAS_MQ7] = 2500; b.sample_period_ms = 500; b.lockout = true;
    assert(!settings_load(&io, &loaded));
    assert(settings_save(&io, &a));
    baseline = f;
    /* 在替换非活动槽的每一个字节处各模拟一次掉电。 */
    for (cut = 0; cut < 17; ++cut) {
        f = baseline; f.budget = cut;
        assert(!settings_save(&io, &b));
        assert(settings_load(&io, &loaded));
        assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    }
    f = baseline; f.budget = -1;
    assert(settings_save(&io, &b));
    assert(settings_load(&io, &loaded));
    assert(loaded.alarm[GAS_MQ7] == 2500 && loaded.sample_period_ms == 500 && loaded.lockout);
    /* 较新槽的 lockout 字节也在 CRC 覆盖范围内；改坏它应当使该槽作废，
     * 回退到较旧的、未锁存的那份。 */
    f.data[28] ^= 1u;
    assert(settings_load(&io, &loaded) && !loaded.lockout);
    f = baseline;
    /* 较新槽损坏，不能把好的那份也拖下水。 */
    f.data[20] ^= 1;
    assert(settings_load(&io, &loaded));
    assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    /* 越界值在写进 EEPROM 之前就被拒绝。 */
    b = a; b.alarm[GAS_MQ4] = GAS_THRESHOLD_MAX + 1u;
    assert(!settings_save(&io, &b));
    b = a; b.sample_period_ms = GAS_PERIOD_MAX_MS + 1u;
    assert(!settings_save(&io, &b));
}
static void test_version_mismatch_falls_back(void)
{
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, loaded;
    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    gas_config_defaults(&a);
    assert(settings_save(&io, &a));
    /* 旧载荷布局留下的记录必须被忽略，而不是被误读。 */
    f.data[1] = 1;
    assert(!settings_load(&io, &loaded));
}
int main(void)
{
    test_store();
    test_version_mismatch_falls_back();
    puts("PASS: settings load/save/power-loss/crc/version/range");
    return 0;
}
