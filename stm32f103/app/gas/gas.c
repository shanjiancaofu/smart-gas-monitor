#include "gas/gas.h"
#include <stdio.h>
#include <string.h>

/* 按键逐档切换的周期列表。从 EEPROM 载入的值可能落在两档之间；此时编辑会
 * 朝按键方向吸附到相邻的下一档。 */
static const uint16_t period_steps[] = {100, 200, 500, 1000, 2000, 5000};
#define PERIOD_STEP_COUNT (sizeof(period_steps) / sizeof(period_steps[0]))

static bool threshold_step(uint16_t *value, bool up)
{
    uint16_t old = *value;
    if (up) {
        *value = *value > GAS_THRESHOLD_MAX - GAS_THRESHOLD_STEP
                     ? GAS_THRESHOLD_MAX
                     : (uint16_t)(*value + GAS_THRESHOLD_STEP);
    } else {
        *value = *value < GAS_THRESHOLD_MIN + GAS_THRESHOLD_STEP
                     ? GAS_THRESHOLD_MIN
                     : (uint16_t)(*value - GAS_THRESHOLD_STEP);
    }
    return *value != old;
}

static bool period_step(uint16_t *value, bool up)
{
    uint16_t old = *value;
    unsigned i;
    if (up) {
        for (i = 0; i < PERIOD_STEP_COUNT; ++i) {
            if (period_steps[i] > old) {
                break;
            }
        }
        *value = i < PERIOD_STEP_COUNT ? period_steps[i] : period_steps[PERIOD_STEP_COUNT - 1];
    } else {
        for (i = PERIOD_STEP_COUNT; i > 0; --i) {
            if (period_steps[i - 1] < old) {
                break;
            }
        }
        *value = i > 0 ? period_steps[i - 1] : period_steps[0];
    }
    return *value != old;
}

/* 蜂鸣器时长的取值排在同一个数轴上，所以是简单加减、两端夹取，不做环绕：
 * 「加」永远意味着响得更久，撞到「一直响」就停在那一档。 */
static bool buzzer_step(uint8_t *value, bool up)
{
    uint8_t old = *value;
    if (up) {
        if (*value < GAS_BUZZER_MAX_S) {
            *value = (uint8_t)(*value + 1u);
        }
    } else {
        if (*value > GAS_BUZZER_OFF) {
            *value = (uint8_t)(*value - 1u);
        }
    }
    return *value != old;
}

static void set_lockout(gas_t *m, uint32_t now)
{
    if (!m->config.lockout) {
        m->config.lockout = true;
        m->dirty = true;
        m->changed_ms = now;
    }
    m->latched = true;
}

static void clear_lockout(gas_t *m, uint32_t now)
{
    if (m->config.lockout) {
        m->config.lockout = false;
        m->dirty = true;
        m->changed_ms = now;
    }
    m->latched = false;
}

/* 改动需要落盘，先记账。延时保存和失败后的重试都看这两个字段。 */
static void mark_dirty(gas_t *m, uint32_t now)
{
    m->dirty = true;
    m->changed_ms = now;
}

/* 影响限值判断的改动走这里，不管变化来自哪里：正在按旧限值计时的那个区间，
 * 不能让它换上新限值后继续计时直到成立。
 *
 * 蜂鸣器时长不走这里，只走 mark_dirty：它不参与「低于危险阈值多久算恢复」的
 * 判断，重启那个窗口只会让 SAFE_WAIT 里的 KEY4 白等三秒。 */
static void config_changed(gas_t *m, uint32_t now)
{
    mark_dirty(m, now);
    m->safe_timing = false;
    gas_update(m, now);
}

const char *gas_channel_name(gas_channel_t channel)
{
    switch (channel) {
    case GAS_MQ4:
        return "MQ4";
    case GAS_MQ6:
        return "MQ6";
    case GAS_MQ7:
        return "MQ7";
    default:
        break;
    }
    return "MQ?";
}

const char *gas_state_name(gas_state_t state)
{
    switch (state) {
    case GAS_WARMUP:
        return "WARMUP";
    case GAS_NORMAL:
        return "NORMAL";
    case GAS_WARNING:
        return "WARNING";
    case GAS_ALARM:
        return "ALARM";
    case GAS_SAFE_WAIT:
        /* 不要写成 SAFE：这个状态是「还锁着，等环境安全满 3 秒再按 KEY4」，
         * 叫 SAFE 会让人以为已经恢复完了。面板上拆成 LOCKED/READY 显示。 */
        return "SAFE_WAIT";
    case GAS_FAULT:
        return "FAULT";
    default:
        break;
    }
    return "?";
}

