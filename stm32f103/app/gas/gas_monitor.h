#ifndef GAS_MONITOR_H
#define GAS_MONITOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Channel order matches mq_sensor_read(); app.c asserts the two stay equal. */
typedef enum {
    GAS_MQ4 = 0,
    GAS_MQ7,
    GAS_MQ8,
    GAS_COUNT
} gas_channel_t;

/* Selector reached by pressing KEY1 repeatedly. The first three line up with
 * the channel indices so alarm[selected - 1] picks the right threshold, and
 * gas_monitor_key() relies on that range to bound the index. */
typedef enum {
    GAS_SEL_MAIN = 0,
    GAS_SEL_MQ4,
    GAS_SEL_MQ7,
    GAS_SEL_MQ8,
    GAS_SEL_PERIOD,
    /* Browse-only page. Nothing here is adjustable, so KEY2/KEY3 are left for
     * the caller to interpret as history scrolling. */
    GAS_SEL_HISTORY,
    GAS_SEL_COUNT
} gas_setting_t;

/* Bounds on what the settings decoder accepts from EEPROM. */
#define GAS_THRESHOLD_MIN 200u
#define GAS_THRESHOLD_MAX 4000u
#define GAS_THRESHOLD_STEP 50u
/* Matches the lowest entry in the key-selectable period list: a minimum that
 * no keypress can reach would only ever be honoured when loaded from EEPROM. */
#define GAS_PERIOD_MIN_MS 100u
#define GAS_PERIOD_MAX_MS 5000u
/* The TIM2 tick the composition root schedules samples on. A period that is not
 * a whole number of ticks would round to a different one than it names, so such
 * a value is refused rather than quietly honoured as its rounded neighbour. */
#define GAS_TICK_MS 10u

#define GAS_WARNING_PERCENT 80u
/* Recovery below the warning band. Proportional so that a threshold near the
 * bottom of the range still leaves a reachable safe region. */
#define GAS_SAFE_PERCENT 70u

#define GAS_SAFE_HOLD_MS 3000u
#define GAS_WARMUP_MS 60000u
#define GAS_SAVE_DELAY_MS 2000u
#define GAS_SAMPLE_TIMEOUT_PERIODS 3u

typedef struct {
    uint16_t alarm[GAS_COUNT];
    uint16_t sample_period_ms;
    bool lockout;
} gas_config_t;

typedef enum { GAS_WARMUP, GAS_NORMAL, GAS_WARNING, GAS_ALARM,
               GAS_SAFE_WAIT, GAS_FAULT, GAS_STATE_COUNT } gas_state_t;

typedef struct {
    gas_config_t config;
    gas_state_t state;
    uint16_t adc[GAS_COUNT];
    uint32_t started_ms, sample_ms, safe_since_ms, changed_ms;
    uint32_t alarm_count;
    uint8_t alarm_mask, selected;
    bool sample_valid, sample_attempted, latched, safe_timing, reset_ready, dirty;
} gas_monitor_t;

/* The display and the serial protocol both name channels and masks, so the
 * spelling lives here once rather than in each of them. */
const char *gas_channel_name(gas_channel_t channel);
/* Short enough for the panel's sixteen-character line. */
const char *gas_state_name(gas_state_t state);
/* "MQ4" / "MQ4+MQ7" / "MQ4+MQ7+MQ8", or "NONE" for an empty mask. */
void gas_alarm_mask_name(uint8_t mask, char *out, size_t size);
void gas_config_defaults(gas_config_t *config);
bool gas_config_valid(const gas_config_t *config);
uint16_t gas_warning_threshold(uint16_t alarm);
uint16_t gas_safe_threshold(uint16_t alarm);
/* A sampler that has fallen this far behind its own period is stalled. */
uint32_t gas_sample_timeout_ms(const gas_monitor_t *m);
void gas_monitor_init(gas_monitor_t *m, const gas_config_t *config, uint32_t now);
/* Pass only complete fresh three-channel samples. ADC failure invalidates all. */
void gas_monitor_sample(gas_monitor_t *m, const uint16_t adc[GAS_COUNT], bool valid, uint32_t now);
void gas_monitor_tick(gas_monitor_t *m, uint32_t now);
/* Debounced press edge, keys 1..4. No remote valve-open API. */
void gas_monitor_key(gas_monitor_t *m, unsigned key, uint32_t now);
/* Configuration from outside the key path, which is only the serial protocol.
 * Both refuse a value the settings decoder would refuse, so a remote write can
 * never produce a configuration that will not survive a power cycle. */
bool gas_monitor_set_threshold(gas_monitor_t *m, gas_channel_t channel,
                               uint16_t value, uint32_t now);
bool gas_monitor_set_period(gas_monitor_t *m, uint16_t ms, uint32_t now);
/* Closes the valve and latches it. Only KEY4 clears the latch, so this cannot
 * be undone remotely: there is deliberately no remote open. */
void gas_monitor_close_valve(gas_monitor_t *m, uint32_t now);
bool gas_monitor_valve_open(const gas_monitor_t *m);
bool gas_monitor_save_due(const gas_monitor_t *m, uint32_t now);
#endif
