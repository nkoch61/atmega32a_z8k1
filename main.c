/*
 * $Header: /home/playground/src/atmega32/z8k1/main.c,v 9e133f52f2b1 2021/03/19 23:03:42 nkoch $
 */


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "common.h"
#include "cmdint.h"
#include "uart.h"
#include "timer.h"


static const char PROGMEM program_version[] = "0.1.1 " __DATE__ " " __TIME__;


/*******************
 * Fehlerinterrupt *
 *******************/

ISR (BADISR_vect)
{
}


/******************************
 * Nicht-Interrupt Background *
 ******************************/

static void z8k1_background (void)
{
}


/********************
 * SLEEP/POWER DOWN *
 ********************/

static void z8k1_sleep (void)
{
#if WITH_IDLESLEEP
  cli ();
  set_sleep_mode (SLEEP_MODE_IDLE);
  sleep_enable ();
  sei ();
  sleep_cpu ();
  sleep_disable ();
#endif
}


static void z8k1_background_sleep (void)
{
  z8k1_background ();
  z8k1_sleep ();
}


/*****************************
 * Kommandoschnittstelle (1) *
 *****************************/

static int z8k1_getc (void)
{
  return uart_getc_wait ();
}


static void z8k1_putc (char c)
{
  uart_putc_wait (c);
}


static void z8k1_puts (const char *s)
{
  while (*s)
  {
    z8k1_putc (*s++);
  }
}


static void z8k1_puts_P (PGM_P s)
{
  char c;

  while ((c = pgm_read_byte (s++)))
  {
    z8k1_putc (c);
  }
}


static void z8k1_bs (void)
{
  z8k1_puts_P (PSTR ("\b \b"));
}


static void z8k1_cr (void)
{
  z8k1_putc ('\r');
}


static void z8k1_lf (void)
{
  z8k1_putc ('\n');
}


static void z8k1_cancel (void)
{
  z8k1_puts_P (PSTR ("#"));
}


static void z8k1_prompt (void)
{
  z8k1_puts_P (PSTR (">"));
}


/*********************
 * konstante Strings *
 *********************/

#define PMN(x) XCAT(x,_s)
#define PMS(x) static const char PROGMEM PMN(x)[] = STR(x)

PMS(stop);
PMS(mi);
PMS(nmi);
PMS(as);
PMS(reset);
PMS(mo);
PMS(bw);
PMS(mreq);
PMS(rw);
PMS(ds);
PMS(busreq);
PMS(ad0);
PMS(ad1);
PMS(ad2);
PMS(ad3);
PMS(ad4);
PMS(ad5);
PMS(ad6);
PMS(ad7);
PMS(ad8);
PMS(ad9);
PMS(ad10);
PMS(ad11);
PMS(ad12);
PMS(ad13);
PMS(ad14);
PMS(ad15);
PMS(sn0);
PMS(sn1);

PMS(help);
PMS(ver);

PMS(z);
PMS(pull);
PMS(l);
PMS(h);
PMS(in);

PMS(run);
PMS(busrel);
PMS(r);
PMS(rb);
PMS(w);
PMS(wb);
PMS(fill);
PMS(fillb);
PMS(s);


/*******
 * E/A *
 ******/


typedef struct
{
  PGM_P name;
  volatile uint8_t *port;
  volatile uint8_t *ddr;
  volatile uint8_t *pin;
  uint8_t m;
} IO;


static const IO ios[] =
{
  { .name = PMN(stop),   .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB0) },
  { .name = PMN(mi),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB1) },
  { .name = PMN(nmi),    .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB2) },
  { .name = PMN(as),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB3) },
  { .name = PMN(reset),  .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB4) },
  { .name = PMN(mo),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB5) },
  { .name = PMN(bw),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB6) },
  { .name = PMN(mreq),   .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB7) },
  { .name = PMN(rw),     .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD2) },
  { .name = PMN(ds),     .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD3) },
  { .name = PMN(busreq), .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD4) },
  { .name = PMN(ad0),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA1) },
  { .name = PMN(ad1),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC0) },
  { .name = PMN(ad2),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC1) },
  { .name = PMN(ad3),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC4) },
  { .name = PMN(ad4),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC7) },
  { .name = PMN(ad5),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC5) },
  { .name = PMN(ad6),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA6) },
  { .name = PMN(ad7),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA4) },
  { .name = PMN(ad8),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA0) },
  { .name = PMN(ad9),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA2) },
  { .name = PMN(ad10),   .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA3) },
  { .name = PMN(ad11),   .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA5) },
  { .name = PMN(ad12),   .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA7) },
  { .name = PMN(ad13),   .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC6) },
  { .name = PMN(ad14),   .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC2) },
  { .name = PMN(ad15),   .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC3) },
  { .name = PMN(sn0),    .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD5) },
  { .name = PMN(sn1),    .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD6) },
  { .name = NULL }
};


