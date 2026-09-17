#include "gas/gas.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 用例里的绝对时刻都从这里起步：run_to_normal() 跑完时最后一次采样的时刻。
 * 之后的采样必须落在这个时刻的采样超时窗口以内，否则
 * gas_sample() 会把它读成采样中断而判 FAULT，而不是要看的状态变化。
 * 写成表达式而不是字面量，是为了改 GAS_WARMUP_MS 时这些时刻跟着走。 */
#define T0 (GAS_WARMUP_MS + GAS_SAFE_HOLD_MS)

static void sample(gas_t *m, uint32_t now, uint16_t a, uint16_t b, uint16_t c)
{
    uint16_t values[GAS_COUNT] = {a, b, c};
    gas_sample(m, values, true, now);
}
/* 跑完一次干净的预热；本函数返回时阀门应当已经打开。 */
static void run_to_normal(gas_t *m, uint32_t start)
{
    uint32_t t;
    gas_init(m, NULL, start);
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(m, start + t, 500, 500, 500);
    assert(m->state == GAS_NORMAL);
    assert(gas_valve_open(m));
}
static void test_states(void)
{
    gas_t m;
    uint32_t t;
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i) {
        uint16_t values[GAS_COUNT] = {500, 500, 500};
        run_to_normal(&m, 0);
        values[i] = gas_warning_threshold(m.config.alarm[i]);
        gas_sample(&m, values, true, (T0 + 100u));
        assert(m.state == GAS_WARNING && gas_valve_open(&m));
        values[i] = m.config.alarm[i];
        gas_sample(&m, values, true, (T0 + 200u));
        assert(m.state == GAS_ALARM && !gas_valve_open(&m));
        assert(m.alarm_mask == (1u << i) && m.alarm_count == 1);
        m.dirty = false;
        sample(&m, (T0 + 250u), values[0], values[1], values[2]);
        assert(m.state == GAS_ALARM && !m.dirty);
        gas_key(&m, GAS_KEY_CONFIRM, (T0 + 250u));
        assert(!gas_valve_open(&m));
        for (t = (T0 + 300u); t <= (T0 + 3300u); t += 100) sample(&m, t, 500, 500, 500);
        assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
        assert(!gas_valve_open(&m));
        gas_key(&m, GAS_KEY_CONFIRM, (T0 + 3300u));
        assert(gas_valve_open(&m));
    }
    run_to_normal(&m, 0);
    gas_update(&m, (T0 + gas_sample_timeout_ms(&m)));
    assert(m.state == GAS_FAULT && !gas_valve_open(&m));
    assert(m.config.lockout && m.dirty);
    m.dirty = false;
    gas_update(&m, (T0 + gas_sample_timeout_ms(&m) + 1u));
    assert(!m.dirty);
    sample(&m, (T0 + gas_sample_timeout_ms(&m) + 100u), 500, 500, 500);
    gas_key(&m, GAS_KEY_CONFIRM, (T0 + gas_sample_timeout_ms(&m) + 100u));
    assert(!gas_valve_open(&m));
    gas_sample(&m, NULL, false, (T0 + gas_sample_timeout_ms(&m) + 200u));
    assert(m.state == GAS_FAULT);
    sample(&m, (T0 + gas_sample_timeout_ms(&m) + 300u), 4096, 0, 0);
    assert(m.state == GAS_FAULT);
    run_to_normal(&m, 0);
    sample(&m, (T0 + gas_sample_timeout_ms(&m) + 400u), 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && !gas_valve_open(&m));
    assert(!m.reset_ready);
    /* 预热计时和安全计时都要跨过回绕点。起点选在距回绕 GAS_WARMUP_MS+1000
     * 处，这样回绕落在预热结束之后、这一段跑完之前，两个计时都能跨过它。 */
    run_to_normal(&m, UINT32_MAX - (GAS_WARMUP_MS + 1000u));
}
static void test_power_up_opens(void)
{
    gas_t m;
    uint32_t t;
    gas_init(&m, NULL, 0);
    for (t = 0; t < GAS_WARMUP_MS; t += 100) {
        sample(&m, t, 500, 500, 500);
        assert(m.state == GAS_WARMUP);
        assert(!gas_valve_open(&m));
    }
    sample(&m, GAS_WARMUP_MS, 500, 500, 500);
    assert(m.state == GAS_NORMAL && gas_valve_open(&m));
    /* 安全保持窗口是用来解除锁存的，不是开阀的前置条件。 */
    assert(!m.reset_ready);
}
static void test_no_fault_before_first_sample(void)
{
    gas_t m;
    gas_init(&m, NULL, 1000);
    gas_update(&m, 1000);
    assert(m.state == GAS_WARMUP && !m.latched);
    /* 一旦成功采过样，再丢掉采样就重新算故障。 */
    sample(&m, 1100, 500, 500, 500);
    gas_update(&m, 1100 + gas_sample_timeout_ms(&m));
    assert(m.state == GAS_FAULT && m.latched);
}
static void test_failed_attempt_is_a_fault(void)
{
    gas_t m;
    uint32_t t;
    /* 开机时就坏掉的 ADC 必须报故障，而不是永远停在预热：「还没有读数」和
     * 「读到了一次失败的读数」是两回事。 */
    gas_init(&m, NULL, 0);
    gas_sample(&m, NULL, false, 100);
    assert(m.state == GAS_FAULT && m.latched);
    assert(!gas_valve_open(&m));
    /* 它和其它锁存走同一条恢复规则：先回到洁净空气，再按 KEY4。 */
    for (t = 200; t <= 100000; t += 100) sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
    assert(!gas_valve_open(&m));
    gas_key(&m, GAS_KEY_CONFIRM, 100000);
    assert(gas_valve_open(&m));
}
static void test_persisted_lockout(void)
{
    gas_t m;
    gas_config_t c;
    uint32_t t;
    config_defaults(&c);
    c.lockout = true;
    gas_init(&m, &c, 0);
    assert(m.latched && !gas_valve_open(&m));
    for (t = 0; t <= GAS_WARMUP_MS + GAS_SAFE_HOLD_MS; t += 100)
        sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);
    gas_key(&m, GAS_KEY_CONFIRM, GAS_WARMUP_MS + GAS_SAFE_HOLD_MS);
    assert(!m.config.lockout && m.dirty && gas_valve_open(&m));
}

