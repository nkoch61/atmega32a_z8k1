/*
 * $Header: /home/playground/src/atmega32/z8k1/main.c,v 4d3f4103818c 2021/05/25 18:58:49 nkoch $
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "common.h"
#include "cmdint.h"
#include "ihex.h"
#include "uart.h"
#include "timer.h"
#include "timerint.h"
#include "interrupt2.h"

#include "zbios.h"
#include "z8k_fcw.h"
#include "disasm.h"


static const __flash char program_version[] = "1.10.1 " __DATE__ " " __TIME__;


static int z8k1_getc (void);
static inline void z8k1_flush ();
static void z8k1_putc (char c);
static void z8k1_noputs (const char *s);
static void z8k1_noputs_P (const __flash char *s);
static void z8k1_puts (const char *s);
static void z8k1_puts_P (const __flash char *s);
static void z8k1_bs (void);
static void z8k1_cr (void);
static void z8k1_lf (void);
static void z8k1_crlf (void);
static void z8k1_putsln (const char *s);
static void z8k1_putsln_P (const __flash char *s);
static void z8k1_green (void);
static void z8k1_white (void);
static void z8k1_inverse (void);
static void z8k1_normal (void);
static void z8k1_cancel (void);
static void z8k1_prompt (void);
static void z8k1_nmi_pulse (void);
static uint16_t z8k1_read_word (uint32_t address);
static void z8k1_write_word (uint32_t address, uint16_t data);
static void z8k1_end_read_write (void);
static void zbios_host_interface_background (void);
static void zbios_regdump ();

static volatile long millisecs, ticount, mocount, nmicount, badcount;
static bool host_interface_active, in_host_interface, owning_bus;
static uint8_t ctrlc, ctrld;

static uint32_t last_address;
static int last_count;

struct Breakpoint
{
  uint32_t addr;
  uint16_t opcode;
  bool perm;
};

#define PERM_BP true
#define TEMP_BP false
#define SET_BREAKPOINT(addr)    do { z8k1_write_word (addr, 0x7f01); } while (false)
#define IS_BREAKPOINT(addr)     (z8k1_read_word (addr) == 0x7f01)

struct Breakpoint breakpoints[16];

struct BreakInfo
{
  uint16_t next, dest;
};

struct z8k_regs regs[2];
bool compare_regs;


/*******************
 * Fehlerinterrupt *
 *******************/

ISR (BADISR_vect)
{
  ++badcount;
}

/*******************
 * Interrupt2 / MO *
 *******************/

static void z8k1_mo (void)
{
  ++mocount;
}


/***********************
 * 20ms Timerinterrupt *
 ***********************/

static void timer_20ms (void)
{
  ++ticount;
  millisecs += 20;
}


/**********************
 * Millisekundentimer *
 **********************/

static long now (void)
{
  long ms;

  do
  {
    ms = millisecs;
  }
  while (ms != millisecs);
  return ms;
}


/***************
 * Zeitmessung *
 ***************/

static void mark (long *since)
{
  *since = now ();
}


static long elapsed (long *since)
{
  return now () - *since;
}


/********************
 * SLEEP/POWER DOWN *
 ********************/

static void sleep (void)
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


static void background_sleep (void)
{
  zbios_host_interface_background ();
  sleep ();
}


/************
 * Diverses *
 ***********/

static inline uint32_t z8k1_segmented_address (uint16_t seg, uint16_t off)
{
  return ((uint32_t) ((seg >> 8) & 0x7f) << 16) + (uint32_t) off;
}


static bool zbios_loaded ()
{
  return z8k1_read_word (ZBIOS_ID_ADDRESS+ZBIOS_ID(zb)) == ('z' << 8) + 'b';
}


static uint16_t zbios_read_pcseg (void)
{
  return regs[0].fcw_pc.pc.seg;
}


static uint16_t zbios_read_pcoff (void)
{
  return regs[0].fcw_pc.pc.off;
}


static uint32_t zbios_read_pcaddr (void)
{
  return z8k1_segmented_address (zbios_read_pcseg (), regs[0].fcw_pc.pc.off);
}


static uint32_t zbios_read_prev_pcaddr ()
{
  return z8k1_segmented_address (zbios_read_pcseg (), regs[1].fcw_pc.pc.off);
}


static uint16_t zbios_read_fcw (void)
{
  return regs[0].fcw_pc.fcw;
}


static bool zbios_normalmode (void)
{
  return zbios_loaded () && !(zbios_read_fcw () & FCW_SYSTEM);
}


static bool zbios_segmentedmode (void)
{
  return !zbios_loaded () || (zbios_read_fcw () & FCW_SEGMENTED);
}


static void zbios_write_fcw (uint16_t fcw)
{
  z8k1_write_word (ZBIOS(REGS.fcw_pc.fcw), fcw);
}


static void zbios_fcw_set (uint16_t mask)
{
  zbios_write_fcw (zbios_read_fcw () | mask);
}


static void zbios_fcw_clr (uint16_t mask)
{
  zbios_write_fcw (zbios_read_fcw () & ~mask);
}


static void zbios_read_regs_ (struct z8k_regs *r)
{
  uint16_t *p = (uint16_t *) r;

  for (uint32_t addr = ZBIOS(regs[0]);
       addr < ZBIOS(regs[1]);
       addr += 2)
  {
    *p++ = z8k1_read_word (addr);
  };
}


static void zbios_read_regs (void)
{
  zbios_read_regs_ (&regs[0]);
  if (zbios_normalmode ())
  {
    regs[0].regs[14] = regs[0].nsp.seg;
    regs[0].regs[15] = regs[0].nsp.off;
  }
}


/*******
 * E/A *
 ******/


typedef struct
{
  const __flash char *name;
  volatile uint8_t *port;
  volatile uint8_t *ddr;
  volatile uint8_t *pin;
  uint8_t m;
} IO;


static const __flash IO ios[] =
{
  { .name = FSTR("stop"),   .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB0) },
  { .name = FSTR("mi"),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB1) },
  { .name = FSTR("mo"),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB2) },
  { .name = FSTR("as"),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB3) },
  { .name = FSTR("reset"),  .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB4) },
  { .name = FSTR("nmi"),    .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB5) },
  { .name = FSTR("bw"),     .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB6) },
  { .name = FSTR("mreq"),   .port = &PORTB, .ddr = &DDRB, .pin = &PINB, .m = _BV(PB7) },
  { .name = FSTR("rw"),     .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD2) },
  { .name = FSTR("ds"),     .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD3) },
  { .name = FSTR("busreq"), .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD4) },
  { .name = FSTR("ad0"),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA1) },
  { .name = FSTR("ad1"),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC0) },
  { .name = FSTR("ad2"),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC1) },
  { .name = FSTR("ad3"),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC4) },
  { .name = FSTR("ad4"),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC7) },
  { .name = FSTR("ad5"),    .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC5) },
  { .name = FSTR("ad6"),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA6) },
  { .name = FSTR("ad7"),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA4) },
  { .name = FSTR("ad8"),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA0) },
  { .name = FSTR("ad9"),    .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA2) },
  { .name = FSTR("ad10"),   .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA3) },
  { .name = FSTR("ad11"),   .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA5) },
  { .name = FSTR("ad12"),   .port = &PORTA, .ddr = &DDRA, .pin = &PINA, .m = _BV(PA7) },
  { .name = FSTR("ad13"),   .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC6) },
  { .name = FSTR("ad14"),   .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC2) },
  { .name = FSTR("ad15"),   .port = &PORTC, .ddr = &DDRC, .pin = &PINC, .m = _BV(PC3) },
  { .name = FSTR("sn0"),    .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD5) },
  { .name = FSTR("sn1"),    .port = &PORTD, .ddr = &DDRD, .pin = &PIND, .m = _BV(PD6) },
  { .name = NULL }
};


