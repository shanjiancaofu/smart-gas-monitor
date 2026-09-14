#ifndef KEY_H
#define KEY_H
#include <stdbool.h>
#include <stdint.h>

#define KEY_COUNT 4u
#define KEY_DEBOUNCE_MS 30u

typedef struct {
    uint32_t changed_ms[KEY_COUNT];
    uint8_t raw, stable;
} key_t;

/* PB12～PB15，CubeMX 配置为上拉输入，低电平有效。 */
void key_init(key_t *keys);
/* 在主循环里轮询；每次消抖后的按下返回一位。EXTI 处理函数只锁存哪一路动了，
 * 就在 key.c 的 HAL_GPIO_EXTI_Callback() 里；消抖和分发留在这里。 */
uint8_t key_poll(key_t *keys, uint32_t now);
#endif
