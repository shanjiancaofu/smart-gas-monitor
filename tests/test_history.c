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

/* 记录区不得越出分配给它的那半个 EEPROM。 */
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
    /* 空白 EEPROM 就是一条空记录，不是值得报错的异常。 */
    assert(!history_init(&h, &io));
    assert(history_count(&h) == 0u);
    assert(!history_get(&h, 0u, &e));
    /* 完全没有后端，答案也一样。 */
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
    /* 下标 0 是最新的一条，所以翻阅是从最近一次报警开始的。 */
    assert(history_count(&h) == 3u);
    assert(history_get(&h, 0u, &e) && e.seq == 3u && e.adc[GAS_MQ4] == 2602u);
    assert(history_get(&h, 1u, &e) && e.seq == 2u);
    assert(history_get(&h, 2u, &e) && e.seq == 1u && e.uptime_s == 0u);
    assert(!history_get(&h, 3u, &e));
    /* 序号跨掉电保留并继续递增，所以这台设备一生中报过多少次警不会被清零。 */
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
    /* 整整两圈再多一条，所以现存最旧的一条是 seq 15 - 14 + 1。 */
    for (i = 0; i < HISTORY_SLOTS * 2u + 1u; ++i) {
        e = make((uint16_t)i, i, 1u);
        assert(history_append(&h, &e));
    }
    assert(history_count(&h) == HISTORY_SLOTS);
    assert(history_get(&h, 0u, &e) && e.seq == 29u && e.adc[GAS_MQ4] == 28u);
    assert(history_get(&h, HISTORY_SLOTS - 1u, &e) && e.seq == 16u);
    assert(!history_get(&h, HISTORY_SLOTS, &e));
    /* 重新装载的结果必须和运行中的状态一致，包括下一个该用哪个槽，否则重启
     * 后的第一条记录会盖掉一条还有效的记录。 */
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
    /* 写一条记录写到一半掉电，原内容仍可读；更要紧的是不会白吃一个序号。 */
    f.budget = 5;
    e = make(2800u, 180u, 3u);
    assert(!history_append(&h, &e));
    f.budget = -1;
    assert(history_count(&h) == 2u);
    e = make(2800u, 180u, 3u);
    assert(history_append(&h, &e));
    assert(e.seq == 3u);
    assert(history_get(&h, 0u, &e) && e.seq == 3u);
    /* 存储中损坏的记录会在下次开机时被丢弃，而不是把残存下来的字段照报出去。
     * 槽 0 放的是 seq 1，所以剩下的是 seq 2。 */
    f = baseline;
    f.data[HISTORY_BASE + 7u] ^= 0x01;
    assert(history_init(&h, &io));
    assert(history_count(&h) == 1u);
    assert(history_get(&h, 0u, &e) && e.seq == 2u);
    /* 只有序号字段被擦掉的记录，同样读作不存在。 */
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