static void test_low_threshold_recovers(void)
{
    gas_t m;
    uint32_t t;
    unsigned i;
    run_to_normal(&m, 0);
    /* 换页到设置页，默认就选中第一项 MQ4，再一路减到下限。 */
    assert(m.item == GAS_ITEM_MQ4);
    for (i = 0; i < 100; ++i) gas_key(&m, GAS_KEY_DOWN, T0);
    assert(m.config.alarm[GAS_MQ4] == GAS_THRESHOLD_MIN);
    assert(m.state == GAS_ALARM && !gas_valve_open(&m));
    /* 安全带按比例取，阈值调到量程最底端时洁净空气仍然够得着；若用固定值，
     * 这种情况永远恢复不了。 */
    for (t = (T0 + 100u); t <= (T0 + 3100u); t += 100) sample(&m, t, 100, 100, 100);
    assert(m.reset_ready);
    assert(m.state == GAS_SAFE_WAIT && !gas_valve_open(&m));
    gas_key(&m, GAS_KEY_CONFIRM, (T0 + 3100u));
    assert(gas_valve_open(&m));
}
static void test_keys(void)
{
    gas_t m;
    unsigned i;
    run_to_normal(&m, 0);
    /* 停在首页时加减键不该改任何东西：首页没有可调项。 */
    gas_key(&m, GAS_KEY_CONFIRM, T0);
    assert(!m.dirty);
    /* 换页到设置页，默认选中 MQ4。 */
    assert(m.item == GAS_ITEM_MQ4);
    gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.alarm[GAS_MQ4] == 2450 && m.dirty);
    assert(!gas_save_due(&m, (T0 + 1999u)));
    assert(gas_save_due(&m, (T0 + 2000u)));
    for (i = 0; i < 100; ++i) gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.alarm[GAS_MQ4] == GAS_THRESHOLD_MAX);
    for (i = 0; i < 100; ++i) gas_key(&m, GAS_KEY_DOWN, T0);
    assert(m.config.alarm[GAS_MQ4] == GAS_THRESHOLD_MIN);
    assert(m.state == GAS_ALARM && !gas_valve_open(&m));
    /* 换页键跳过报警页：它在循环里，但报警页只能由报警状态顶上来。 */

}
static void test_sample_period(void)
{
    gas_t m;
    gas_config_t c;
    uint32_t t;
    unsigned i;
    config_defaults(&c);
    assert(c.sample_period_ms == 100 && config_valid(&c));
    c.sample_period_ms = 0;
    assert(!config_valid(&c));
    c.sample_period_ms = GAS_PERIOD_MAX_MS + 1u;
    assert(!config_valid(&c));

    run_to_normal(&m, 0);
    /* KEY1 换页到设置页，KEY2 换到「采样周期」那一项。 */
    assert(m.item == GAS_ITEM_MQ4);
    for (i = 0; i < GAS_ITEM_PERIOD; ++i) gas_key(&m, GAS_KEY_SELECT, T0);
    assert(m.item == GAS_ITEM_PERIOD);
    gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.sample_period_ms == 200 && m.dirty);
    assert(gas_sample_timeout_ms(&m) == 200u + GAS_SAMPLE_TIMEOUT_MARGIN_MS);
    /* 停顿判定的窗口跟着配置的采样周期走，不是一个固定常数。 */
    for (t = (T0 + 100u); t <= (T0 + 3900u); t += 200) sample(&m, t, 500, 500, 500);
    assert(m.state == GAS_NORMAL);
    /* 窗口跟着配置的采样周期走（当前是 200 ms 周期加固定余量），
     * 差一毫秒还不算中断，刚好到点才算。 */
    gas_update(&m, (T0 + 3900u) + gas_sample_timeout_ms(&m) - 1u);
    assert(m.state == GAS_NORMAL);
    gas_update(&m, (T0 + 3900u) + gas_sample_timeout_ms(&m));
    assert(m.state == GAS_FAULT && !gas_valve_open(&m));
}
/* 取值 → 毫秒 → 名字这条链，以及它在两端和越界处的行为。 */
static void test_buzzer_value(void)
{
    gas_config_t c;
    char name[16];
    char tiny[2];

    config_defaults(&c);
    assert(c.buzzer == 5u && config_valid(&c));
    c.buzzer = GAS_BUZZER_ALWAYS;
    assert(config_valid(&c));
    /* 61 是「一直响」的编码，不是「61 秒」；再往上就没有定义了。 */
    c.buzzer = GAS_BUZZER_ALWAYS + 1u;
    assert(!config_valid(&c));

    c.buzzer = GAS_BUZZER_OFF;
    assert(config_buzzer_duration_ms(&c) == 0u);
    c.buzzer = 1u;
    assert(config_buzzer_duration_ms(&c) == 1000u);
    c.buzzer = 60u;
    assert(config_buzzer_duration_ms(&c) == 60000u);
    c.buzzer = GAS_BUZZER_ALWAYS;
    assert(config_buzzer_duration_ms(&c) == GAS_BUZZER_FOREVER_MS);
    /* 越界的值一律读作「一直响」。若先相乘再截断，66 会回绕成 464 毫秒——
     * 报警路径上最不该出现的失败方式，就是响了 0.5 秒然后安静下来。 */
    c.buzzer = 66u;
    assert(config_buzzer_duration_ms(&c) == GAS_BUZZER_FOREVER_MS);
    c.buzzer = 255u;
    assert(config_buzzer_duration_ms(&c) == GAS_BUZZER_FOREVER_MS);

    config_buzzer_name(GAS_BUZZER_OFF, name, sizeof(name));
    assert(strcmp(name, "OFF") == 0);
    config_buzzer_name(1u, name, sizeof(name));
    assert(strcmp(name, "1S") == 0);
    config_buzzer_name(60u, name, sizeof(name));
    assert(strcmp(name, "60S") == 0);
    config_buzzer_name(GAS_BUZZER_ALWAYS, name, sizeof(name));
    assert(strcmp(name, "ALWAYS") == 0);
    /* 越界同样读作最长档，而不是印出 "66S" 这种根本不存在的设置。 */
    config_buzzer_name(66u, name, sizeof(name));
    assert(strcmp(name, "ALWAYS") == 0);

    /* 缓冲区不够时截断，不越界；大小为 0 时一个字节都不写。 */
    tiny[0] = (char)0x7f;
    tiny[1] = (char)0x7f;
    config_buzzer_name(60u, tiny, sizeof(tiny));
    assert(tiny[0] == '6' && tiny[1] == '\0');
    name[0] = 'X';
    config_buzzer_name(GAS_BUZZER_OFF, name, 0u);
    assert(name[0] == 'X');
}

