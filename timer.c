/*
 * $Header: /home/playground/src/atmega32/z8k1/timer.c,v 7092e039564f 2021/03/12 19:44:19 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "common.h"
#include "timer.h"


void timer_init (void)
{
  TCCR2 = _BV(WGM21) | _BV(COM20) | _BV(CS20);
  OCR2 = 0;
  TCNT2 = 0;
  DDRD |= _BV(DDD7);
}
