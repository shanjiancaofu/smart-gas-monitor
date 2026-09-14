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

/* 蜂鸣器从报警开始那一刻起鸣响的时长。它是固定的而不是可配置的：它的职责是
 * 被注意到，而不是被调来调去。 */
#define BUZZER_ALARM_TICKS 500u /* TIM2：每个节拍 10 ms */

/* 把继电器驱动到关闭，并让所有指示输出静默。 */
void alarm_output_init(void);
/* 把继电器和阀门指示驱动到关闭状态、让蜂鸣器静默，并且可以从故障处理入口安全
 * 调用。所有输出都要在这里收口：这些入口之后 alarm_output_apply() 不再运行，
 * 漏掉哪个引脚，它就会停在最后一次写入的电平上。它写的是 ODR，引脚仍配置为
 * 输入时驱动不了：复位后到 MX_GPIO_Init 之间继电器引脚是浮空的，只有硬件下拉
 * 能把它保持在安全状态。 */
void alarm_output_force_safe(void);
/* 先落实阀门决定，再处理指示器与其他慢速 I/O。蜂鸣器在每一次新报警之后驱动
 * BUZZER_ALARM_TICKS 个节拍，随后归于静默，因此锁存的故障不会响到有人确认它
 * 为止。 */
void alarm_output_apply(bool valve_open, bool green, bool yellow, bool alarm,
                        uint32_t tick);
#endif
