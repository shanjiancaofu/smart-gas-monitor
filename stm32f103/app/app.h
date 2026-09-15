#ifndef APP_H
#define APP_H
#include <stdbool.h>

/* 系统入口；初始化失败由 main 的 Error_Handler 处理。 */
bool app_init(void);
/* 主循环调用：采样、按键、协议、报警、历史、显示和参数保存。 */
void app_update(void);
/* TIM2 中断每 10 ms 调用一次，只累加软件节拍。 */
void app_tick_isr(void);
#endif
