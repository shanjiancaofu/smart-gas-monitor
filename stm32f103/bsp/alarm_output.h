#ifndef ALARM_OUTPUT_H
#define ALARM_OUTPUT_H
/* main.h 里是本模块所要驱动的那些 CubeMX User Label 宏。 */
#include "main.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* 需按实物继电器接点确认：线圈通电表示阀门开启。 */
#ifndef RELAY_OPEN_LEVEL
#define RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif
#ifndef BUZZER_ON_LEVEL
#define BUZZER_ON_LEVEL GPIO_PIN_SET
#endif

/* alarm_output_apply() 的蜂鸣器时长，单位毫秒。0 表示不响：窗口长度为 0 个
 * 节拍，「已经过去的时间 < 0」恒假，蜂鸣器自然就不会打开。 */
#define BUZZER_FOREVER 0xffffu
/* TIM2 的节拍周期。 */
#define BUZZER_TICK_MS 10u

/* 把继电器驱动到关闭，并让所有指示输出静默。 */
void alarm_output_init(void);
/* 把继电器、阀门指示和蜂鸣器都驱动到关闭状态，并且可以从故障处理入口安全调用。
 * 它写的是 ODR，引脚仍配置为输入时驱动不了：复位后到 MX_GPIO_Init 之间继电器
 * 引脚是浮空的，只有硬件下拉能把它保持在安全状态。
 *
 * 蜂鸣器也在这里关掉，是因为故障处理入口之后 alarm_output_apply() 不再被调用，
 * 引脚会停在最后写入的电平上：故障若发生在响铃期间，它就那样响到复位为止。 */
void alarm_output_force_safe(void);
/* 先落实阀门决定，再处理指示器与其他慢速 I/O。蜂鸣器在每一次新报警之后驱动
 * buzzer_ms 毫秒，随后归于静默，因此锁存的故障不会响到有人确认它为止；
 * BUZZER_FOREVER 取消这个上限，一直响到报警解除。 */
void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm,
                        uint16_t buzzer_ms, uint32_t tick);
#endif
