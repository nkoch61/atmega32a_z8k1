/* z8k_hostcomm.h */


#ifndef Z8K_HOSTCOMM_H
#define Z8K_HOSTCOMM_H


#define HC_NOP                  0
#define HC_RESET                1
#define HC_EPU_TRAP             2
#define HC_PRIV_TRAP            3
#define HC_SEG_TRAP             4
#define HC_VI_TRAP              5
#define HC_NVI_TRAP             6
#define HC_EXIT                 7
#define HC_BREAKPOINT           8
#define HC_SYSSTACK_OVERRUN     9
#define HC_SYSSTACK_UNDERRUN    10
#define HC_TERMIN               11
#define HC_TERMINNOWAIT         12
#define HC_TERMOUT              13
#define HC_TERMOUTNOWAIT        14
#define HC_TERMREADLINE         15
#define HC_TERMINSTAT           16
#define HC_TERMOUTSTAT          17

#define HC_MAXPAYLOAD           256

#define HC_MSG_NOP              0
#define HC_MSG_CONT             1
#define HC_MSG_BREAKPOINT       2


#endif
