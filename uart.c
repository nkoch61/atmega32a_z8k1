/*
 * $Header: /home/playground/src/atmega32/z8k1/uart.c,v 2111f61244b4 2021/03/12 19:43:54 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/setbaud.h>
#if USE2X
#error
#endif

#include "common.h"
#define CBUF_ID         u_
#define CBUF_LEN        UART_CBUF_LEN
#define CBUF_TYPE       uint8_t
#include "cbuf.h"
#include "uart.h"


static struct u_CBuf uart_rxd, uart_txd;


static void dummy_sleep (void)
{
}


void (*uart_getc_sleep) (void) = dummy_sleep;
void (*uart_putc_sleep) (void) = dummy_sleep;


// UART Empfangsinterrupt
ISR (USART_RXC_vect)
{
  const uint8_t status = UCSRA;
  uint8_t c = UDR;

  if (status & (_BV(FE) | _BV(DOR) | _BV(PE)))
  {
    c = '~';
  };

  if (!u_full (&uart_rxd))
  {
    u_put (&uart_rxd, c);
  }
  else
  {
    u_set_overrun (&uart_rxd);
  }
}


// UART Sendeinterrupt
ISR (USART_UDRE_vect)
{
  if (!u_empty (&uart_txd))
  {
    UDR = u_get (&uart_txd);
  }
  else
  {
    UCSRB &= ~ _BV(UDRIE);
  }
}


int uart_getc_nowait (void)
{
  if (!u_empty (&uart_rxd))
  {
    return u_get (&uart_rxd);
  };
  return -1;
}


uint8_t uart_getc_wait (void)
{
  while (u_empty (&uart_rxd))
  {
    uart_getc_sleep ();
  };
  return u_get (&uart_rxd);
}


static inline void uart_putc (uint8_t c)
{
  u_put (&uart_txd, c);
  UCSRB |= _BV(UDRIE);
}


bool uart_putc_nowait (uint8_t c)
{
  if (u_full (&uart_txd))
  {
    return false;
  };
  uart_putc (c);
  return true;
}


void uart_putc_wait (uint8_t c)
{
  while (u_full (&uart_txd))
  {
    uart_putc_sleep ();
  };
  uart_putc (c);
}


void uart_init (void)
{
  u_init (&uart_rxd);
  u_init (&uart_txd);

  UBRRH = UBRRH_VALUE;
  UBRRL = UBRRL_VALUE;
#if USE_2X
  UCSRA |= _BV(U2X);
#else
  UCSRA &= ~ _BV(U2X);
#endif

  // UART Receiver und Transmitter anschalten, Receive-Interrupt aktivieren
  // ASYNC, 8N1, asynchron
  UCSRB = _BV(RXEN) | _BV(TXEN) | _BV(RXCIE);
  UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);

  // Flush Receive-Buffer
  do
  {
    UDR;
  }
  while (UCSRA & _BV(RXC));

  // Rücksetzen von Receive und Transmit Complete-Flags
  UCSRA |= _BV(RXC) | _BV(TXC);
}
