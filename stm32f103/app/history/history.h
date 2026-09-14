#ifndef HISTORY_H
#define HISTORY_H
#include "gas/gas_monitor.h"
#include "settings/settings.h"
#include <stdbool.h>
#include <stdint.h>

/* Alarm log in the upper half of the AT24C02. 0x00..0x1f belongs to the
 * settings module, 0x20..0xff is 224 bytes, which is exactly 14 records of 16.
 * Filling up overwrites the oldest rather than stopping the log. */
#define HISTORY_BASE 0x20u
#define HISTORY_SLOTS 14u
#define HISTORY_SLOT_SIZE 16u

/* There is no RTC, so a record carries how long the unit had been powered
 * rather than a wall-clock date it cannot know. */
typedef struct {
    uint16_t seq;                 /* 1-based, so the newest record is obvious */
    uint32_t uptime_s;
    uint16_t adc[GAS_COUNT];
    uint8_t alarm_mask;           /* which channels were over threshold */
} history_entry_t;

typedef struct {
    const settings_io_t *io;
    uint16_t next_seq;
    uint8_t next_slot;            /* the oldest slot, and the next to reuse */
    uint8_t count;
} history_t;

/* Scans the slots and returns true when at least one record was readable. An
 * unreadable EEPROM leaves the log empty, which is not an error worth failing
 * startup over. */
bool history_init(history_t *h, const settings_io_t *io);
/* Writes one record. seq is assigned here and written back into *entry. */
bool history_append(history_t *h, history_entry_t *entry);
uint8_t history_count(const history_t *h);
/* Index 0 is the newest record. Returns false when empty or past the end, and
 * when the slot fails its CRC, so a half-written record reads as absent. */
bool history_get(const history_t *h, uint8_t index, history_entry_t *out);
#endif
