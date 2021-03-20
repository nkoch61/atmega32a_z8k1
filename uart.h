/*
 * $Header: /home/playground/src/atmega32/z8k1/uart.h,v 40a1410d27f4 2021/03/09 22:05:59 nkoch $
 */


#ifndef UART_H
#define UART_H


#include <stdbool.h>
#include <stdint.h>


extern void (*uart_putc_sleep) (void);
extern void (*uart_getc_sleep) (void);
extern int uart_getc_nowait (void);
extern uint8_t uart_getc_wait (void);
extern bool uart_putc_nowait (uint8_t c);
extern void uart_putc_wait (uint8_t c);
extern void uart_init (void);


#endif
