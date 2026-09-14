#ifndef DISPLAY_H
#define DISPLAY_H
#include "gas/gas_monitor.h"
#include "history/history.h"
#include "ssd1306.h"
#include <stdbool.h>
#include <stdint.h>

/* 重绘按固定间隔限流，而不是由变化触发：驱动只推送字节确实不同的页，
 * 所以没有产生变化的重绘不占总线流量。这个上限约束的是格式化开销，
 * 而不是 I2C。 */
#define DISPLAY_REFRESH_MS 200u

typedef struct {
    ssd1306_t oled;
    /* 历史界面显示哪一条记录。0 表示最新的一条。 */
    uint8_t history_index;
    uint8_t screen;
    bool ready;
    uint32_t last_draw_ms;
} display_t;

void display_init(display_t *d, I2C_HandleTypeDef *i2c);
/* 绘制选择器当前所在的界面，即三个界面之一。 */
void display_update(display_t *d, const gas_monitor_t *m, const history_t *h,
                    bool storage_ok, uint32_t now);
/* 历史界面没有可调项，那里的 KEY2/KEY3 用来翻阅记录。返回 true 表示按键
 * 已被消费；KEY1 和 KEY4 从不会被消费。 */
bool display_history_key(display_t *d, const history_t *h, unsigned key);
#endif
