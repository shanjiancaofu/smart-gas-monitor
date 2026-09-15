#ifndef BSP_ADC_H
#define BSP_ADC_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef struct {
    ADC_HandleTypeDef *handle;
} bsp_adc_t;

bool bsp_adc_init(bsp_adc_t *adc, ADC_HandleTypeDef *handle);
/* 硬件通道号 0～15，失败通过返回值报告，不用 0 读数冒充失败。 */
bool bsp_adc_read(void *context, uint8_t channel, uint16_t *value);
#endif
