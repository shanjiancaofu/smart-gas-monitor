#include "key.h"
#include "stm32f1xx_hal.h"
#include <string.h>

static uint8_t key_bits(void)
{
    return (uint8_t)((~GPIOB->IDR >> 12) & 15u);
}

void key_init(key_t *keys)
{
    memset(keys, 0, sizeof(*keys));
    /* A key held at startup must be released before it can register. */
    keys->raw = keys->stable = key_bits();
}

uint8_t key_poll(key_t *keys, uint32_t now)
{
    uint8_t raw = key_bits(), events = 0;
    unsigned i;
    for (i = 0; i < KEY_COUNT; ++i) {
        uint8_t mask = (uint8_t)(1u << i);
        if ((raw & mask) != (keys->raw & mask)) {
            keys->raw ^= mask;
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
