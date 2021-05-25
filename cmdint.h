/*
 * $Header: /home/playground/src/atmega32/z8k1/cmdint.h,v 79c4a60b1113 2021/05/23 14:28:53 nkoch $
 */


#ifndef CMDINT_H
#define CMDINT_H


#include <stdint.h>
#include <stdbool.h>


struct CmdIntCallback
{
  int (*getc_f) (void);
  void (*putc_f) (char c);
  void (*bs_f) (void);
  void (*crlf_f) (void);
  void (*cancel_f) (void);
  void (*prompt_f) (void);
  int8_t (*interp_f) (int8_t argc, char **argv);
  int8_t (*line_f) (void);
};
typedef struct CmdIntCallback CmdIntCallback_t;


extern int8_t cmdint_getline (const CmdIntCallback_t *callback,
                             char *ptr,
                             char *bptr,
                             uint8_t len);
extern void cmdint (const CmdIntCallback_t *callback);


#endif