/* 面板上的按键：在同一个数轴上加减，两端夹取，不环绕。 */
static void test_buzzer_keys(void)
{
    gas_t m;
    unsigned i;
    run_to_normal(&m, 0);
    for (i = 0; i < GAS_ITEM_BUZZER; ++i) gas_key(&m, GAS_KEY_SELECT, T0);
    assert(m.item == GAS_ITEM_BUZZER);
    m.dirty = false;

    /* 加号一路加到顶就停在「一直响」：加的语义永远是「响得更久」，不做环绕
     * 回到「不响」——那会让同一个按键在两端之间来回跳。 */
    for (i = 0; i < 100; ++i) gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.buzzer == GAS_BUZZER_ALWAYS && m.dirty);
    gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.buzzer == GAS_BUZZER_ALWAYS);

    for (i = 0; i < 100; ++i) gas_key(&m, GAS_KEY_DOWN, T0);
    assert(m.config.buzzer == GAS_BUZZER_OFF);
    gas_key(&m, GAS_KEY_DOWN, T0);
    assert(m.config.buzzer == GAS_BUZZER_OFF);

    /* 一步一格，中间没有跳档：OFF → 1S → 2S → … → 60S → ALWAYS。 */
    gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.buzzer == 1u);
    gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.buzzer == 2u);
    for (i = 0; i < GAS_BUZZER_MAX_S - 2u; ++i) gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.buzzer == GAS_BUZZER_MAX_S);
    gas_key(&m, GAS_KEY_UP, T0);
    assert(m.config.buzzer == GAS_BUZZER_ALWAYS);
    gas_key(&m, GAS_KEY_DOWN, T0);
    assert(m.config.buzzer == GAS_BUZZER_MAX_S);

    /* 改蜂鸣器时长不影响别的设置项，别的设置项也不影响它。 */
    assert(m.config.alarm[GAS_MQ4] == 2400u);
    assert(m.config.sample_period_ms == 100u);
}

