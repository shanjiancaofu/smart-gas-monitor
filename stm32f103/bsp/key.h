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
/* Poll from the main loop; returns one bit per debounced press. The EXTI handler
 * only latches which line moved, in HAL_GPIO_EXTI_Callback() over in key.c; the
 * debounce and the dispatch stay here. */
uint8_t key_poll(key_t *keys, uint32_t now);
#endif
