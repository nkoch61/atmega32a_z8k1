/*
 * $Header: /home/playground/src/atmega32/z8k1/cmdint.h,v 40a1410d27f4 2021/03/09 22:05:59 nkoch $
 */


#ifndef CMDINT_H
#define CMDINT_H


struct CmdIntCallback
{
  int (*getc_f) (void);
  void (*putc_f) (char c);
  void (*bs_f) (void);
  void (*cr_f) (void);
  void (*lf_f) (void);
  void (*cancel_f) (void);
  void (*prompt_f) (void);
  int (*interp_f) (int argc, char **argv);
  int (*line_f) (void);
};
typedef struct CmdIntCallback CmdIntCallback_t;


extern void cmdint (const CmdIntCallback_t *callback);


#endif
