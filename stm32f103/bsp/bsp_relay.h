#ifndef BSP_RELAY_H
#define BSP_RELAY_H
#include <stdbool.h>

/* 排风扇继电器，PA8，高电平吸合。
 *
 * 燃气阀门不在这里：它由 PB8 的舵机控制（见 bsp_servo.h）。报警时两个一起
 * 动——继电器开风扇排风，舵机关阀断气。 */
#ifndef RELAY_OPEN_LEVEL
#define RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif

/* 只控制开关，什么时候开由应用层决定。 */
void bsp_fan_set(bool on);
#endif
