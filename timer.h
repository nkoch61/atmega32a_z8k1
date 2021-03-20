/*
 * $Header: /home/playground/src/atmega32/z8k1/timer.h,v 8bde2c2edd07 2021/03/11 21:32:48 nkoch $
 */


#ifndef TIMER_H
#define TIMER_H


#include <stdbool.h>
#include <stdint.h>


extern void (*timer) (uint16_t);
extern uint16_t timerint_cmpval (void);
extern void timer_init (void);


#endif
