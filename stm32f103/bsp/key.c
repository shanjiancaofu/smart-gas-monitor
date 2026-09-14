#include "key.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* key_bits() 一次读整个半字节，所以四个按键必须正好落在这个半字节上。在 .ioc
 * 里改动它们的顺序会在这里触发断言，而不是把 KEY1 悄悄映射到错误的位上。 */
_Static_assert(KEY1_Pin == GPIO_PIN_12 && KEY2_Pin == GPIO_PIN_13 &&
               KEY3_Pin == GPIO_PIN_14 && KEY4_Pin == GPIO_PIN_15,
               "key_bits() assumes KEY1..KEY4 are PB12..PB15");

/* PB12～PB15 对应按键位 0～3，与 key_bits() 产生的顺序一致。 */
static const uint16_t key_pins[KEY_COUNT] = {
    KEY1_Pin, KEY2_Pin, KEY3_Pin, KEY4_Pin
};

/* EXTI 处理函数已经看到下降沿、但主循环尚未取走的那些线。 */
static volatile uint8_t exti_pending;

static uint8_t key_bits(void)
{
    /* 低电平有效，因此取反：按下的键读出来是 1。 */
    return (uint8_t)(~(KEY1_GPIO_Port->IDR >> 12) & 15u);
}

void key_init(key_t *keys)
{
    memset(keys, 0, sizeof(*keys));
    /* 上电时就已按住的键必须先释放一次，之后才能产生事件。 */
    keys->raw = keys->stable = key_bits();
    exti_pending = 0;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    unsigned i;
    /* 由 EXTI15_10_IRQHandler 调用。这里只记住哪一路动了：消抖和分发都留在
     * 主循环，所以此处不碰 ADC、I2C 总线和 EEPROM。 */
    for (i = 0; i < KEY_COUNT; ++i)
        if (GPIO_Pin == key_pins[i]) exti_pending |= (uint8_t)(1u << i);
}

static uint8_t key_take_edges(void)
{
    uint32_t primask = __get_PRIMASK();
    uint8_t edges;
    /* 恢复而不是使能：万一将来有调用方在自己的临界区里轮询，这里不能悄悄
     * 把中断重新打开。 */
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
        /* 即使本次轮询已经看到该线回到稳定状态，锁存的边沿仍然重新开始计时
         * 窗口，于是在两次轮询之间按下又松开的动作，仍以边沿而不是以轮询
         * 时刻计时。 */
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
