#include "bsp_key.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdbool.h>

_Static_assert(KEY1_Pin != KEY2_Pin && KEY1_Pin != KEY3_Pin && KEY1_Pin != KEY4_Pin &&
                   KEY1_Pin != KEY5_Pin && KEY2_Pin != KEY3_Pin && KEY2_Pin != KEY4_Pin &&
                   KEY2_Pin != KEY5_Pin && KEY3_Pin != KEY4_Pin && KEY3_Pin != KEY5_Pin &&
                   KEY4_Pin != KEY5_Pin,
               "every key needs its own pin: EXTI lines are shared by pin number");

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

    keys->raw = keys->stable = bsp_key_bits();
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
    uint8_t raw = bsp_key_bits(), edges = bsp_key_take_edges(), events = 0;
    unsigned i;
    for (i = 0; i < KEY_COUNT; ++i) {
        uint8_t mask = (uint8_t)(1u << i);

        if ((raw & mask) != (keys->raw & mask) || (edges & mask)) {
            keys->raw = (uint8_t)((keys->raw & (uint8_t)~mask) | (raw & mask));
            keys->changed_ms[i] = now;
        }
        if ((raw & mask) != (keys->stable & mask) &&
            (uint32_t)(now - keys->changed_ms[i]) >= KEY_DEBOUNCE_MS) {
            keys->stable ^= mask;
            if (raw & mask) {
                events |= mask;
            }
        }
    }
    return events;
}

bool bsp_key_is_down(const bsp_key_t *keys, unsigned key)
{
    return key < KEY_COUNT && (keys->stable & (1u << key)) != 0u;
}
