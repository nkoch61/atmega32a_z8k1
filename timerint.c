/*
 * $Header: /home/playground/src/atmega32/z8k1/timerint.c,v 2cf4997fff1e 2021/03/28 15:35:08 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "common.h"
#include "timerint.h"


#define F_INTERRUPT     50
#define PRESCALER       64
//  F_CPU/(PRESCALER*F_INTERRUPT)
#define TIMERVALUE      2500


static void dummy (void)
{
}


void (*timerint_callback) (void) = dummy;


// 20ms-Interrupt
ISR (TIMER1_COMPA_vect)
{
  sei ();
  timerint_callback ();
}


void timerint_init (void)
{
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
  OCR1AH = TIMERVALUE / 256;
  OCR1AL = TIMERVALUE % 256;
  TCNT1H = 0;
  TCNT1L = 0;
  TIMSK  = _BV(OCIE1A);
}
