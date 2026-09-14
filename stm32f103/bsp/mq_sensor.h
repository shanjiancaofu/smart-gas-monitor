#ifndef MQ_SENSOR_H
#define MQ_SENSOR_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* Values are indexed MQ4, MQ7, MQ8 and must stay in gas_channel_t order. */
#define MQ_SENSOR_CHANNELS 3u

typedef struct {
    ADC_HandleTypeDef *adc;
} mq_sensor_t;

bool mq_sensor_init(mq_sensor_t *sensor, ADC_HandleTypeDef *adc);
/* Reads all three channels. A single failed conversion invalidates the set. */
bool mq_sensor_read(mq_sensor_t *sensor, uint16_t values[MQ_SENSOR_CHANNELS]);
#endif
