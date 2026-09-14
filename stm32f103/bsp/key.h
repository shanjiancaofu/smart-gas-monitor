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

/* PB12..PB15 with the CubeMX pull-ups, active low. */
void key_init(key_t *keys);
/* Poll from the main loop; returns one bit per debounced press. When CubeMX
 * gains EXTI the interrupt only feeds this state, the debounce stays here. */
uint8_t key_poll(key_t *keys, uint32_t now);
#endif
