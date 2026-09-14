#include "parameters/gas_store.h"
#include <string.h>

/* Two 16-byte slots at 0x00 and 0x10. Last byte is commit marker. */
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
    if (b[0] != 0xa5 || b[1] != 1 || b[15] != 0x5a || get16(b + 12) != crc16(b, 12)) return false;
    for (i = 0; i < 3; ++i) c->alarm[i] = get16(b + 4 + i * 2);
    return gas_config_valid(c);
}
static int latest(uint8_t b[2][16], bool valid[2])
{
    if (!valid[0]) return valid[1] ? 1 : -1;
    if (!valid[1]) return 0;
    return (uint16_t)(get16(b[1] + 2) - get16(b[0] + 2)) < 0x8000u ? 1 : 0;
}
static bool read_slots(const gas_store_io_t *io, uint8_t b[2][16], bool valid[2])
{
    gas_config_t c;
    unsigned i;
    for (i = 0; i < 2; ++i) {
        if (!io->read(io->context, (uint16_t)(i * 16u), b[i], 16)) return false;
        valid[i] = decode(b[i], &c);
    }
    return true;
}
bool gas_store_load(const gas_store_io_t *io, gas_config_t *c)
{
    uint8_t b[2][16]; bool valid[2]; int slot;
    if (!read_slots(io, b, valid)) return false;
    slot = latest(b, valid);
    return slot >= 0 && decode(b[slot], c);
}
bool gas_store_save(const gas_store_io_t *io, const gas_config_t *c)
{
    uint8_t b[2][16], record[16] = {0}, verify[16], marker = 0;
    bool valid[2]; int slot; unsigned i; uint16_t address, sequence;
    if (!gas_config_valid(c) || !read_slots(io, b, valid)) return false;
    slot = latest(b, valid);
    sequence = slot < 0 ? 0 : (uint16_t)(get16(b[slot] + 2) + 1u);
    address = slot == 0 ? 16 : 0;
    record[0] = 0xa5; record[1] = 1; put16(record + 2, sequence);
    for (i = 0; i < 3; ++i) put16(record + 4 + i * 2, c->alarm[i]);
    put16(record + 12, crc16(record, 12)); record[15] = 0x5a;
    if (!io->write(io->context, (uint16_t)(address + 15), &marker, 1) ||
        !io->write(io->context, address, record, 15) ||
        !io->write(io->context, (uint16_t)(address + 15), record + 15, 1) ||
        !io->read(io->context, address, verify, 16)) return false;
    return memcmp(record, verify, 16) == 0;
}
