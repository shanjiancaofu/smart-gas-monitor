#include "gas/gas_monitor.h"
#include "history/history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t data[256]; int budget; } fake_t;

static bool read_mem(void *ctx, uint16_t addr, uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    assert((size_t)addr + size <= 256);
    memcpy(data, f->data + addr, size);
    return true;
}
static bool write_mem(void *ctx, uint16_t addr, const uint8_t *data, size_t size)
{
    fake_t *f = ctx;
    size_t i;
    assert((size_t)addr + size <= 256);
    for (i = 0; i < size; ++i) {
        if (f->budget == 0) return false;
        if (f->budget > 0) --f->budget;
        f->data[addr + i] = data[i];
    }
    return true;
}

static history_entry_t make(uint16_t mq4, uint32_t uptime_s, uint8_t mask)
{
    history_entry_t e;
    memset(&e, 0, sizeof(e));
    e.adc[GAS_MQ4] = mq4;
    e.adc[GAS_MQ7] = (uint16_t)(mq4 + 100u);
    e.adc[GAS_MQ8] = (uint16_t)(mq4 + 200u);
    e.uptime_s = uptime_s;
    e.alarm_mask = mask;
    return e;
}

/* The log must not reach outside the half of the EEPROM it was given. */
static void test_region(void)
{
    assert(HISTORY_BASE + HISTORY_SLOTS * HISTORY_SLOT_SIZE == 256u);
    assert(HISTORY_BASE >= 0x20u);
}

static void test_empty(void)
{
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    history_entry_t e;
    memset(&f, 0xff, sizeof(f));
    f.budget = -1;
    /* A blank EEPROM is an empty log, not an error worth failing over. */
    assert(!history_init(&h, &io));
    assert(history_count(&h) == 0u);
    assert(!history_get(&h, 0u, &e));
    /* No backend at all is the same answer. */
    assert(!history_init(&h, NULL));
    assert(history_count(&h) == 0u);
}

static void test_order_and_reboot(void)
{
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    history_entry_t e;
    unsigned i;
    memset(&f, 0xff, sizeof(f));
    f.budget = -1;
    assert(!history_init(&h, &io));
    for (i = 0; i < 3u; ++i) {
        e = make((uint16_t)(2600u + i), 100u * i, (uint8_t)(1u << i));
        assert(history_append(&h, &e));
        assert(e.seq == i + 1u);
    }
    /* Index 0 is the newest, so browsing starts at the most recent alarm. */
    assert(history_count(&h) == 3u);
    assert(history_get(&h, 0u, &e) && e.seq == 3u && e.adc[GAS_MQ4] == 2602u);
    assert(history_get(&h, 1u, &e) && e.seq == 2u);
    assert(history_get(&h, 2u, &e) && e.seq == 1u && e.uptime_s == 0u);
    assert(!history_get(&h, 3u, &e));
    /* The sequence number survives a power cycle and keeps counting, so the
     * count of alarms raised over the life of the unit is not reset. */
    assert(history_init(&h, &io));
    assert(history_count(&h) == 3u);
    e = make(2700u, 500u, 1u);
    assert(history_append(&h, &e));
    assert(e.seq == 4u);
    assert(history_get(&h, 0u, &e) && e.seq == 4u);
    assert(history_get(&h, 3u, &e) && e.seq == 1u);
}

static void test_wraparound(void)
{
    fake_t f;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    history_entry_t e;
    unsigned i;
    memset(&f, 0xff, sizeof(f));
    f.budget = -1;
    assert(!history_init(&h, &io));
    /* Two full laps plus one, so the oldest record is seq 15 - 14 + 1. */
    for (i = 0; i < HISTORY_SLOTS * 2u + 1u; ++i) {
        e = make((uint16_t)i, i, 1u);
        assert(history_append(&h, &e));
    }
    assert(history_count(&h) == HISTORY_SLOTS);
    assert(history_get(&h, 0u, &e) && e.seq == 29u && e.adc[GAS_MQ4] == 28u);
    assert(history_get(&h, HISTORY_SLOTS - 1u, &e) && e.seq == 16u);
    assert(!history_get(&h, HISTORY_SLOTS, &e));
    /* Reloading must agree with the running state, including which slot is
     * next, or the first record after a reboot lands on top of a live one. */
    assert(history_init(&h, &io));
    assert(history_count(&h) == HISTORY_SLOTS);
    assert(history_get(&h, 0u, &e) && e.seq == 29u);
    e = make(999u, 9u, 1u);
    assert(history_append(&h, &e));
    assert(e.seq == 30u);
    assert(history_get(&h, 0u, &e) && e.seq == 30u && e.adc[GAS_MQ4] == 999u);
    assert(history_get(&h, HISTORY_SLOTS - 1u, &e) && e.seq == 17u);
}

static void test_torn_and_corrupt(void)
{
    fake_t f, baseline;
    settings_io_t io = {&f, read_mem, write_mem};
    history_t h;
    history_entry_t e;
    memset(&f, 0xff, sizeof(f));
    f.budget = -1;
    assert(!history_init(&h, &io));
    e = make(2600u, 60u, 1u);
    assert(history_append(&h, &e));
    e = make(2700u, 120u, 2u);
    assert(history_append(&h, &e));
    baseline = f;
    /* Cutting power part way through a record leaves the previous contents
     * readable and, crucially, does not consume a sequence number. */
    f.budget = 5;
    e = make(2800u, 180u, 3u);
    assert(!history_append(&h, &e));
    f.budget = -1;
    assert(history_count(&h) == 2u);
    e = make(2800u, 180u, 3u);
    assert(history_append(&h, &e));
    assert(e.seq == 3u);
    assert(history_get(&h, 0u, &e) && e.seq == 3u);
    /* A record damaged in storage is dropped on the next boot rather than
     * reported with whatever values survived. Slot 0 holds seq 1, so seq 2 is
     * the one left standing. */
    f = baseline;
    f.data[HISTORY_BASE + 7u] ^= 0x01;
    assert(history_init(&h, &io));
    assert(history_count(&h) == 1u);
    assert(history_get(&h, 0u, &e) && e.seq == 2u);
    /* A record whose sequence field alone was erased reads as absent too. */
    f = baseline;
    f.data[HISTORY_BASE] = 0xff;
    f.data[HISTORY_BASE + 1u] = 0xff;
    assert(history_init(&h, &io));
    assert(history_count(&h) == 1u);
    assert(history_get(&h, 0u, &e) && e.seq == 2u);
}

int main(void)
{
    test_region();
    test_empty();
    test_order_and_reboot();
    test_wraparound();
    test_torn_and_corrupt();
    printf("PASS: history ring/order/reboot/wrap/torn-write/corruption\n");
    return 0;
}
