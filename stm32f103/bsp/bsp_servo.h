#ifndef BSP_SERVO_H
#define BSP_SERVO_H
#include <stdbool.h>
#include <stdint.h>
/* 返回 false 表示 PWM 外设没启动起来；阀门位置无从保证，调用方应当中止初始化。 */
bool bsp_servo_init(void);
void bsp_servo_set(bool open);
#endif
