#include "display/display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char rows[8][32];
static bool ready = true;

bool bsp_oled_is_ready(const bsp_oled_t *oled) { (void)oled; return ready; }
void bsp_oled_clear(bsp_oled_t *oled) { (void)oled; memset(rows, 0, sizeof(rows)); }
static unsigned last_flush_pages;   /* 上一次 flush 允许推几个脏页 */
void bsp_oled_flush(bsp_oled_t *oled, unsigned max_pages)
{
    (void)oled;
    last_flush_pages = max_pages;
}
bool bsp_oled_text(bsp_oled_t *oled, unsigned x, unsigned page, const char *text, bool large)
{
    (void)oled; (void)x; (void)large;
    if (page >= 8) return false;
    snprintf(rows[page], sizeof(rows[page]), "%s", text);
    return true;
}

/* 历史页的记录来自 EEPROM，所以要一块能读的假存储器：不管地址一律回同一条合法
 * 槽记录，并数一数被读了几次。「停在同一条记录上不重复回读」正是要钉住的行为。 */
static uint8_t slot_image[16];
static unsigned slot_reads;

static uint16_t test_crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0xffffu;
    unsigned i;
    while (n--) {
        crc ^= (uint16_t)*p++ << 8;
        for (i = 0; i < 8u; ++i) {
            crc = (uint16_t)((crc & 0x8000u) ? (crc << 1) ^ 0x1021u : crc << 1);
        }
    }
    return crc;
}

/* 小端写入。走一层函数是为了让截断发生在形参上——直接写 (uint8_t)2989u 会被
 * MSVC 的 /W4 判成 C4310，而本仓库的测试是开着 /WX 编译的。 */
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static bool read_slot_fake(void *ctx, uint16_t addr, uint8_t *data, size_t size)
{
    (void)ctx;
    (void)addr;
    ++slot_reads;
    memcpy(data, slot_image, size);
    return true;
}

static config_io_t fake_io = {NULL, read_slot_fake, NULL};

static void make_normal(gas_t *gas)
{
    config_defaults(&gas->config);
    gas_init(gas, &gas->config, 0);
    gas->state = GAS_NORMAL;
    gas->sample_valid = true;
    gas->sample_attempted = true;
}

