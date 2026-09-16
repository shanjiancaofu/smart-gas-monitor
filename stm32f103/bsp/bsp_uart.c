#include "bsp_uart.h"
#include <string.h>

/* 两个环形缓冲都是 2 的幂，下标运算就是一次掩码；两个长度都在编译期固定，
 * 因此没有任何路径会分配内存。 */
#define RX_MASK (SERIAL_RX_RING - 1u)
#define TX_MASK (SERIAL_TX_RING - 1u)

/* HAL 每种事件只有一个回调，且靠句柄辨认端口，因此已打开的端口在这里登记，
 * 回调通过它找到自己的宿主。 */
#define SERIAL_MAX 2u
static bsp_uart_t *ports[SERIAL_MAX];
static unsigned port_count;
/* 在途的那次发送是从发送环形缓冲的哪些字节开始的，完成回调据此知道队列可以
 * 回收到哪里。 */
static uint16_t tx_end[SERIAL_MAX];

static bsp_uart_t *port_of(UART_HandleTypeDef *uart)
{
    unsigned i;
    for (i = 0; i < port_count; ++i) {
        if (ports[i]->uart->Instance == uart->Instance) {
            return ports[i];
        }
    }
    return NULL;
}

static unsigned port_index(const bsp_uart_t *s)
{
    unsigned i;
    for (i = 0; i < port_count; ++i) {
        if (ports[i] == s) {
            return i;
        }
    }
    return 0u;
}

static uint16_t queued(const bsp_uart_t *s)
{
    return (uint16_t)((s->tx_head - s->tx_tail) & TX_MASK);
}

/* HAL 是从一段连续缓冲里发送的，所以即使后面还排着字节，一个分块也在回绕处
 * 停下。 */
static uint16_t tx_chunk(const bsp_uart_t *s)
{
    uint16_t head = s->tx_head, tail = s->tx_tail;
    if (head >= tail) {
        return (uint16_t)(head - tail);
    }
    return (uint16_t)(SERIAL_TX_RING - tail);
}

static void tx_start(bsp_uart_t *s)
{
    unsigned index = port_index(s);
    uint16_t chunk = tx_chunk(s);
    if (chunk == 0u) {
        s->tx_busy = false;
        return;
    }
    tx_end[index] = (uint16_t)((s->tx_tail + chunk) & TX_MASK);
    if (HAL_UART_Transmit_IT(s->uart, (uint8_t *)&s->tx[s->tx_tail], chunk) != HAL_OK) {
        /* 把字节留在队列里。bsp_uart_poll() 会重试，所以启动被拒绝只是一次
         * 延迟，而不是丢掉的应答。 */
        s->tx_busy = false;
        return;
    }
    s->tx_busy = true;
}

static void rx_push(bsp_uart_t *s, uint8_t byte)
{
    uint16_t next = (uint16_t)((s->rx_head + 1u) & RX_MASK);
    /* 环形缓冲满时丢弃这个字节，不去动行的组装。覆盖写会破坏正在拼装的这
     * 一行。 */
    if (next == s->rx_tail) {
        return;
    }
    s->rx[s->rx_head] = byte;
    s->rx_head = next;
}

void bsp_uart_init(bsp_uart_t *s, UART_HandleTypeDef *uart)
{
    memset(s, 0, sizeof(*s));
    s->uart = uart;
    if (port_count < SERIAL_MAX) {
        ports[port_count++] = s;
    }
    /* 在这里、在任何字节可能到达之前就挂上接收，并由完成回调重新挂上，
     * 因此主循环忙的时候落下的字节仍然会被收下。 */
    s->ready = HAL_UART_Receive_IT(uart, &s->rx_byte, 1) == HAL_OK;
}

void bsp_uart_poll(bsp_uart_t *s)
{
    if (!s->ready || s->tx_busy || queued(s) == 0u) {
        return;
    }
    tx_start(s);
}

bool bsp_uart_busy(const bsp_uart_t *s)
{
    return s->tx_busy || queued(s) != 0u;
}

bool bsp_uart_write(bsp_uart_t *s, const char *text)
{
    size_t length = strlen(text);
    uint16_t head;
    size_t i;
    if (!s->ready || length == 0u || length >= SERIAL_TX_RING) {
        return false;
    }
    /* 全有或全无：对端是按行读取的文本，半条应答比没有应答更糟。 */
    if (queued(s) + length >= SERIAL_TX_RING) {
        return false;
    }
    head = s->tx_head;
    for (i = 0; i < length; ++i) {
        s->tx[head] = (uint8_t)text[i];
        head = (uint16_t)((head + 1u) & TX_MASK);
    }
    s->tx_head = head;
    /* 空闲的发送器需要启动；正在发送的那个会随完成回调回收队列而把新字节
     * 接着发出去。 */
    if (!s->tx_busy) {
        tx_start(s);
    }
    return true;
}

bool bsp_uart_read_line(bsp_uart_t *s, char *line, size_t size)
{
    while (s->rx_tail != s->rx_head) {
        uint8_t ch = s->rx[s->rx_tail];
        s->rx_tail = (uint16_t)((s->rx_tail + 1u) & RX_MASK);
        /* 终端发的是 CRLF；单独的 LF 也接受，这样脚本也能发。 */
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            size_t length = s->length;
            bool complete = !s->dropping;
            s->length = 0u;
            s->dropping = false;
            if (!complete || length == 0u) {
                continue;
            }
            s->line[length] = '\0';
            if (size > 0u) {
                size_t copy = length < size - 1u ? length : size - 1u;
                memcpy(line, s->line, copy);
                line[copy] = '\0';
            }
            return true;
        }
        if (ch < 0x20u || ch > 0x7eu) {
            continue;
        }
        if (s->dropping) {
            continue;
        }
        if ((unsigned)s->length + 1u >= SERIAL_LINE_MAX) {
            /* 这一行剩下的部分，无论写的是什么，都用不了了。 */
            s->dropping = true;
            continue;
        }
        s->line[s->length++] = (char)ch;
    }
    return false;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    bsp_uart_t *s = port_of(uart);
    if (s == NULL) {
        return;
    }
    rx_push(s, s->rx_byte);
    s->ready = HAL_UART_Receive_IT(s->uart, &s->rx_byte, 1) == HAL_OK;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    bsp_uart_t *s = port_of(uart);
    if (s == NULL) {
        return;
    }
    /* 只回收真正启动的那一个分块，以防主循环在移位输出期间又排进了更多
     * 字节。 */
    s->tx_tail = tx_end[port_index(s)];
    if (queued(s) != 0u) {
        tx_start(s);
    } else {
        s->tx_busy = false;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    bsp_uart_t *s = port_of(uart);
    if (s == NULL) {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(s->uart);
    __HAL_UART_CLEAR_FEFLAG(s->uart);
    __HAL_UART_CLEAR_NEFLAG(s->uart);
    __HAL_UART_CLEAR_PEFLAG(s->uart);
    /* 帧错误或溢出错误会让 HAL 的接收停下来。重新启动正是关键：无线链路的
     * 一个坏字节，不该让这个口在剩下的运行时间里一直沉默。 */
    (void)HAL_UART_AbortReceive(s->uart);
    (void)HAL_UART_Receive_IT(s->uart, &s->rx_byte, 1);
}