void gas_alarm_mask_name(uint8_t mask, char *out, size_t size)
{
    size_t used = 0u;
    unsigned i;
    if (size == 0u) {
        return;
    }
    out[0] = '\0';
    for (i = 0; i < GAS_COUNT; ++i) {
        if ((mask & (1u << i)) == 0u) {
            continue;
        }
        (void)snprintf(out + used, size - used, "%s%s", used != 0u ? "+" : "",
                       gas_channel_name((gas_channel_t)i));
        used = strlen(out);
        if (used + 1u >= size) {
            break;
        }
    }
    if (used == 0u) {
        (void)snprintf(out, size, "NONE");
    }
}

uint16_t gas_warning_threshold(uint16_t alarm)
{
    return (uint16_t)((uint32_t)alarm * GAS_WARNING_PERCENT / 100u);
}
uint16_t gas_safe_threshold(uint16_t alarm)
{
    return (uint16_t)((uint32_t)alarm * GAS_SAFE_PERCENT / 100u);
}
uint32_t gas_sample_timeout_ms(const gas_t *m)
{
    return (uint32_t)m->config.sample_period_ms + GAS_SAMPLE_TIMEOUT_MARGIN_MS;
}
void gas_init(gas_t *m, const gas_config_t *c, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    if (c != 0 && config_valid(c)) {
        m->config = *c;
    } else {
        config_defaults(&m->config);
    }
    m->started_ms = now;
    m->state = GAS_WARMUP;
    /* 正常上电会在预热结束且环境安全时自动开阀。只有报警或采样器故障才会
     * 锁存关阀，等待人工解除。 */
    m->latched = m->config.lockout;
}
void gas_update(gas_t *m, uint32_t now)
{
    unsigned i;
    bool warning = false, safe_enter = true, safe_hold = true;
    uint8_t alarm = 0;
    m->reset_ready = false;
    if (!m->sample_valid || (uint32_t)(now - m->sample_ms) >= gas_sample_timeout_ms(m)) {
        /* 第一次转换之前，采样器不是失败了，而是还没开始：这仍属预热，不能
         * 锁存阀门。只要尝试过一次，读数缺失就是采样器故障。 */
        m->state = m->sample_attempted ? GAS_FAULT : GAS_WARMUP;
        if (m->sample_attempted) {
            set_lockout(m, now);
        }
        m->safe_timing = false;
        ++m->dbg_timeout;
        return;
    }
    if ((uint32_t)(now - m->started_ms) < GAS_WARMUP_MS) {
        ++m->dbg_warmup;
        m->alarm_mask = 0;
        m->state = GAS_WARMUP;
        m->safe_timing = false;
        return;
    }
    for (i = 0; i < GAS_COUNT; ++i) {
        if (m->adc[i] >= m->config.alarm[i]) {
            alarm |= (uint8_t)(1u << i);
        }
        if (m->adc[i] >= gas_warning_threshold(m->config.alarm[i])) {
            warning = true;
        }
        if (m->adc[i] >= gas_safe_threshold(m->config.alarm[i])) {
            safe_enter = false;
        }
        if (m->adc[i] >= (uint16_t)((uint32_t)m->config.alarm[i] *
                                     GAS_SAFE_RELEASE_PERCENT / 100u)) {
            safe_hold = false;
        }
    }
    if (alarm != 0) {
        if (m->alarm_mask == 0) {
            ++m->alarm_count;
        }
        m->alarm_mask = alarm;
        ++m->dbg_alarm;
        set_lockout(m, now);
        m->state = GAS_ALARM;
        m->safe_timing = false;
        return;
    }
    m->alarm_mask = 0;
    /* Recovery hysteresis: enter the safe window below 80%, but once the
       timer/READY state is active, do not drop it until a channel reaches 90%.
       This prevents ADC quantization and potentiometer noise from flashing
       LOCKED/READY while still rejecting a real concentration rise. */
    if (!m->safe_timing) {
        if (safe_enter) {
            ++m->dbg_safe;
            m->safe_since_ms = now;
            m->safe_timing = true;
        } else {
            ++m->dbg_unsafe;
        }
    } else if (!safe_hold) {
        ++m->dbg_unsafe;
        m->safe_timing = false;
    } else {
        ++m->dbg_safe;
    }
    if (m->safe_timing) {
        m->reset_ready = (uint32_t)(now - m->safe_since_ms) >= GAS_SAFE_HOLD_MS;
    }
    m->state = m->latched ? GAS_SAFE_WAIT : (warning ? GAS_WARNING : GAS_NORMAL);
}
void gas_sample(gas_t *m, const uint16_t adc[GAS_COUNT], bool valid, uint32_t now)
{
    unsigned i;
    /* 新样本不能掩盖之前发生过的调度或采样中断。 */
    if (m->sample_attempted && (uint32_t)(now - m->sample_ms) >= gas_sample_timeout_ms(m)) {
        gas_update(m, now);
    }
    /* 每次转换尝试都算数，成功与否都一样：一直读不出东西的采样器是故障，
     * 不是没完没了的预热。 */
    m->sample_attempted = true;
    m->sample_valid = valid;
    if (valid) {
        for (i = 0; i < GAS_COUNT; ++i) {
            if (adc[i] > 4095) {
                m->sample_valid = false;
            }
            m->adc[i] = adc[i];
        }
        m->sample_ms = now;
    }
    gas_update(m, now);
}
/* 设置页里按 KEY3/KEY4 时，改的是当前选中项。前三项的编号与通道编号一致，所以
 * item 直接就是 alarm[] 的下标。 */