static void test_buzzer_set(void)
{
    gas_t m;
    uint32_t t;
    unsigned i;
    char name[16];

    run_to_normal(&m, 0);
    /* 60 以内逐档都对，61 是「一直响」。 */
    assert(gas_set_buzzer(&m, 1u, T0) && m.config.buzzer == 1u);
    assert(gas_set_buzzer(&m, GAS_BUZZER_MAX_S, T0));
    assert(m.config.buzzer == GAS_BUZZER_MAX_S);
    assert(gas_set_buzzer(&m, GAS_BUZZER_ALWAYS, T0));
    assert(m.config.buzzer == GAS_BUZZER_ALWAYS);
    assert(gas_set_buzzer(&m, GAS_BUZZER_OFF, T0));
    assert(m.config.buzzer == GAS_BUZZER_OFF && config_buzzer_duration_ms(&m.config) == 0u);

    /* 超出编码范围的值必须在收窄成 uint8 之前就被拒：256 截断成 0 会成为
     * 一个合法取值，静默地把蜂鸣器关掉，而调用者要的显然不是这个。 */
    assert(!gas_set_buzzer(&m, GAS_BUZZER_ALWAYS + 1u, T0));
    assert(!gas_set_buzzer(&m, 256u, T0));
    assert(!gas_set_buzzer(&m, 0xffffu, T0));
    assert(m.config.buzzer == GAS_BUZZER_OFF);

    /* 面板上显示的名字就是串口回显的名字，两边共用一份拼写。 */
    config_buzzer_name(m.config.buzzer, name, sizeof(name));
    assert(strcmp(name, "OFF") == 0);

    /* 蜂鸣器时长不参与限值判断，所以调它不重启「低于危险阈值多久算恢复」那个
     * 窗口：重启只会让站在 SAFE_WAIT 里等 KEY5 的人白等三秒。阈值参与，调它
     * 就必须重启。 */
    for (i = 0; i < 100u; ++i) gas_key(&m, GAS_KEY_DOWN, T0);
    for (t = (T0 + 100u); t <= (T0 + 3100u); t += 100) sample(&m, t, 100, 100, 100);
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);

    m.dirty = false;
    assert(gas_set_buzzer(&m, 30u, (T0 + 3101u)));
    assert(m.dirty && m.reset_ready);
    assert(gas_set_threshold(&m, GAS_MQ4, 2600u, (T0 + 3101u)));
    assert(!m.reset_ready);
    /* 窗口从改动那一刻重新开始计时，之后照常成立。 */
    for (t = (T0 + 3200u); t <= (T0 + 6200u); t += 100) sample(&m, t, 100, 100, 100);
    assert(m.reset_ready);
    gas_key(&m, GAS_KEY_CONFIRM, (T0 + 6200u));
    assert(gas_valve_open(&m));
}