int main(void)
{
    display_t display;
    bsp_oled_t oled;
    gas_t gas;
    history_t history = {0};
    unsigned old_item;

    memset(&oled, 0, sizeof(oled));
    make_normal(&gas);
    display_init(&display, &oled);
    assert(display.page == DISPLAY_MAIN);

    old_item = gas.item;
    display_key(&display, &gas, &history, GAS_KEY_SELECT, 100);
    assert(gas.item == old_item);
    display_key(&display, &gas, &history, GAS_KEY_PAGE, 101);
    assert(display.page == DISPLAY_SETTINGS);
    display_key(&display, &gas, &history, GAS_KEY_SELECT, 102);
    assert(gas.item == GAS_ITEM_MQ6);
    display_key(&display, &gas, &history, GAS_KEY_UP, 103);
    assert(gas.config.alarm[GAS_MQ6] == 1850);
    display_key(&display, &gas, &history, GAS_KEY_DOWN, 104);
    assert(gas.config.alarm[GAS_MQ6] == 1800);

    /* 参数页要把全部 5 个可调项都画出来，光标跟着 item 走。此前它只循环
     * GAS_COUNT 画三路通道，KEY5 切到采样周期或蜂鸣器档位时光标会整个消失，
     * 那两项只能盲调。 */
    {
        static const char *const names[GAS_ITEM_COUNT] = {"MQ4", "MQ6", "MQ7", "PERIOD",
                                                          "BUZZER"};
        unsigned item;
        for (item = 0; item < GAS_ITEM_COUNT; ++item) {
            gas.item = (uint8_t)item;
            /* 重绘被 DISPLAY_REFRESH_MS 限流，两次调用至少要隔这么久。 */
            display_update(&display, &gas, &history, true,
                           200u + item * DISPLAY_REFRESH_MS);
            assert(strstr(rows[item + 1u], names[item]) != NULL);
            assert(rows[item + 1u][0] == '>');
        }
        assert(strstr(rows[0], "SETTINGS") != NULL);
        /* 存储状态摆在设置页最后一行：历史页只会说「没有记录」，分不清是真的没
         * 报过警还是存储掉了。 */
        assert(strstr(rows[GAS_ITEM_BUZZER + 2u], "MEM ") != NULL);
    }

    /* 造一条合法的槽记录。字段值取自实际验收时看到的第 8 条：MQ4 819、
     * MQ6 2989、MQ7 737、A2。 */
    {
        uint16_t crc;
        put16(slot_image, 1u);        /* seq = 1 */
        slot_image[2] = 42u;          /* uptime_s = 42 */
        put16(slot_image + 6, 819u);  /* MQ4 */
        put16(slot_image + 8, 2989u); /* MQ6 */
        put16(slot_image + 10, 737u); /* MQ7 */
        slot_image[12] = 2u;          /* alarm_mask = MQ6 */
        crc = test_crc16(slot_image, 14u);
        put16(slot_image + 14, crc);
    }
    history.io = &fake_io;
    history.count = 2u;      /* 下标 0 和 1，用来验「到头了不再走」 */
    history.next_slot = 1u;
    history.next_seq = 9u;

    display_key(&display, &gas, &history, GAS_KEY_PAGE, 105);
    assert(display.page == DISPLAY_HISTORY);

    /* 换页那一帧要整屏推。清屏把八个 page 全标了脏，只推一个的话要八帧、约
     * 0.7 秒才铺完，这期间新旧两页的内容并存，屏幕上就是花的。 */
    slot_reads = 0u;
    display_update(&display, &gas, &history, true, 60000);
    assert(last_flush_pages == SSD1306_PAGES);
    assert(slot_reads == 1u);
    assert(strstr(rows[2], "SEQ 1 UP42s") != NULL);
    assert(strstr(rows[4], "MQ4 819 MQ6 2989") != NULL);
    assert(strstr(rows[6], "MQ7 737 A2") != NULL);

    /* 停在同一条记录上：既不整屏推，也不再回读 EEPROM——记录不会变，每 200 ms
     * 读它一次只是白花总线时间，而软件 I2C 下这一次读并不便宜。 */
    slot_reads = 0u;
    display_update(&display, &gas, &history, true, 60000 + DISPLAY_REFRESH_MS);
    assert(last_flush_pages == 1u);
    assert(slot_reads == 0u);

    /* 翻到另一条：整屏推（半条旧记录配半条新记录还不如不画），并回读一次。 */
    display_key(&display, &gas, &history, GAS_KEY_DOWN, 60000 + 2u * DISPLAY_REFRESH_MS);
    assert(display.history_index == 1u);
    slot_reads = 0u;
    display_update(&display, &gas, &history, true, 60000 + 3u * DISPLAY_REFRESH_MS);
    assert(last_flush_pages == SSD1306_PAGES);
    assert(slot_reads == 1u);

    /* 已经在最后一条上，再按同方向：下标不动，什么都不用推。 */
    display_key(&display, &gas, &history, GAS_KEY_DOWN, 60000 + 4u * DISPLAY_REFRESH_MS);
    assert(display.history_index == 1u);
    display_update(&display, &gas, &history, true, 60000 + 5u * DISPLAY_REFRESH_MS);
    assert(last_flush_pages == 1u);

    display_key(&display, &gas, &history, GAS_KEY_UP, 60000 + 6u * DISPLAY_REFRESH_MS);
    assert(display.history_index == 0u);
    display_key(&display, &gas, &history, GAS_KEY_PAGE, 60000 + 7u * DISPLAY_REFRESH_MS);
    assert(display.page == DISPLAY_MAIN);

    gas.state = GAS_SAFE_WAIT;
    gas.latched = true;
    gas.config.lockout = true;
    gas.sample_valid = true;
    gas.sample_attempted = true;
    gas.sample_ms = 64000;
    gas.safe_timing = true;
    gas.safe_since_ms = 60000;
    gas.adc[GAS_MQ4] = 100;
    gas.adc[GAS_MQ6] = 100;
    gas.adc[GAS_MQ7] = 100;
    display_key(&display, &gas, &history, GAS_KEY_CONFIRM, 64000);
    assert(!gas.latched && !gas.config.lockout);

    gas.state = GAS_ALARM;
    gas.latched = true;
    gas.alarm_mask = 2u;
    gas.adc[GAS_MQ6] = 3000;
    display_update(&display, &gas, &history, true, 200);
    assert(strstr(rows[0], "ALARM") != NULL);
    /* 报警页三行的内容和顺序：最后一行是第三路通道。这里原先断言的是
     * "KEY4"，那是报警页上还没有通道列表时的写法；KEY4 现在走 SAFE_WAIT
     * 那条路径提示。alarm_mask 只置了 MQ6，所以只有它带 '>' 标记。 */
    assert(strstr(rows[4], ">MQ6") != NULL);
    assert(strstr(rows[6], "MQ7") != NULL);

    puts("PASS: display page/item keys/history/alarm overlay/confirm");
    return 0;
}
