#include "bsp_servo.h"
#include "tim.h"

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

#if USE_SOFT_I2C
#define SOFT_SERVO_PERIOD_MS 20u
#define SOFT_SERVO_CLOSE_MS 1u
#define SOFT_SERVO_OPEN_MS 2u
static volatile uint8_t soft_phase_ms;
static volatile uint8_t soft_high_ms = SOFT_SERVO_CLOSE_MS;

bool bsp_servo_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    soft_phase_ms = 0u;
    soft_high_ms = SOFT_SERVO_CLOSE_MS;
    return true;
}

void bsp_servo_set(bool open)
{
    soft_high_ms = open ? SOFT_SERVO_OPEN_MS : SOFT_SERVO_CLOSE_MS;
}

void bsp_servo_tick_isr(void)
{
    if (soft_phase_ms == 0u) {
        GPIOB->BSRR = GPIO_PIN_8;
    } else if (soft_phase_ms == soft_high_ms) {
        GPIOB->BRR = GPIO_PIN_8;
    }
    ++soft_phase_ms;
    if (soft_phase_ms >= SOFT_SERVO_PERIOD_MS) {
        soft_phase_ms = 0u;
    }
}
#else
/* Full physical-servo travel; calibrate against the valve linkage. */
#define SERVO_CLOSE_US 500u
#define SERVO_OPEN_US 2500u

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

void bsp_servo_tick_isr(void)
{
}
#endif
