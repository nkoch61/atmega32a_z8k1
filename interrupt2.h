/*
 * $Header: /home/playground/src/atmega32/z8k1/interrupt2.h,v 2cf4997fff1e 2021/03/28 15:35:08 nkoch $
 */


#ifndef INTERRUPT2_H
#define INTERRUPT2_H


#include <stdbool.h>
#include <stdint.h>


extern void (*interrupt2_callback) (void);
extern void interrupt2_init (void);


#endif
