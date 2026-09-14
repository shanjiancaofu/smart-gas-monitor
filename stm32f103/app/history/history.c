#include "history/history.h"
#include <string.h>

/*   0..1   seq
 *   2..5   uptime_s
 *   6..11  adc[GAS_COUNT]
 *   12     alarm_mask
 *   13     reserved, written as zero
 *   14..15 crc16 over 0..13
 * An erased slot reads as all ones, which fails both the CRC and the seq
 * sentinel, so it is skipped without a separate "is empty" flag. */
#define HISTORY_CRC_AT 14u
#define HISTORY_SEQ_NONE 0xffffu

static uint16_t crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0xffff;
    unsigned i;
    while (n--) {
        crc ^= (uint16_t)*p++ << 8;
        for (i = 0; i < 8; ++i)
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint16_t slot_offset(unsigned slot)
{
    return (uint16_t)(HISTORY_BASE + slot * HISTORY_SLOT_SIZE);
}

static bool decode(const uint8_t *b, history_entry_t *out)
{
    unsigned i;
    if (get16(b + HISTORY_CRC_AT) != crc16(b, HISTORY_CRC_AT)) return false;
    out->seq = get16(b);
    if (out->seq == HISTORY_SEQ_NONE) return false;
    out->uptime_s = get32(b + 2);
    for (i = 0; i < GAS_COUNT; ++i) out->adc[i] = get16(b + 6 + i * 2);
    out->alarm_mask = b[12];
    return true;
}

/* Reached only with a slot already in hand, so the CRC is not repeated. */
static bool read_slot(const history_t *h, unsigned slot, history_entry_t *out)
{
    uint8_t raw[HISTORY_SLOT_SIZE];
    return h->io->read(h->io->context, slot_offset(slot), raw, sizeof(raw)) &&
           decode(raw, out);
}

bool history_init(history_t *h, const settings_io_t *io)
{
    unsigned newest = 0;
    bool found = false;
    unsigned i;
    memset(h, 0, sizeof(*h));
    h->io = io;
    h->next_seq = 1u;
    if (io == NULL || io->read == NULL) return false;
    for (i = 0; i < HISTORY_SLOTS; ++i) {
        history_entry_t entry;
        if (!read_slot(h, i, &entry)) continue;
        /* Wrapped sequence numbers compare as an unsigned distance, the same
         * way the settings slots pick the newer of two copies. */
        if (!found || (uint16_t)(entry.seq - h->next_seq) < 0x8000u) {
            h->next_seq = entry.seq;
            newest = i;
            found = true;
        }
        ++h->count;
    }
    if (!found) {
        h->next_seq = 1u;
        h->next_slot = 0u;
        return false;
    }
    h->next_seq = (uint16_t)(h->next_seq + 1u);
    /* The slot after the newest one is both the oldest and the next to write,
     * whether or not the log has wrapped yet. */
    h->next_slot = (uint8_t)((newest + 1u) % HISTORY_SLOTS);
    return true;
}

bool history_append(history_t *h, history_entry_t *entry)
{
    uint8_t raw[HISTORY_SLOT_SIZE];
    unsigned slot;
    unsigned i;
    if (h->io == NULL || h->io->write == NULL) return false;
    slot = h->next_slot;
    memset(raw, 0, sizeof(raw));
    entry->seq = h->next_seq;
    put16(raw, entry->seq);
    put32(raw + 2, entry->uptime_s);
    for (i = 0; i < GAS_COUNT; ++i) put16(raw + 6 + i * 2, entry->adc[i]);
    raw[12] = entry->alarm_mask;
    put16(raw + HISTORY_CRC_AT, crc16(raw, HISTORY_CRC_AT));
    if (!h->io->write(h->io->context, slot_offset(slot), raw, sizeof(raw))) return false;
    /* Only bookkept once the write is confirmed, so a failed write is retried
     * into the same slot instead of leaving a gap behind. */
    h->next_seq = (uint16_t)(h->next_seq + 1u);
    if (h->count < HISTORY_SLOTS) ++h->count;
    h->next_slot = (uint8_t)((slot + 1u) % HISTORY_SLOTS);
    return true;
}

uint8_t history_count(const history_t *h) { return h->count; }

bool history_get(const history_t *h, uint8_t index, history_entry_t *out)
{
    unsigned slot;
    if (index >= h->count || h->io == NULL || h->io->read == NULL) return false;
    /* Walk backwards from the slot just before next_slot, which is the newest. */
    slot = ((unsigned)h->next_slot + HISTORY_SLOTS - 1u - index) % HISTORY_SLOTS;
    return read_slot(h, slot, out);
}