#define STOP    &ios[0]
#define MI      &ios[1]
#define NMI     &ios[2]
#define AS      &ios[3]
#define RESET   &ios[4]
#define MO      &ios[5]
#define BW      &ios[6]
#define MREQ    &ios[7]
#define RW      &ios[8]
#define DS      &ios[9]
#define BUSREQ  &ios[10]
#define AD0     &ios[11]
#define AD1     &ios[12]
#define AD2     &ios[13]
#define AD3     &ios[14]
#define AD4     &ios[15]
#define AD5     &ios[16]
#define AD6     &ios[17]
#define AD7     &ios[18]
#define AD8     &ios[19]
#define AD9     &ios[20]
#define AD10    &ios[21]
#define AD11    &ios[22]
#define AD12    &ios[23]
#define AD13    &ios[24]
#define AD14    &ios[25]
#define AD15    &ios[26]
#define SN0     &ios[27]
#define SN1     &ios[28]



static inline void z8k1_Z (const IO *io)
{
  *io->port &= ~io->m;
  *io->ddr  &= ~io->m;
}


static inline void z8k1_PULL (const IO *io)
{
  *io->ddr  &= ~io->m;
  *io->port |= io->m;
}


static inline void z8k1_L (const IO *io)
{
  *io->port &= ~io->m;
  *io->ddr  |= io->m;
}


static inline void z8k1_H (const IO *io)
{
  *io->port |= io->m;
  *io->ddr  |= io->m;
}


static void z8k1_write_data (uint16_t n)
{
  uint8_t pa = 0, pc = 0;
  pa |= (n & _BV( 0)) ? _BV(PA1) : 0;
  pc |= (n & _BV( 1)) ? _BV(PC0) : 0;
  pc |= (n & _BV( 2)) ? _BV(PC1) : 0;
  pc |= (n & _BV( 3)) ? _BV(PC4) : 0;
  pc |= (n & _BV( 4)) ? _BV(PC7) : 0;
  pc |= (n & _BV( 5)) ? _BV(PC5) : 0;
  pa |= (n & _BV( 6)) ? _BV(PA6) : 0;
  pa |= (n & _BV( 7)) ? _BV(PA4) : 0;
  pa |= (n & _BV( 8)) ? _BV(PA0) : 0;
  pa |= (n & _BV( 9)) ? _BV(PA2) : 0;
  pa |= (n & _BV(10)) ? _BV(PA3) : 0;
  pa |= (n & _BV(11)) ? _BV(PA5) : 0;
  pa |= (n & _BV(12)) ? _BV(PA7) : 0;
  pc |= (n & _BV(13)) ? _BV(PC6) : 0;
  pc |= (n & _BV(14)) ? _BV(PC2) : 0;
  pc |= (n & _BV(15)) ? _BV(PC3) : 0;
  PORTA = pa;
  DDRA  = 0b11111111;
  PORTC = pc;
  DDRC  = 0b11111111;
}


static void z8k1_write_data_even (uint8_t n)
{
  z8k1_write_data (n << 8);
}


static void z8k1_write_data_odd (uint8_t n)
{
  z8k1_write_data (n);
}


static inline void z8k1_Z_address ()
{
  PORTA  =  0b00000000;
  DDRA   =  0b00000000;
  PORTC  =  0b00000000;
  DDRC   =  0b00000000;
  PORTD &= ~0b01100000;
  DDRD  &= ~0b01100000;
}


static inline void z8k1_write_address (uint32_t n)
{
  z8k1_write_data (n & 0xFFFF);
  if (n & _BV(16))
  {
    z8k1_H (SN0);
  }
  else
  {
    z8k1_L (SN0);
  };
  if (n & _BV(17))
  {
    z8k1_H (SN1);
  }
  else
  {
    z8k1_L (SN1);
  }
}


