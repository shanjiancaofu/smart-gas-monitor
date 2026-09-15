#ifndef BSP_LED_H
#define BSP_LED_H
#include <stdbool.h>

/* 仅设置灯的电平，不判断业务状态。 */
void bsp_led_set(bool green, bool yellow, bool red, bool valve_open);
void bsp_led_valve(bool open);
#endif
