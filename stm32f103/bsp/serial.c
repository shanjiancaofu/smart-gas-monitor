#include "serial.h"
#include <string.h>

/* Both rings are a power of two so the index arithmetic is a mask, and both
 * sizes are fixed at compile time so no path can allocate. */
#define RX_MASK (SERIAL_RX_RING - 1u)
#define TX_MASK (SERIAL_TX_RING - 1u)

/* The HAL has one callback per event and identifies the port by handle, so the
 * open ports register here and the callbacks find their owner through it. */
#define SERIAL_MAX 2u
static serial_t *ports[SERIAL_MAX];
static unsigned port_count;
/* Which bytes of the transmit ring the transfer in flight was started from, so
 * the completion callback knows how far the queue may be retired. */
static uint16_t tx_end[SERIAL_MAX];

static serial_t *port_of(UART_HandleTypeDef *uart)
{
    unsigned i;
    for (i = 0; i < port_count; ++i)
        if (ports[i]->uart->Instance == uart->Instance) return ports[i];
    return NULL;
}

static unsigned port_index(const serial_t *s)
{
    unsigned i;
    for (i = 0; i < port_count; ++i)
        if (ports[i] == s) return i;
    return 0u;
}

static uint16_t queued(const serial_t *s)
{
    return (uint16_t)((s->tx_head - s->tx_tail) & TX_MASK);
}

/* The HAL transmits from one contiguous buffer, so a chunk stops at the wrap
 * even when more bytes are queued behind it. */
static uint16_t tx_chunk(const serial_t *s)
{
    uint16_t head = s->tx_head, tail = s->tx_tail;
    if (head >= tail) return (uint16_t)(head - tail);
    return (uint16_t)(SERIAL_TX_RING - tail);
}

static void tx_start(serial_t *s)
{
    unsigned index = port_index(s);
    uint16_t chunk = tx_chunk(s);
    if (chunk == 0u) {
        s->tx_busy = false;
        return;
    }
    tx_end[index] = (uint16_t)((s->tx_tail + chunk) & TX_MASK);
    if (HAL_UART_Transmit_IT(s->uart, (uint8_t *)&s->tx[s->tx_tail], chunk) != HAL_OK) {
        /* Leave the bytes queued. serial_poll() retries, so a refused start is
         * a delay rather than a lost response. */
        s->tx_busy = false;
        return;
    }
    s->tx_busy = true;
}

static void rx_push(serial_t *s, uint8_t byte)
{
    uint16_t next = (uint16_t)((s->rx_head + 1u) & RX_MASK);
    /* A full ring drops the byte and leaves the framing alone. Overwriting
     * would corrupt the line that is already being assembled. */
    if (next == s->rx_tail) return;
    s->rx[s->rx_head] = byte;
    s->rx_head = next;
}

void serial_init(serial_t *s, UART_HandleTypeDef *uart)
{
    memset(s, 0, sizeof(*s));
    s->uart = uart;
    if (port_count < SERIAL_MAX) ports[port_count++] = s;
    /* Armed here, before any byte can arrive, and re-armed from the completion
     * callback so a byte landing while the main loop is busy is still kept. */
    s->ready = HAL_UART_Receive_IT(uart, &s->rx_byte, 1) == HAL_OK;
}

void serial_poll(serial_t *s)
{
    if (!s->ready || s->tx_busy || queued(s) == 0u) return;
    tx_start(s);
}

bool serial_busy(const serial_t *s)
{
    return s->tx_busy || queued(s) != 0u;
}

bool serial_write(serial_t *s, const char *text)
{
    size_t length = strlen(text);
    uint16_t head;
    size_t i;
    if (!s->ready || length == 0u || length >= SERIAL_TX_RING) return false;
    /* All or nothing: the far end reads this as line-oriented text, so half a
     * response is worse than none. */
    if (queued(s) + length >= SERIAL_TX_RING) return false;
    head = s->tx_head;
    for (i = 0; i < length; ++i) {
        s->tx[head] = (uint8_t)text[i];
        head = (uint16_t)((head + 1u) & TX_MASK);
    }
    s->tx_head = head;
    /* An idle transmitter needs starting; a running one picks the new bytes up
     * as its completion callback drains them. */
    if (!s->tx_busy) tx_start(s);
    return true;
}

bool serial_read_line(serial_t *s, char *line, size_t size)
{
    while (s->rx_tail != s->rx_head) {
        uint8_t ch = s->rx[s->rx_tail];
        s->rx_tail = (uint16_t)((s->rx_tail + 1u) & RX_MASK);
        /* Terminals send CRLF; a bare LF is accepted so a script can too. */
        if (ch == '\r') continue;
        if (ch == '\n') {
            size_t length = s->length;
            bool complete = !s->dropping;
            s->length = 0u;
            s->dropping = false;
            if (!complete || length == 0u) continue;
            s->line[length] = '\0';
            if (size > 0u) {
                size_t copy = length < size - 1u ? length : size - 1u;
                memcpy(line, s->line, copy);
                line[copy] = '\0';
            }
            return true;
        }
        if (ch < 0x20u || ch > 0x7eu) continue;
        if (s->dropping) continue;
        if ((unsigned)s->length + 1u >= SERIAL_LINE_MAX) {
            /* The rest of this line is unusable, whatever it says. */
            s->dropping = true;
            continue;
        }
        s->line[s->length++] = (char)ch;
    }
    return false;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    serial_t *s = port_of(uart);
    if (s == NULL) return;
    rx_push(s, s->rx_byte);
    (void)HAL_UART_Receive_IT(s->uart, &s->rx_byte, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    serial_t *s = port_of(uart);
    if (s == NULL) return;
    /* Retire exactly the chunk that was started, in case the main loop queued
     * more while it was shifting out. */
    s->tx_tail = tx_end[port_index(s)];
    if (queued(s) != 0u) tx_start(s);
    else s->tx_busy = false;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    serial_t *s = port_of(uart);
    if (s == NULL) return;
    __HAL_UART_CLEAR_OREFLAG(s->uart);
    __HAL_UART_CLEAR_FEFLAG(s->uart);
    __HAL_UART_CLEAR_NEFLAG(s->uart);
    __HAL_UART_CLEAR_PEFLAG(s->uart);
    /* A framing or overrun error stops the HAL's reception. Restarting it is
     * the point: one bad byte from a radio link must not silence the port for
     * the rest of the run. */
    (void)HAL_UART_AbortReceive(s->uart);
    (void)HAL_UART_Receive_IT(s->uart, &s->rx_byte, 1);
}
