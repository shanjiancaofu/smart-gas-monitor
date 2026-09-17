#include "bsp_servo.h"
#include "tim.h"

/* Same pulse mapping as the verified reference project: -90..+90 degrees. */
#define SERVO_CLOSE_US 500u
#define SERVO_OPEN_US 2500u

bool bsp_servo_init(void)
{
    /* Load CLOSE before enabling PWM so the first pulse is deterministic. */
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, SERVO_CLOSE_US);
    return HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3) == HAL_OK;
}

void bsp_servo_set(bool open)
{
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) == 0u) {
        return;
    }
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3,
                          open ? SERVO_OPEN_US : SERVO_CLOSE_US);
}
