#ifndef BSP_KEY_H
#define BSP_KEY_H
#include <stdbool.h>
#include <stdint.h>

#define KEY_COUNT 5u

/* 下降沿之后等这么久才认这次按下，用来滤掉抖动。
 *
 * 事件在窗口走完时发出，**不要求那一刻按键还按着**——主循环被 OLED 的软件 I2C
 * 刷新挡住几十毫秒时，一次短按可能开始又结束了，如果要求回来时还读到按下状态，
 * 这次按键就被丢掉了。 */
#define KEY_DEBOUNCE_MS 30u

typedef enum {
    KEY_PAGE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_CONFIRM,
    KEY_SELECT,
    KEY_EVENT_COUNT
} bsp_key_event_t;

typedef struct {
    uint32_t edge_ms[KEY_COUNT];
    uint8_t pending; /* 正在消抖窗口里的按键 */
    uint8_t held;    /* 最近一次读到的引脚电平，只给调试看 */
} bsp_key_t;

void bsp_key_init(bsp_key_t *keys);
/* 返回本次产生的按下事件位图，位序与 bsp_key_event_t 一致。 */
uint8_t bsp_key_poll(bsp_key_t *keys, uint32_t now);
#endif
