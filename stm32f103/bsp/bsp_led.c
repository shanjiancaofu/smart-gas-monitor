#include "bsp_led.h"
#include "main.h"

#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

#if USE_SOFT_I2C
/* 仿真分支：阀门灯从 PA5 挪到 PB9 —— PA5 让给 MQ6 的模拟输入了。
 *
 * 起因和舵机同源（见 bsp_servo.c）：Proteus 不实现 TIM4_CH3 走 PB8，舵机改走
 * PA1/TIM2_CH2，而 PA1 原本是 MQ6；C8 是 48 脚封装，**没有 PC0~PC5**，能接模拟
 * 输入的十个脚（PA0~PA7、PB0/PB1）全被占满，所以必须再挤一个器件出来。挑中
 * 阀门灯是因为它最不显眼——报警时红灯和风扇已经足够提示，阀门状态在 OLED 上
 * 也有文字。PB9 在这一版里完全空闲（早期 I2C1 重映射时它曾作过 SDA，后来
 * I2C1 移到默认的 PB6/PB7，就一直空着了）。
 *
 * 实物分支仍用 PA5，接线一根不动。 */
#define VALVE_LED_PORT GPIOB
#define VALVE_LED_BIT GPIO_PIN_9
#else
#define VALVE_LED_PORT VALVE_LED_GPIO_Port
#define VALVE_LED_BIT VALVE_LED_Pin
#endif

void bsp_led_init(void)
{
#if USE_SOFT_I2C
    /* 实物那边 PA5 是 MX_GPIO_Init() 按 .ioc 配成推挽输出的，PB9 不在里面，
     * 得自己配。 */
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = VALVE_LED_BIT;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(VALVE_LED_PORT, &gpio);
    HAL_GPIO_WritePin(VALVE_LED_PORT, VALVE_LED_BIT, GPIO_PIN_RESET);
#endif
}

void bsp_led_set(bool green, bool yellow, bool red, bool valve_open)
{
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, yellow ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, red ? GPIO_PIN_SET : GPIO_PIN_RESET);
    bsp_led_valve(valve_open);
}

void bsp_led_valve(bool open)
{
    HAL_GPIO_WritePin(VALVE_LED_PORT, VALVE_LED_BIT, open ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
