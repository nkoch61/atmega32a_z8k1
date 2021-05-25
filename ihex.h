/*
 * $Header: /home/playground/src/atmega32/z8k1/ihex.h,v 79c4a60b1113 2021/05/23 14:28:53 nkoch $
 */


#ifndef IHEX_H
#define IHEX_H


typedef struct
{
  size_t (*getline) (char *buf, size_t bufsize);
  void (*debug) (const char *buf);
  void (*debug_P) (const __flash char *buf);
  void (*error) (const char *buf);
  void (*error_P) (const __flash char *buf);
  void (*write_byte) (uint32_t address, uint8_t byte);
  void (*write_word) (uint32_t address, uint16_t word);
}
ihex_interface;


extern int8_t ihex_loader (ihex_interface *interface, uint32_t load_offset, uint32_t *start_address);


#endif
