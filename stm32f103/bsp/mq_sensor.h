#ifndef MQ_SENSOR_H
#define MQ_SENSOR_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* 数组下标按 MQ4、MQ7、MQ8 排列，必须与 gas_channel_t 的顺序一致。 */
#define MQ_SENSOR_CHANNELS 3u
/* 每路平均的转换次数。MQ 输出上带着工频干扰和加热丝开关噪声，单次转换不是一个
 * 稳定的读数。 */
#define MQ_SENSOR_AVERAGES 8u

typedef struct {
    ADC_HandleTypeDef *adc;
} mq_sensor_t;

bool mq_sensor_init(mq_sensor_t *sensor, ADC_HandleTypeDef *adc);
/* 读取全部三路，每路平均 MQ_SENSOR_AVERAGES 次转换。任意一次转换失败，整组
 * 读数作废。 */
bool mq_sensor_read(mq_sensor_t *sensor, uint16_t values[MQ_SENSOR_CHANNELS]);
#endif
