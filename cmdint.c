/*
 * $Header: /home/playground/src/atmega32/z8k1/cmdint.c,v 79c4a60b1113 2021/05/23 14:28:53 nkoch $
 */


#include <string.h>
#include <ctype.h>
#include "cmdint.h"


int8_t cmdint_getline (const CmdIntCallback_t *callback,
                    char *ptr,
                    char *bptr,
                    uint8_t len)
{
  int c;
  char *p;

  if (callback->prompt_f)
  {
    callback->prompt_f ();
  };
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
          callback->crlf_f ();
          return p-ptr;

        case 'R'-'@':
          if (bptr && p == ptr)
          {
            strcpy (ptr, bptr);
          };
          while (p-ptr < len && *p)
          {
            callback->putc_f (*p++);
          };
          continue;

        case 'C' - '@':
          *ptr = '\0';
          callback->cancel_f ();
          callback->crlf_f ();
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
  int8_t argc;
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
    if (!callback->interp_f)
    {
      return;
    };
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
    if (callback->line_f && callback->line_f () == -1)
    {
      return;
    }
  }
}
