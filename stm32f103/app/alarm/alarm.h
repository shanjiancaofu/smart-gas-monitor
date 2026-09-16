#ifndef ALARM_H
#define ALARM_H
#include "gas/gas.h"

/* 蜂鸣节奏，单位是 GAS_TICK_MS(10 ms) 的节拍数：单滴 500 ms，相邻两滴的起点
 * 相隔 700 ms（也就是滴与滴之间停 200 ms），一组响完停 500 ms。报警的通道越
 * 多，一组里的滴声越多。
 *
 * 放在头文件里是为了让主机测试按同一组常量推导时刻——测试里写死数字的话，
 * 每次调这几个档位都要回去改测试。 */
#define BEEP_ON_TICKS 50u
#define BEEP_STEP_TICKS 70u
#define BEEP_GROUP_GAP_TICKS 50u

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