#define STOP    (&ios[0])
#define MI      (&ios[1])
#define MO      (&ios[2])
#define AS      (&ios[3])
#define RESET   (&ios[4])
#define NMI     (&ios[5])
#define BW      (&ios[6])
#define MREQ    (&ios[7])
#define RW      (&ios[8])
#define DS      (&ios[9])
#define BUSREQ  (&ios[10])
#define AD0     (&ios[11])
#define AD1     (&ios[12])
#define AD2     (&ios[13])
#define AD3     (&ios[14])
#define AD4     (&ios[15])
#define AD5     (&ios[16])
#define AD6     (&ios[17])
#define AD7     (&ios[18])
#define AD8     (&ios[19])
#define AD9     (&ios[20])
#define AD10    (&ios[21])
#define AD11    (&ios[22])
#define AD12    (&ios[23])
#define AD13    (&ios[24])
#define AD14    (&ios[25])
#define AD15    (&ios[26])
#define SN0     (&ios[27])
#define SN1     (&ios[28])



static inline void z8k1_Z (const __flash IO *io)
{
  *io->port &= ~io->m;
  *io->ddr  &= ~io->m;
}


static inline void z8k1_PULL (const __flash IO *io)
{
  *io->port |= io->m;
  *io->ddr  &= ~io->m;
}


static inline void z8k1_L (const __flash IO *io)
{
  *io->port &= ~io->m;
  *io->ddr  |= io->m;
}


static inline void z8k1_H (const __flash IO *io)
{
  *io->port |= io->m;
  *io->ddr  |= io->m;
}


