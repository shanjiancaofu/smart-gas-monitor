#include "bsp_key.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <string.h>

#if !defined(__ARMCC_VERSION)
_Static_assert(KEY1_Pin != KEY2_Pin && KEY1_Pin != KEY3_Pin && KEY1_Pin != KEY4_Pin &&
                   KEY1_Pin != KEY5_Pin && KEY2_Pin != KEY3_Pin && KEY2_Pin != KEY4_Pin &&
                   KEY2_Pin != KEY5_Pin && KEY3_Pin != KEY4_Pin && KEY3_Pin != KEY5_Pin &&
                   KEY4_Pin != KEY5_Pin,
               "every key needs its own pin: EXTI lines are shared by pin number");
#endif

/* KEY1～KEY4 走 EXTI 下降沿，KEY5 是上拉轮询。位图的位序就是 KEY1..KEY5，
 * 所以前四位是「有 EXTI 的键」，最后一位不是。 */
#define KEY_EXTI_MASK 0x0fu

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} bsp_key_slot_t;

static const bsp_key_slot_t bsp_key_slots[KEY_COUNT] = {
    {KEY1_GPIO_Port, KEY1_Pin}, {KEY2_GPIO_Port, KEY2_Pin}, {KEY3_GPIO_Port, KEY3_Pin},
    {KEY4_GPIO_Port, KEY4_Pin}, {KEY5_GPIO_Port, KEY5_Pin},
};

static volatile uint8_t exti_pending;

static uint8_t bsp_key_bits(void)
{
    uint8_t bits = 0u;
    unsigned i;

    for (i = 0; i < KEY_COUNT; ++i) {
        if (HAL_GPIO_ReadPin(bsp_key_slots[i].port, bsp_key_slots[i].pin) == GPIO_PIN_RESET) {
            bits |= (uint8_t)(1u << i);
        }
    }
    return bits;
}

void bsp_key_init(bsp_key_t *keys)
{
    memset(keys, 0, sizeof(*keys));
    keys->held = bsp_key_bits();
    exti_pending = 0;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    unsigned i;

    for (i = 0; i < KEY_COUNT; ++i) {
        if (GPIO_Pin == bsp_key_slots[i].pin) {
            exti_pending |= (uint8_t)(1u << i);
        }
    }
}

static uint8_t bsp_key_take_edges(void)
{
    uint32_t primask = __get_PRIMASK();
    uint8_t edges;

    __disable_irq();
    edges = exti_pending;
    exti_pending = 0;
    __set_PRIMASK(primask);
    return edges;
}

uint8_t bsp_key_poll(bsp_key_t *keys, uint32_t now)
{
    uint8_t raw = bsp_key_bits();
    uint8_t edges = bsp_key_take_edges();
    uint8_t events = 0u;
    unsigned i;

    for (i = 0; i < KEY_COUNT; ++i) {
        uint8_t mask = (uint8_t)(1u << i);
        bool pressed = (raw & mask) != 0u;
        bool start;

        if ((KEY_EXTI_MASK & mask) != 0u) {
            /* 有 EXTI 的键：下降沿就是权威触发源。它记在中断里，主循环什么时候
             * 回来处理都算数，不要求那一刻按键还按着。 */
            start = (edges & mask) != 0u;
        } else {
            /* KEY5 没有 EXTI，只能靠轮询发现按下的那一瞬。 */
            start = pressed && (keys->held & mask) == 0u;
        }
        keys->held = (uint8_t)((keys->held & (uint8_t)~mask) | (pressed ? mask : 0u));

        if (start) {
            keys->edge_ms[i] = now;
            keys->pending |= mask;
        }
        /* 消抖窗口走完才发事件；窗口里又来了新的下降沿就顺延，抖动不会连发。 */
        if ((keys->pending & mask) != 0u &&
            (uint32_t)(now - keys->edge_ms[i]) >= KEY_DEBOUNCE_MS) {
            keys->pending &= (uint8_t)~mask;
            events |= mask;
        }
    }
    return events;
}