static uint16_t z8k1_read_data ()
{
  DDRA  = 0b00000000;
  DDRC  = 0b00000000;
  PORTA = 0b00000000;
  PORTC = 0b00000000;
  const uint8_t pa = PINA;
  const uint8_t pc = PINC;
  uint16_t n = 0;
  n |= (_BV(PA1) & pa) ? _BV( 0) : 0;
  n |= (_BV(PC0) & pc) ? _BV( 1) : 0;
  n |= (_BV(PC1) & pc) ? _BV( 2) : 0;
  n |= (_BV(PC4) & pc) ? _BV( 3) : 0;
  n |= (_BV(PC7) & pc) ? _BV( 4) : 0;
  n |= (_BV(PC5) & pc) ? _BV( 5) : 0;
  n |= (_BV(PA6) & pa) ? _BV( 6) : 0;
  n |= (_BV(PA4) & pa) ? _BV( 7) : 0;
  n |= (_BV(PA0) & pa) ? _BV( 8) : 0;
  n |= (_BV(PA2) & pa) ? _BV( 9) : 0;
  n |= (_BV(PA3) & pa) ? _BV(10) : 0;
  n |= (_BV(PA5) & pa) ? _BV(11) : 0;
  n |= (_BV(PA7) & pa) ? _BV(12) : 0;
  n |= (_BV(PC6) & pc) ? _BV(13) : 0;
  n |= (_BV(PC2) & pc) ? _BV(14) : 0;
  n |= (_BV(PC3) & pc) ? _BV(15) : 0;
  return n;
}


static uint8_t z8k1_read_data_even ()
{
  return z8k1_read_data () >> 8;
}


static uint8_t z8k1_read_data_odd ()
{
  return z8k1_read_data () & 0xFF;
}


static void z8k1_write_word (uint32_t address, uint16_t data)
{
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (1.0);
  z8k1_H (AS);
  z8k1_write_data (data);
  z8k1_L (BW);
  z8k1_L (RW);
  z8k1_L (DS);
  _delay_us (1.0);
  z8k1_H (DS);
  z8k1_H (RW);
}


static void z8k1_write_byte (uint32_t address, uint8_t data)
{
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (1.0);
  z8k1_H (AS);
  if (address & 1)
  {
    z8k1_write_data_odd (data);
  }
  else
  {
    z8k1_write_data_even (data);
  };
  z8k1_H (BW);
  z8k1_L (RW);
  z8k1_L (DS);
  _delay_us (1.0);
  z8k1_H (DS);
  z8k1_H (RW);
}


static uint16_t z8k1_read_word (uint32_t address)
{
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (1.0);
  z8k1_H (AS);
  z8k1_Z_address ();
  z8k1_L (BW);
  z8k1_H (RW);
  z8k1_L (DS);
  const uint16_t x = z8k1_read_data ();
  z8k1_H (DS);
  return x;
}


static uint8_t z8k1_read_byte (uint32_t address)
{
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (1.0);
  z8k1_H (AS);
  z8k1_Z_address ();
  z8k1_H (BW);
  z8k1_H (RW);
  z8k1_L (DS);
  const uint8_t x = (address & 1) ? z8k1_read_data_odd () : z8k1_read_data_even ();
  z8k1_H (DS);
  return x;
}


static void z8k1_begin_read_write ()
{
  z8k1_L (BUSREQ);
  _delay_us (5.0);
  z8k1_PULL (RW);
  z8k1_PULL (BW);
  z8k1_PULL (DS);
  z8k1_PULL (AS);
  z8k1_L (MREQ);
}


static void z8k1_end_read_write ()
{
  z8k1_PULL (RW);
  z8k1_PULL (BW);
  z8k1_PULL (DS);
  z8k1_PULL (AS);
  z8k1_PULL (MREQ);
  z8k1_Z_address ();
  z8k1_Z (RW);
  z8k1_Z (BW);
  z8k1_Z (DS);
  z8k1_Z (AS);
  z8k1_Z (MREQ);
  z8k1_PULL (BUSREQ);
}


/*************
 * Steuerung *
 *************/

/* Pin hochohmig */
static int z8k1_z (int argc, char **argv)
{
  if (argc == 2)
  {
    for (const IO *io = ios; io->name; ++io)
    {
      if (strcmp_P (argv[1], io->name) == 0)
      {
        z8k1_Z (io);
        return 0;
      }
    }
  };
  return -1;
}