static inline bool z8k1_IN (const __flash IO *io)
{
  return !!(*io->pin & io->m);
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


static void z8k1_Z_address (void)
{
  PORTA  =  0b00000000;
  DDRA   =  0b00000000;
  PORTC  =  0b00000000;
  DDRC   =  0b00000000;
  PORTD &= ~0b01100000;
  DDRD  &= ~0b01100000;
}


static void z8k1_write_address (uint32_t n)
{
  z8k1_write_data (n & 0xFFFF);
  if (n & 0x10000)
  {
    z8k1_H (SN0);
  }
  else
  {
    z8k1_L (SN0);
  };
  if (n & 0x20000)
  {
    z8k1_H (SN1);
  }
  else
  {
    z8k1_L (SN1);
  }
}


static uint16_t z8k1_read_data (void)
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


static uint8_t z8k1_read_data_even (void)
{
  return z8k1_read_data () >> 8;
}


static uint8_t z8k1_read_data_odd (void)
{
  return z8k1_read_data () & 0xFF;
}


static void z8k1_begin_read_write (void)
{
  if (!owning_bus)
  {
    z8k1_L (BUSREQ);
    _delay_us (5.0);
    z8k1_PULL (RW);
    z8k1_PULL (BW);
    z8k1_PULL (DS);
    z8k1_PULL (AS);
    z8k1_L (MREQ);
    owning_bus = true;
  }
}


static void z8k1_end_read_write_ (void)
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


static void z8k1_end_read_write (void)
{
  if (owning_bus)
  {
    z8k1_end_read_write_ ();
    owning_bus = false;
  }
}


static void z8k1_write_word (uint32_t address, uint16_t data)
{
  z8k1_begin_read_write ();
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (0.1);
  z8k1_H (AS);
  z8k1_write_data (data);
  z8k1_L (BW);
  z8k1_L (RW);
  z8k1_L (DS);
  _delay_us (0.1);
  z8k1_H (DS);
  z8k1_H (RW);
}


static void z8k1_write_byte (uint32_t address, uint8_t data)
{
  z8k1_begin_read_write ();
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (0.1);
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
  _delay_us (0.1);
  z8k1_H (DS);
  z8k1_H (RW);
}


static uint16_t z8k1_read_word (uint32_t address)
{
  z8k1_begin_read_write ();
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (0.1);
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
  z8k1_begin_read_write ();
  z8k1_write_address (address);
  z8k1_L (AS);
  _delay_us (0.1);
  z8k1_H (AS);
  z8k1_Z_address ();
  z8k1_H (BW);
  z8k1_H (RW);
  z8k1_L (DS);
  const uint8_t x = (address & 1) ? z8k1_read_data_odd () : z8k1_read_data_even ();
  z8k1_H (DS);
  return x;
}


static void z8k1_nmi_pulse (void)
{
  z8k1_end_read_write ();
  z8k1_L (NMI);
  _delay_us (0.2);
  z8k1_H (NMI);
  z8k1_PULL (STOP);
  ++nmicount;
}


/***************
 * Breakpoints *
 **************/


bool zbios_add_breakpoint (uint32_t addr, bool perm)
{
  if (addr & 1)
  {
    return false;
  };

  struct Breakpoint *bp_free = NULL;
  uint8_t free = 0;

  for (struct Breakpoint *bp = breakpoints; bp != &breakpoints[LENGTH (breakpoints)]; ++bp)
  {
    if (bp->addr == addr)
    {
      if (perm)
      {
        bp->perm = PERM_BP;
      };
      return true;
    }
    else if (!bp->addr)
    {
      bp_free = bp;
      ++free;
    }
  };

  if (free == 0 || (perm && free <= 2))
  {
    return false;
  };

  bp_free->addr = addr;
  bp_free->perm = perm;
  return true;
}


void zbios_clear_breakpoint (uint32_t addr)
{
  for (struct Breakpoint *bp = breakpoints; bp != &breakpoints[LENGTH (breakpoints)]; ++bp)
  {
    if (bp->addr == addr)
    {
      bp->addr = 0;
      return;
    }
  }
}


void zbios_clear_breakpoints (bool perm)
{
  for (struct Breakpoint *bp = breakpoints; bp != &breakpoints[LENGTH (breakpoints)]; ++bp)
  {
    if (bp->perm == perm)
    {
      bp->addr = 0;
    }
  }
}


void zbios_activate_breakpoints (uint32_t pc)
{
  for (struct Breakpoint *bp = breakpoints; bp != &breakpoints[LENGTH (breakpoints)]; ++bp)
  {
    if (bp->addr && bp->addr != pc)
    {
      bp->opcode = z8k1_read_word (bp->addr);
      SET_BREAKPOINT(bp->addr);
    }
  }
}


void zbios_deactivate_and_clear_breakpoints ()
{
  for (struct Breakpoint *bp = breakpoints; bp != &breakpoints[LENGTH (breakpoints)]; ++bp)
  {
    if (bp->addr && IS_BREAKPOINT(bp->addr))
    {
      z8k1_write_word (bp->addr, bp->opcode);
    }
  };
  z8k1_end_read_write ();
  zbios_clear_breakpoints (TEMP_BP);
}


/* Hostkommunikation */

static bool zbios_hostmessage (uint16_t message)
{
  if (z8k1_read_word (ZBIOS(host_message)) != HC_MSG_NOP)
  {
    z8k1_putsln_P (PSTR("MSG !CLR"));
    z8k1_nmi_pulse ();
    return false;
  };
  z8k1_write_word (ZBIOS(host_message), message);
  z8k1_nmi_pulse ();
  long since;
  mark (&since);
  for (bool done = false; !done;)
  {
    background_sleep ();
    if (elapsed (&since) < 25)
    {
      continue;
    };
    if (elapsed (&since) > 1000)
    {
      z8k1_putsln_P (PSTR("MSG !PROC"));
      return false;
    };
    done = z8k1_read_word (ZBIOS(host_message)) == HC_MSG_NOP;
    z8k1_end_read_write ();
  };
  return true;
}


static void zbios_hostmode ()
{
  zbios_read_regs ();
  zbios_activate_breakpoints (zbios_read_pcaddr ());
  if (z8k1_read_word (ZBIOS(rc_halted)) != z8k1_read_word (ZBIOS(rc_cont)))
  {
    if (!zbios_hostmessage (HC_MSG_CONT))
    {
      zbios_deactivate_and_clear_breakpoints ();
      return;
    }
  };
  regs[1] = regs[0];
  compare_regs = true;
  z8k1_end_read_write ();
  ctrlc = 0;
  ctrld = 0;
  host_interface_active = true;
  z8k1_white ();
  do
  {
    background_sleep ();
  }
  while (host_interface_active);
  z8k1_green ();
  z8k1_flush ();
  zbios_deactivate_and_clear_breakpoints ();
  zbios_regdump ();
}


/*************************
 * gemeinsame Funktionen *
 ************************/

static bool z8k1_pinfunc (const char *name, void (*pinfunc) (const __flash IO *io))
{
  for (const __flash IO *io = ios; io->name; ++io)
  {
    if (strcmp_P (name, io->name) == 0)
    {
      pinfunc (io);
      return true;
    }
  };
  return false;
}


static bool z8k1_lastcount (const char *arg)
{
  char c;

  return sscanf_P (arg, PSTR ("%i%c"), &last_count, &c) == 1;
}


static bool z8k1_input_address (const char *arg, uint32_t *address)
{
  char c;

  if (!zbios_segmentedmode ())
  {
    uint16_t off;
    if (sscanf_P (arg, PSTR ("%hx%c"), &off, &c) == 1)
    {
      *address = z8k1_segmented_address (zbios_read_pcseg (), off);
      z8k1_end_read_write ();
      return true;
    }
  };
  return sscanf_P (arg, PSTR ("%lx%c"), address, &c) == 1;
}


static bool z8k1_input_last_address (const char *arg)
{
  return z8k1_input_address (arg, &last_address);
}


bool valid_byte_address (uint32_t address)
{
  return address <= 0x3FFFF;
}


bool valid_word_address (uint32_t address)
{
  return (address & 1) == 0 && address <= 0x3FFFE;
}


/*************
 * Steuerung *
 *************/

/* Pin hochohmig */
static int8_t z8k1_z (int8_t argc, char **argv)
{
  if (argc == 2 && z8k1_pinfunc (argv[1], z8k1_Z))
  {
    return 0;
  };
  return -1;
}


/* Pin mit Abschlusswiderstand */
static int8_t z8k1_pull (int8_t argc, char **argv)
{
  if (argc == 2 && z8k1_pinfunc (argv[1], z8k1_PULL))
  {
    return 0;
  };
  return -1;
}


/* Pin L */
static int8_t z8k1_l (int8_t argc, char **argv)
{
  if (argc == 2 && z8k1_pinfunc (argv[1], z8k1_L))
  {
    return 0;
  };
  return -1;
}


/* Pin H */
static int8_t z8k1_h (int8_t argc, char **argv)
{
  if (argc == 2 && z8k1_pinfunc (argv[1], z8k1_H))
  {
    return 0;
  };
  return -1;
}


/* Pin lesen */
static int8_t z8k1_in (int8_t argc, char **argv)
{
  if (argc == 2)
  {
    for (const __flash IO *io = ios; io->name; ++io)
    {
      if (strcmp_P (argv[1], io->name) == 0)
      {
        z8k1_putc (z8k1_IN(io) ? '1' : '0');
        return 0;
      }
    }
  };
  return -1;
}


/* Z8001 starten */
static int8_t z8k1_run (int8_t argc, char **argv)
{
  z8k1_PULL (STOP);
  z8k1_PULL (BUSREQ);
  z8k1_H (RESET);
  return 0;
}


/* Z8001 zurücksetzen */
static int8_t z8k1_reset (int8_t argc, char **argv)
{
  z8k1_L (RESET);
  z8k1_PULL (STOP);
  z8k1_PULL (BUSREQ);
  mocount = 0;
  nmicount = 0;
  return 0;
}


/* NMI */
static int8_t z8k1_nmi (int8_t argc, char **argv)
{
  z8k1_nmi_pulse ();
  return 0;
}


/* Worte lesen */
static int8_t z8k1_read_words (int8_t argc, char **argv)
{
  switch (argc)
  {
    case 1:
      break;

    case 2:
      last_count = 1;
      break;

    case 3:
      if (!z8k1_lastcount (argv[2]))
      {
        return -1;
      };
      break;

    default:
      return -1;
  };

  if (argc > 1
      &&
      (!z8k1_input_last_address (argv[1])
       ||
       !valid_word_address (last_address)))
  {
    return -1;
  };

  if (last_count < 1)
  {
    last_count = 1;
  };

  for (int c = last_count; c-- && last_address <= 0x3FFFE;)
  {
    char buf[64];
    const uint16_t n = z8k1_read_word (last_address);
    snprintf_P (buf, sizeof buf, PSTR ("%05lx: %04x%6u%7d"),
                                 last_address,
                                 n,
                                 n,
                                 (int16_t) n);
    z8k1_putsln (buf);
    last_address += 2;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Bytes lesen */
static int8_t z8k1_read_bytes (int8_t argc, char **argv)
{
  switch (argc)
  {
    case 1:
      break;

    case 2:
      last_count = 1;
      break;

    case 3:
      if (!z8k1_lastcount (argv[2]))
      {
        return -1;
      };
      break;

    default:
      return -1;
  };

  if (argc > 1
      &&
      (!z8k1_input_last_address (argv[1])
       ||
       !valid_byte_address (last_address)))
  {
    return -1;
  };

  if (last_count < 1)
  {
    last_count = 1;
  };

  for (int c = last_count; c-- && last_address <= 0x3FFFF;)
  {
    char buf[64];
    const uint8_t n = z8k1_read_byte (last_address);

    snprintf_P (buf, sizeof buf, PSTR ("%05lx: %02x%4u%5d%2c"),
                                 last_address,
                                 n,
                                 n,
                                 * (int8_t *) &n,
                                 isascii (n) && isprint (n) ? n : '~');
    z8k1_putsln (buf);
    last_address += 1;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Disassembler */
static int disasm (uint32_t address, char *buf, struct BreakInfo *info)
{
  char *p = buf + sprintf_P (buf, PSTR("%05lx: "), address);
  const int index = deasm (address, p);
  const int size = deasm_getnext (index);
  if (info)
  {
    uint8_t regno;

    info->next = address+size;
    switch (index)
    {
      case IDX_CALL_DA:
      case IDX_CALR:
      case IDX_JPCC_DA:
      case IDX_JP_DA:
      case IDX_JR:
      case IDX_JRCC:
      case IDX_DJNZ:
      case IDX_DBJNZ:
        sscanf_P (p+strlen (p)-4, PSTR("%x"), &info->dest);
        if (index == IDX_JP_DA  /* jp addr */
            ||
            index == IDX_JR)    /* jr addr */
        {
          info->next = 1;       /* nur ein Breakpoint */
        };
        break;

      case IDX_CALL_IR:
      case IDX_JP_IR:
      case IDX_JPCC_IR:
        sscanf_P (p, PSTR("%*[^@]@r%hhu"), &regno);
        info->dest = regs[0].regs[regno];
        if (index == IDX_JP_IR) /* jp @reg */
        {
          info->next = 1;       /* nur ein Breakpoint */
        };
        break;

      case IDX_CALL_X:
      case IDX_JP_X:
      case IDX_JPCC_X:
        if (sscanf_P (p, PSTR("%*[^ ] %x(r%hhu"), &info->dest, &regno) != 2)
        {
          sscanf_P (p, PSTR("%*[^,],%x(r%hhu"), &info->dest, &regno);
        };
        info->dest += regs[0].regs[regno];
        if (index == IDX_JP_X)  /* jp addr(reg) */
        {
          info->next = 1;       /* nur ein Breakpoint */
        };
        break;

      case IDX_RET:
      case IDX_RETCC:
        info->dest = z8k1_read_word (z8k1_segmented_address (zbios_read_pcseg (), regs[0].regs[15]));
        if (index == IDX_RET)   /* ret */
        {
          info->next = 1;       /* nur ein Breakpoint */
        };
        break;

      default:
        info->dest = 1;       /* keine Verzweigung: nur ein Breakpoint */
        break;
    }
  };

  z8k1_end_read_write ();
  return size;
}


static int8_t z8k1_disassemble (int8_t argc, char **argv)
{
  switch (argc)
  {
    case 1:
      break;

    case 2:
      last_count = 1;
      break;

    case 3:
      if (!z8k1_lastcount (argv[2]))
      {
        return -1;
      };
      break;

    default:
      return -1;
  };
  if (argc > 1
      &&
      (!z8k1_input_last_address (argv[1])
       ||
       !valid_word_address (last_address)))
  {
    return -1;
  };

  if (last_count < 1)
  {
    last_count = 1;
  };

  for (int c = last_count; c-- && last_address <= 0x3FFFE;)
  {
    char buf[64];
    last_address += disasm (last_address, buf, NULL);
    z8k1_putsln (buf);
  };
  return 0;
}


/* Worte schreiben */
static int8_t z8k1_write_words (int8_t argc, char **argv)
{
  if (argc < 3)
  {
    return -1;
  };

  if (!z8k1_input_last_address (argv[1])
      ||
      !valid_word_address (last_address))
  {
    return -1;
  };

  while (argv[2] && last_address <= 0x3FFFE)
  {
    uint16_t n;
    char c;

    if (sscanf_P (argv[2], PSTR ("%i%c"), &n, &c) != 1)
    {
      z8k1_end_read_write ();
      return -1;
    };
    z8k1_write_word (last_address, n);
    last_address += 2;
    ++argv;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Bytes schreiben */
static int8_t z8k1_write_bytes (int8_t argc, char **argv)
{
  if (argc < 3)
  {
    return -1;
  };

  if (!z8k1_input_last_address (argv[1])
      ||
      !valid_byte_address (last_address))
  {
    return -1;
  };

  while (argv[2] && last_address <= 0x3FFFF)
  {
    uint16_t n;
    char c;

    if (sscanf_P (argv[2], PSTR ("%i%c"), &n, &c) != 1
        ||
        n > 0xFF)
    {
      z8k1_end_read_write ();
      return -1;
    };
    z8k1_write_byte (last_address, n & 0xFF);
    last_address += 1;
    ++argv;
  };
  z8k1_end_read_write ();
  return 0;
}


/* Leitungszustand */
static int8_t z8k1_stat (int8_t argc, char **argv)
{
  for (const __flash IO *io = ios; io->name; ++io)
  {
    char buf[32];
    snprintf_P (buf, sizeof buf, PSTR("%6S=%u"),
                                 io->name,
                                 z8k1_IN (io));
    z8k1_putsln (buf);
  };
  return 0;
}


/* Hilfsfunktionen Intel-Hex-Datei */
static size_t z8k1_ihex_getline (char *buf, size_t bufsize)
{
  for (char *p = buf; p < &buf[bufsize-1];)
  {
    uint8_t c;
    switch ((c = z8k1_getc ()))
    {
      case '\x03':
        return 0;

      case '\r':
      case '\n':
        if (p != buf)
        {
          *p = '\0';
          return p - buf;
        };
        continue;

      default:
        *p++ = c;
        continue;
    }
  };
  return 0;
}


static void z8k1_ihex_eat_chars (void)
{
  z8k1_putsln_P (PSTR ("^C:"));
  for (;;)
  {
    switch (z8k1_getc ())
    {
      case '\x03':
        return;

      default:
        continue;
    }
  }
}


/* Intel-Hex-Datei laden */
static int8_t z8k1_ihexload (int8_t argc, char **argv)
{
  if (argc == 1 || argc == 2)
  {
    ihex_interface interface =
    {
      .getline          = z8k1_ihex_getline,
      .debug            = z8k1_noputs,
      .debug_P          = z8k1_noputs_P,
      .error            = z8k1_putsln,
      .error_P          = z8k1_putsln_P,
      .write_byte       = z8k1_write_byte,
      .write_word       = z8k1_write_word
    };
    uint32_t load_offset = 0, start_address = 0;
    char c;
    if (argc == 2)
    {
      if (sscanf_P (argv[1], PSTR ("%lx%c"), &load_offset, &c) != 1)
      {
        return -1;
      }
    };
    const bool ok = ihex_loader (&interface, load_offset, &start_address) == 0;
    z8k1_end_read_write ();
    if (!ok)
    {
      z8k1_ihex_eat_chars ();
      return -1;
    };
    char buf[32];
    snprintf_P (buf, sizeof buf, PSTR("start %05lx"), load_offset+start_address);
    z8k1_putsln (buf);
    return 0;
  };
  return -1;
}


/* Speichertest */
#if 0
static int8_t z8k1_memtest (int8_t argc, char **argv)
{
  ctrlc = 0;
  srand (0xDEAD);
  for (uint32_t address = 0; address <= 0x3FFFE; address += 2)
  {
    z8k1_write_word (address, rand ());
  };
  srand (0xDEAD);
  for (uint32_t address = 0; address <= 0x3FFFE; address += 2)
  {
    if (z8k1_read_word (address) != rand ())
    {
      char buf[32];
      snprintf_P (buf, sizeof buf, PSTR("? %05lx"), address);
      z8k1_putsln (buf);
      if (ctrlc)
      {
        break;
      }
    }
  };
  z8k1_end_read_write ();
  return 0;
}
#endif


/* Interruptstatistik */
static int8_t z8k1_istat (int8_t argc, char **argv)
{
  char buf[64];
  snprintf_P (buf, sizeof buf, PSTR("up=%lums ti=%lu mo=%lu nmi=%lu bad=%lu"),
                               millisecs,
                               ticount,
                               mocount,
                               nmicount,
                               badcount);
  z8k1_putsln (buf);
  return 0;
}


static bool assert_zbios_loaded ()
{
  const bool ok = zbios_loaded ();
  z8k1_end_read_write ();
  if (!ok)
  {
    z8k1_putsln_P (PSTR("zbios?"));
  };
  return ok;
}


/* Registerinhalte lesen und anzeigen */
typedef struct
{
  const char name[2][4];
  uint16_t mask;
} Flags;


static const __flash Flags flags[] =
{
  {.name = {"sg",  "ns" },   FCW_SEGMENTED  },
  {.name = {"sy",  "no" },   FCW_SYSTEM     },
  {.name = {"epa", ""   },   FCW_EPA        },
  {.name = {"vie", ""   },   FCW_VIE        },
  {.name = {"nie", ""   },   FCW_NVIE       },
  {.name = {"c",   "nc" },   FCW_C          },
  {.name = {"z",   "nz" },   FCW_Z          },
  {.name = {"neg", "pos"},   FCW_S          },
  {.name = {"pv",  ""   },   FCW_PV         },
  {.name = {"da",  ""   },   FCW_DA         },
  {.name = {"h",   ""   },   FCW_H          },
};


static void zbios_regdump ()
{
  char buf[128];

  zbios_read_regs ();
  for (int8_t i = 0; i  < 16; i += 4)
  {
    for (int8_t j = -1; ++j < 4;)
    {
      const int8_t rno = i+j;
      union
      {
        uint16_t u;
        int16_t i;
      }
      reg;
      reg.u = regs[0].regs[rno];

      if (j)
      {
        z8k1_puts_P (PSTR("  "));
      };
      snprintf_P (buf, sizeof buf, PSTR("r%-2u: "),
                                   i+j);
      z8k1_puts (buf);

      if (compare_regs && regs[1].regs[rno] != reg.u)
      {
        z8k1_inverse ();
      };
      snprintf_P (buf, sizeof buf, PSTR("%04x%6u%7d"),
                                   reg.u,
                                   reg.u,
                                   reg.i);
      z8k1_puts (buf);
      z8k1_normal ();
    };
    z8k1_crlf ();
  };

  const uint16_t fcw = zbios_read_fcw ();
  z8k1_puts_P (PSTR("fcw: "));
  for (int8_t i = -1; ++i < LENGTH (flags);)
  {
    z8k1_putc (' ');
    const uint16_t mask = flags[i].mask;
    if (compare_regs && ((fcw ^ regs[1].fcw_pc.fcw) & mask))
    {
      z8k1_inverse ();
    };
    z8k1_puts_P (flags[i].name[!(fcw & mask)]);
    z8k1_normal ();
  };
  z8k1_crlf ();
  z8k1_crlf ();

  if (compare_regs
      &&
      zbios_read_prev_pcaddr () != zbios_read_pcaddr ())
  {
    disasm (zbios_read_prev_pcaddr (), buf, NULL);
    z8k1_putsln (buf);
  };
  struct BreakInfo info;
  z8k1_inverse ();
  disasm (zbios_read_pcaddr (), buf, &info);
  z8k1_putsln (buf);
  z8k1_normal ();
  if (info.next != 1)
  {
    disasm (z8k1_segmented_address (zbios_read_pcseg (), info.next), buf, NULL);
    z8k1_putsln (buf);
  };
  if (info.dest != 1)
  {
    disasm (z8k1_segmented_address (zbios_read_pcseg (), info.dest), buf, NULL);
    z8k1_putsln (buf);
  }
}


static int8_t zbios_regs (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  zbios_regdump ();
  return 0;
}


/* Registerinhalt ändern */
typedef struct
{
  const __flash char *name;
  uint16_t addr;
} Reg;


static const __flash Reg reglist[] =
{
  {.name = FSTR("r10"),    .addr = ZBIOS(REGS.regs[10])      },
  {.name = FSTR("r11"),    .addr = ZBIOS(REGS.regs[11])      },
  {.name = FSTR("r12"),    .addr = ZBIOS(REGS.regs[12])      },
  {.name = FSTR("r13"),    .addr = ZBIOS(REGS.regs[13])      },
  {.name = FSTR("r14"),    .addr = ZBIOS(REGS.regs[14])      },
  {.name = FSTR("r15"),    .addr = ZBIOS(REGS.regs[15])      },
  {.name = FSTR("r0"),     .addr = ZBIOS(REGS.regs[0])       },
  {.name = FSTR("r1"),     .addr = ZBIOS(REGS.regs[1])       },
  {.name = FSTR("r2"),     .addr = ZBIOS(REGS.regs[2])       },
  {.name = FSTR("r3"),     .addr = ZBIOS(REGS.regs[3])       },
  {.name = FSTR("r4"),     .addr = ZBIOS(REGS.regs[4])       },
  {.name = FSTR("r5"),     .addr = ZBIOS(REGS.regs[5])       },
  {.name = FSTR("r6"),     .addr = ZBIOS(REGS.regs[6])       },
  {.name = FSTR("r7"),     .addr = ZBIOS(REGS.regs[7])       },
  {.name = FSTR("r8"),     .addr = ZBIOS(REGS.regs[8])       },
  {.name = FSTR("r9"),     .addr = ZBIOS(REGS.regs[9])       },
  {.name = FSTR("fcw"),    .addr = ZBIOS(REGS.fcw_pc.fcw)    },
  {.name = FSTR("pcseg"),  .addr = ZBIOS(REGS.fcw_pc.pc.seg) },
  {.name = FSTR("pcoff"),  .addr = ZBIOS(REGS.fcw_pc.pc.off) },
  {.name = FSTR("nspseg"), .addr = ZBIOS(REGS.nsp.seg)       },
  {.name = FSTR("nspoff"), .addr = ZBIOS(REGS.nsp.off)       },
  {.name = NULL }
};
#define REG_R14         reglist[4]
#define REG_R15         reglist[5]
#define REG_FCW         reglist[16]
#define REG_NSPSEG      reglist[19]
#define REG_NSP         reglist[20]


static int8_t zbios_reg (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  if (argc == 3)
  {
    zbios_read_regs ();
    const bool normal = zbios_normalmode ();
    for (const __flash Reg *reg = reglist; reg->name; ++reg)
    {
      if (strcmp_P (argv[1], reg->name) == 0)
      {
        uint16_t n;
        char c;

        if (normal)
        {
          if (reg == &REG_R15)
          {
            reg = &REG_NSP;
          }
          else if (reg == &REG_R14)
          {
            reg = &REG_NSPSEG;
          }
        };

        if (sscanf_P (argv[2], PSTR ("%i%c"), &n, &c) == 1)
        {
          z8k1_write_word (reg->addr, n);
          zbios_regdump ();
          return 0;
        }
      }
    }
  };
  return -1;
}


/* PC */
static int8_t zbios_pc (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  if (argc == 2)
  {
    uint32_t addr;
    if (z8k1_input_address (argv[1], &addr)
        &&
        valid_word_address (addr))
    {
      z8k1_write_word (ZBIOS(REGS.fcw_pc.pc.seg), (addr >> 16) << 8);
      z8k1_write_word (ZBIOS(REGS.fcw_pc.pc.off), addr & 0xFFFE);
      zbios_regdump ();
      return 0;
    }
  };
  return -1;
}


/* Segmented Mode */
static int8_t zbios_seg (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  zbios_read_regs ();
  zbios_fcw_set (FCW_SEGMENTED);
  zbios_regdump ();
  return 0;
}


/* Non-Segmented Mode */
static int8_t zbios_nonseg (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  zbios_read_regs ();
  zbios_fcw_clr (FCW_SEGMENTED);
  zbios_regdump ();
  return 0;
}


/* System Mode */
static int8_t zbios_system (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  zbios_read_regs ();
  zbios_fcw_set (FCW_SYSTEM);
  zbios_regdump ();
  return 0;
}


/* Normal Mode */
static int8_t zbios_normal (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  zbios_read_regs ();
  zbios_fcw_clr (FCW_SYSTEM);
  zbios_regdump ();
  return 0;
}


/* Trap-Info */
static int8_t zbios_tstat (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  char buf[128];

  snprintf_P (buf, sizeof buf, PSTR("bios %04x%04x"),
                   z8k1_read_word (ZBIOS_ID_ADDRESS+ZBIOS_ID(year)),
                   z8k1_read_word (ZBIOS_ID_ADDRESS+ZBIOS_ID(date)));
  z8k1_putsln (buf);
  z8k1_puts_P(PSTR("trp-c: "));
  switch (z8k1_read_word (ZBIOS(trap_code)))
  {
    case HC_RESET:
      z8k1_putsln_P(PSTR("rst"));
      break;

    case HC_EPU_TRAP:
      z8k1_putsln_P(PSTR("epu"));
      break;

    case HC_PRIV_TRAP:
      z8k1_putsln_P(PSTR("priv"));
      break;

    case HC_SEG_TRAP:
      z8k1_putsln_P(PSTR("seg"));
      break;

    case HC_VI_TRAP:
      z8k1_putsln_P(PSTR("vi"));
      break;

    case HC_NVI_TRAP:
      z8k1_putsln_P(PSTR("nvi"));
      break;

    case HC_EXIT:
      z8k1_putsln_P(PSTR("exit"));
      break;

    case HC_BREAKPOINT:
      z8k1_putsln_P(PSTR("brk"));
      break;

    case HC_SYSSTACK_OVERRUN:
      z8k1_putsln_P(PSTR("stk overrun"));
      break;

    case HC_SYSSTACK_UNDERRUN:
      z8k1_putsln_P(PSTR("stk underrun"));
      break;

    default:
      z8k1_putsln_P(PSTR("?"));
      break;
  };
  snprintf_P (buf, sizeof buf, PSTR("trp-r: %04x\r\nl-syc: %04x\r\nexit : %04x"),
                               z8k1_read_word (ZBIOS(trap_reason)),
                               z8k1_read_word (ZBIOS(last_sc_code)),
                               z8k1_read_word (ZBIOS(exit_code)));
  z8k1_putsln (buf);
  z8k1_puts_P(PSTR("hist :"));
  for (int8_t i = -1; ++i < LENGTH (FIELD(struct zbios_data, pc_hist));)
  {
    snprintf_P (buf, sizeof buf, PSTR(" %04x"), z8k1_read_word (ZBIOS(pc_hist[i])));
    z8k1_puts (buf);
  };
  z8k1_puts_P(PSTR("\r\nihist:"));
  for (int8_t i = -1; ++i < LENGTH (FIELD(struct zbios_data, pc_ihist));)
  {
    snprintf_P (buf, sizeof buf, PSTR(" %04x"), z8k1_read_word (ZBIOS(pc_ihist[i])));
    z8k1_puts (buf);
  };
  snprintf_P (buf, sizeof buf, PSTR("\r\nnmi  : %04x\r\nd-brk: %04x\r\nhsema: %04x\r\nht/co: %04x/%04x"),
                               z8k1_read_word (ZBIOS(nmi_count)),
                               z8k1_read_word (ZBIOS(deferred_break)),
                               z8k1_read_word (ZBIOS(host_sema)),
                               z8k1_read_word (ZBIOS(rc_halted)),
                               z8k1_read_word (ZBIOS(rc_cont)));
  z8k1_putsln (buf);
  z8k1_end_read_write ();
  return 0;
}


/* Z8001 ggf. mittels NMI starten und Hostmodus aktivieren */
static int8_t zbios_cont (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };

  if (!z8k1_IN (RESET)
      ||
      !z8k1_IN (BUSREQ)
      ||
      !z8k1_IN (STOP))
  {
    z8k1_putsln_P (PSTR ("cant"));
    return 0;
  };

  switch (argc)
  {
    case 1:
    case 2:
      break;

    default:
      return -1;
  };

  if (argc > 1)
  {
    uint32_t address;

    if (z8k1_input_address (argv[1], &address))
    {
      if ((address & 1) || address > 0x3FFFE)
      {
        return -1;
      };
      zbios_add_breakpoint (address, TEMP_BP);
    }
  };

  zbios_hostmode ();
  return 0;
}


/* Einzelschritt und Hostmodus */
static int8_t zbios_s_n_u (int8_t argc, char **argv)
{
  if (!assert_zbios_loaded ())
  {
    return 0;
  };
  zbios_read_regs ();
  if (!zbios_normalmode ())
  {
    z8k1_end_read_write ();
    z8k1_putsln_P (PSTR ("cant"));
    return 0;
  };
  struct BreakInfo info;
  char buf[32];
  if (disasm (zbios_read_pcaddr (), buf, &info) < 2)
  {
    return -1;
  };

  if (argv[0][0] != 'u'         /* s oder n */
      ||
      info.dest == 1)
  {
    zbios_add_breakpoint (z8k1_segmented_address (zbios_read_pcseg (), info.next), TEMP_BP);
  };
  if (argv[0][0] != 'n'         /* s oder u */
      ||
      info.next == 1)
  {
    zbios_add_breakpoint (z8k1_segmented_address (zbios_read_pcseg (), info.dest), TEMP_BP);
  };

  zbios_hostmode ();
  return 0;
}


/* Breakpoint setzen */
static int8_t zbios_breakpoint (int8_t argc, char **argv)
{
  if (argc != 2
      ||
      !z8k1_input_last_address (argv[1])
      ||
      !valid_word_address (last_address))
  {
    return -1;
  };

  if (!zbios_add_breakpoint (last_address, PERM_BP))
  {
    return -1;
  };

  return 0;
}


static int8_t zbios_breakdel (int8_t argc, char **argv)
{
  zbios_clear_breakpoints (PERM_BP);
  return 0;
}


static int8_t zbios_breaklist (int8_t argc, char **argv)
{
  for (struct Breakpoint *bp = breakpoints; bp != &breakpoints[LENGTH (breakpoints)]; ++bp)
  {
    if (bp->addr)
    {
      char buf[16];
      snprintf_P (buf, sizeof buf, PSTR (" %05lx"), bp->addr);
      z8k1_puts (buf);
      if (bp->perm == TEMP_BP)
      {
      z8k1_puts_P (PSTR(" t "));
      }
    }
  };
  z8k1_crlf ();
  return 0;
}


static int8_t zbios_breakclear (int8_t argc, char **argv)
{
  if (argc != 2
      ||
      !z8k1_input_last_address (argv[1])
      ||
      !valid_word_address (last_address))
  {
    return -1;
  };

  zbios_clear_breakpoint (last_address);
  return 0;
}


/* Z8001 anhalten */
static int8_t zbios_stop (int8_t argc, char **argv)
{
  z8k1_L    (STOP);
  z8k1_PULL (BUSREQ);
  z8k1_H    (RESET);
  if (zbios_loaded ())
  {
    zbios_hostmessage (HC_MSG_BREAKPOINT);
  };
  return 0;
}


/******************
 * Host-Interface *
 ******************/

/* Host-Modus */
static void ctrlcd_handler (uint8_t c)
{
  ctrlc += c == '\003';
  ctrld += c == '\004';
}


static void zbios_cancel (void)
{
  ++ctrlc;
}


static bool zbios_interrupted (void)
{
  return ctrlc || ctrld;
}


static void zbios_hc_exit (void)
{
  char buf[32];
  const uint16_t exitcode = z8k1_read_word (ZBIOS(exit_code));
  snprintf_P (buf, sizeof buf, PSTR("\r\nexit(%u)"), exitcode);
  z8k1_putsln (buf);
  host_interface_active = false;
}


static void zbios_hc_breakpoint (void)
{
  char buf[64];
  zbios_read_regs ();
  snprintf_P (buf, sizeof buf, PSTR("\r\nbreak at %05x"), zbios_read_pcaddr ());
  z8k1_putsln (buf);
  host_interface_active = false;
}



static void zbios_hc_trap (const __flash char *str)
{
  z8k1_putsln_P (str);
  host_interface_active = false;
}


static void zbios_hc_termin ()
{
  uint16_t n = z8k1_read_word (ZBIOS (host_size));
  uint16_t nin = 0;

  if (n > HC_MAXPAYLOAD)
  {
    n = HC_MAXPAYLOAD;
  };
  for (uint16_t addr = ZBIOS(host_payload); n--;)
  {
    if (zbios_interrupted ())
    {
      break;
    };
    z8k1_write_byte (addr++, uart_getc_wait ());
    ++nin;
  };
  z8k1_write_word (ZBIOS(host_size), nin);
}


static void zbios_hc_terminnowait ()
{
  uint16_t n = z8k1_read_word (ZBIOS(host_size));
  uint16_t nin = 0;

  if (n > HC_MAXPAYLOAD)
  {
    n = HC_MAXPAYLOAD;
  };
  for (uint16_t addr = ZBIOS(host_payload); n--;)
  {
    if (zbios_interrupted ())
    {
      break;
    };
    int c = uart_getc_nowait ();
    if (c == -1)
    {
      break;
    };
    z8k1_write_byte (addr++, uart_getc_wait ());
    ++nin;
  };
  z8k1_write_word (ZBIOS(host_size), nin);
}


static void zbios_hc_termout ()
{
  uint16_t n = z8k1_read_word (ZBIOS(host_size));
  uint16_t nout = 0;

  if (n > HC_MAXPAYLOAD)
  {
    n = HC_MAXPAYLOAD;
  };
  for (uint16_t addr = ZBIOS(host_payload); n--;)
  {
    if (zbios_interrupted ())
    {
      break;
    };
    uart_putc_wait (z8k1_read_byte (addr++));
    ++nout;
  };
  z8k1_write_word (ZBIOS(host_size), nout);
}


static void zbios_hc_termoutnowait ()
{
  uint16_t n = z8k1_read_word(ZBIOS (host_size));
  uint16_t nout = 0;

  if (n > HC_MAXPAYLOAD)
  {
    n = HC_MAXPAYLOAD;
  };
  for (uint16_t addr = ZBIOS(host_payload); n--;)
  {
    if (zbios_interrupted ())
    {
      break;
    };
    if (!uart_putc_nowait (z8k1_read_byte (addr++)))
    {
      break;
    };
    ++nout;
  };
  z8k1_write_word (ZBIOS(host_size), nout);
}


static void zbios_hc_termreadline ()
{
  static const struct CmdIntCallback cic =
  {
    .getc_f   = z8k1_getc,
    .putc_f   = z8k1_putc,
    .bs_f     = z8k1_bs,
    .crlf_f   = z8k1_crlf,
    .cancel_f = zbios_cancel,
    .prompt_f = NULL,
    .interp_f = NULL,
    .line_f   = NULL
  };

  uint16_t n = z8k1_read_word (ZBIOS(host_size));
  if (n > HC_MAXPAYLOAD)
  {
    n = HC_MAXPAYLOAD;
  };
  if (n > 126)
  {
    n = 126;
  };
  char line[HC_MAXPAYLOAD+1];
  int8_t len = cmdint_getline (&cic, line, NULL, n);
  if (len <= 0)
  {
    z8k1_write_word (ZBIOS(host_size), 0);
  }
  else
  {
    z8k1_write_word (ZBIOS(host_size), len);
    char *p = line;
    for (uint16_t addr = ZBIOS(host_payload); len--;)
    {
      z8k1_write_byte (addr++, *p++);
    }
  }
}


static void zbios_hc_terminstat ()
{
  z8k1_write_word (ZBIOS(host_size), uart_in_used ());
}


static void zbios_hc_termoutstat ()
{
  z8k1_write_word (ZBIOS(host_size), uart_out_avail ());
}


static void handle_ctrlc (void)
{
  if (ctrlc)
  {
    ctrlc = 0;
    z8k1_putsln_P (PSTR("\r\nINTR"));
    zbios_stop (1, NULL);
  }
}


static void handle_ctrld (void)
{
  if (ctrld)
  {
    z8k1_putsln_P (PSTR("\r\nDETA"));
    host_interface_active = false;
  }
}


static void zbios_host_data_exchange (void)
{
  switch (z8k1_read_word (ZBIOS(host_command)))
  {
    case HC_RESET:
      zbios_hc_trap (PSTR("rst"));
      break;

    case HC_EPU_TRAP:
      zbios_hc_trap (PSTR("epu"));
      break;

    case HC_PRIV_TRAP:
      zbios_hc_trap (PSTR("priv"));
      break;

    case HC_SEG_TRAP:
      zbios_hc_trap (PSTR("seg"));
      break;

    case HC_VI_TRAP:
      zbios_hc_trap (PSTR("vi"));
      break;

    case HC_NVI_TRAP:
      zbios_hc_trap (PSTR("nvi"));
      break;

    case HC_EXIT:
      zbios_hc_exit ();
      break;

    case HC_BREAKPOINT:
      zbios_hc_breakpoint ();
      break;

    case HC_SYSSTACK_OVERRUN:
      zbios_hc_trap (PSTR("stack overrun"));
      break;

    case HC_SYSSTACK_UNDERRUN:
      zbios_hc_trap (PSTR("stack underrun"));
      break;

    case HC_TERMIN:
      zbios_hc_termin ();
      break;

    case HC_TERMINNOWAIT:
      zbios_hc_terminnowait ();
      break;

    case HC_TERMOUT:
      zbios_hc_termout ();
      break;

    case HC_TERMOUTNOWAIT:
      z8k1_white ();
      zbios_hc_termoutnowait ();
      break;

    case HC_TERMREADLINE:
      z8k1_white ();
      zbios_hc_termreadline ();
      break;

    case HC_TERMINSTAT:
      zbios_hc_terminstat ();
      break;

    case HC_TERMOUTSTAT:
      zbios_hc_termoutstat ();
      break;

    default:
      break;
  };
  z8k1_end_read_write ();
  handle_ctrlc ();
}


static void zbios_host_interface_background (void)
{
  if (host_interface_active && !in_host_interface)
  {
    in_host_interface = true;
    if (!z8k1_IN (MO))
    {
      zbios_host_data_exchange ();
      z8k1_L (MI);
      long since;
      mark (&since);
      while (!z8k1_IN (MO) && elapsed (&since) < 500) ;
      z8k1_PULL (MI);
      if (!z8k1_IN (MO))
      {
        z8k1_putsln_P (PSTR("\r\nHOST IF ERR"));
        host_interface_active = false;
      }
    };
    handle_ctrlc ();
    handle_ctrld ();
    in_host_interface = false;
  }
}


/*****************************
 * Kommandoschnittstelle (1) *
 *****************************/

static int z8k1_getc (void)
{
  return uart_getc_wait ();
}


static inline void z8k1_flush (void)
{
  uart_flush ();
}


static void z8k1_putc (char c)
{
  uart_putc_wait (c);
}


static void z8k1_noputs (const char *s)
{
}


static void z8k1_noputs_P (const __flash char *s)
{
}


static void z8k1_puts (const char *s)
{
  z8k1_green ();
  while (*s)
  {
    z8k1_putc (*s++);
  }
}


static void z8k1_puts_P_ (const __flash char *s)
{
  char c;

  while ((c = *s++))
  {
    z8k1_putc (c);
  }
}


static void z8k1_puts_P (const __flash char *s)
{
  z8k1_green ();
  z8k1_puts_P_ (s);
}


static void z8k1_bs (void)
{
  z8k1_puts_P_ (PSTR ("\b \b"));
}


static void z8k1_cr (void)
{
  z8k1_putc ('\r');
}


static void z8k1_lf (void)
{
  z8k1_putc ('\n');
}


static void z8k1_crlf (void)
{
  z8k1_puts_P (PSTR ("\r\n"));
}


static void z8k1_putsln (const char *s)
{
  z8k1_puts (s);
  z8k1_crlf ();
}


static void z8k1_putsln_P (const __flash char *s)
{
  z8k1_puts_P (s);
  z8k1_crlf ();
}


static void z8k1_green (void)
{
  z8k1_puts_P_ (PSTR("\e[32m"));
}


static void z8k1_white (void)
{
  z8k1_puts_P_ (PSTR("\e[37m"));
}


static void z8k1_inverse (void)
{
  z8k1_puts_P_ (PSTR("\e[7m"));
}


static void z8k1_normal (void)
{
  z8k1_puts_P_ (PSTR("\e[27m"));
}


static void z8k1_cancel (void)
{
  z8k1_puts_P_ (PSTR ("#"));
}


static void z8k1_prompt (void)
{
  if (!z8k1_IN (RESET))
  {
    z8k1_puts_P (PSTR ("|rst"));
  };
  if (!z8k1_IN (BUSREQ))
  {
    z8k1_puts_P (PSTR ("|brq"));
  };
  if (!z8k1_IN (STOP))
  {
    z8k1_puts_P (PSTR ("|stp"));
  };
  if (!z8k1_IN (MO))
  {
    z8k1_puts_P (PSTR ("|mo"));
  };
  z8k1_puts_P (PSTR (">"));
}


/*****************************
 * Kommandoschnittstelle (2) *
 *****************************/

static int8_t z8k1_help (int8_t argc, char **argv);
static int8_t z8k1_version (int8_t argc, char **argv);


static const __flash struct CMD
{
  const __flash char *name;
  int8_t (*func) (int8_t argc, char **argv);
} cmds[] =
{
  { .name = FSTR("b"),           .func = zbios_breakpoint },
  { .name = FSTR("bc"),          .func = zbios_breakclear },
  { .name = FSTR("bd"),          .func = zbios_breakdel   },
  { .name = FSTR("bl"),          .func = zbios_breaklist  },
  { .name = FSTR("c"),           .func = zbios_cont       },
  { .name = FSTR("d"),           .func = z8k1_disassemble },
  { .name = FSTR("?"),           .func = z8k1_help        },
  { .name = FSTR("h"),           .func = z8k1_h           },
  { .name = FSTR("in"),          .func = z8k1_in          },
  { .name = FSTR("ist"),         .func = z8k1_istat       },
  { .name = FSTR("l"),           .func = z8k1_l           },
  { .name = FSTR("ld"),          .func = z8k1_ihexload    },
#if 0
  { .name = FSTR("memt"),        .func = z8k1_memtest     },
#endif
  { .name = FSTR("n"),           .func = zbios_s_n_u      },
  { .name = FSTR("nmi"),         .func = z8k1_nmi         },
  { .name = FSTR("no"),          .func = zbios_normal     },
  { .name = FSTR("ns"),          .func = zbios_nonseg     },
  { .name = FSTR("pc"),          .func = zbios_pc         },
  { .name = FSTR("pul"),         .func = z8k1_pull        },
  { .name = FSTR("r"),           .func = z8k1_read_words  },
  { .name = FSTR("rb"),          .func = z8k1_read_bytes  },
  { .name = FSTR("reg"),         .func = zbios_reg        },
  { .name = FSTR("run"),         .func = z8k1_run         },
  { .name = FSTR("regs"),        .func = zbios_regs       },
  { .name = FSTR("reset"),       .func = z8k1_reset       },
  { .name = FSTR("s"),           .func = zbios_s_n_u      },
  { .name = FSTR("sg"),          .func = zbios_seg        },
  { .name = FSTR("st"),          .func = z8k1_stat        },
  { .name = FSTR("stp"),         .func = zbios_stop       },
  { .name = FSTR("sy"),          .func = zbios_system     },
  { .name = FSTR("tst"),         .func = zbios_tstat      },
  { .name = FSTR("u"),           .func = zbios_s_n_u      },
  { .name = FSTR("ver"),         .func = z8k1_version     },
  { .name = FSTR("w"),           .func = z8k1_write_words },
  { .name = FSTR("wb"),          .func = z8k1_write_bytes },
  { .name = FSTR("z"),           .func = z8k1_z           },
};


static int8_t z8k1_help (int8_t argc, char **argv)
{
  for (int8_t i = -1; ++i < LENGTH (cmds);)
  {
    z8k1_putc (' ');
    z8k1_puts_P (cmds[i].name);
  };
  return 0;
}


static int8_t z8k1_version (int8_t argc, char **argv)
{
  z8k1_puts_P (program_version);
  return 0;
}


static int8_t z8k1_line (void)
{
  return 0;
}


static int8_t z8k1_interp (int8_t argc, char **argv)
{
  if (argc >= 1)
  {
    for (int8_t i = -1; ++i < LENGTH (cmds);)
    {
      const __flash char *name = cmds[i].name;
      if (strcmp_P (argv[0], name) == 0)
      {
        if (cmds[i].func (argc, argv) == 0)
        {
          z8k1_crlf ();
          return 0;
        };
        break;
      }
    }
  };
  z8k1_putsln_P (PSTR ("?"));
  return 0;
}


/*******************
 * Initialisierung *
 *******************/

static void init (void)
{
  cli ();
  z8k1_L (RESET);
  z8k1_PULL (NMI);
  z8k1_PULL (MI);
  z8k1_PULL (MO);
  z8k1_PULL (STOP);
  z8k1_end_read_write_ ();
  uart_sleep = background_sleep;
  uart_inevent = ctrlcd_handler;
  uart_init ();
  timer_init ();
  timerint_callback = timer_20ms;
  timerint_init ();
  interrupt2_callback = z8k1_mo;
  interrupt2_init ();
  sei ();
  ldintp = z8k1_read_word;
}


static void signon_message (void)
{
  z8k1_puts_P (PSTR ("\r\n\n\nz8k1 "));
  z8k1_putsln_P (program_version);
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
    .crlf_f   = z8k1_crlf,
    .cancel_f = z8k1_cancel,
    .prompt_f = z8k1_prompt,
    .interp_f = z8k1_interp,
    .line_f   = z8k1_line
  };

  static_assert (sizeof (struct zbios_data) == EXPECTED_SIZE_ZBIOS_DATA, "struct zbios: wrong size");
  static_assert (ZBIOS(trap_code) == 0x36, "struct zbios: wrong offset trap_code");
  static_assert (ZBIOS(host_message) == 0xFA, "struct zbios: wrong offset host_message");
  static_assert (ZBIOS(host_command) == 0xFC, "struct zbios: wrong offset host_command");
  init ();
  signon_message ();

  for (;;)
  {
    cmdint (&cic);
  };
  return 0;
}
