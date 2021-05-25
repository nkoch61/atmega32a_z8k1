/*
 * $Header: /home/playground/src/atmega32/z8k1/common.h,v a4c7093cba04 2021/05/01 13:19:05 nkoch $
 */


#ifndef COMMON_H
#define COMMON_H


#define CAT(a,b)                a##b
#define XCAT(a,b)               CAT (a,b)
#define STR(x)                  #x
#define XSTR(x)                 STR(x)
#define ASIZE(a)                (sizeof (a) / sizeof ((a)[0]))
#define LENGTH(a)               (ASIZE (a))
#define FIELD(t, f)             (((t*)0)->f)
#define FIELD_SIZEOF(t, f)      (sizeof(FIELD(t, f)))

#define RNDDIV(x, y)    (((x) + ((y) >> 1)) / (y))


#define FSTR(x)         (const __flash char []) {x}

#endif
