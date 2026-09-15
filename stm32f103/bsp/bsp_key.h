#ifndef BSP_KEY_H
#define BSP_KEY_H
#include <stdbool.h>
#include <stdint.h>

#define KEY_COUNT 4u
#define KEY_DEBOUNCE_MS 30u

typedef struct {
    uint32_t changed_ms[KEY_COUNT];
    uint8_t raw, stable;
} bsp_key_t;

/* PB12～PB15，CubeMX 配置为上拉输入，低电平有效。 */
void bsp_key_init(bsp_key_t *keys);
/* 在主循环里轮询；每次消抖后的按下返回一位。EXTI 处理函数只锁存哪一路动了，
 * 就在 bsp_key.c 的 HAL_GPIO_EXTI_Callback() 里；消抖和分发留在这里。 */
uint8_t bsp_key_poll(bsp_key_t *keys, uint32_t now);
#endif
