#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H
#include <stdbool.h>

/* 本项目的蜂鸣器是低电平触发：引脚拉低才响。默认值改成 GPIO_PIN_SET 的话，
 * 「关」会输出低电平，正好让它在待机时一直响。 */
#ifndef BUZZER_ON_LEVEL
#define BUZZER_ON_LEVEL GPIO_PIN_RESET
#endif
/* 只控制开关，响铃时间由应用层管理。 */
void bsp_buzzer_set(bool on);
#endif
