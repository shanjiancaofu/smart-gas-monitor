#include "bsp_servo.h"
#include "tim.h"
#define SERVO_CLOSE_US 1000u
#define SERVO_OPEN_US 2000u
void bsp_servo_init(void) { HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3); bsp_servo_set(false); }
void bsp_servo_set(bool open) { __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, open ? SERVO_OPEN_US : SERVO_CLOSE_US); }
