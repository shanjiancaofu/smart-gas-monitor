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
    b = a; b.alarm[GAS_MQ7] = 2500; b.sample_period_ms = 500;
    assert(!settings_load(&io, &loaded));
    assert(settings_save(&io, &a));
    baseline = f;
    /* Simulated power loss at every byte of inactive-slot replacement. */
    for (cut = 0; cut < 17; ++cut) {
        f = baseline; f.budget = cut;
        assert(!settings_save(&io, &b));
        assert(settings_load(&io, &loaded));
        assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    }
    f = baseline; f.budget = -1;
    assert(settings_save(&io, &b));
    assert(settings_load(&io, &loaded));
    assert(loaded.alarm[GAS_MQ7] == 2500 && loaded.sample_period_ms == 500);
    /* A corrupted newer slot must not take the good one down with it. */
    f.data[20] ^= 1;
    assert(settings_load(&io, &loaded));
    assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    /* Out-of-range values are refused before they reach the EEPROM. */
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
    /* A record from the previous payload layout must be ignored, not misread. */
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
