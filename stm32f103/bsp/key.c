#include "key.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* PB12..PB15 as key bits 0..3, the same order key_bits() produces. */
static const uint16_t key_pins[KEY_COUNT] = {
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15
};

/* Lines the EXTI handler has seen a falling edge on but the main loop has not
 * consumed yet. */
static volatile uint8_t exti_pending;

static uint8_t key_bits(void)
{
    return (uint8_t)((~GPIOB->IDR >> 12) & 15u);
}

void key_init(key_t *keys)
{
    memset(keys, 0, sizeof(*keys));
    /* A key held at startup must be released before it can register. */
    keys->raw = keys->stable = key_bits();
    exti_pending = 0;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    unsigned i;
    /* Called from EXTI15_10_IRQHandler. It only remembers which line moved: the
     * debounce and the dispatch stay in the main loop, so nothing here touches
     * the ADC, the I2C bus or the EEPROM. */
    for (i = 0; i < KEY_COUNT; ++i)
        if (GPIO_Pin == key_pins[i]) exti_pending |= (uint8_t)(1u << i);
}

static uint8_t key_take_edges(void)
{
    uint32_t primask = __get_PRIMASK();
    uint8_t edges;
    /* Restore rather than enable: this must not silently unmask interrupts if a
     * caller ever polls from inside its own critical section. */
    __disable_irq();
    edges = exti_pending;
    exti_pending = 0;
    __set_PRIMASK(primask);
    return edges;
}

uint8_t key_poll(key_t *keys, uint32_t now)
{
    uint8_t raw = key_bits(), edges = key_take_edges(), events = 0;
    unsigned i;
    for (i = 0; i < KEY_COUNT; ++i) {
        uint8_t mask = (uint8_t)(1u << i);
        /* A latched edge restarts the window even when this poll already sees the
         * line settled again, so a press that came and went between two polls is
         * still timed from the edge rather than from the poll. */
        if ((raw & mask) != (keys->raw & mask) || (edges & mask)) {
            keys->raw = (uint8_t)((keys->raw & (uint8_t)~mask) | (raw & mask));
            keys->changed_ms[i] = now;
        }
        if ((raw & mask) != (keys->stable & mask) &&
            (uint32_t)(now - keys->changed_ms[i]) >= KEY_DEBOUNCE_MS) {
            keys->stable ^= mask;
            if (raw & mask) events |= mask;
        }
    }
    return events;
}
