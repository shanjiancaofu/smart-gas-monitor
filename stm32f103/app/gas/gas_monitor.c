#include "gas/gas_monitor.h"
#include <stdio.h>
#include <string.h>

/* Periods the keys step through. A value loaded from EEPROM may sit between
 * two entries; editing then snaps to the next one in the pressed direction. */
static const uint16_t period_steps[] = {100, 200, 500, 1000, 2000, 5000};
#define PERIOD_STEP_COUNT (sizeof(period_steps) / sizeof(period_steps[0]))

static bool threshold_step(uint16_t *value, bool up)
{
    uint16_t old = *value;
    if (up) *value = *value > GAS_THRESHOLD_MAX - GAS_THRESHOLD_STEP
        ? GAS_THRESHOLD_MAX : (uint16_t)(*value + GAS_THRESHOLD_STEP);
    else *value = *value < GAS_THRESHOLD_MIN + GAS_THRESHOLD_STEP
        ? GAS_THRESHOLD_MIN : (uint16_t)(*value - GAS_THRESHOLD_STEP);
    return *value != old;
}

static bool period_step(uint16_t *value, bool up)
{
    uint16_t old = *value;
    unsigned i;
    if (up) {
        for (i = 0; i < PERIOD_STEP_COUNT; ++i)
            if (period_steps[i] > old) break;
        *value = i < PERIOD_STEP_COUNT ? period_steps[i] : period_steps[PERIOD_STEP_COUNT - 1];
    } else {
        for (i = PERIOD_STEP_COUNT; i > 0; --i)
            if (period_steps[i - 1] < old) break;
        *value = i > 0 ? period_steps[i - 1] : period_steps[0];
    }
    return *value != old;
}

void gas_config_defaults(gas_config_t *c)
{
    /* Demonstration ADC counts, not calibrated ppm thresholds. */
    c->alarm[GAS_MQ4] = 2400;
    c->alarm[GAS_MQ7] = 2000;
    c->alarm[GAS_MQ8] = 2400;
    c->sample_period_ms = 100;
    c->lockout = false;
}
bool gas_config_valid(const gas_config_t *c)
{
    unsigned i;
    for (i = 0; i < GAS_COUNT; ++i)
        if (c->alarm[i] < GAS_THRESHOLD_MIN || c->alarm[i] > GAS_THRESHOLD_MAX) return false;
    return c->sample_period_ms >= GAS_PERIOD_MIN_MS &&
           c->sample_period_ms <= GAS_PERIOD_MAX_MS &&
           c->sample_period_ms % GAS_TICK_MS == 0u;
}

/* Everything that has to happen when a limit changes, wherever the change came
 * from: the new value needs saving, and an interval that was being timed
 * against the old limits must not be allowed to mature under the new ones. */
static void config_changed(gas_monitor_t *m, uint32_t now)
{
    m->dirty = true;
    m->changed_ms = now;
    m->safe_timing = false;
    gas_monitor_tick(m, now);
}

const char *gas_channel_name(gas_channel_t channel)
{
    switch (channel) {
    case GAS_MQ4: return "MQ4";
    case GAS_MQ7: return "MQ7";
    case GAS_MQ8: return "MQ8";
    default: break;
    }
    return "MQ?";
}

const char *gas_state_name(gas_state_t state)
{
    switch (state) {
    case GAS_WARMUP: return "WARMUP";
    case GAS_NORMAL: return "NORMAL";
    case GAS_WARNING: return "WARNING";
    case GAS_ALARM: return "ALARM";
    case GAS_SAFE_WAIT: return "SAFE";
    case GAS_FAULT: return "FAULT";
    default: break;
    }
    return "?";
}

void gas_alarm_mask_name(uint8_t mask, char *out, size_t size)
{
    size_t used = 0u;
    unsigned i;
    if (size == 0u) return;
    out[0] = '\0';
    for (i = 0; i < GAS_COUNT; ++i) {
        if ((mask & (1u << i)) == 0u) continue;
        (void)snprintf(out + used, size - used, "%s%s", used != 0u ? "+" : "",
                       gas_channel_name((gas_channel_t)i));
        used = strlen(out);
        if (used + 1u >= size) break;
    }
    if (used == 0u) (void)snprintf(out, size, "NONE");
}

