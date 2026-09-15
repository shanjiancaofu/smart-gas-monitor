#ifndef BSP_RELAY_H
#define BSP_RELAY_H

/* 更换有效电平时，同时检查 CubeMX 初始电平和复位下拉。 */
#ifndef RELAY_OPEN_LEVEL
#define RELAY_OPEN_LEVEL GPIO_PIN_SET
#endif
void bsp_relay_open(void);
void bsp_relay_close(void);
#endif
