/*
 * $Header: /home/playground/src/atmega32/z8k1/interrupt2.c,v 2cf4997fff1e 2021/03/28 15:35:08 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "common.h"
#include "interrupt2.h"


static void dummy (void)
{
}


void (*interrupt2_callback) (void) = dummy;


// EXTINT2-Interrupt
ISR (INT2_vect)
{
  sei ();
  interrupt2_callback ();
}


void interrupt2_init (void)
{
  GICR &= ~_BV(INT2);
  MCUCSR &= ~_BV(ISC2);
  GIFR |= _BV(INTF2);
  GICR |= _BV(INT2);
}