uint16_t gas_warning_threshold(uint16_t alarm)
{
    return (uint16_t)((uint32_t)alarm * GAS_WARNING_PERCENT / 100u);
}
uint16_t gas_safe_threshold(uint16_t alarm)
{
    return (uint16_t)((uint32_t)alarm * GAS_SAFE_PERCENT / 100u);
}
uint32_t gas_sample_timeout_ms(const gas_monitor_t *m)
{
    return (uint32_t)m->config.sample_period_ms * GAS_SAMPLE_TIMEOUT_PERIODS;
}
void gas_monitor_init(gas_monitor_t *m, const gas_config_t *c, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    if (c != 0 && gas_config_valid(c)) m->config = *c;
    else gas_config_defaults(&m->config);
    m->started_ms = now;
    m->state = GAS_WARMUP;
    /* A healthy power-up opens the valve once warm-up confirms safety. Only an
     * alarm or a sampler fault latches it shut for manual release. */
    m->latched = m->config.lockout;
}
void gas_monitor_tick(gas_monitor_t *m, uint32_t now)
{
    unsigned i;
    bool warning = false, safe = true;
    uint8_t alarm = 0;
    m->reset_ready = false;
    if (!m->sample_valid || (uint32_t)(now - m->sample_ms) >= gas_sample_timeout_ms(m)) {
        /* Before the first conversion the sampler has not failed, it has not
         * started: that is still warm-up, and it must not latch the valve. Once
         * an attempt has been made, a missing reading is a sampler fault. */
        m->state = m->sample_attempted ? GAS_FAULT : GAS_WARMUP;
        if (m->sample_attempted) m->latched = true;
        m->safe_timing = false;
        return;
    }
    for (i = 0; i < GAS_COUNT; ++i) {
        if (m->adc[i] >= m->config.alarm[i]) alarm |= (uint8_t)(1u << i);
        if (m->adc[i] >= gas_warning_threshold(m->config.alarm[i])) warning = true;
        if (m->adc[i] >= gas_safe_threshold(m->config.alarm[i])) safe = false;
    }
    if (alarm != 0) {
        if (m->alarm_mask == 0) ++m->alarm_count;
        m->alarm_mask = alarm; m->latched = true; m->config.lockout = true;
        m->dirty = true;
        m->state = GAS_ALARM; m->safe_timing = false;
        return;
    }
    m->alarm_mask = 0;
    if ((uint32_t)(now - m->started_ms) < GAS_WARMUP_MS) {
        m->state = GAS_WARMUP; m->safe_timing = false;
        return;
    }
    if (safe) {
        if (!m->safe_timing) { m->safe_since_ms = now; m->safe_timing = true; }
        m->reset_ready = (uint32_t)(now - m->safe_since_ms) >= GAS_SAFE_HOLD_MS;
    } else m->safe_timing = false;
    m->state = m->latched ? GAS_SAFE_WAIT : (warning ? GAS_WARNING : GAS_NORMAL);
}
void gas_monitor_sample(gas_monitor_t *m, const uint16_t adc[GAS_COUNT], bool valid, uint32_t now)
{
    unsigned i;
    /* A fresh sample must not conceal an earlier scheduler/sample outage. */
    if (m->sample_attempted && (uint32_t)(now - m->sample_ms) >= gas_sample_timeout_ms(m))
        gas_monitor_tick(m, now);
    /* Every conversion attempt counts, successful or not: a sampler that keeps
     * returning nothing is a fault, not an endless warm-up. */
    m->sample_attempted = true;
    m->sample_valid = valid;
    if (valid) {
        for (i = 0; i < GAS_COUNT; ++i) {
            if (adc[i] > 4095) m->sample_valid = false;
            m->adc[i] = adc[i];
        }
        m->sample_ms = now;
    }
    gas_monitor_tick(m, now);
}
void gas_monitor_key(gas_monitor_t *m, unsigned key, uint32_t now)
{
    bool changed = false;
    gas_monitor_tick(m, now);
    if (key == 1) m->selected = (uint8_t)((m->selected + 1u) % GAS_SEL_COUNT);
    else if (key == 2 || key == 3) {
        /* Test the selector by name and not by "anything but MAIN": the upper
         * bound is what keeps selected - 1 inside alarm[]. */
        if (m->selected == GAS_SEL_PERIOD) {
            changed = period_step(&m->config.sample_period_ms, key == 2);
        } else if (m->selected >= GAS_SEL_MQ4 && m->selected <= GAS_SEL_MQ8) {
            changed = threshold_step(&m->config.alarm[m->selected - 1u], key == 2);
        }
        if (changed) config_changed(m, now);
    } else if (key == 4 && m->reset_ready && m->state == GAS_SAFE_WAIT) {
        m->latched = false; m->config.lockout = false; m->dirty = true;
        gas_monitor_tick(m, now);
    }
}
bool gas_monitor_set_threshold(gas_monitor_t *m, gas_channel_t channel,
                               uint16_t value, uint32_t now)
{
    gas_config_t candidate = m->config;
    if (channel >= GAS_COUNT) return false;
    candidate.alarm[channel] = value;
    /* Validating the whole candidate rather than just the new field means the
     * running configuration can never drift out of the range the decoder
     * accepts, whichever field a caller thought it was changing. */
    if (!gas_config_valid(&candidate)) return false;
    m->config = candidate;
    config_changed(m, now);
    return true;
}

bool gas_monitor_set_period(gas_monitor_t *m, uint16_t ms, uint32_t now)
{
    gas_config_t candidate = m->config;
    candidate.sample_period_ms = ms;
    if (!gas_config_valid(&candidate)) return false;
    m->config = candidate;
    config_changed(m, now);
    return true;
}

void gas_monitor_close_valve(gas_monitor_t *m, uint32_t now)
{
    m->latched = true; m->config.lockout = true; m->dirty = true;
    gas_monitor_tick(m, now);
}
bool gas_monitor_valve_open(const gas_monitor_t *m)
{
    return !m->latched && (m->state == GAS_NORMAL || m->state == GAS_WARNING);
}
bool gas_monitor_save_due(const gas_monitor_t *m, uint32_t now)
{
    return m->dirty && (uint32_t)(now - m->changed_ms) >= GAS_SAVE_DELAY_MS;
}