static void test_ready_hysteresis(void)
{
    gas_t m;
    uint32_t t;
    uint16_t mid;
    uint16_t exit;

    run_to_normal(&m, 0u);
    sample(&m, T0 + 100u, m.config.alarm[0], 500u, 500u);
    assert(m.state == GAS_ALARM);
    for (t = T0 + 200u; t <= T0 + 3200u; t += 100u) {
        sample(&m, t, 500u, 500u, 500u);
    }
    assert(m.state == GAS_SAFE_WAIT && m.reset_ready);

    mid = (uint16_t)((uint32_t)m.config.alarm[0] * 85u / 100u);
    sample(&m, T0 + 3300u, mid, 500u, 500u);
    assert(m.reset_ready);

    exit = (uint16_t)((uint32_t)m.config.alarm[0] * GAS_SAFE_RELEASE_PERCENT / 100u);
    sample(&m, T0 + 3400u, exit, 500u, 500u);
    assert(!m.reset_ready && !m.safe_timing);
}

typedef struct { unsigned calls; unsigned fail_at; bool invalid; } adc_fake_t;

static bool read_adc(void *context, uint8_t channel, uint16_t *value)
{
    adc_fake_t *fake = context;
    static const uint8_t expected[] = {0, 1, 4};
    unsigned index = fake->calls++;
    assert(channel == expected[index / 8]);
    if (fake->calls == fake->fail_at) return false;
    *value = fake->invalid ? 4096 : (uint16_t)(100u * (index / 8 + 1u) + index % 8);
    return true;
}

static void test_adc_average(void)
{
    adc_fake_t fake = {0, 0, false};
    uint16_t values[GAS_COUNT];
    assert(gas_read_samples(read_adc, &fake, values));
    assert(fake.calls == 24);
    assert(values[0] == 103 && values[1] == 203 && values[2] == 303);
    fake.calls = 0; fake.fail_at = 10;
    assert(!gas_read_samples(read_adc, &fake, values));
    assert(fake.calls == 10);
    fake.calls = 0; fake.fail_at = 0; fake.invalid = true;
    assert(!gas_read_samples(read_adc, &fake, values));
    assert(fake.calls == 1);
}

int main(void)
{
    test_buzzer_value();
    test_buzzer_keys();
    test_buzzer_set();
    test_adc_average();
    test_ready_hysteresis();
    test_states();
    test_power_up_opens();
    test_no_fault_before_first_sample();
    test_failed_attempt_is_a_fault();
    test_persisted_lockout();
    test_low_threshold_recovers();
    test_keys();
    test_sample_period();
    puts("PASS: gas states/power-up/fault/recovery/keys/period/buzzer/wraparound");
    return 0;
}
