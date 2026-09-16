#ifndef ALARM_H
#define ALARM_H
#include "gas/gas.h"

typedef struct {
    bool active;
    uint32_t started_tick;
    uint8_t alarm_mask;
} alarm_t;

void alarm_init(alarm_t *alarm);
/* tick 为 TIM2 的 10 ms 计数；LED、响铃窗口及继电器动作在这里决策。 */
void alarm_update(alarm_t *alarm, const gas_t *gas, uint32_t tick);
/* 异常入口直接关阀并静默输出；复位浮空阶段仍依赖硬件下拉。 */
void alarm_force_safe(void);
#endif
