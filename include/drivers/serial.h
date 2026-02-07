#ifndef _DRIVERS_SERIAL_H
#define _DRIVERS_SERIAL_H

#include <stdint.h>

#define COM1_PORT   0x3F8
#define COM2_PORT   0x2F8
#define COM3_PORT   0x3E8
#define COM4_PORT   0x2E8

#define UART_THR    0   /* Transmit Holding Register (write) */
#define UART_RBR    0   /* Receive Buffer Register (read) */
#define UART_DLL    0   /* Divisor Latch Low (DLAB=1) */
#define UART_IER    1   /* Interrupt Enable Register */
#define UART_DLH    1   /* Divisor Latch High (DLAB=1) */
#define UART_IIR    2   /* Interrupt Identification Register (read) */
#define UART_FCR    2   /* FIFO Control Register (write) */
#define UART_LCR    3   /* Line Control Register */
#define UART_MCR    4   /* Modem Control Register */
#define UART_LSR    5   /* Line Status Register */
#define UART_MSR    6   /* Modem Status Register */
#define UART_SR     7   /* Scratch Register */

#define LSR_DATA_READY      0x01
#define LSR_OVERRUN_ERROR   0x02
#define LSR_PARITY_ERROR    0x04
#define LSR_FRAMING_ERROR   0x08
#define LSR_BREAK_INDICATOR 0x10
#define LSR_THR_EMPTY       0x20
#define LSR_TX_EMPTY        0x40
#define LSR_FIFO_ERROR      0x80

void serial_init(void);
void serial_init_port(uint16_t port);
int serial_received(uint16_t port);
char serial_read(uint16_t port);
int serial_is_transmit_empty(uint16_t port);
void serial_putc(char c);
void serial_puts(const char *str);
void serial_write(uint16_t port, char c);
void e9_putc(char c);
void e9_puts(const char *str);
void serial_printf(const char *fmt, ...);

#endif
