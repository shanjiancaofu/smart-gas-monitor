#ifndef BSP_SERVO_H
#define BSP_SERVO_H
#include <stdbool.h>
#include <stdint.h>
void bsp_servo_init(void);
void bsp_servo_set(bool open);
#endif