/* Pin mit Abschlusswiderstand */
static int z8k1_pull (int argc, char **argv)
{
  if (argc == 2)
  {
    for (const IO *io = ios; io->name; ++io)
    {
      if (strcmp_P (argv[1], io->name) == 0)
      {
        z8k1_PULL (io);
        return 0;
      }
    }
  };
  return -1;
}


/* Pin L */
static int z8k1_l (int argc, char **argv)
{
  if (argc == 2)
  {
    for (const IO *io = ios; io->name; ++io)
    {
      if (strcmp_P (argv[1], io->name) == 0)
      {
        z8k1_L (io);
        return 0;
      }
    }
  };
  return -1;
}


/* Pin H */
static int z8k1_h (int argc, char **argv)
{
  if (argc == 2)
  {
    for (const IO *io = ios; io->name; ++io)
    {
      if (strcmp_P (argv[1], io->name) == 0)
      {
        z8k1_H (io);
        return 0;
      }
    }
  };
  return -1;
}


/* Pin lesen */
static int z8k1_in (int argc, char **argv)
{
  if (argc == 2)
  {
    for (const IO *io = ios; io->name; ++io)
    {
      if (strcmp_P (argv[1], io->name) == 0)
      {
        z8k1_putc (*io->pin & io->m ? '1' : '0');
        return 0;
      }
    }
  };
  return -1;
}


/* Z8001 starten */
static int z8k1_run (int argc, char **argv)
{
  if (argc == 1)
  {
    z8k1_H    (STOP);
    z8k1_PULL (BUSREQ);
    z8k1_H    (RESET);
    return 0;
  };
  return -1;
}


/* Z8001 anhalten */
static int z8k1_stop (int argc, char **argv)
{
  if (argc == 1)
  {
    z8k1_L (STOP);
    return 0;
  };
  return -1;
}


/* Z8001 zurücksetzen */
static int z8k1_reset (int argc, char **argv)
{
  if (argc == 1)
  {
    z8k1_L (RESET);
    return 0;
  };
  return -1;
}


/* Z8001-Bus übernehmen */
static int z8k1_busreq (int argc, char **argv)
{
  if (argc == 1)
  {
    z8k1_L (BUSREQ);
    return 0;
  };
  return -1;
}


/* Z8001-Bus freigeben */
static int z8k1_busrel (int argc, char **argv)
{
  if (argc == 1)
  {
    z8k1_PULL (BUSREQ);
    return 0;
  };
  return -1;
}


