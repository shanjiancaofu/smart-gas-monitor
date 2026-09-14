#include "mq_sensor.h"

/* PA0, PA1, PA4 in MQ4, MQ7, MQ8 order. */
static const uint32_t channels[MQ_SENSOR_CHANNELS] = {
    ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_4
};

bool mq_sensor_init(mq_sensor_t *sensor, ADC_HandleTypeDef *adc)
{
    sensor->adc = adc;
    return HAL_ADCEx_Calibration_Start(adc) == HAL_OK;
}

bool mq_sensor_read(mq_sensor_t *sensor, uint16_t values[MQ_SENSOR_CHANNELS])
{
    ADC_ChannelConfTypeDef config = {0};
    unsigned i, n;
    config.Rank = ADC_REGULAR_RANK_1;
    /* Long sampling suits the high-impedance divider on the sensor outputs. */
    config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    for (i = 0; i < MQ_SENSOR_CHANNELS; ++i) {
        uint32_t sum = 0;
        config.Channel = channels[i];
        if (HAL_ADC_ConfigChannel(sensor->adc, &config) != HAL_OK) return false;
        /* Single conversion mode: every pass needs its own start. */
        for (n = 0; n < MQ_SENSOR_AVERAGES; ++n) {
            if (HAL_ADC_Start(sensor->adc) != HAL_OK ||
                HAL_ADC_PollForConversion(sensor->adc, 5) != HAL_OK) {
                (void)HAL_ADC_Stop(sensor->adc);
                return false;
            }
            sum += HAL_ADC_GetValue(sensor->adc);
        }
        if (HAL_ADC_Stop(sensor->adc) != HAL_OK) return false;
        values[i] = (uint16_t)(sum / MQ_SENSOR_AVERAGES);
    }
    return true;
}
