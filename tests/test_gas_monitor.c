#include "gas/gas_monitor.h"
#include <assert.h>
#include <stdio.h>

static void sample(gas_monitor_t *m, uint32_t now, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t values[GAS_COUNT] = {a, b, c};
    gas_monitor_sample(m, values, true, now);
}
/* 跑完一次干净的预热；本函数返回时阀门应当已经打开。 */
static void run_to_normal(gas_monitor_t *m, uint32_t start)
{
    uint32_t t;
    gas_monitor_init(m, NULL, start);
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(m, start + t, 500, 500, 500);
    assert(m->state == GAS_NORMAL);
    assert(gas_monitor_valve_open(m));
}
static void test_states(void)
{
    gas_monitor_t m;
    uint32_t t;
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i) {
        uint16_t values[GAS_COUNT] = {500, 500, 500};
        run_to_normal(&m, 0);
        values[i] = gas_warning_threshold(m.config.alarm[i]);
        gas_monitor_sample(&m, values, true, 63100);
        assert(m.state == GAS_WARNING && gas_monitor_valve_open(&m));
        values[i] = m.config.alarm[i];
        gas_monitor_sample(&m, values, true, 63200);
        assert(m.state == GAS_ALARM && !gas_monitor_valve_open(&m));
        assert(m.alarm_mask == (1u << i) && m.alarm_count == 1);
        m.dirty = false;
        sample(&m, 63250, values[0], values[1], values[2]);
        assert(m.state == GAS_ALARM && !m.dirty);
        gas_monitor_key(&m, 4, 63250);
        assert(!gas_monitor_valve_open(&m));
        for (t = 63300; t <= 66300; t += 100) sample(&m, t, 500, 500, 500);
        assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
        assert(!gas_monitor_valve_open(&m));
        gas_monitor_key(&m, 4, 66300);
        assert(gas_monitor_valve_open(&m));
    }
    run_to_normal(&m, 0);
    gas_monitor_tick(&m, 63500);
    assert(m.state == GAS_FAULT && !gas_monitor_valve_open(&m));
    assert(m.config.lockout && m.dirty);
    m.dirty = false;
    gas_monitor_tick(&m, 63501);
    assert(!m.dirty);
    sample(&m, 63600, 500, 500, 500);
    gas_monitor_key(&m, 4, 63600);
    assert(!gas_monitor_valve_open(&m));
    gas_monitor_sample(&m, NULL, false, 63700);
    assert(m.state == GAS_FAULT);
    sample(&m, 63800, 4096, 0, 0);
    assert(m.state == GAS_FAULT);
    run_to_normal(&m, 0);
    sample(&m, 64000, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && !gas_monitor_valve_open(&m));
    assert(!m.reset_ready);
    /* 预热计时和安全计时都要跨过回绕点。 */
    run_to_normal(&m, UINT32_MAX - 61000u);
}
static void test_power_up_opens(void)
{
    gas_monitor_t m;
    uint32_t t;
    gas_monitor_init(&m, NULL, 0);
    for (t = 0; t < GAS_WARMUP_MS; t += 100) {
        sample(&m, t, 500, 500, 500);
        assert(m.state == GAS_WARMUP);
        assert(!gas_monitor_valve_open(&m));
    }
    sample(&m, GAS_WARMUP_MS, 500, 500, 500);
    assert(m.state == GAS_NORMAL && gas_monitor_valve_open(&m));
    /* 安全保持窗口是用来解除锁存的，不是开阀的前置条件。 */
    assert(!m.reset_ready);
}
static void test_no_fault_before_first_sample(void)
{
    gas_monitor_t m;
    gas_monitor_init(&m, NULL, 1000);
    gas_monitor_tick(&m, 1000);
    assert(m.state == GAS_WARMUP && !m.latched);
    /* 一旦成功采过样，再丢掉采样就重新算故障。 */
    sample(&m, 1100, 500, 500, 500);
    gas_monitor_tick(&m, 1100 + gas_sample_timeout_ms(&m));
    assert(m.state == GAS_FAULT && m.latched);
}
static void test_failed_attempt_is_a_fault(void)
{
    gas_monitor_t m;
    uint32_t t;
    /* 开机时就坏掉的 ADC 必须报故障，而不是永远停在预热：「还没有读数」和
     * 「读到了一次失败的读数」是两回事。 */
    gas_monitor_init(&m, NULL, 0);
    gas_monitor_sample(&m, NULL, false, 100);
    assert(m.state == GAS_FAULT && m.latched);
    assert(!gas_monitor_valve_open(&m));
    /* 它和其它锁存走同一条恢复规则：先回到洁净空气，再按 KEY4。 */
    for (t = 200; t <= 100000; t += 100) sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
    assert(!gas_monitor_valve_open(&m));
    gas_monitor_key(&m, 4, 100000);
    assert(gas_monitor_valve_open(&m));
}
static void test_persisted_lockout(void)
{
    gas_monitor_t m;
    gas_config_t c;
    uint32_t t;
    gas_config_defaults(&c);
    c.lockout = true;
    gas_monitor_init(&m, &c, 0);
    assert(m.latched && !gas_monitor_valve_open(&m));
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
    gas_monitor_key(&m, 4, GAS_WARMUP_MS + GAS_SAFE_HOLD_MS);
    assert(!m.config.lockout && m.dirty && gas_monitor_valve_open(&m));
}

