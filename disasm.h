/*
********	Z800 opcode table	(from z800.c)
*/

#include <stdint.h>


#define	BYTE	int8_t
#define	WORD	int16_t
#define	UWORD	uint16_t
#define	LONG	uint32_t


#define IDX_0           221
#define IDX_CALL_IR     (259-IDX_0)
#define IDX_CALL_X      (260-IDX_0)
#define IDX_CALL_DA     (261-IDX_0)
#define IDX_CALR        (262-IDX_0)
#define IDX_DJNZ        (337-IDX_0)
#define IDX_DBJNZ       (338-IDX_0)
#define IDX_JP_IR       (373-IDX_0)
#define IDX_JP_X        (374-IDX_0)
#define IDX_JP_DA       (375-IDX_0)
#define IDX_JPCC_IR     (376-IDX_0)
#define IDX_JPCC_X      (377-IDX_0)
#define IDX_JPCC_DA     (378-IDX_0)
#define IDX_JR          (379-IDX_0)
#define IDX_JRCC        (380-IDX_0)
#define IDX_RET         (530-IDX_0)
#define IDX_RETCC       (531-IDX_0)


UWORD (*ldintp) (LONG addr);
int deasm (LONG base, char *lptr);
int deasm_find (LONG base);
int deasm_getnext( int opnum);
