/*
 * $Header: /home/playground/src/atmega32/z8k1/uart.h,v 482ed9f39b30 2021/05/11 18:25:09 nkoch $
 */


#ifndef UART_H
#define UART_H


#include <stdbool.h>
#include <stdint.h>


extern void (*uart_sleep) (void);
extern void (*uart_inevent) (uint8_t c);
extern int uart_getc_nowait (void);
extern uint8_t uart_getc_wait (void);
extern bool uart_putc_nowait (uint8_t c);
extern void uart_putc_wait (uint8_t c);
extern void uart_drain ();
extern void uart_flush ();
extern int uart_in_peek (void);
extern bool uart_in_empty ();
extern uint16_t uart_in_used ();
extern uint16_t uart_out_avail ();
extern void uart_init (void);


#endif
