#include "display/display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char rows[8][32];
static bool ready = true;

bool bsp_oled_is_ready(const bsp_oled_t *oled) { (void)oled; return ready; }
void bsp_oled_clear(bsp_oled_t *oled) { (void)oled; memset(rows, 0, sizeof(rows)); }
void bsp_oled_flush(bsp_oled_t *oled, unsigned max_pages) { (void)oled; (void)max_pages; }
bool bsp_oled_text(bsp_oled_t *oled, unsigned x, unsigned page, const char *text, bool large)
{
    (void)oled; (void)x; (void)large;
    if (page >= 8) return false;
    snprintf(rows[page], sizeof(rows[page]), "%s", text);
    return true;
}

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
    assert(gas.config.alarm[GAS_MQ6] == 2450);
    display_key(&display, &gas, &history, GAS_KEY_DOWN, 104);
    assert(gas.config.alarm[GAS_MQ6] == 2400);

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
    }

    display_key(&display, &gas, &history, GAS_KEY_PAGE, 105);
    assert(display.page == DISPLAY_HISTORY);
    history.count = 3;
    display_key(&display, &gas, &history, GAS_KEY_DOWN, 106);
    assert(display.history_index == 1);
    display_key(&display, &gas, &history, GAS_KEY_UP, 107);
    assert(display.history_index == 0);
    display_key(&display, &gas, &history, GAS_KEY_PAGE, 108);
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
