/*
 * $Header: /home/playground/src/atmega32/z8k1/cbuf.h,v 40a1410d27f4 2021/03/09 22:05:59 nkoch $
 */


#include "common.h"
#include <stdbool.h>
#include <stdint.h>


#ifndef CBUF_ID
#error
#endif
#ifndef CBUF_LEN
#error
#endif
#if CBUF_LEN > 256
#error
#endif
#ifndef CBUF_TYPE
#error
#endif


struct XCAT(CBUF_ID,CBuf)
{
  CBUF_TYPE data[CBUF_LEN];
  volatile uint8_t put, get, overrun;
};


static inline uint8_t XCAT(CBUF_ID,used) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  return (cbuf->put - cbuf->get + CBUF_LEN) % CBUF_LEN;
}

static inline uint8_t XCAT(CBUF_ID,avail) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  return CBUF_LEN - XCAT(CBUF_ID,used) (cbuf) - 1;
}

static inline bool XCAT(CBUF_ID,empty) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  return cbuf->get == cbuf->put;
}

static inline bool XCAT(CBUF_ID,full) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  return (cbuf->get == 0 && cbuf->put == CBUF_LEN -1)
         ||
         (cbuf->put == cbuf->get - 1);
}

static inline CBUF_TYPE XCAT(CBUF_ID,peek) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  return cbuf->data[cbuf->get];
}

static inline void XCAT(CBUF_ID,less) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  cbuf->get = (cbuf->get + 1) % CBUF_LEN;
}

static inline CBUF_TYPE XCAT(CBUF_ID,get) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  const CBUF_TYPE v = XCAT(CBUF_ID,peek) (cbuf);
  XCAT(CBUF_ID,less) (cbuf);
  return v;
}

static inline void XCAT(CBUF_ID,poke) (struct XCAT(CBUF_ID,CBuf) *cbuf, CBUF_TYPE v)
{
  cbuf->data[cbuf->put] = v;
}

static inline void XCAT(CBUF_ID,more) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  cbuf->put = (cbuf->put + 1) % CBUF_LEN;
}

static inline void XCAT(CBUF_ID,put) (struct XCAT(CBUF_ID,CBuf) *cbuf, CBUF_TYPE v)
{
  XCAT(CBUF_ID,poke) (cbuf, v);
  XCAT(CBUF_ID,more) (cbuf);
}

static inline void XCAT(CBUF_ID,clear) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  cbuf->put = cbuf->get = cbuf->put % CBUF_LEN;
}

static inline void XCAT(CBUF_ID,init) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  cbuf->put = cbuf->get = cbuf->overrun = 0;
}

static inline void XCAT(CBUF_ID,set_overrun) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  ++cbuf->overrun;
}

static inline bool XCAT(CBUF_ID,get_overrun) (struct XCAT(CBUF_ID,CBuf) *cbuf)
{
  const bool overrun = cbuf->overrun > 0;

  if (overrun)
  {
    --cbuf->overrun;
    return true;
  };
  return false;
}
