#ifndef GAS_H
#define GAS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config/config.h"

typedef enum { GAS_MQ4 = 0, GAS_MQ6, GAS_MQ7 } gas_channel_t;

typedef enum {
    GAS_ITEM_MQ4 = 0,
    GAS_ITEM_MQ6,
    GAS_ITEM_MQ7,
    GAS_ITEM_PERIOD,
    GAS_ITEM_BUZZER,
    GAS_ITEM_COUNT
} gas_item_t;

typedef enum {
    GAS_KEY_PAGE = 1,
    GAS_KEY_UP,
    GAS_KEY_DOWN,
    GAS_KEY_CONFIRM,
    GAS_KEY_SELECT
} gas_key_t;

typedef enum {
    GAS_WARMUP,
    GAS_NORMAL,
    GAS_WARNING,
    GAS_ALARM,
    GAS_SAFE_WAIT,
    GAS_FAULT,
    GAS_STATE_COUNT
} gas_state_t;

typedef struct {
    gas_config_t config;
    gas_state_t state;
    uint16_t adc[GAS_COUNT];
    uint32_t started_ms, sample_ms, safe_since_ms, changed_ms;
    uint32_t alarm_count;
    uint8_t alarm_mask;

    uint8_t item;
    bool sample_valid, sample_attempted, latched, safe_timing, reset_ready, dirty;
} gas_t;

const char *gas_channel_name(gas_channel_t channel);

const char *gas_state_name(gas_state_t state);

void gas_alarm_mask_name(uint8_t mask, char *out, size_t size);
uint16_t gas_warning_threshold(uint16_t alarm);
uint16_t gas_safe_threshold(uint16_t alarm);

uint32_t gas_sample_timeout_ms(const gas_t *m);
void gas_init(gas_t *m, const gas_config_t *config, uint32_t now);

void gas_sample(gas_t *m, const uint16_t adc[GAS_COUNT], bool valid, uint32_t now);
void gas_update(gas_t *m, uint32_t now);

void gas_key(gas_t *m, unsigned key, uint32_t now);

bool gas_set_threshold(gas_t *m, gas_channel_t channel, uint16_t value, uint32_t now);
bool gas_set_period(gas_t *m, uint16_t ms, uint32_t now);

bool gas_set_buzzer(gas_t *m, uint16_t value, uint32_t now);

void gas_close_valve(gas_t *m, uint32_t now);
bool gas_valve_open(const gas_t *m);
bool gas_save_due(const gas_t *m, uint32_t now);

typedef bool (*gas_adc_read_fn)(void *, uint8_t, uint16_t *);
bool gas_read_samples(gas_adc_read_fn read, void *context, uint16_t values[GAS_COUNT]);
uint16_t gas_get_value(const gas_t *gas, uint8_t id);
gas_state_t gas_get_state(const gas_t *gas);
#endif
