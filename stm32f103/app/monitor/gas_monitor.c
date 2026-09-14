#include "monitor/gas_monitor.h"
#include <string.h>

void gas_config_defaults(gas_config_t *c)
{
    /* Demonstration ADC counts, not calibrated ppm thresholds. */
    c->alarm[0] = 2400; c->alarm[1] = 2000; c->alarm[2] = 2400;
}
bool gas_config_valid(const gas_config_t *c)
{
    unsigned i;
    for (i = 0; i < GAS_CHANNELS; ++i)
        if (c->alarm[i] < 200 || c->alarm[i] > 4000) return false;
    return true;
}
uint16_t gas_warning_threshold(uint16_t alarm)
{
    return (uint16_t)((uint32_t)alarm * 80u / 100u);
}
void gas_monitor_init(gas_monitor_t *m, const gas_config_t *c, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    if (c != 0 && gas_config_valid(c)) m->config = *c;
    else gas_config_defaults(&m->config);
    m->started_ms = now;
    m->state = GAS_WARMUP;
    /* A reboot never bypasses the manual valve release. */
    m->latched = true;
}
void gas_monitor_tick(gas_monitor_t *m, uint32_t now)
{
    unsigned i;
    bool warning = false, safe = true;
    uint8_t alarm = 0;
    m->reset_ready = false;
    if (!m->sample_valid || (uint32_t)(now - m->sample_ms) >= GAS_SAMPLE_TIMEOUT_MS) {
        m->state = GAS_FAULT; m->latched = true; m->safe_timing = false;
        return;
    }
    for (i = 0; i < GAS_CHANNELS; ++i) {
        uint16_t warn = gas_warning_threshold(m->config.alarm[i]);
        if (m->adc[i] >= m->config.alarm[i]) alarm |= (uint8_t)(1u << i);
        if (m->adc[i] >= warn) warning = true;
        if (m->adc[i] >= warn - 100u) safe = false;
    }
    if (alarm != 0) {
        if (m->alarm_mask == 0) ++m->alarm_count;
        m->alarm_mask = alarm; m->latched = true;
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
void gas_monitor_sample(gas_monitor_t *m, const uint16_t adc[3], bool valid, uint32_t now)
{
    unsigned i;
    /* A fresh sample must not conceal an earlier scheduler/sample outage. */
    if (m->sample_valid && (uint32_t)(now - m->sample_ms) >= GAS_SAMPLE_TIMEOUT_MS)
        gas_monitor_tick(m, now);
    m->sample_valid = valid;
    if (valid) {
        for (i = 0; i < GAS_CHANNELS; ++i) {
            if (adc[i] > 4095) m->sample_valid = false;
            m->adc[i] = adc[i];
        }
        m->sample_ms = now;
    }
    gas_monitor_tick(m, now);
}
void gas_monitor_key(gas_monitor_t *m, unsigned key, uint32_t now)
{
    gas_monitor_tick(m, now);
    if (key == 1) m->selected = (uint8_t)((m->selected + 1u) % 4u);
    else if ((key == 2 || key == 3) && m->selected != 0) {
        uint16_t *v = &m->config.alarm[m->selected - 1u];
        uint16_t old = *v;
        if (key == 2) *v = *v > 3950 ? 4000 : (uint16_t)(*v + 50u);
        else *v = *v < 250 ? 200 : (uint16_t)(*v - 50u);
        if (*v != old) {
            m->dirty = true; m->changed_ms = now;
            /* Changed thresholds cannot inherit an earlier safe interval. */
            m->safe_timing = false;
            gas_monitor_tick(m, now);
        }
    } else if (key == 4 && m->reset_ready && m->state == GAS_SAFE_WAIT) {
        m->latched = false;
        gas_monitor_tick(m, now);
    }
}
bool gas_monitor_valve_open(const gas_monitor_t *m)
{
    return !m->latched && (m->state == GAS_NORMAL || m->state == GAS_WARNING);
}
bool gas_monitor_save_due(const gas_monitor_t *m, uint32_t now)
{
    return m->dirty && (uint32_t)(now - m->changed_ms) >= GAS_SAVE_DELAY_MS;
}
