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
    DISPLAY_ALARM,
    DISPLAY_FAULT
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
    /* 已经画在屏上的那条历史记录，连同它的解码结果。记录存在 EEPROM 里，整条
     * 16 字节读一次在软件 I2C 下不便宜，而站着这一页不动的时候它根本不会变；
     * 但换了一条就得整屏推——半条记录还不如不画。next_seq 是缓存的有效期：
     * 期间写进过新记录就作废，否则环满时条数不变、下标不变，会一直显示旧内容。 */
    uint16_t drawn_history_index;
    uint16_t drawn_history_seq;
    history_entry_t drawn_history;
    bool history_cached;
    /* 下一次重画要整屏推。由使用者的动作置位（翻页、翻记录），display_update()
     * 用完就清。比在这里反过来猜「这次变了几页」可靠：实时页八页都在变，按变化
     * 量判会让它每帧都整屏推，主循环就再也追不上采样。 */
    bool force_full;
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