static void item_step(gas_t *m, bool up, uint32_t now)
{
    switch (m->item) {
    case GAS_ITEM_MQ4:
    case GAS_ITEM_MQ6:
    case GAS_ITEM_MQ7:
        if (threshold_step(&m->config.alarm[m->item], up)) {
            config_changed(m, now);
        }
        break;
    case GAS_ITEM_PERIOD:
        if (period_step(&m->config.sample_period_ms, up)) {
            config_changed(m, now);
        }
        break;
    default: /* GAS_ITEM_BUZZER */
        /* 只记账，不重启安全窗口——理由见 config_changed()。 */
        if (buzzer_step(&m->config.buzzer, up)) {
            mark_dirty(m, now);
        }
        break;
    }
}

/* 设置项的增减只由设置页分发；KEY4 永远只尝试安全解除。 */
void gas_key(gas_t *m, unsigned key, uint32_t now)
{
    gas_update(m, now);
    if (key == GAS_KEY_SELECT) {
        m->item = (uint8_t)((m->item + 1u) % GAS_ITEM_COUNT);
    } else if (key == GAS_KEY_UP || key == GAS_KEY_DOWN) {
        item_step(m, key == GAS_KEY_UP, now);
    } else if (key == GAS_KEY_CONFIRM && m->state == GAS_SAFE_WAIT && m->reset_ready) {
        clear_lockout(m, now);
        gas_update(m, now);
    }
}
bool gas_set_threshold(gas_t *m, gas_channel_t channel, uint16_t value, uint32_t now)
{
    gas_config_t candidate = m->config;
    if (channel >= GAS_COUNT) {
        return false;
    }
    candidate.alarm[channel] = value;
    /* 校验整个候选配置而不只是被改动的那个字段，这样无论调用者以为自己在
     * 改哪个字段，运行中的配置都不会漂出解码器接受的范围。 */
    if (!config_valid(&candidate)) {
        return false;
    }
    m->config = candidate;
    config_changed(m, now);
    return true;
}

bool gas_set_period(gas_t *m, uint16_t ms, uint32_t now)
{
    gas_config_t candidate = m->config;
    candidate.sample_period_ms = ms;
    if (!config_valid(&candidate)) {
        return false;
    }
    m->config = candidate;
    config_changed(m, now);
    return true;
}

bool gas_set_buzzer(gas_t *m, uint16_t value, uint32_t now)
{
    gas_config_t candidate = m->config;
    /* 先判上界再收窄：256 截断成 0 会被 config_valid() 放行，静默变成
     * 「不响」，那是个安全的取值，但绝不是调用者要的那个。 */
    if (value > GAS_BUZZER_MAX_S) {
        return false;
    }
    candidate.buzzer = (uint8_t)value;
    if (!config_valid(&candidate)) {
        return false;
    }
    m->config = candidate;
    mark_dirty(m, now);
    return true;
}

void gas_close_valve(gas_t *m, uint32_t now)
{
    set_lockout(m, now);
    gas_update(m, now);
}
bool gas_valve_open(const gas_t *m)
{
    return !m->latched && (m->state == GAS_NORMAL || m->state == GAS_WARNING);
}
bool gas_save_due(const gas_t *m, uint32_t now)
{
    return m->dirty && (uint32_t)(now - m->changed_ms) >= GAS_SAVE_DELAY_MS;
}

bool gas_read_samples(gas_adc_read_fn read, void *context, uint16_t values[GAS_COUNT])
{
    static const uint8_t channels[GAS_COUNT] = {0, 1, 4};
    unsigned channel, sample;

    for (channel = 0; channel < GAS_COUNT; ++channel) {
        uint32_t sum = 0;
        for (sample = 0; sample < 8; ++sample) {
            uint16_t value;
            if (!read(context, channels[channel], &value) || value > 4095u) {
                return false;
            }
            sum += value;
        }
        values[channel] = (uint16_t)(sum / 8u);
    }
    return true;
}

uint16_t gas_get_value(const gas_t *gas, uint8_t id)
{
    return id < GAS_COUNT ? gas->adc[id] : 0u;
}

gas_state_t gas_get_state(const gas_t *gas)
{
    return gas->state;
}
