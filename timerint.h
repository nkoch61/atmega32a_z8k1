/*
 * $Header: /home/playground/src/atmega32/z8k1/timerint.h,v 0ca5a2c9aece 2021/03/27 17:51:21 nkoch $
 */


#ifndef TIMERINT_H
#define TIMERINT_H


#include <stdbool.h>
#include <stdint.h>


extern void (*timerint_callback) (void);
extern void timerint_init (void);


#endif
