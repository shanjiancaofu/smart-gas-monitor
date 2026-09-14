#ifndef GAS_MONITOR_H
#define GAS_MONITOR_H
#include <stdbool.h>
#include <stdint.h>

#define GAS_CHANNELS 3u
#define GAS_SAMPLE_TIMEOUT_MS 500u
#define GAS_SAFE_HOLD_MS 3000u
#define GAS_WARMUP_MS 60000u
#define GAS_SAVE_DELAY_MS 2000u
typedef enum { GAS_WARMUP, GAS_NORMAL, GAS_WARNING, GAS_ALARM,
               GAS_SAFE_WAIT, GAS_FAULT } gas_state_t;
typedef struct { uint16_t alarm[GAS_CHANNELS]; } gas_config_t;
typedef struct {
    gas_config_t config;
    gas_state_t state;
    uint16_t adc[GAS_CHANNELS];
    uint32_t started_ms, sample_ms, safe_since_ms, changed_ms;
    uint32_t alarm_count;
    uint8_t alarm_mask, selected;
    bool sample_valid, latched, safe_timing, reset_ready, dirty;
} gas_monitor_t;

void gas_config_defaults(gas_config_t *config);
bool gas_config_valid(const gas_config_t *config);
uint16_t gas_warning_threshold(uint16_t alarm);
void gas_monitor_init(gas_monitor_t *m, const gas_config_t *config, uint32_t now);
/* Pass only complete fresh three-channel samples. ADC failure invalidates all. */
void gas_monitor_sample(gas_monitor_t *m, const uint16_t adc[3], bool valid, uint32_t now);
void gas_monitor_tick(gas_monitor_t *m, uint32_t now);
/* Debounced press edge, keys 1..4. No remote valve-open API. */
void gas_monitor_key(gas_monitor_t *m, unsigned key, uint32_t now);
bool gas_monitor_valve_open(const gas_monitor_t *m);
bool gas_monitor_save_due(const gas_monitor_t *m, uint32_t now);
#endif
