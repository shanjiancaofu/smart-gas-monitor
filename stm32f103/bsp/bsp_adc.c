#include "bsp_adc.h"

bool bsp_adc_init(bsp_adc_t *adc, ADC_HandleTypeDef *handle)
{
    adc->handle = handle;
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
