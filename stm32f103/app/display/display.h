#ifndef DISPLAY_H
#define DISPLAY_H
#include "gas/gas.h"
#include "history/history.h"
#include "bsp_oled.h"
#include <stdbool.h>
#include <stdint.h>

/* 重绘按固定间隔限流，而不是由变化触发：驱动只推送字节确实不同的页，
 * 所以没有产生变化的重绘不占总线流量。这个上限约束的是格式化开销，
 * 而不是 I2C。 */
#define DISPLAY_REFRESH_MS 200u
typedef enum {
    DISPLAY_MAIN,
    DISPLAY_SETTINGS,
    DISPLAY_HISTORY,
    DISPLAY_PAGE_COUNT,
    DISPLAY_ALARM
} display_page_t;

typedef struct {
    /* 指向组合层持有的那块面板，本模块只画，不负责建。 */
    bsp_oled_t *oled;
    /* 历史界面显示哪一条记录。0 表示最新的一条。 */
    uint16_t history_index;
    /* 上一次画的是哪一页（gas_page_t，外加初值 -1 表示还没画过）。报警页可能
     * 顶掉使用者选的那一页，所以这里存的是实际画出来的那一页，不是 gas 状态数据。 */
    uint8_t screen, page;
    uint32_t last_draw_ms;
} display_t;

/* 只记下面板对象，不初始化它：建面板是 BSP 的活，组合层做。
 * 面板未就绪时 display_update() 自己会跳过，不需要调用方先判断。 */
void display_init(display_t *d, bsp_oled_t *oled);
/* 画 gas 的页面选择器指到的那一页，报警时改画报警页。 */
void display_update(display_t *d, const gas_t *m, const history_t *h, bool storage_ok,
                    uint32_t now);
/* 历史界面没有可调项，那里的 KEY3/KEY4 用来翻阅记录。返回 true 表示按键
 * 已被消费；KEY1、KEY2 和 KEY5 从不会被消费。 */
bool display_history_key(display_t *d, const history_t *h, unsigned key);
void display_key(display_t *d, gas_t *gas, const history_t *history, unsigned key, uint32_t now);
#endif
