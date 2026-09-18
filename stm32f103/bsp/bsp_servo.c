#include "bsp_servo.h"
#include "tim.h"

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

/* 脉宽必须两个分支完全一致，所以放在分叉之外，只此一份。 */
#define SERVO_CLOSE_US 500u
#define SERVO_OPEN_US 2500u

#if USE_SOFT_I2C

/* 仿真分支：TIM2_CH2 / PA1 / 硬件 AF 推挽 PWM，50 Hz。
 *
 * 为什么不和实物一样用 TIM4_CH3：Proteus 的 STM32 模型不实现那一路，PB8 上量
 * 不到任何波形（同类的模型缺失它还犯过一次，见 hardware/proteus/README.md 里
 * I2C1 重映射那一条）。而 TIM2 它是实现的——本工程的 10 ms 系统节拍一直跑在
 * TIM2 上，从来没出过问题。能跑通这个仿真的参考工程用的也是 TIM2_CH2 / PA1，
 * 时基和脉宽与我们完全相同（PSC=71、ARR=19999、CCR 500~2500）。
 *
 * 代价是 PA1 原本是 MQ6 的模拟输入，仿真里让给了舵机，MQ6 改接 PC0。这只影响
 * 仿真分支：实物分支仍是 PB8 / TIM4_CH3，接线一根都不用动。
 *
 * TIM2 的周期也因此从 10 ms 变成 20 ms（50 Hz 是舵机的硬要求）。系统节拍仍是
 * 10 ms —— 由 stm32f1xx_it.c 的 TIM2 中断里补第二次 app_tick_isr() 维持。 */
bool bsp_servo_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_1;          /* PA1 = TIM2_CH2，默认引脚，不用重映射 */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* MX_TIM2_Init() 已经把 PSC 设成 71（1 MHz 计数），这里只把周期改成 20 ms。
     * 计数精度不变，舵机拿到 50 Hz。 */
    __HAL_TIM_SET_AUTORELOAD(&htim2, 19999u);

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = SERVO_CLOSE_US;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_2) != HAL_OK) {
        return false;
    }
    /* 这里**刻意不调 HAL_TIM_PWM_Start()**：它会把 htim2.State 置成 BUSY，而
     * app_init() 紧接着要用 HAL_TIM_Base_Start_IT(&htim2) 起系统节拍，那个函数
     * 一看到 State != READY 就直接返回 HAL_ERROR，于是一路走到 Error_Handler，
     * 整个系统起不来。只开通道输出，计数器留给 Base_Start_IT 去启动——它本来
     * 就要做这件事，PWM 波形从那一刻开始。 */
    TIM_CCxChannelCmd(htim2.Instance, TIM_CHANNEL_2, TIM_CCx_ENABLE);
    return true;
}

void bsp_servo_set(bool open)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2,
                          open ? SERVO_OPEN_US : SERVO_CLOSE_US);
}

#else

/* 实物分支：TIM4_CH3 / PB8 / 硬件 AF 推挽 PWM，50 Hz。一行未动。 */
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
