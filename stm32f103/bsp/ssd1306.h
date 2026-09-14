#ifndef SSD1306_H
#define SSD1306_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_WIDTH  128u
#define SSD1306_HEIGHT 64u
#define SSD1306_PAGES  (SSD1306_HEIGHT / 8u)

/* 7-bit address 0x3C shifted left, which is what the HAL expects. */
#define SSD1306_I2C_ADDR 0x78u

/* Two fixed fonts. The small one is 6x8 and fits one page; the large one is
 * 8x16 and spans two. There is no scaling or proportional layout. */
#define SSD1306_SMALL_W 6u
#define SSD1306_SMALL_H 8u
#define SSD1306_LARGE_W 8u
#define SSD1306_LARGE_H 16u

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
    /* One bit per page, set by every draw and cleared by flush, so a redraw of
     * the clock or one changing number does not push all 1 KB over the bus. */
    uint8_t dirty;
} ssd1306_t;

/* Runs the power-on sequence and blanks the panel. */
bool ssd1306_init(ssd1306_t *oled, I2C_HandleTypeDef *i2c);
/* Clears the backing buffer; the panel follows on the next flush. */
void ssd1306_clear(ssd1306_t *oled);
/* Sends the pages that changed since the last call. */
void ssd1306_flush(ssd1306_t *oled);

/* Text is placed by pixel column and page row. Both return false when the text
 * would run off the panel, and draw nothing in that case. */
bool ssd1306_text(ssd1306_t *oled, unsigned x, unsigned page, const char *text,
                  bool large);
bool ssd1306_textf(ssd1306_t *oled, unsigned x, unsigned page, bool large,
                   const char *format, ...);
#endif
