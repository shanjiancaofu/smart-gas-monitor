#ifndef BSP_OLED_H
#define BSP_OLED_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_WIDTH 128u
#define SSD1306_HEIGHT 64u
#define SSD1306_PAGES (SSD1306_HEIGHT / 8u)

/* 7 位地址 0x3C 左移一位，HAL 的 I2C 接口要的就是这种 8 位地址。 */
#define SSD1306_I2C_ADDR 0x78u

/* 两套固定字模。小号 6x8，正好占满一个 page；大号 8x16，跨两个 page。既不做
 * 缩放，也不按字符宽度比例排布。 */
#define SSD1306_SMALL_W 6u
#define SSD1306_SMALL_H 8u
#define SSD1306_LARGE_W 8u
#define SSD1306_LARGE_H 16u

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
    /* 每个 page 占一位，绘制时置位、flush 时清除。重画时钟、或只重画一个变化的
     * 数字时，不必把 1 KB 全都推上总线。 */
    uint8_t dirty;
} bsp_oled_t;

/* 执行上电初始化序列，并把面板清空。 */
bool bsp_oled_init(bsp_oled_t *oled, I2C_HandleTypeDef *i2c);
/* 清空后备缓冲；面板要等下一次 flush 才跟着变。 */
void bsp_oled_clear(bsp_oled_t *oled);
/* 只把自上次调用以来发生变化的 page 发出去。 */
void bsp_oled_flush(bsp_oled_t *oled);

/* 文本按像素列与 page 行定位。两个函数在文本会越出面板时返回 false，这时
 * 什么都不画。 */
bool bsp_oled_text(bsp_oled_t *oled, unsigned x, unsigned page, const char *text, bool large);
bool bsp_oled_textf(bsp_oled_t *oled, unsigned x, unsigned page, bool large, const char *format,
                    ...);
#endif
