#include "bsp_servo.h"
#include "tim.h"

/* Calibrate these pulse widths against the physical valve linkage. */
#define SERVO_CLOSE_US 1000u
#define SERVO_OPEN_US 2000u

bool bsp_servo_init(void)
{
    /* Load CLOSE before enabling PWM so the first output pulse is safe. */
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, SERVO_CLOSE_US);
    return HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3) == HAL_OK;
}

void bsp_servo_set(bool open)
{
    /* Fault handlers can run before TIM4 is initialized; avoid touching a
       disabled peripheral in that case. */
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) == 0u) {
        return;
    }
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3,
                          open ? SERVO_OPEN_US : SERVO_CLOSE_US);
}
