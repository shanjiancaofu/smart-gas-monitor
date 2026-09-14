#include "monitor/gas_monitor.h"
#include "parameters/gas_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void sample(gas_monitor_t *m, uint32_t now, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t values[3] = {a, b, c};
    gas_monitor_sample(m, values, true, now);
}
static void open_safe(gas_monitor_t *m, uint32_t start)
{
    uint32_t t;
    gas_monitor_init(m, NULL, start);
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(m, start + t, 500, 500, 500);
    assert(m->state == GAS_SAFE_WAIT);
    assert(!gas_monitor_valve_open(m));
    gas_monitor_key(m, 4, start + GAS_WARMUP_MS + GAS_SAFE_HOLD_MS);
    assert(m->state == GAS_NORMAL && gas_monitor_valve_open(m));
}
static void test_states(void)
{
    gas_monitor_t m;
    uint32_t t;
    unsigned i;
    for (i = 0; i < 3; ++i) {
        uint16_t values[3] = {500, 500, 500};
        open_safe(&m, 0);
        values[i] = gas_warning_threshold(m.config.alarm[i]);
        gas_monitor_sample(&m, values, true, 63100);
        assert(m.state == GAS_WARNING && gas_monitor_valve_open(&m));
        values[i] = m.config.alarm[i];
        gas_monitor_sample(&m, values, true, 63200);
        assert(m.state == GAS_ALARM && !gas_monitor_valve_open(&m));
        assert(m.alarm_mask == (1u << i) && m.alarm_count == 1);
        gas_monitor_key(&m, 4, 63200);
        assert(!gas_monitor_valve_open(&m));
        for (t = 63300; t <= 66300; t += 100) sample(&m, t, 500, 500, 500);
        assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
        assert(!gas_monitor_valve_open(&m));
        gas_monitor_key(&m, 4, 66300);
        assert(gas_monitor_valve_open(&m));
    }
    open_safe(&m, 0);
    gas_monitor_tick(&m, 63500);
    assert(m.state == GAS_FAULT && !gas_monitor_valve_open(&m));
    sample(&m, 63600, 500, 500, 500);
    gas_monitor_key(&m, 4, 63600);
    assert(!gas_monitor_valve_open(&m));
    gas_monitor_sample(&m, NULL, false, 63700);
    assert(m.state == GAS_FAULT);
    sample(&m, 63800, 4096, 0, 0);
    assert(m.state == GAS_FAULT);
    open_safe(&m, 0);
    sample(&m, 64000, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && !gas_monitor_valve_open(&m));
    assert(!m.reset_ready);
    /* Wrap during warmup and safety timing. */
    open_safe(&m, UINT32_MAX - 61000u);
}
static void test_keys(void)
{
    gas_monitor_t m;
    unsigned i;
    open_safe(&m, 0);
    gas_monitor_key(&m, 2, 63000);
    assert(!m.dirty);
    gas_monitor_key(&m, 1, 63000);
    gas_monitor_key(&m, 2, 63000);
    assert(m.config.alarm[0] == 2450 && m.dirty);
    assert(!gas_monitor_save_due(&m, 64999));
    assert(gas_monitor_save_due(&m, 65000));
    for (i = 0; i < 100; ++i) gas_monitor_key(&m, 2, 63000);
    assert(m.config.alarm[0] == 4000);
    for (i = 0; i < 100; ++i) gas_monitor_key(&m, 3, 63000);
    assert(m.config.alarm[0] == 200);
    assert(m.state == GAS_ALARM && !gas_monitor_valve_open(&m));
}
typedef struct { uint8_t data[256]; int budget; } fake_t;
static bool read_mem(void *ctx, uint16_t addr, uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert(addr + size <= 256); memcpy(data, f->data + addr, size); return true;
}
static bool write_mem(void *ctx, uint16_t addr, const uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    size_t i;
    assert(addr + size <= 256);
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
    gas_store_io_t io = {&f, read_mem, write_mem};
    gas_config_t a, b, loaded;
    int cut;
    memset(&f, 0xff, sizeof(f)); f.budget = -1;
    gas_config_defaults(&a); b = a; b.alarm[1] = 2500;
    assert(!gas_store_load(&io, &loaded));
    assert(gas_store_save(&io, &a));
    baseline = f;
    /* Simulated power loss at every byte of inactive-slot replacement. */
    for (cut = 0; cut < 17; ++cut) {
        f = baseline; f.budget = cut;
        assert(!gas_store_save(&io, &b));
        assert(gas_store_load(&io, &loaded));
        assert(memcmp(&loaded, &a, sizeof(a)) == 0);
    }
    f = baseline; f.budget = -1;
    assert(gas_store_save(&io, &b));
    assert(gas_store_load(&io, &loaded) && loaded.alarm[1] == 2500);
    f.data[20] ^= 1;
    assert(gas_store_load(&io, &loaded) && loaded.alarm[1] == a.alarm[1]);
    b.alarm[0] = 4095;
    assert(!gas_store_save(&io, &b));
}
int main(void)
{
    test_states(); test_keys(); test_store();
    puts("PASS: alarm/reset/freshness/wraparound/keys/config/power-loss");
    return 0;
}