/* Worte lesen */
static int z8k1_read_words (int argc, char **argv)
{
  uint32_t address;
  int count;
  char c;

  switch (argc)
  {
    case 2:
      count = 1;
      break;

    case 3:
      if (sscanf_P (argv[2], PSTR ("%i%c"), &count, &c) != 1)
      {
        z8k1_puts_P (PSTR ("illegal count"));
        return -1;
      };
      break;

    default:
      z8k1_puts_P (PSTR ("address {count}"));
      return -1;
  };
  if (sscanf_P (argv[1], PSTR ("%lx%c"), &address, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal address"));
    return -1;
  };

  if ((address & 1) || address > 0x3FFFE || count < 1)
  {
    z8k1_puts_P (PSTR ("range error"));
    return -1;
  };

  z8k1_begin_read_write ();
  while (count-- && address <= 0x3FFFE)
  {
    char buf[64];
    const uint16_t n = z8k1_read_word (address);
    snprintf_P (buf, sizeof buf, PSTR ("%05lX: %04X%6u%7d\r\n"), address, n, n, (int16_t) n);
    z8k1_puts (buf);
    address += 2;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Bytes lesen */
static int z8k1_read_bytes (int argc, char **argv)
{
  uint32_t address;
  int count;
  char c;

  switch (argc)
  {
    case 2:
      count = 1;
      break;

    case 3:
      if (sscanf_P (argv[2], PSTR ("%i%c"), &count, &c) != 1)
      {
        z8k1_puts_P (PSTR ("illegal count"));
        return -1;
      };
      break;

    default:
      z8k1_puts_P (PSTR ("address {count}"));
      return -1;
  };
  if (sscanf_P (argv[1], PSTR ("%lx%c"), &address, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal address"));
    return -1;
  };

  if (address > 0x3FFFF || count < 1)
  {
    z8k1_puts_P (PSTR ("range error"));
    return -1;
  };

  z8k1_begin_read_write ();
  while (count-- && address <= 0x3FFFF)
  {
    char buf[64];
    const int16_t n = (int8_t) z8k1_read_byte (address);

    snprintf_P (buf, sizeof buf, PSTR ("%05lX: %02X%4u%5d%2c\r\n"), address, n, (uint16_t) n, n, isascii (n & 0xFF) ? (int8_t) (n & 0xFF) : (int8_t) '~');
    z8k1_puts (buf);
    address += 1;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Worte schreiben */
static int z8k1_write_words (int argc, char **argv)
{
  uint32_t address;
  char c;

  if (argc < 3)
  {
    z8k1_puts_P (PSTR ("address word {...}"));
    return -1;
  };

  if (sscanf_P (argv[1], PSTR ("%lx%c"), &address, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal address"));
    return -1;
  };

  if ((address & 1) || address > 0x3FFFE)
  {
    z8k1_puts_P (PSTR ("address range error"));
    return -1;
  };

  z8k1_begin_read_write ();
  while (argv[2] && address <= 0x3FFFE)
  {
    uint16_t n;

    if (sscanf_P (argv[2], PSTR ("%x%c"), &n, &c) != 1)
    {
      z8k1_end_read_write ();
      z8k1_puts_P (PSTR ("illegal value"));
      return -1;
    };
    z8k1_write_word (address, n);
    address += 2;
    ++argv;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Bytes schreiben */
static int z8k1_write_bytes (int argc, char **argv)
{
  uint32_t address;
  char c;

  if (argc < 3)
  {
    z8k1_puts_P (PSTR ("address byte {...}"));
    return -1;
  };

  if (sscanf_P (argv[1], PSTR ("%lx%c"), &address, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal address"));
    return -1;
  };

  if (address > 0x3FFFF)
  {
    z8k1_puts_P (PSTR ("address range error"));
    return -1;
  };

  z8k1_begin_read_write ();
  while (argv[2] && address <= 0x3FFFF)
  {
    uint16_t n;

    if (sscanf_P (argv[2], PSTR ("%x%c"), &n, &c) != 1
        ||
        n > 0xFF)
    {
      z8k1_end_read_write ();
      z8k1_puts_P (PSTR ("illegal value"));
      return -1;
    };
    z8k1_write_byte (address, n & 0xFF);
    address += 1;
    ++argv;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Bereich mit Wort füllen */
static int z8k1_fill (int argc, char **argv)
{
  uint32_t address;
  int count;
  uint16_t n;
  char c;

  if (argc != 4)
  {
    z8k1_puts_P (PSTR ("address count value"));
    return -1;
  };

  if (sscanf_P (argv[1], PSTR ("%lx%c"), &address, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal address"));
    return -1;
  };

  if (sscanf_P (argv[2], PSTR ("%i%c"), &count, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal count"));
    return -1;
  };

  if (sscanf_P (argv[3], PSTR ("%x%c"), &n, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal value"));
    return -1;
  };

  if ((address & 1) || address > 0x3FFFE)
  {
    z8k1_puts_P (PSTR ("address range error"));
    return -1;
  };

  z8k1_begin_read_write ();
  while (address <= 0x3FFFE)
  {
    z8k1_write_word (address, n);
    address += 2;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Bereich mit Byte füllen */
static int z8k1_fillb (int argc, char **argv)
{
  uint32_t address;
  int count;
  uint16_t n;
  char c;

  if (argc != 4)
  {
    z8k1_puts_P (PSTR ("address count value"));
    return -1;
  };

  if (sscanf_P (argv[1], PSTR ("%lx%c"), &address, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal address"));
    return -1;
  };

  if (sscanf_P (argv[2], PSTR ("%i%c"), &count, &c) != 1)
  {
    z8k1_puts_P (PSTR ("illegal count"));
    return -1;
  };

  if (sscanf_P (argv[3], PSTR ("%x%c"), &n, &c) != 1
      ||
      n > 0xFF)
  {
    z8k1_puts_P (PSTR ("illegal value"));
    return -1;
  };

  if (address > 0x3FFFF)
  {
    z8k1_puts_P (PSTR ("address range error"));
    return -1;
  };

  z8k1_begin_read_write ();
  while (address <= 0x3FFFF)
  {
    z8k1_write_byte (address, n);
    address += 1;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Leitungszustand */
static int z8k1_state (int argc, char **argv)
{
  if (argc == 1)
  {
    for (const IO *io = ios; io->name; ++io)
    {
      z8k1_puts_P (io->name);
      z8k1_putc ('=');
      z8k1_putc (*io->pin & io->m ? '1' : '0');
      z8k1_putc (' ');
    }
    return 0;
  };
  return -1;
}


/*****************************
 * Kommandoschnittstelle (2) *
 *****************************/

static int z8k1_help (int argc, char **argv);
static int z8k1_version (int argc, char **argv);


static struct CMD
{
  PGM_P name;
  int (*func) (int argc, char **argv);
} const cmds[] PROGMEM =
{
  { .name = PMN(z),           .func = z8k1_z           },
  { .name = PMN(pull),        .func = z8k1_pull        },
  { .name = PMN(l),           .func = z8k1_l           },
  { .name = PMN(h),           .func = z8k1_h           },
  { .name = PMN(in),          .func = z8k1_in          },
  { .name = PMN(run),         .func = z8k1_run         },
  { .name = PMN(stop),        .func = z8k1_stop        },
  { .name = PMN(reset),       .func = z8k1_reset       },
  { .name = PMN(busreq),      .func = z8k1_busreq      },
  { .name = PMN(busrel),      .func = z8k1_busrel      },
  { .name = PMN(r),           .func = z8k1_read_words  },
  { .name = PMN(rb),          .func = z8k1_read_bytes  },
  { .name = PMN(w),           .func = z8k1_write_words },
  { .name = PMN(wb),          .func = z8k1_write_bytes },
  { .name = PMN(fill),        .func = z8k1_fill        },
  { .name = PMN(fillb),       .func = z8k1_fillb       },
  { .name = PMN(s),           .func = z8k1_state       },
  { .name = PMN(help),        .func = z8k1_help        },
  { .name = PMN(ver),         .func = z8k1_version     },
};


static int z8k1_help (int argc, char **argv)
{
  if (argc == 1)
  {
    for (int i = -1; ++i < LENGTH (cmds);)
    {
      PGM_P name = (PGM_P) pgm_read_word (&cmds[i].name);
      z8k1_puts_P (name);
      z8k1_putc (' ');
    };
    return 0;
  };
  return -1;
}


static int z8k1_version (int argc, char **argv)
{
  if (argc == 1)
  {
    z8k1_puts_P (program_version);
    return 0;
  };
  return -1;
}


static int z8k1_line (void)
{
  return 0;
}


static int z8k1_interp (int argc, char **argv)
{
  PGM_P name;
  int (*func) (int, char **);

  if (argc >= 1)
  {
    for (int i = -1; ++i < LENGTH (cmds);)
    {
      name = (PGM_P) pgm_read_word (&cmds[i].name);
      if (strcmp_P (argv[0], name) == 0)
      {
        func = (int (*) (int, char **)) pgm_read_word (&cmds[i].func);
        if (func (argc, argv) == 0)
        {
          z8k1_puts_P (PSTR ("\r\n"));
          return 0;
        };
        break;
      }
    }
  };
  z8k1_puts_P (PSTR ("?\r\n"));
  return 0;
}


/*******************
 * Initialisierung *
 *******************/

static void init (void)
{
  cli ();
  z8k1_L (RESET);
  z8k1_end_read_write ();
  uart_getc_sleep = z8k1_background_sleep;
  uart_putc_sleep = z8k1_background_sleep;
  uart_init ();
  timer_init ();
  sei ();
}


static void signon_message (void)
{
  z8k1_puts_P (PSTR ("\r\n\n\nz8k1 atmega32 "));
  z8k1_puts_P (program_version);
  z8k1_puts_P (PSTR ("\r\n\n"));
}


/*************************
 * Hauptprogrammschleife *
 *************************/

int main (void)
{
  static const struct CmdIntCallback cic =
  {
    .getc_f   = z8k1_getc,
    .putc_f   = z8k1_putc,
    .bs_f     = z8k1_bs,
    .cr_f     = z8k1_cr,
    .lf_f     = z8k1_lf,
    .cancel_f = z8k1_cancel,
    .prompt_f = z8k1_prompt,
    .interp_f = z8k1_interp,
    .line_f   = z8k1_line
  };

  init ();
  signon_message ();

  for (;;)
  {
    cmdint (&cic);
  };
  return 0;
}
