#include "bsp_servo.h"
#include "tim.h"

/* 舵机的脉宽。1000 us 对应关、2000 us 对应开，具体方向取决于舵机臂的装配，
 * 实物上要按实际行程校准这两个值。 */
#define SERVO_CLOSE_US 1000u
#define SERVO_OPEN_US 2000u

void bsp_servo_init(void)
{
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    bsp_servo_set(false);
}

void bsp_servo_set(bool open)
{
    /* alarm_force_safe() 会从 HardFault / NMI / Error_Handler 里调用，也就是
     * 可能在 MX_TIM4_Init() 之前。TIM4 的时钟没开时写它的寄存器，在 STM32F1
     * 上不保证安全——在故障处理程序里再触发一次故障就是 lockup。所以先看时钟
     * 使能位；读 RCC 本身不需要任何外设时钟。 */
    if ((RCC->APB1ENR & RCC_APB1ENR_TIM4EN) == 0u) {
        return;
    }
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, open ? SERVO_OPEN_US : SERVO_CLOSE_US);
}
