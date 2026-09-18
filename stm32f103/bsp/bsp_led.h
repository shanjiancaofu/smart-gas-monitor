#ifndef BSP_LED_H
#define BSP_LED_H
#include <stdbool.h>

/* 只配引脚。实物分支的四个灯都由 CubeMX 的 MX_GPIO_Init() 配好了，这里是空
 * 实现；仿真分支的阀门灯不在 .ioc 里（它挪到了 PB9），要在这里配。 */
void bsp_led_init(void);
/* 仅设置灯的电平，不判断业务状态。 */
void bsp_led_set(bool green, bool yellow, bool red, bool valve_open);
void bsp_led_valve(bool open);
#endif
