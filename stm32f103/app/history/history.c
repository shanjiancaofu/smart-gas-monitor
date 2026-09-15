#include "history/history.h"
#include <string.h>

/*   0..1   seq
 *   2..5   uptime_s
 *   6..11  adc[GAS_COUNT]
 *   12     alarm_mask
 *   13     预留，写零
 *   14..15 crc16，覆盖 0..13
 * 擦除后的槽位读出全 1，既通不过 CRC 也等于 seq 哨兵值，因此不需要额外
 * 的「是否为空」标志就会被跳过。 */
#define HISTORY_CRC_AT 14u
#define HISTORY_SEQ_NONE 0xffffu

static uint16_t crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0xffff;
    unsigned i;
    while (n--) {
        crc ^= (uint16_t)*p++ << 8;
        for (i = 0; i < 8; ++i) {
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
        }
    }
    return crc;
}
static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t slot_offset(unsigned slot)
{
    return (uint16_t)(HISTORY_BASE + slot * HISTORY_SLOT_SIZE);
}

static bool decode(const uint8_t *b, history_entry_t *out)
{
    unsigned i;
    if (get16(b + HISTORY_CRC_AT) != crc16(b, HISTORY_CRC_AT)) {
        return false;
    }
    out->seq = get16(b);
    if (out->seq == HISTORY_SEQ_NONE) {
        return false;
    }
    out->uptime_s = get32(b + 2);
    for (i = 0; i < GAS_COUNT; ++i) {
        out->adc[i] = get16(b + 6 + i * 2);
    }
    out->alarm_mask = b[12];
    return true;
}

/* 只在已经确定槽位的情况下调用，所以不再重复校验 CRC。 */
static bool read_slot(const history_t *h, unsigned slot, history_entry_t *out)
{
    uint8_t raw[HISTORY_SLOT_SIZE];
    return h->io->read(h->io->context, slot_offset(slot), raw, sizeof(raw)) && decode(raw, out);
}

bool history_init(history_t *h, const config_io_t *io)
{
    unsigned newest = 0;
    bool found = false;
    unsigned i;
    memset(h, 0, sizeof(*h));
    h->io = io;
    h->next_seq = 1u;
    if (io == NULL || io->read == NULL) {
        return false;
    }
    for (i = 0; i < HISTORY_SLOTS; ++i) {
        history_entry_t entry;
        if (!read_slot(h, i, &entry)) {
            continue;
        }
        /* 回绕后的序号按无符号距离比较，与配置槽位在两份副本中挑出较新
         * 的那一份是同一套办法。 */
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
    /* 最新一条之后的那个槽位既是最旧的一条，也是下一个要写的槽位，
     * 无论记录是否已经回绕。 */
    h->next_slot = (uint8_t)((newest + 1u) % HISTORY_SLOTS);
    return true;
}

bool history_add(history_t *h, history_entry_t *entry)
{
    uint8_t raw[HISTORY_SLOT_SIZE];
    unsigned slot;
    unsigned i;
    if (h->io == NULL || h->io->write == NULL) {
        return false;
    }
    slot = h->next_slot;
    memset(raw, 0, sizeof(raw));
    entry->seq = h->next_seq;
    put16(raw, entry->seq);
    put32(raw + 2, entry->uptime_s);
    for (i = 0; i < GAS_COUNT; ++i) {
        put16(raw + 6 + i * 2, entry->adc[i]);
    }
    raw[12] = entry->alarm_mask;
    put16(raw + HISTORY_CRC_AT, crc16(raw, HISTORY_CRC_AT));
    if (!h->io->write(h->io->context, slot_offset(slot), raw, sizeof(raw))) {
        return false;
    }
    /* 写入确认之后才记账，因此写失败会在同一个槽位重试，而不是留下一个
     * 空洞。 */
    h->next_seq = (uint16_t)(h->next_seq + 1u);
    if (h->count < HISTORY_SLOTS) {
        ++h->count;
    }
    h->next_slot = (uint8_t)((slot + 1u) % HISTORY_SLOTS);
    return true;
}

uint8_t history_count(const history_t *h)
{
    return h->count;
}

bool history_clear(history_t *h)
{
    static const uint8_t empty_seq[2] = {0xff, 0xff};
    unsigned slot;

    if (h->io == NULL || h->io->read == NULL || h->io->write == NULL) {
        return false;
    }
    for (slot = 0; slot < HISTORY_SLOTS; ++slot) {
        if (!h->io->write(h->io->context, slot_offset(slot), empty_seq, sizeof(empty_seq))) {
            (void)history_init(h, h->io);
            return false;
        }
    }
    h->count = 0;
    h->next_slot = 0;
    h->next_seq = 1;
    return true;
}

bool history_get(const history_t *h, uint8_t index, history_entry_t *out)
{
    unsigned slot;
    if (index >= h->count || h->io == NULL || h->io->read == NULL) {
        return false;
    }
    /* 从 next_slot 的前一个槽位向前回溯，那个槽位就是最新的一条。 */
    slot = ((unsigned)h->next_slot + HISTORY_SLOTS - 1u - index) % HISTORY_SLOTS;
    return read_slot(h, slot, out);
}
