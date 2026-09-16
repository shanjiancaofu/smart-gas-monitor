#ifndef BSP_OLED_H
#define BSP_OLED_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_WIDTH 128u
#define SSD1306_HEIGHT 64u
#define SSD1306_PAGES (SSD1306_HEIGHT / 8u)

#define SSD1306_I2C_ADDR 0x78u

#define SSD1306_SMALL_W 6u
#define SSD1306_SMALL_H 8u
#define SSD1306_LARGE_W 8u
#define SSD1306_LARGE_H 16u

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];

    uint8_t dirty;

    bool ready;
} bsp_oled_t;

bool bsp_oled_init(bsp_oled_t *oled, I2C_HandleTypeDef *i2c);

bool bsp_oled_is_ready(const bsp_oled_t *oled);

void bsp_oled_clear(bsp_oled_t *oled);

/* 最多推送 max_pages 个脏页。整屏 1024 字节走软件 I2C 要 700 ms 以上，会把主
 * 循环整个挡住；采样器错过 GAS_SAMPLE_TIMEOUT_PERIODS 的超时线后，gas_update()
 * 会当成采样故障，把「环境持续安全 3 秒」的窗口清零——KEY4 就再也解不开锁。
 * 限量之后单次阻塞降到一页约 93 ms，剩下的脏页留给下一次刷新。 */
void bsp_oled_flush(bsp_oled_t *oled, unsigned max_pages);

bool bsp_oled_text(bsp_oled_t *oled, unsigned x, unsigned page, const char *text, bool large);
bool bsp_oled_textf(bsp_oled_t *oled, unsigned x, unsigned page, bool large, const char *format,
                    ...);
#endif
