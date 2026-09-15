#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H
#include <stdbool.h>

#ifndef BUZZER_ON_LEVEL
#define BUZZER_ON_LEVEL GPIO_PIN_SET
#endif
/* 只控制开关，响铃时间由应用层管理。 */
void bsp_buzzer_set(bool on);
#endif