static void test_low_threshold_recovers(void)
{
    gas_monitor_t m;
    uint32_t t;
    unsigned i;
    run_to_normal(&m, 0);
    gas_monitor_key(&m, 1, 63000);
    for (i = 0; i < 100; ++i) gas_monitor_key(&m, 3, 63000);
    assert(m.config.alarm[GAS_MQ4] == GAS_THRESHOLD_MIN);
    assert(m.state == GAS_ALARM && !gas_monitor_valve_open(&m));
    /* 安全带按比例取，阈值调到量程最底端时洁净空气仍然够得着；若用固定值，
     * 这种情况永远恢复不了。 */
    for (t = 63100; t <= 66100; t += 100) sample(&m, t, 100, 100, 100);
    assert(m.reset_ready);
    assert(m.state == GAS_SAFE_WAIT && !gas_monitor_valve_open(&m));
    gas_monitor_key(&m, 4, 66100);
    assert(gas_monitor_valve_open(&m));
}
static void test_keys(void)
{
    gas_monitor_t m;
    unsigned i;
    run_to_normal(&m, 0);
    gas_monitor_key(&m, 2, 63000);
    assert(!m.dirty);
    gas_monitor_key(&m, 1, 63000);
    gas_monitor_key(&m, 2, 63000);
    assert(m.config.alarm[GAS_MQ4] == 2450 && m.dirty);
    assert(!gas_monitor_save_due(&m, 64999));
    assert(gas_monitor_save_due(&m, 65000));
    for (i = 0; i < 100; ++i) gas_monitor_key(&m, 2, 63000);
    assert(m.config.alarm[GAS_MQ4] == GAS_THRESHOLD_MAX);
    for (i = 0; i < 100; ++i) gas_monitor_key(&m, 3, 63000);
    assert(m.config.alarm[GAS_MQ4] == GAS_THRESHOLD_MIN);
    assert(m.state == GAS_ALARM && !gas_monitor_valve_open(&m));
}
static void test_sample_period(void)
{
    gas_monitor_t m;
    gas_config_t c;
    uint32_t t;
    unsigned i;
    gas_config_defaults(&c);
    assert(c.sample_period_ms == 100 && gas_config_valid(&c));
    c.sample_period_ms = 0;
    assert(!gas_config_valid(&c));
    c.sample_period_ms = GAS_PERIOD_MAX_MS + 1u;
    assert(!gas_config_valid(&c));

    run_to_normal(&m, 0);
    for (i = 0; i < GAS_SEL_PERIOD; ++i) gas_monitor_key(&m, 1, 63000);
    assert(m.selected == GAS_SEL_PERIOD);
    gas_monitor_key(&m, 2, 63000);
    assert(m.config.sample_period_ms == 200 && m.dirty);
    assert(gas_sample_timeout_ms(&m) == 600);
    /* 停顿判定的窗口跟着配置的采样周期走，不是一个固定常数。 */
    for (t = 63100; t <= 66900; t += 200) sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_NORMAL);
    gas_monitor_tick(&m, 66900 + 599u);
    assert(m.state == GAS_NORMAL);
    gas_monitor_tick(&m, 66900 + 600u);
    assert(m.state == GAS_FAULT && !gas_monitor_valve_open(&m));
}
int main(void)
{
    test_states();
    test_power_up_opens();
    test_no_fault_before_first_sample();
    test_failed_attempt_is_a_fault();
    test_persisted_lockout();
    test_low_threshold_recovers();
    test_keys();
    test_sample_period();
    puts("PASS: gas_monitor states/power-up/fault/recovery/keys/period/wraparound");
    return 0;
}
