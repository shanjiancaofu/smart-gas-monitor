#ifndef APP_H
#define APP_H
#include <stdbool.h>

/* 系统入口。TIM2 启动失败时返回 false，交由 main 的 Error_Handler 处理。 */
bool app_init(void);
/* 主循环调用，依次调度采样、按键、协议、报警、存储和显示。 */
void app_update(void);
/* TIM2 中断只累加 10 ms 节拍。 */
void app_tick_isr(void);
#endif
