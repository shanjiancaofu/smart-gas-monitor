#include "bsp_adc.h"

/* 这个文件没有 include config.h，看不到那份兜底定义；两个构建都不带 -Wundef
 * 之外的宽容，补上以免以后开了告警才炸。 */
#ifndef USE_SOFT_I2C
#define USE_SOFT_I2C 0
#endif

bool bsp_adc_init(bsp_adc_t *adc, ADC_HandleTypeDef *handle)
{
    adc->handle = handle;
#if USE_SOFT_I2C
    /* PA5 = ADC_IN5，仿真分支里 MQ6 的输入。
     *
     * HAL_ADC_MspInit()（Core/Src/adc.c，CubeMX 生成）只按 .ioc 把 PA0/PA1/PA4
     * 配成模拟输入。PA5 原本是阀门灯、被 MX_GPIO_Init() 配成了推挽输出，这里改
     * 成模拟；阀门灯则在 bsp_led.c 里挪到了 PB9。整条链的起因见 bsp_servo.c：
     * Proteus 不实现 TIM4_CH3 走 PB8，舵机改走 PA1/TIM2_CH2，而 PA1 原本是 MQ6，
     * 48 脚封装又没有空闲的模拟脚，只能再挤一个器件出来。
     *
     * 放在这里而不是改 .ioc / adc.c：这根引脚只属于仿真分支，动 CubeMX 那一侧
     * 会连带重写实物分支的 Core/，而实物接线不能动。 */
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
#endif
    return HAL_ADCEx_Calibration_Start(handle) == HAL_OK;
}

bool bsp_adc_read(void *context, uint8_t channel, uint16_t *value)
{
    bsp_adc_t *adc = context;
    ADC_ChannelConfTypeDef config = {0};

    if (channel > 15u) {
        return false;
    }
    config.Channel = channel;
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(adc->handle, &config) != HAL_OK ||
        HAL_ADC_Start(adc->handle) != HAL_OK ||
        HAL_ADC_PollForConversion(adc->handle, 5) != HAL_OK) {
        (void)HAL_ADC_Stop(adc->handle);
        return false;
    }
    *value = (uint16_t)HAL_ADC_GetValue(adc->handle);
    return HAL_ADC_Stop(adc->handle) == HAL_OK;
}
