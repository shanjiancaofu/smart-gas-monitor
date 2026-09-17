#include "bsp_servo.h"
#include "tim.h"

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

#define SERVO_CLOSE_US 500u
#define SERVO_OPEN_US 2500u

#if USE_SOFT_I2C
static volatile uint16_t requested_pulse_us = SERVO_CLOSE_US;

bool bsp_servo_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_TIM_DISABLE(&htim4);
    __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE | TIM_IT_CC3);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    requested_pulse_us = SERVO_CLOSE_US;
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, SERVO_CLOSE_US);
    __HAL_TIM_SET_COUNTER(&htim4, 0u);
    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE | TIM_FLAG_CC3);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);

    HAL_NVIC_SetPriority(TIM4_IRQn, 0u, 2u);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE | TIM_IT_CC3);
    __HAL_TIM_ENABLE(&htim4);
    return true;
}

void bsp_servo_set(bool open)
{
    requested_pulse_us = open ? SERVO_OPEN_US : SERVO_CLOSE_US;
}

/* Proteus does not reliably expose TIM4_CH3 through PB8 alternate function.
   Keep TIM4's precise 1 MHz counter, but drive PB8 directly on update/compare. */
void TIM4_IRQHandler(void)
{
    bool update = __HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_UPDATE) != RESET &&
                  __HAL_TIM_GET_IT_SOURCE(&htim4, TIM_IT_UPDATE) != RESET;
    bool compare = __HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_CC3) != RESET &&
                   __HAL_TIM_GET_IT_SOURCE(&htim4, TIM_IT_CC3) != RESET;

    if (compare) {
        __HAL_TIM_CLEAR_IT(&htim4, TIM_IT_CC3);
        GPIOB->BRR = GPIO_PIN_8;
    }
    if (update) {
        __HAL_TIM_CLEAR_IT(&htim4, TIM_IT_UPDATE);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, requested_pulse_us);
        GPIOB->BSRR = GPIO_PIN_8;
    }
}
#else
bool bsp_servo_init(void)
{
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
#endif
