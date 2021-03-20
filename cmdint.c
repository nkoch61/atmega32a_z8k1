/*
 * $Header: /home/playground/src/atmega32/z8k1/cmdint.c,v 40a1410d27f4 2021/03/09 22:05:59 nkoch $
 */


#include <string.h>
#include <ctype.h>
#include "cmdint.h"


static int cmdint_getline (const CmdIntCallback_t *callback,
                           char *ptr,
                           char *bptr,
                           size_t len)
{
  int c;
  char *p;

  callback->prompt_f ();
  for (p = ptr; (c = callback->getc_f ()) != -1;)
  {
    if (isascii (c) && isprint (c) && p-ptr < len)
    {
      *p++ = c;
      callback->putc_f (c);
    }
    else
    {
      switch (c)
      {
        case '\r':
          *p = '\0';
          callback->cr_f ();
          callback->lf_f ();
          return p-ptr;

        case 'R'-'@':
          if (p == ptr)
          {
            strcpy (ptr, bptr);
          };
          while (*p)
          {
            callback->putc_f (*p++);
          };
          continue;

        case 'C' - '@':
          *ptr = '\0';
          callback->cancel_f ();
          callback->cr_f ();
          callback->lf_f ();
          return -1;

        case '\b':
        case '\x7F':
          if (p > ptr)
          {
            --p;
            callback->bs_f ();
          };
          continue;

        case 'X' - '@':
          while (p > ptr)
          {
            --p;
            callback->bs_f ();
          };
          continue;

        default:
          continue;
      }
    }
  };
  return -1;
}


void cmdint (const CmdIntCallback_t *callback)
{
  static char backup[81];
  char line[81];
  int argc;
  char *argv[11];
  char *str1, *str2;
  char *token, *subtoken;
  char *saveptr1, *saveptr2;

  for (;;)
  {
    if (cmdint_getline (callback, line, backup, sizeof line) == -1)
    {
      break;
    };
    strcpy (backup, line);
    for (str1 = line; (token = strtok_r (str1, ";", &saveptr1)); str1 = NULL)
    {
      for (argc = 0, str2 = token; argc < 10 && (subtoken = strtok_r (str2, " ", &saveptr2)); str2 = NULL)
      {
        argv[argc++] = subtoken;
      };
      argv[argc] = NULL;
      if (callback->interp_f (argc, argv) == -1)
      {
        return;
      }
    };
    if (callback->line_f () == -1)
    {
      return;
    }
  }
}
