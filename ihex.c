/*
 * $Header: /home/playground/src/atmega32/z8k1/ihex.c,v 79c4a60b1113 2021/05/23 14:28:53 nkoch $
 */


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <avr/pgmspace.h>

#include "common.h"
#include "ihex.h"


int8_t ihex_loader (ihex_interface *interface, uint32_t load_offset, uint32_t *start_address)
{
  char buf[256];
  size_t bufsize;
  uint32_t base_address = load_offset;
  *start_address = 0;

  while ((bufsize = interface->getline (buf, sizeof buf)))
  {
    uint16_t bytes;
    uint32_t address;
    uint16_t type;
    int n;
    int checksum = 0;
    bool extended = false;

    if (buf[0] != ':'
        ||
        bufsize < 10)
    {
      interface->error (buf);
      interface->error_P (PSTR("?"));
      return -1;
    };
    for (char *p = &buf[1]; *p && isxdigit (*p) && sscanf_P (p, PSTR("%2x"), &n) == 1; p += 2)
    {
      checksum += n;
    };
    if (checksum & 0xFF)
    {
      interface->error (buf);
      interface->error_P (PSTR("?"));
      return -1;
    };
    if (sscanf_P (buf, PSTR("%*1c%2x%4lx%2x%n"), &bytes, &address, &type, &n) != 3
        ||
        n != 9)
    {
      interface->error (buf);
      interface->error_P (PSTR("?"));
      return -1;
    };
    switch (type)
    {
      case 0x00:
        if ((address & 1) || (bytes & 1))
        {
          while (bytes >= 1)
          {
            uint16_t x;
            if (strlen (buf+n) < 4
                ||
                sscanf_P (buf+n, PSTR("%2x"), &x) != 1)
            {
              interface->error (buf);
              interface->error_P (PSTR("?"));
              return -1;
            };
            interface->write_byte (base_address+address, x);
            n += 2;
            address += 1;
            bytes -= 1;
          }
        }
        else
        {
          while (bytes >= 2)
          {
            uint16_t x;
            if (strlen (buf+n) < 6
                ||
                sscanf_P (buf+n, PSTR("%4x"), &x) != 1)
            {
              interface->error (buf);
              interface->error_P (PSTR("?"));
              return -1;
            };
            interface->write_word (base_address+address, x);
            n += 4;
            address += 2;
            bytes -= 2;
          }
        }
        continue;

      case 0x01:
        if (bytes != 0 || (extended && address != 0))
        {
          interface->error (buf);
          interface->error_P (PSTR("?"));
          return -1;
        };
        *start_address = address;
        return 0;

      case 0x04:
        if (bytes != 2
            ||
            address != 0
            ||
            sscanf_P (&buf[n], PSTR("%4lx"), &base_address) != 1)
        {
          interface->error (buf);
          interface->error_P (PSTR("?"));
          return -1;
        };
        base_address <<= 16;
        base_address += load_offset;
        extended = true;
        continue;

      case 0x05:
        if (bytes != 4
            ||
            address != 0
            ||
            sscanf_P (&buf[n], PSTR("%8lx"), start_address) != 1)
        {
          interface->error (buf);
          interface->error_P (PSTR("?"));
          return -1;
        };
        extended = true;
        continue;

      default:
        interface->error (buf);
        interface->error_P (PSTR("?"));
        return -1;
    }
  };
  return -1;
}
