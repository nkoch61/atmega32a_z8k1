/*
 * $Header: /home/playground/src/atmega32/z8k1/common.h,v ae4527d86205 2021/03/14 16:33:56 nkoch $
 */


#ifndef COMMON_H
#define COMMON_H


#define CAT(a,b)        a##b
#define XCAT(a,b)       CAT (a,b)
#define STR(x)          #x
#define XSTR(x)         STR(x)
#define ASIZE(a)        (sizeof (a) / sizeof ((a)[0]))
#define LENGTH(a)       (ASIZE (a))

#define RNDDIV(x, y)    (((x) + ((y) >> 1)) / (y))


#endif
