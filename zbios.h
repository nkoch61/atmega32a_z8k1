/* zbios.h */


#include "z8k_hostcomm.h"


#ifndef  ZBIOS_H
#define ZBIOS_H


struct segmented_address
{
  uint16_t seg;
  uint16_t off;
} __attribute__ ((packed));


struct fcw_pc
{
  uint16_t null;
  uint16_t fcw;
  struct segmented_address pc;
} __attribute__ ((packed));


struct z8k_regs
{
  struct fcw_pc fcw_pc;
  uint16_t regs[16];
  struct segmented_address nsp;
} __attribute__ ((packed));


struct zbios_data
{
  struct fcw_pc reset;
  struct z8k_regs regs[1];
  uint16_t trap_reason;
  uint16_t trap_code;
  uint16_t  rc_halted;
  uint16_t  rc_cont;
  uint16_t last_sc_code;
  uint16_t exit_code;
  uint16_t pc_hist[8];
  uint16_t pc_ihist[8];
  uint16_t nmi_count;
  uint16_t deferred_break;
  uint8_t unused[148];
  uint16_t host_sema;
  uint16_t host_message;
  uint16_t host_command;
  uint16_t host_size;
  uint8_t host_payload[HC_MAXPAYLOAD];
} __attribute__ ((packed));


struct z8k_psa
{
  uint16_t reserved[4];
  struct fcw_pc epu;
  struct fcw_pc priv;
  struct fcw_pc sc;
  struct fcw_pc seg;
  struct fcw_pc nmi;
  struct fcw_pc nvi;
  struct
  {
    uint16_t null;
    uint16_t fcw;
    struct segmented_address pc[128];
  } vi;
} __attribute__ ((packed));


struct zbios_id
{
  uint16_t zb;
  uint16_t year;
  uint16_t date;
} __attribute__ ((packed));


#define ZBIOS(x)                        (offsetof (struct zbios_data, x))
#define ZBIOS_PSA(x)                    (offsetof (struct zbios_psa, x))
#define ZBIOS_ID(x)                     (offsetof (struct zbios_id, x))
#define REGS                            regs[0]
#define ZBIOS_DATA_ADDRESS              0x000
#define EXPECTED_SIZE_ZBIOS_DATA        0x200
#define ZBIOS_PSA_ADDRESS               0x200
#define ZBIOS_ID_ADDRESS                0x500


#endif
