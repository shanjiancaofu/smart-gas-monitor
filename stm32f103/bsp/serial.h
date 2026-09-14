#ifndef SERIAL_H
#define SERIAL_H
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 接收端拼装的最长一行，含终止符。更长的行整行丢弃，而不是截断成另一条
 * 命令。 */
#define SERIAL_LINE_MAX 48u
/* 足以容纳协议单次调用发出的最长应答，因此在空闲端口上开始的一次写总能放
 * 得下，不会被丢。 */
#define SERIAL_TX_RING 128u
#define SERIAL_RX_RING 64u

typedef struct {
    UART_HandleTypeDef *uart;
    uint8_t rx_byte;                 /* 逐个字节交给 HAL */
    volatile uint8_t rx[SERIAL_RX_RING];
    volatile uint16_t rx_head, rx_tail;
    volatile uint8_t tx[SERIAL_TX_RING];
    volatile uint16_t tx_head, tx_tail;
    char line[SERIAL_LINE_MAX];
    uint8_t length;
    bool dropping;
    bool ready;
    bool tx_busy;
} serial_t;

/* 收发两个方向都是中断驱动：主循环从不等待字符移位输出，否则一条长应答会让
 * 采样停住的时间超过采样器自己的故障超时。 */
void serial_init(serial_t *s, UART_HandleTypeDef *uart);
/* 重试 HAL 拒绝启动的发送。主循环每轮调用一次。 */
void serial_poll(serial_t *s);

/* 用目前已经到达的内容凑出一整行，否则返回 false。一个调用点只允许读一个
 * 端口；每次调用会消耗掉它取走的内容。 */
bool serial_read_line(serial_t *s, char *line, size_t size);
/* 把文本排进发送队列并立即返回。队列放不下时返回 false，一个字节也不发：
 * 半行比没有更糟，因为对端是按行读取的文本。 */
bool serial_write(serial_t *s, const char *text);
bool serial_busy(const serial_t *s);
#endif
