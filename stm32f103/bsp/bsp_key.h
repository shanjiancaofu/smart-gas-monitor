#ifndef BSP_KEY_H
#define BSP_KEY_H
#include <stdbool.h>
#include <stdint.h>
#define KEY_COUNT 5u
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
    uint32_t changed_ms[KEY_COUNT];
    uint8_t raw;
    uint8_t stable;
} bsp_key_t;
void bsp_key_init(bsp_key_t *keys);
uint8_t bsp_key_poll(bsp_key_t *keys, uint32_t now);
bool bsp_key_is_down(const bsp_key_t *keys, unsigned key);
#endif
