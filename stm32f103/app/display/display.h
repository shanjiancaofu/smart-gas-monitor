#ifndef DISPLAY_H
#define DISPLAY_H
#include "gas/gas_monitor.h"
#include "history/history.h"
#include "ssd1306.h"
#include <stdbool.h>
#include <stdint.h>

/* Repainting is rate limited rather than change driven: the driver only pushes
 * the pages whose bytes actually differ, so a repaint that changes nothing
 * costs no bus traffic. This bound is about formatting work, not I2C. */
#define DISPLAY_REFRESH_MS 200u

typedef struct {
    ssd1306_t oled;
    /* Which record the history page shows. 0 is the newest. */
    uint8_t history_index;
    uint8_t screen;
    bool ready;
    uint32_t last_draw_ms;
} display_t;

void display_init(display_t *d, I2C_HandleTypeDef *i2c);
/* Draws whichever of the three pages the selector is on. */
void display_update(display_t *d, const gas_monitor_t *m, const history_t *h,
                    bool storage_ok, uint32_t now);
/* The history page has nothing adjustable, so its KEY2/KEY3 browse the log.
 * Returns true when the key was consumed; keys 1 and 4 are never consumed. */
bool display_history_key(display_t *d, const history_t *h, unsigned key);
#endif
