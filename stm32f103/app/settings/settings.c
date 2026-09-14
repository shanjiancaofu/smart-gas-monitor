#include "settings/settings.h"
#include <string.h>

/* Two 16-byte slots at 0x00 and 0x10. Last byte is commit marker. */
#define SETTINGS_SLOT_SIZE 16u
#define SETTINGS_MAGIC 0xa5u
/* Version 4 stores lockout at byte 12 and protects it with the record CRC. */
#define SETTINGS_VERSION 4u
#define SETTINGS_COMMIT 0x5au

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
static bool decode(const uint8_t *b, gas_config_t *c)
{
    unsigned i;
    if (b[0] != SETTINGS_MAGIC || b[1] != SETTINGS_VERSION || b[15] != SETTINGS_COMMIT ||
        get16(b + 13) != crc16(b, 13)) return false;
    for (i = 0; i < GAS_COUNT; ++i) c->alarm[i] = get16(b + 4 + i * 2);
    c->sample_period_ms = get16(b + 10);
    c->lockout = b[12] != 0u;
    return gas_config_valid(c);
}
static int latest(uint8_t b[2][SETTINGS_SLOT_SIZE], bool valid[2])
{
    if (!valid[0]) return valid[1] ? 1 : -1;
    if (!valid[1]) return 0;
    return (uint16_t)(get16(b[1] + 2) - get16(b[0] + 2)) < 0x8000u ? 1 : 0;
}
static bool read_slots(const settings_io_t *io, uint8_t b[2][SETTINGS_SLOT_SIZE], bool valid[2])
{
    gas_config_t c;
    unsigned i;
    for (i = 0; i < 2; ++i) {
        if (!io->read(io->context, (uint16_t)(i * SETTINGS_SLOT_SIZE), b[i], SETTINGS_SLOT_SIZE))
            return false;
        valid[i] = decode(b[i], &c);
    }
    return true;
}
bool settings_load(const settings_io_t *io, gas_config_t *c)
{
    uint8_t b[2][SETTINGS_SLOT_SIZE]; bool valid[2]; int slot;
    if (!read_slots(io, b, valid)) return false;
    slot = latest(b, valid);
    return slot >= 0 && decode(b[slot], c);
}
bool settings_save(const settings_io_t *io, const gas_config_t *c)
{
    uint8_t b[2][SETTINGS_SLOT_SIZE], record[SETTINGS_SLOT_SIZE] = {0}, verify[SETTINGS_SLOT_SIZE];
    uint8_t marker = 0;
    bool valid[2]; int slot; unsigned i; uint16_t address, sequence;
    if (!gas_config_valid(c) || !read_slots(io, b, valid)) return false;
    slot = latest(b, valid);
    sequence = slot < 0 ? 0 : (uint16_t)(get16(b[slot] + 2) + 1u);
    address = slot == 0 ? SETTINGS_SLOT_SIZE : 0;
    record[0] = SETTINGS_MAGIC; record[1] = SETTINGS_VERSION; put16(record + 2, sequence);
    for (i = 0; i < GAS_COUNT; ++i) put16(record + 4 + i * 2, c->alarm[i]);
    put16(record + 10, c->sample_period_ms);
    record[12] = c->lockout ? 1u : 0u;
    put16(record + 13, crc16(record, 13)); record[15] = SETTINGS_COMMIT;
    /* Clear the commit marker first so an interrupted write leaves the slot
     * invalid rather than half-updated. */
    if (!io->write(io->context, (uint16_t)(address + SETTINGS_SLOT_SIZE - 1u), &marker, 1) ||
        !io->write(io->context, address, record, SETTINGS_SLOT_SIZE - 1u) ||
        !io->write(io->context, (uint16_t)(address + SETTINGS_SLOT_SIZE - 1u),
                   record + SETTINGS_SLOT_SIZE - 1u, 1) ||
        !io->read(io->context, address, verify, SETTINGS_SLOT_SIZE)) return false;
    return memcmp(record, verify, SETTINGS_SLOT_SIZE) == 0;
}
