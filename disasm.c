/*
**
**
**
**
**		Z8000 Assembler / Disassembler
**
**
**		July 1, 1982	Thorn Smith
**
**
**
**
**		***** PROPRIETARY ******
**
**	Copyright 1982 Zilog Corporation, all rights reserved.
**
**
**	This document is the property of Zilog corporation,
**	and is protected from unauthorized duplication under
**	applicable federal, state, and civil copyright laws.
**	No part of this document may be reproduced, stored,
**	or transmitted, in any form or by any means, such as
**	electronic, mechanical, photocopying, recording, or
**	otherwise, without the prior written permission of
**	Zilog, inc. 1315 Dell Ave., Campbell, Calif. 95008
**	tel: (408) 370-8000		twx: 910-338-7621
**
**
** date listed:		______	____/____/____	____:____	____
**
** project name:	619ss - ZRTS software enhancement development
**
** program name:	MAD (Mini Assembler/Disassembler)
**
** specifications:	Thorn Smith x 370-8317
**
** implementation:	Thorn Smith x 370-8317
**
** documentation:	Thorn Smith x 370-8317
**
** environment:		Zilog zeus z8001
**
** language:		Unix- C
**
** approvals-mgr:	________________________________________, Zilog
**
** approvals-q.a:	________________________________________, Zilog
**
** approvals-d.v:	________________________________________, Zilog
**
** program description:
**
**
*/

/*
**	*************************************************
**	*						*
**	*	I N C L U D E	F I L E S		*
**	*						*
**	*************************************************
**
*/

#include <stdio.h>
#include <avr/pgmspace.h>
#include "common.h"
#include "disasm.h"


#define	OFFSET	0x000F

/* standard data types */
#define	B1	0x0100	/* bit */
#define	B2	0x0200	/* nib */
#define	B3	0x0300	/* oct */
#define	B4	0x0400	/* nibble */
#define B5	0x0500	/* 5-bit (used for bitL) */

#define	X1	0x1100	/* byte */
#define	X2	0x1200	/* word */
#define	X3	0x1300	/* triple-byte? */
#define	X4	0x1400	/* long */
#define	X5	0x1500	/* quad */
#define	X6	0x1600	/* float */
#define	X7	0x1700	/* double */

#define	AA	0x2100	/* address */
#define	AL	0x2200	/* long address */

#define	RB	0x3100	/* byte register */
#define	RR	0x3200	/* register */
#define	RL	0x3300	/* long register */
#define	RQ	0x3400	/* quad register */

#define	II	0x4100	/* indirect register either long or short, but not 0 */
#define	I0	0x4200	/* indirect register either long or short, incl 0 */
#define	IR	0x4300	/* indirect register, but not r0 */
#define	IS	0x4400	/* indirect register including r0 */
#define	IL	0x4500	/* indirect long register, but not rr0 */
#define	L0	0x4600	/* indirect long register including rr0 */
#define	PP	0x4800	/* parenthesized register, either long or short, not 0*/
#define	PR	0x4900	/* parenthesized register, but not (R0)  */
#define	P0	0x4A00	/* parenthesized register, but not rr0 */
#define	PL	0x4B00	/* parenthesized long register */
#define	BB	0x4C00	/* base register (an RR or RL but not R0 or RR0) */
#define	BR	0x4D00	/* base register (an RR but not RR), not R0 */
#define	BL	0x4E00	/* long base register (an RL but not RR), not RR0 */

#define	CC	0x5100	/* condition code */
#define	CF	0x5200	/* conditional flag for comflg,resflg,setflg */

#define	D1	0x6100	/*  7-bit displacement (for djnz,dbjnz) */
#define	D2	0x6200	/*  8-bit displacement (for jr) */
#define	D3	0x6300	/* 12-bit displacement (for calr) */
#define	D4	0x6400	/* 16-bit displacement (for ld BA) */
#define	D5	0x6500	/* 16-bit displacement (for ldr) */

#define	SR	0x7100	/* special (control) register */
#define	UR	0x7200	/* user special (control) register */
#define	VI	0x7400	/* interrupts (vi,nvi) */
#define	NI	0x7500	/* interrupts (vi,nvi) */
#define	BN	0x7600	/* 1,2 bit field for rl,rlc,rr,rrc */
#define	ID	0x7700	/* increment and decrement fields (to be bumped) */
#define	P1	0x7800	/* +b field for sla,sll */
#define	P2	0x7900	/* +b field for slab,sllb */
#define	P3	0x7A00	/* +b field for slal,slll */
#define	N1	0x7B00	/* -b field for sra,srl */
#define	N2	0x7C00	/* -b field for srab,srlb */
#define	N3	0x7D00	/* -b field for sral,srll */
#define SL	0x7E00	/* long special (control) register */

/* bizarre data types */
#define	XRST	0x8100
#define	XBOF	0x8200
#define	XBWD	0x8300

/* register data types */
#define	RNS1	0x8400 /* normal	single	reg(b, c, d, e, h, l, m ) */
#define	RNS2	0x8500 /* normal	single	reg(b, c, d, e, h, l, m ) */
#define	XPSR	0x8600 /* prime	single	reg(b',c',d',e',h',l',m') */
#define	XIXY	0x8700 /* XY	single	reg(ixh, ixl, iyh, iyl) */
#define	XISR	0x8800 /* indexed	single	reg(@(c))	*/
#define	XNDR	0x8900 /* normal	double	reg(bc, de, hl, pc, sp) */
#define	XPDR	0x8a00 /* prime	double	reg(bc',de',hl') */
#define	XIDR	0x8b00 /* indexed	double	reg(@(bc,de,hl,sp,ix,iy)) */
#define	XIRR	0x8c00 /* indexed	registr	reg(@(hl+ix,hl+iy,ix+iy))*/
#define	XIII	0x8d00 /* i register */
#define	XRRR	0x8e00 /* r register */
#define	XTXR	0x8f00 /* TX register */
#define	DREG	0xA000
#define	PSRG	0xA100
#define	PDRG	0xA200

#define	XOPC	0xA300

#define	EM	0xB000
#define	BP	0xB100
#define	BS	0xB200

#define	RINT	0x20
#define	RCHR	0x10
#define	RCR	0x08

#define	MAXA	4
#define	MAXB	4
#define	MAXF	10


/*
********	reserved symbol table	(from z800.c)
*/

#define	SYMLEN	8

struct	symbol {
	int	utype;
	int	uvalue;
	const char uname[SYMLEN+1];
};


/*
********	globals for assembler / disassembler
*/

static int	types[MAXF];
static long	values[MAXF];
static int	args,opcode;

static int	opsa;
static int	ivalue;
static long	tvalue;

static int	ttype,fldtype,bitoff,offset,bitnum;

static char     *lptr;
static int      error;

#define CACHED  1
struct cache {
  LONG addr;
  WORD data;
  BYTE valid;
};

#if CACHED
static struct cache op_cache[4];
#endif

static void disop (LONG base, int opnum);
static int typfind (int t, int v);
static void swinit (int opnum, int field);


struct op {
 BYTE	opc;
 BYTE	opb[4];
 WORD	opa[4];
};

static const __flash struct op ops[] = {
 0002, 0x00,0x00,0x00,0x00, X2+00,    0,    0,    0, /*  0 .word #00123 */
 0222, 0xb5,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /*  1 adc r3,r2 */
 0222, 0xb4,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*  2 adcb rh3,rh2 */
 0222, 0x81,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /*  3 add r3,r2 */
 0022, 0x01,0x00,0x00,0x00, RR+14,II+10,    0,    0, /*  3 add r3,@r2 */
 0024, 0x01,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /*  3 add r3,#04567 */
 0024, 0x41,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /*  3 add r3,4567(r2) */
 0024, 0x41,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /*  3 add r3,4567 */
 0222, 0x80,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*  4 addb rh3,rh2 */
 0022, 0x00,0x00,0x00,0x00, RB+14,II+10,    0,    0, /*  4 addb rh3,@r2 */
 0024, 0x00,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /*  4 addb rh3,#045 */
 0024, 0x40,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /*  4 addb rh3,4567(r2) */
 0024, 0x40,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /*  4 addb rh3,4567 */
 0222, 0x96,0x00,0x00,0x00, RL+14,RL+10,    0,    0, /*  5 addl rr2,rr2 */
 0022, 0x16,0x00,0x00,0x00, RL+14,II+10,    0,    0, /*  5 addl rr2,@r2 */
 0026, 0x16,0x00,0x00,0x00, RL+14,X4+20,    0,    0, /*  5 addl rr2, #0022b */
 0024, 0x56,0x00,0x00,0x00, RL+14,AA+20,PR+10,    0, /*  5 addl rr2,4567(r2) */
 0024, 0x56,0x00,0x00,0x00, RL+14,AA+20,    0,    0, /*  5 addl rr2,4567 */
 0222, 0x87,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /*  6 and r3,r2 */
 0022, 0x07,0x00,0x00,0x00, RR+14,II+10,    0,    0, /*  6 and r3,@r2 */
 0024, 0x07,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /*  6 and r3,#04567 */
 0024, 0x47,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /*  6 and r3,4567(r2) */
 0024, 0x47,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /*  6 and r3,4567 */
 0222, 0x86,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*  7 andb rh3,rh2 */
 0022, 0x06,0x00,0x00,0x00, RB+14,II+10,    0,    0, /*  7 andb rh3,@r2 */
 0024, 0x06,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /*  7 andb rh3,#045 */
 0024, 0x46,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /*  7 andb rh3,4567(r2) */
 0024, 0x46,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /*  7 andb rh3,4567 */
 0222, 0xa7,0x00,0x00,0x00, RR+10,B4+14,    0,    0, /*  8 bit r2,#3 */
 0022, 0x27,0x00,0x00,0x00, II+10,B4+14,    0,    0, /*  8 bit @r2,#3 */
 0044, 0x27,0x00,0x00,0x00, RR+24,RR+14,    0,    0, /*  8 bit r5,r3 */
 0024, 0x67,0x00,0x00,0x00, AA+20,PR+10,B4+14,    0, /*  8 bit 4567(r2),#3 */
 0024, 0x67,0x00,0x00,0x00, AA+20,B4+14,    0,    0, /*  8 bit 4567,#3 */
 0222, 0xa6,0x00,0x00,0x00, RB+10,B4+14,    0,    0, /*  9 bitb rh2,#3 */
 0022, 0x26,0x00,0x00,0x00, II+10,B4+14,    0,    0, /*  9 bitb @r2,#3 */
 0044, 0x26,0x00,0x00,0x00, RB+24,RR+14,    0,    0, /*  9 bitb rh5,r3 */
 0024, 0x66,0x00,0x00,0x00, AA+20,PR+10,B4+14,    0, /*  9 bitb 4567(r2),#3 */
 0024, 0x66,0x00,0x00,0x00, AA+20,B4+14,    0,    0, /*  9 bitb 4567,#3 */
 0222, 0x1f,0x00,0x00,0x00, II+10,    0,    0,    0, /* 10 call @r2 */
 0024, 0x5f,0x00,0x00,0x00, AA+20,PR+10,    0,    0, /* 10 call 4567(r2) */
 0024, 0x5f,0x00,0x00,0x00, AA+20,    0,    0,    0, /* 10 call 4567 */
 0222, 0xd0,0x00,0x00,0x00, D3+04,    0,    0,    0, /* 11 calr 0a0c */
 0222, 0x8d,0x08,0x00,0x00, RR+10,    0,    0,    0, /* 12 clr r2 */
 0022, 0x0d,0x08,0x00,0x00, II+10,    0,    0,    0, /* 12 clr @r2 */
 0024, 0x4d,0x08,0x00,0x00, AA+20,PR+10,    0,    0, /* 12 clr 4567(r2) */
 0024, 0x4d,0x08,0x00,0x00, AA+20,    0,    0,    0, /* 12 clr 4567 */
 0222, 0x8c,0x08,0x00,0x00, RB+10,    0,    0,    0, /* 13 clrb rh2 */
 0022, 0x0c,0x08,0x00,0x00, II+10,    0,    0,    0, /* 13 clrb @r2 */
 0024, 0x4c,0x08,0x00,0x00, AA+20,PR+10,    0,    0, /* 13 clrb 4567(r2) */
 0024, 0x4c,0x08,0x00,0x00, AA+20,    0,    0,    0, /* 13 clrb 4567 */
 0222, 0x8d,0x00,0x00,0x00, RR+10,    0,    0,    0, /* 14 com r2 */
 0022, 0x0d,0x00,0x00,0x00, II+10,    0,    0,    0, /* 14 com @r2 */
 0024, 0x4d,0x00,0x00,0x00, AA+20,PR+10,    0,    0, /* 14 com 4567(r2) */
 0024, 0x4d,0x00,0x00,0x00, AA+20,    0,    0,    0, /* 14 com 4567 */
 0222, 0x8c,0x00,0x00,0x00, RB+10,    0,    0,    0, /* 15 comb rh2 */
 0022, 0x0c,0x00,0x00,0x00, II+10,    0,    0,    0, /* 15 comb @r2 */
 0024, 0x4c,0x00,0x00,0x00, AA+20,PR+10,    0,    0, /* 15 comb 4567(r2) */
 0024, 0x4c,0x00,0x00,0x00, AA+20,    0,    0,    0, /* 15 comb 4567 */
 0222, 0x8d,0x05,0x00,0x00, CF+10,CF+10,CF+10,CF+10, /* 16 comflg le */
 0222, 0x8b,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /* 17 cp r3,r2 */
 0022, 0x0b,0x00,0x00,0x00, RR+14,II+10,    0,    0, /* 17 cp r3,@r2 */
 0024, 0x0b,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /* 17 cp r3,#04567 */
 0024, 0x4b,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /* 17 cp r3,4567(r2) */
 0024, 0x4b,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /* 17 cp r3,4567 */
 0024, 0x0d,0x01,0x00,0x00, IR+10,X2+20,    0,    0, /* 17 cp @r2,#04567 */
 0026, 0x4d,0x01,0x00,0x00, AA+20,PR+10,X2+40,    0, /* 17 cp 4567(r2),#089ab */
 0026, 0x4d,0x01,0x00,0x00, AA+20,X2+40,    0,    0, /* 17 cp 4567,#089ab */
 0222, 0x8a,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /* 18 cpb rh3,rh2 */
 0022, 0x0a,0x00,0x00,0x00, RB+14,II+10,    0,    0, /* 18 cpb rh3,@r2 */
 0024, 0x0a,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /* 18 cpb rh3,#045 */
 0024, 0x4a,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /* 18 cpb rh3,4567(r2) */
 0024, 0x4a,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /* 18 cpb rh3,4567 */
 0024, 0x0c,0x01,0x00,0x00, IR+10,X1+20,    0,    0, /* 18 cpb @r2,#045 */
 0026, 0x4c,0x01,0x00,0x00, AA+20,PR+10,X1+40,    0, /* 18 cpb 4567(r2),#089 */
 0026, 0x4c,0x01,0x00,0x00, AA+20,X2+40,    0,    0, /* 18 cpb 4567,#089ab */
 0222, 0x90,0x00,0x00,0x00, RL+14,RL+10,    0,    0, /* 19 cpl rr2,rr2 */
 0022, 0x10,0x00,0x00,0x00, RL+14,II+10,    0,    0, /* 19 cpl rr2,@r2 */
 0026, 0x10,0x00,0x00,0x00, RL+14,X4+20,    0,    0, /* 19 cpl rr2, #0022b */
 0024, 0x50,0x00,0x00,0x00, RL+14,AA+20,PR+10,    0, /* 19 cpl rr2,4567(r2) */
 0024, 0x50,0x00,0x00,0x00, RL+14,AA+20,    0,    0, /* 19 cpl rr2,4567 */
 0244, 0xbb,0x08,0x00,0x00, RR+30,II+10,RR+24,CC+34, /* 20 cpd r6,@r2,r5,cy */
 0244, 0xba,0x08,0x00,0x00, RB+30,II+10,RR+24,CC+34, /* 21 cpdb r6,@r2,r5,cy */
 0244, 0xbb,0x0c,0x00,0x00, RR+30,II+10,RR+24,CC+34, /* 22 cpdr r6,@r2,r5,cy */
 0244, 0xba,0x0c,0x00,0x00, RB+30,II+10,RR+24,CC+34, /* 23 cpdrb r6,@r2,r5,cy */
 0244, 0xbb,0x00,0x00,0x00, RR+30,II+10,RR+24,CC+34, /* 24 cpi r6,@r2,r5,cy */
 0244, 0xba,0x00,0x00,0x00, RB+30,II+10,RR+24,CC+34, /* 25 cpib r6,@r2,r5,cy */
 0244, 0xbb,0x04,0x00,0x00, RR+30,II+10,RR+24,CC+34, /* 26 cpir r6,@r2,r5,cy */
 0244, 0xba,0x04,0x00,0x00, RB+30,II+10,RR+24,CC+34, /* 27 cpirb r6,@r2,r5,cy */
 0244, 0xbb,0x0a,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 28 cpsd @r6,@r2,r5,cy */
 0244, 0xba,0x0a,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 29 cpsdb @r6,@r2,r5,cy */
 0244, 0xbb,0x0e,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 30 cpsdr @r6,@r2,r5,cy */
 0244, 0xba,0x0e,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 31 cpsdrb @r6,@r2,r5,cy */
 0244, 0xbb,0x02,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 32 cpsi @r6,@r2,r5,cy */
 0244, 0xba,0x02,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 33 cpsib @r6,@r2,r5,cy */
 0244, 0xbb,0x06,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 34 cpsir @r6,@r2,r5,cy */
 0244, 0xba,0x06,0x00,0x00, II+30,II+10,RR+24,CC+34, /* 35 cpsirb @r6,@r2,r5,cy */
 0222, 0xb0,0x00,0x00,0x00, RB+10,    0,    0,    0, /* 36 dab rh2 */
 0222, 0xab,0x00,0x00,0x00, RR+10,ID+14,    0,    0, /* 37 dec r2,#4 */
 0022, 0x2b,0x00,0x00,0x00, II+10,ID+14,    0,    0, /* 37 dec @r2,#4 */
 0024, 0x6b,0x00,0x00,0x00, AA+20,PR+10,ID+14,    0, /* 37 dec 4567(r2),#4 */
 0024, 0x6b,0x00,0x00,0x00, AA+20,ID+14,    0,    0, /* 37 dec 4567,#4 */
 0222, 0xaa,0x00,0x00,0x00, RB+10,ID+14,    0,    0, /* 38 decb rh2,#4 */
 0022, 0x2a,0x00,0x00,0x00, II+10,ID+14,    0,    0, /* 38 decb @r2,#4 */
 0024, 0x6a,0x00,0x00,0x00, AA+20,PR+10,ID+14,    0, /* 38 decb 4567(r2),#4 */
 0024, 0x6a,0x00,0x00,0x00, AA+20,ID+14,    0,    0, /* 38 decb 4567,#4 */
 0222, 0x7c,0x00,0x00,0x00, VI+16,VI+17,    0,    0, /* 39 di vi,nvi, */
 0222, 0x9b,0x00,0x00,0x00, RL+14,RR+10,    0,    0, /* 40 div rr4,r2 */
 0022, 0x1b,0x00,0x00,0x00, RL+14,II+10,    0,    0, /* 40 div rr4,@r2 */
 0024, 0x1b,0x00,0x00,0x00, RL+14,X2+20,    0,    0, /* 40 div rr4,#4 */
 0024, 0x5b,0x00,0x00,0x00, RL+14,AA+20,PR+10,    0, /* 40 div rr4,4567(r2) */
 0024, 0x5b,0x00,0x00,0x00, RL+14,AA+20,    0,    0, /* 40 div rr4,4567 */
 0222, 0x9a,0x00,0x00,0x00, RQ+14,RL+10,    0,    0, /* 41 divl rq4,rr2 */
 0022, 0x1a,0x00,0x00,0x00, RQ+14,II+10,    0,    0, /* 41 divl rq4,@r2 */
 0026, 0x1a,0x00,0x00,0x00, RQ+14,X4+20,    0,    0, /* 41 divl rq4,#4 */
 0024, 0x5a,0x00,0x00,0x00, RQ+14,AA+20,PR+10,    0, /* 41 divl rq4,4567(r2) */
 0024, 0x5a,0x00,0x00,0x00, RQ+14,AA+20,    0,    0, /* 41 divl rq4,4567 */
 0222, 0xf0,0x80,0x00,0x00, RR+04,D1+11,    0,    0, /* 42 djnz r1,2c0c */
 0222, 0xf0,0x00,0x00,0x00, RB+04,D1+11,    0,    0, /* 43 dbjnz rh1,2c0c */
 0222, 0x7c,0x04,0x00,0x00, VI+16,VI+17,    0,    0, /* 44 ei vi,nvi, */
 0222, 0xad,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /* 45 ex r3,r2 */
 0022, 0x2d,0x00,0x00,0x00, RR+14,II+10,    0,    0, /* 45 ex r3,@r2 */
 0024, 0x6d,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /* 45 ex r3,4567(r2) */
 0024, 0x6d,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /* 45 ex r3,4567 */
 0222, 0xac,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /* 46 exb rh3,rh2 */
 0022, 0x2c,0x00,0x00,0x00, RB+14,II+10,    0,    0, /* 46 exb rh3,@r2 */
 0024, 0x6c,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /* 46 exb rh3,4567(r2) */
 0024, 0x6c,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /* 46 exb rh3,4567 */
 0222, 0xb1,0x0a,0x00,0x00, RL+10,    0,    0,    0, /* 47 exts rr2 */
 0222, 0xb1,0x00,0x00,0x00, RR+10,    0,    0,    0, /* 48 extsb r2 */
 0222, 0xb1,0x07,0x00,0x00, RQ+10,    0,    0,    0, /* 49 extsl rq0 */
 0222, 0x7a,0x00,0x00,0x00,     0,    0,    0,    0, /* 50 halt  */
 0222, 0x3d,0x00,0x00,0x00, RR+14,IR+10,    0,    0, /* 51 in r3,@r2 */
 0024, 0x3b,0x04,0x00,0x00, RR+10,D4+20,    0,    0, /* 51 in r2(#4567) */
 0222, 0x3c,0x00,0x00,0x00, RB+14,IR+10,    0,    0, /* 52 inb rh3,@r2 */
 0024, 0x3a,0x04,0x00,0x00, RB+10,D4+20,    0,    0, /* 52 inb rh2(#4567) */
 0222, 0xa9,0x00,0x00,0x00, RR+10,ID+14,    0,    0, /* 53 inc r2,#4 */
 0022, 0x29,0x00,0x00,0x00, II+10,ID+14,    0,    0, /* 53 inc @r2,#4 */
 0024, 0x69,0x00,0x00,0x00, AA+20,PR+10,ID+14,    0, /* 53 inc 4567(r2),#4 */
 0024, 0x69,0x00,0x00,0x00, AA+20,ID+14,    0,    0, /* 53 inc 4567,#4 */
 0222, 0xa8,0x00,0x00,0x00, RB+10,ID+14,    0,    0, /* 54 incb rh2,#4 */
 0022, 0x28,0x00,0x00,0x00, II+10,ID+14,    0,    0, /* 54 incb @r2,#4 */
 0024, 0x68,0x00,0x00,0x00, AA+20,PR+10,ID+14,    0, /* 54 incb 4567(r2),#4 */
 0024, 0x68,0x00,0x00,0x00, AA+20,ID+14,    0,    0, /* 54 incb 4567,#4 */
 0244, 0x3b,0x08,0x00,0x08, II+30,IR+10,RR+24,    0, /* 55 ind @r6,@r2,r5 */
 0244, 0x3a,0x08,0x00,0x08, II+30,IR+10,RR+24,    0, /* 56 indb @r6,@r2,r5 */
 0244, 0x3b,0x08,0x00,0x00, II+30,IR+10,RR+24,    0, /* 57 indr @r6,@r2,r5 */
 0244, 0x3a,0x08,0x00,0x00, II+30,IR+10,RR+24,    0, /* 58 indrb @r6,@r2,r5 */
 0244, 0x3b,0x00,0x00,0x08, II+30,IR+10,RR+24,    0, /* 59 ini @r6,@r2,r5 */
 0244, 0x3a,0x00,0x00,0x08, II+30,IR+10,RR+24,    0, /* 60 inib @r6,@r2,r5 */
 0244, 0x3b,0x00,0x00,0x00, II+30,IR+10,RR+24,    0, /* 61 inir @r6,@r2,r5 */
 0244, 0x3a,0x00,0x00,0x00, II+30,IR+10,RR+24,    0, /* 62 inirb @r6,@r2,r5 */
 0222, 0x7b,0x00,0x00,0x00,     0,    0,    0,    0, /* 63 iret  */
 0222, 0x1e,0x08,0x00,0x00, II+10,    0,    0,    0, /* 64 jp @r2 */
 0024, 0x5e,0x08,0x00,0x00, AA+20,PR+10,    0,    0, /* 64 jp 4567(r2) */
 0024, 0x5e,0x08,0x00,0x00, AA+20,    0,    0,    0, /* 64 jp 4567 */
 0022, 0x1e,0x00,0x00,0x00, CC+14,II+10,    0,    0, /* 64 jp ule,@r2 */
 0024, 0x5e,0x00,0x00,0x00, CC+14,AA+20,PR+10,    0, /* 64 jp ule,4567(r2) */
 0024, 0x5e,0x00,0x00,0x00, CC+14,AA+20,    0,    0, /* 64 jp ule,4567 */
 0222, 0xe8,0x00,0x00,0x00, D2+10,    0,    0,    0, /* 65 jr 2c98 */
 0022, 0xe0,0x00,0x00,0x00, CC+04,D2+10,    0,    0, /* 65 jr lt,2c98 */
 0222, 0xa1,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /* 66 ld r3,r2 */
 0022, 0x21,0x00,0x00,0x00, RR+14,II+10,    0,    0, /* 66 ld r3,@r2 */
 0024, 0x21,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /* 66 ld r3,#04567 */
 0024, 0x61,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /* 66 ld r3,4567(r2) */
 0024, 0x61,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /* 66 ld r3,4567 */
 0024, 0x31,0x00,0x00,0x00, RR+14,BB+10,D4+20,    0, /* 66 ld r3,r2(#4567) */
 0044, 0x71,0x00,0x00,0x00, RR+14,BB+10,PR+24,    0, /* 66 ld r3,r2(r5) */
 0022, 0x2f,0x00,0x00,0x00, II+10,RR+14,    0,    0, /* 66 ld @r2,r3 */
 0024, 0x6f,0x00,0x00,0x00, AA+20,PR+10,RR+14,    0, /* 66 ld 4567(r2),r3 */
 0024, 0x6f,0x00,0x00,0x00, AA+20,RR+14,    0,    0, /* 66 ld 4567,r3 */
 0024, 0x33,0x00,0x00,0x00, BB+10,D4+20,RR+14,    0, /* 66 ld r2(#4567),r3 */
 0044, 0x73,0x00,0x00,0x00, BB+10,PR+24,RR+14,    0, /* 66 ld r2(r5),r3 */
 0024, 0x0d,0x05,0x00,0x00, II+10,X2+20,    0,    0, /* 66 ld @r2,#04567 */
 0026, 0x4d,0x05,0x00,0x00, AA+20,PR+10,X2+40,    0, /* 66 ld 4567(r2),#089ab */
 0026, 0x4d,0x05,0x00,0x00, AA+20,X2+40,    0,    0, /* 66 ld 4567,#089ab */
 0222, 0xa0,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /* 67 ldb rh3,rh2 */
 0022, 0x20,0x00,0x00,0x00, RB+14,II+10,    0,    0, /* 67 ldb rh3,@r2 */
 0024, 0x20,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /* 67 ldb rh3,#045 */
 0024, 0x60,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /* 67 ldb rh3,4567(r2) */
 0024, 0x60,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /* 67 ldb rh3,4567 */
 0024, 0x30,0x00,0x00,0x00, RB+14,BB+10,D4+20,    0, /* 67 ldb rh3,r2(#4567) */
 0044, 0x70,0x00,0x00,0x00, RB+14,BB+10,PR+24,    0, /* 67 ldb rh3,r2(r5) */
 0022, 0x2e,0x00,0x00,0x00, II+10,RB+14,    0,    0, /* 67 ldb @r2,rh3 */
 0024, 0x6e,0x00,0x00,0x00, AA+20,PR+10,RB+14,    0, /* 67 ldb 4567(r2),rh3 */
 0024, 0x6e,0x00,0x00,0x00, AA+20,RB+14,    0,    0, /* 67 ldb 4567,rh3 */
 0024, 0x32,0x00,0x00,0x00, BB+10,D4+20,RB+14,    0, /* 67 ldb r2(#4567),rh3 */
 0044, 0x72,0x00,0x00,0x00, BB+10,PR+24,RB+14,    0, /* 67 ldb r2(r5),rh3 */
 0022, 0xc0,0x00,0x00,0x00, RB+04,B4+10,B4+14,    0, /* 67 ldb rh1,#2,#3 */
 0024, 0x0c,0x05,0x00,0x00, II+10,X1+20,    0,    0, /* 67 ldb @r2,#045 */
 0026, 0x4c,0x05,0x00,0x00, AA+20,PR+10,X1+40,    0, /* 67 ldb 4567(r2),#089 */
 0026, 0x4c,0x05,0x00,0x00, AA+20,X1+40,    0,    0, /* 67 ldb 4567,#089 */
 0222, 0x94,0x00,0x00,0x00, RL+14,RL+10,    0,    0, /* 68 ldl rr2,rr2 */
 0022, 0x14,0x00,0x00,0x00, RL+14,II+10,    0,    0, /* 68 ldl rr2,@r2 */
 0026, 0x14,0x00,0x00,0x00, RL+14,X4+20,    0,    0, /* 68 ldl rr2, #0022b */
 0024, 0x54,0x00,0x00,0x00, RL+14,AA+20,PR+10,    0, /* 68 ldl rr2,4567(r2) */
 0024, 0x54,0x00,0x00,0x00, RL+14,AA+20,    0,    0, /* 68 ldl rr2,4567 */
 0024, 0x35,0x00,0x00,0x00, RL+14,BB+10,D4+20,    0, /* 68 ldl rr2,r2(#4567) */
 0044, 0x75,0x00,0x00,0x00, RL+14,BB+10,PR+24,    0, /* 68 ldl rr2,r2(r5) */
 0022, 0x1d,0x00,0x00,0x00, II+10,RL+14,    0,    0, /* 68 ldl @r2,rr2 */
 0024, 0x5d,0x00,0x00,0x00, AA+20,PR+10,RL+14,    0, /* 68 ldl 4567(r2),rr2 */
 0024, 0x5d,0x00,0x00,0x00, AA+20,RL+14,    0,    0, /* 68 ldl 4567,rr2 */
 0024, 0x37,0x00,0x00,0x00, BB+10,D4+20,RL+14,    0, /* 68 ldl r2(#4567),rr2 */
 0044, 0x77,0x00,0x00,0x00, BB+10,PR+24,RL+14,    0, /* 68 ldl r2(r5),rr2 */
 0224, 0x76,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /* 69 lda r3,4567 */
 0024, 0x76,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /* 69 lda r3,4567(r2) */
 0024, 0x34,0x00,0x00,0x00, RR+14,BB+10,D4+20,    0, /* 69 lda r3,r2(#4567) */
 0044, 0x74,0x00,0x00,0x00, RR+14,BB+10,PR+24,    0, /* 69 lda r3,r2(r5) */
 0224, 0x34,0x00,0x00,0x00, RR+14,D4+20,    0,    0, /* 70 ldar r3(#4567) */
 0222, 0x7d,0x08,0x00,0x00, SR+14,RR+10,    0,    0, /* 71 ldctl refresh,r2 */
 0022, 0x7d,0x00,0x00,0x00, RR+10,SR+14,    0,    0, /* 71 ldctl r2,refresh */
 0222, 0x8c,0x09,0x00,0x00, UR+14,RB+10,    0,    0, /* 72 ldctlb flags,rh2 */
 0022, 0x8c,0x01,0x00,0x00, RB+10,UR+14,    0,    0, /* 72 ldctlb rh2,flags */
 0244, 0xbb,0x09,0x00,0x08, II+30,II+10,RR+24,    0, /* 73 ldd @r6,@r2,r5 */
 0244, 0xba,0x09,0x00,0x08, II+30,II+10,RR+24,    0, /* 74 lddb @r6,@r2,r5 */
 0244, 0xbb,0x09,0x00,0x00, II+30,II+10,RR+24,    0, /* 75 lddr @r6,@r2,r5 */
 0244, 0xba,0x09,0x00,0x00, II+30,II+10,RR+24,    0, /* 76 lddrb @r6,@r2,r5 */
 0244, 0xbb,0x01,0x00,0x08, II+30,II+10,RR+24,    0, /* 77 ldi @r6,@r2,r5 */
 0244, 0xba,0x01,0x00,0x08, II+30,II+10,RR+24,    0, /* 78 ldib @r6,@r2,r5 */
 0244, 0xbb,0x01,0x00,0x00, II+30,II+10,RR+24,    0, /* 79 ldir @r6,@r2,r5 */
 0244, 0xba,0x01,0x00,0x00, II+30,II+10,RR+24,    0, /* 80 ldirb @r6,@r2,r5 */
 0222, 0xbd,0x00,0x00,0x00, RR+10,B4+14,    0,    0, /* 81 ldk r2,#3 */
 0224, 0x1c,0x01,0x00,0x00, RR+24,II+10,ID+34,    0, /* 82 ldm r5,@r2,#2 */
 0026, 0x5c,0x01,0x00,0x00, RR+24,AA+40,PR+10,ID+34, /* 82 ldm r5,4567(r2),#2 */
 0026, 0x5c,0x01,0x00,0x00, RR+24,AA+40,ID+34,    0, /* 82 ldm r5,4567, #0022b */
 0024, 0x1c,0x09,0x00,0x00, II+10,RR+24,ID+34,    0, /* 82 ldm @r2,r5,#2 */
 0026, 0x5c,0x09,0x00,0x00, AA+40,PR+10,RR+24,ID+34, /* 82 ldm 4567(r2),r5,#2 */
 0026, 0x5c,0x09,0x00,0x00, AA+40,RR+24,ID+34,    0, /* 82 ldm 4567,r5,#2 */
 0222, 0x39,0x00,0x00,0x00, II+10,    0,    0,    0, /* 83 ldps @r2 */
 0024, 0x79,0x00,0x00,0x00, AA+20,PR+10,    0,    0, /* 83 ldps 4567(r2) */
 0024, 0x79,0x00,0x00,0x00, AA+20,    0,    0,    0, /* 83 ldps 4567 */
 0224, 0x31,0x00,0x00,0x00, RR+14,D4+20,    0,    0, /* 84 ldr r3(#4567) */
 0024, 0x33,0x00,0x00,0x00, D4+20,RR+14,    0,    0, /* 84 ldr (#4567),r3 */
 0224, 0x30,0x00,0x00,0x00, RR+14,D4+20,    0,    0, /* 85 ldrb r3(#4567) */
 0024, 0x32,0x00,0x00,0x00, D4+20,RR+14,    0,    0, /* 85 ldrb (#4567),r3 */
 0224, 0x35,0x00,0x00,0x00, RR+14,D4+20,    0,    0, /* 86 ldrl r3(#4567) */
 0024, 0x37,0x00,0x00,0x00, D4+20,RR+14,    0,    0, /* 86 ldrl (#4567),r3 */
 0222, 0x7b,0x0a,0x00,0x00,     0,    0,    0,    0, /* 87 mbit  */
 0222, 0x7b,0x0d,0x00,0x00, RR+10,    0,    0,    0, /* 88 mreq r2 */
 0222, 0x7b,0x09,0x00,0x00,     0,    0,    0,    0, /* 89 mres  */
 0222, 0x7b,0x08,0x00,0x00,     0,    0,    0,    0, /* 90 mset  */
 0222, 0x99,0x00,0x00,0x00, RL+14,RR+10,    0,    0, /* 91 mult rr4,r2 */
 0022, 0x19,0x00,0x00,0x00, RL+14,II+10,    0,    0, /* 91 mult rr4,@r2 */
 0024, 0x19,0x00,0x00,0x00, RL+14,X2+20,    0,    0, /* 91 mult rr4,#4 */
 0024, 0x59,0x00,0x00,0x00, RL+14,AA+20,PR+10,    0, /* 91 mult rr4,4567(r2) */
 0024, 0x59,0x00,0x00,0x00, RL+14,AA+20,    0,    0, /* 91 mult rr4,4567 */
 0222, 0x98,0x00,0x00,0x00, RQ+14,RL+10,    0,    0, /* 92 multl rq4,rr2 */
 0022, 0x18,0x00,0x00,0x00, RQ+14,II+10,    0,    0, /* 92 multl rq4,@r2 */
 0026, 0x18,0x00,0x00,0x00, RQ+14,X4+20,    0,    0, /* 92 multl rq4,#4 */
 0024, 0x58,0x00,0x00,0x00, RQ+14,AA+20,PR+10,    0, /* 92 multl rq4,4567(r2) */
 0024, 0x58,0x00,0x00,0x00, RQ+14,AA+20,    0,    0, /* 92 multl rq4,4567 */
 0222, 0x8d,0x02,0x00,0x00, RR+10,    0,    0,    0, /* 93 neg r2 */
 0022, 0x0d,0x02,0x00,0x00, II+10,    0,    0,    0, /* 93 neg @r2 */
 0024, 0x4d,0x02,0x00,0x00, AA+20,PR+10,    0,    0, /* 93 neg 4567(r2) */
 0024, 0x4d,0x02,0x00,0x00, AA+20,    0,    0,    0, /* 93 neg 4567 */
 0222, 0x8c,0x02,0x00,0x00, RB+10,    0,    0,    0, /* 94 negb rh2 */
 0022, 0x0c,0x02,0x00,0x00, II+10,    0,    0,    0, /* 94 negb @r2 */
 0024, 0x4c,0x02,0x00,0x00, AA+20,PR+10,    0,    0, /* 94 negb 4567(r2) */
 0024, 0x4c,0x02,0x00,0x00, AA+20,    0,    0,    0, /* 94 negb 4567 */
 0222, 0x8d,0x07,0x00,0x00,     0,    0,    0,    0, /* 95 nop  */
 0222, 0x85,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /* 96 or r3,r2 */
 0022, 0x05,0x00,0x00,0x00, RR+14,II+10,    0,    0, /* 96 or r3,@r2 */
 0024, 0x05,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /* 96 or r3,#04567 */
 0024, 0x45,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /* 96 or r3,4567(r2) */
 0024, 0x45,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /* 96 or r3,4567 */
 0222, 0x84,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /* 97 orb rh3,rh2 */
 0022, 0x04,0x00,0x00,0x00, RB+14,II+10,    0,    0, /* 97 orb rh3,@r2 */
 0024, 0x04,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /* 97 orb rh3,#045 */
 0024, 0x44,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /* 97 orb rh3,4567(r2) */
 0024, 0x44,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /* 97 orb rh3,4567 */
 0244, 0x3b,0x0a,0x00,0x00, IR+30,II+10,RR+24,    0, /* 98 otdr @r6,@r2,r5 */
 0244, 0x3a,0x0a,0x00,0x00, IR+30,II+10,RR+24,    0, /* 99 otdrb @r6,@r2,r5 */
 0244, 0x3b,0x02,0x00,0x00, IR+30,II+10,RR+24,    0, /*100 otir @r6,@r2,r5 */
 0244, 0x3a,0x02,0x00,0x00, IR+30,II+10,RR+24,    0, /*101 otirb @r6,@r2,r5 */
 0222, 0x3f,0x00,0x00,0x00, II+10,RR+14,    0,    0, /*102 out @r2,r3 */
 0024, 0x3b,0x06,0x00,0x00, D4+20,RR+10,    0,    0, /*102 out (#4567),r2 */
 0222, 0x3e,0x00,0x00,0x00, II+10,RB+14,    0,    0, /*103 outb @r2,rh3 */
 0024, 0x3a,0x06,0x00,0x00, D4+20,RB+10,    0,    0, /*103 outb (#4567),rh2 */
 0244, 0x3b,0x0a,0x00,0x08, IR+30,II+10,RR+24,    0, /*104 outd @r6,@r2,r5 */
 0244, 0x3a,0x0a,0x00,0x08, IR+30,II+10,RR+24,    0, /*105 outdb @r6,@r2,r5 */
 0244, 0x3b,0x02,0x00,0x08, IR+30,II+10,RR+24,    0, /*106 outi @r6,@r2,r5 */
 0244, 0x3a,0x02,0x00,0x08, IR+30,II+10,RR+24,    0, /*107 outib @r6,@r2,r5 */
 0222, 0x97,0x00,0x00,0x00, RR+14,II+10,    0,    0, /*108 pop r3,@r2 */
 0022, 0x17,0x00,0x00,0x00, II+14,II+10,    0,    0, /*108 pop @r3,@r2 */
 0024, 0x57,0x00,0x00,0x00, AA+20,PR+14,II+10,    0, /*108 pop 4567(r3),@r2 */
 0024, 0x57,0x00,0x00,0x00, AA+20,II+10,    0,    0, /*108 pop 4567,@r2 */
 0222, 0x95,0x00,0x00,0x00, RL+14,II+10,    0,    0, /*109 popl rr2,@r2 */
 0022, 0x15,0x00,0x00,0x00, II+14,II+10,    0,    0, /*109 popl @r3,@r2 */
 0024, 0x55,0x00,0x00,0x00, AA+20,PR+14,II+10,    0, /*109 popl 4567(r3),@r2 */
 0024, 0x55,0x00,0x00,0x00, AA+20,II+10,    0,    0, /*109 popl 4567,@r2 */
 0222, 0x93,0x00,0x00,0x00, II+10,RR+14,    0,    0, /*110 push @r2,r3 */
 0022, 0x13,0x00,0x00,0x00, II+10,II+14,    0,    0, /*110 push @r2,@r3 */
 0024, 0x53,0x00,0x00,0x00, II+10,AA+20,PR+14,    0, /*110 push @r2,4567(r3) */
 0024, 0x53,0x00,0x00,0x00, II+10,AA+20,    0,    0, /*110 push @r2,4567 */
 0024, 0x0d,0x09,0x00,0x00, II+10,X2+20,    0,    0, /*111 push @r2,#04567 */
 0222, 0x91,0x00,0x00,0x00, II+10,RL+14,    0,    0, /*111 pushl @r2,rr2 */
 0022, 0x11,0x00,0x00,0x00, II+10,II+14,    0,    0, /*111 pushl @r2,@r3 */
 0024, 0x51,0x00,0x00,0x00, II+10,AA+20,PR+14,    0, /*111 pushl @r2,4567(r3) */
 0024, 0x51,0x00,0x00,0x00, II+10,AA+20,    0,    0, /*111 pushl @r2,4567 */
 0222, 0xa3,0x00,0x00,0x00, RR+10,B4+14,    0,    0, /*112 res r2,#3 */
 0022, 0x23,0x00,0x00,0x00, II+10,B4+14,    0,    0, /*112 res @r2,#3 */
 0044, 0x23,0x00,0x00,0x00, RR+24,RR+14,    0,    0, /*112 res r5,r3 */
 0024, 0x63,0x00,0x00,0x00, AA+20,PR+10,B4+14,    0, /*112 res 4567(r2),#3 */
 0024, 0x63,0x00,0x00,0x00, AA+20,B4+14,    0,    0, /*112 res 4567,#3 */
 0222, 0xa2,0x00,0x00,0x00, RB+10,B4+14,    0,    0, /*113 resb rh2,#3 */
 0022, 0x22,0x00,0x00,0x00, II+10,B4+14,    0,    0, /*113 resb @r2,#3 */
 0044, 0x22,0x00,0x00,0x00, RB+24,RR+14,    0,    0, /*113 resb rh5,r3 */
 0024, 0x62,0x00,0x00,0x00, AA+20,PR+10,B4+14,    0, /*113 resb 4567(r2),#3 */
 0024, 0x62,0x00,0x00,0x00, AA+20,B4+14,    0,    0, /*113 resb 4567,#3 */
 0222, 0x8d,0x03,0x00,0x00, CF+10,CF+10,CF+10,CF+10, /*114 resflg le */
 0222, 0x9e,0x08,0x00,0x00,     0,    0,    0,    0, /*115 ret  */
 0022, 0x9e,0x00,0x00,0x00, CC+14,    0,    0,    0, /*115 ret ule */
 0222, 0xb3,0x00,0x00,0x00, RR+10,BN+16,    0,    0, /*116 rl r2,#9 */
 0222, 0xb2,0x00,0x00,0x00, RB+10,BN+16,    0,    0, /*117 rlb rh2,#9 */
 0222, 0xb3,0x08,0x00,0x00, RR+10,BN+16,    0,    0, /*118 rlc r2,#9 */
 0222, 0xb2,0x08,0x00,0x00, RB+10,BN+16,    0,    0, /*119 rlcb rh2,#9 */
 0222, 0xbe,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*120 rldb rh3,rh2 */
 0222, 0xb3,0x04,0x00,0x00, RR+10,BN+16,    0,    0, /*121 rr r2,#9 */
 0222, 0xb2,0x04,0x00,0x00, RB+10,BN+16,    0,    0, /*122 rrb rh2,#9 */
 0222, 0xb3,0x0c,0x00,0x00, RR+10,BN+16,    0,    0, /*123 rrc r2,#9 */
 0222, 0xb2,0x0c,0x00,0x00, RB+10,BN+16,    0,    0, /*124 rrcb rh2,#9 */
 0222, 0xbc,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*125 rrdb rh3,rh2 */
 0222, 0xb7,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /*126 sbc r3,r2 */
 0222, 0xb6,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*127 sbcb rh3,rh2 */
 0222, 0x7f,0x00,0x00,0x00, X1+10,    0,    0,    0, /*128 sc #023 */
 0244, 0xb3,0x0b,0x00,0x00, RR+10,RR+24,    0,    0, /*129 sda r2,r5 */
 0244, 0xb2,0x0b,0x00,0x00, RB+10,RR+24,    0,    0, /*130 sdab rh2,r5 */
 0244, 0xb3,0x0f,0x00,0x00, RL+10,RR+24,    0,    0, /*131 sdal rr2,r5 */
 0244, 0xb3,0x03,0x00,0x00, RR+10,RR+24,    0,    0, /*132 sdl r2,r5 */
 0244, 0xb2,0x03,0x00,0x00, RB+10,RR+24,    0,    0, /*133 sdlb rh2,r5 */
 0244, 0xb3,0x07,0x00,0x00, RL+10,RR+24,    0,    0, /*134 sdll rr2,r5 */
 0222, 0xa5,0x00,0x00,0x00, RR+10,B4+14,    0,    0, /*135 set r2,#3 */
 0022, 0x25,0x00,0x00,0x00, II+10,B4+14,    0,    0, /*135 set @r2,#3 */
 0044, 0x25,0x00,0x00,0x00, RR+24,RR+14,    0,    0, /*135 set r5,r3 */
 0024, 0x65,0x00,0x00,0x00, AA+20,PR+10,B4+14,    0, /*135 set 4567(r2),#3 */
 0024, 0x65,0x00,0x00,0x00, AA+20,B4+14,    0,    0, /*135 set 4567,#3 */
 0222, 0xa4,0x00,0x00,0x00, RB+10,B4+14,    0,    0, /*136 setb rh2,#3 */
 0022, 0x24,0x00,0x00,0x00, II+10,B4+14,    0,    0, /*136 setb @r2,#3 */
 0044, 0x24,0x00,0x00,0x00, RB+24,RR+14,    0,    0, /*136 setb rh5,r3 */
 0024, 0x64,0x00,0x00,0x00, AA+20,PR+10,B4+14,    0, /*136 setb 4567(r2),#3 */
 0024, 0x64,0x00,0x00,0x00, AA+20,B4+14,    0,    0, /*136 setb 4567,#3 */
 0222, 0x8d,0x01,0x00,0x00, CF+10,CF+10,CF+10,CF+10, /*137 setflg le */
 0224, 0x3b,0x05,0x00,0x00, RR+10,D4+20,    0,    0, /*138 sin r2(#4567) */
 0224, 0x3a,0x05,0x00,0x00, RB+10,D4+20,    0,    0, /*139 sinb rh2(#4567) */
 0244, 0x3b,0x09,0x00,0x08, II+30,II+10,RR+24,    0, /*140 sind @r6,@r2,r5 */
 0244, 0x3a,0x09,0x00,0x08, II+30,II+10,RR+24,    0, /*141 sindb @r6,@r2,r5 */
 0244, 0x3b,0x09,0x00,0x00, II+30,II+10,RR+24,    0, /*142 sindr @r6,@r2,r5 */
 0244, 0x3a,0x09,0x00,0x00, II+30,II+10,RR+24,    0, /*143 sindrb @r6,@r2,r5 */
 0244, 0x3b,0x01,0x00,0x08, II+30,II+10,RR+24,    0, /*144 sini @r6,@r2,r5 */
 0244, 0x3a,0x01,0x00,0x08, II+30,II+10,RR+24,    0, /*145 sinib @r6,@r2,r5 */
 0244, 0x3b,0x01,0x00,0x00, II+30,II+10,RR+24,    0, /*146 sinir @r6,@r2,r5 */
 0244, 0x3a,0x01,0x00,0x00, II+30,II+10,RR+24,    0, /*147 sinirb @r6,@r2,r5 */
 0244, 0xb3,0x09,0x00,0x00, RR+10,P1+20,    0,    0, /*148 sla r2,#7 */
 0244, 0xb2,0x09,0x00,0x00, RB+10,P2+30,    0,    0, /*149 slab rh2,#7 */
 0244, 0xb3,0x0d,0x00,0x00, RL+10,P3+20,    0,    0, /*150 slal rr2,#7 */
 0244, 0xb3,0x01,0x00,0x00, RR+10,P1+20,    0,    0, /*151 sll r2,#7 */
 0244, 0xb2,0x01,0x00,0x00, RB+10,P2+30,    0,    0, /*152 sllb rh2,#7 */
 0244, 0xb3,0x05,0x00,0x00, RL+10,P3+20,    0,    0, /*153 slll rr2,#7 */
 0244, 0x3b,0x0b,0x00,0x00, II+30,II+10,RR+24,    0, /*154 sotdr @r6,@r2,r5 */
 0244, 0x3a,0x0b,0x00,0x00, II+30,II+10,RR+24,    0, /*155 sotdrb @r6,@r2,r5 */
 0244, 0x3b,0x03,0x00,0x00, II+30,II+10,RR+24,    0, /*156 sotir @r6,@r2,r5 */
 0244, 0x3a,0x03,0x00,0x00, II+30,II+10,RR+24,    0, /*157 sotirb @r6,@r2,r5 */
 0224, 0x3b,0x07,0x00,0x00, D4+20,RR+10,    0,    0, /*158 sout (#4567),r2 */
 0224, 0x3a,0x07,0x00,0x00, D4+20,RB+10,    0,    0, /*159 soutb (#4567),rh2 */
 0244, 0x3b,0x0b,0x00,0x08, II+30,II+10,RR+24,    0, /*160 soutd @r6,@r2,r5 */
 0244, 0x3a,0x0b,0x00,0x08, II+30,II+10,RR+24,    0, /*161 soutdb @r6,@r2,r5 */
 0244, 0x3b,0x03,0x00,0x08, II+30,II+10,RR+24,    0, /*162 souti @r6,@r2,r5 */
 0244, 0x3a,0x03,0x00,0x08, II+30,II+10,RR+24,    0, /*163 soutib @r6,@r2,r5 */
 0244, 0xb3,0x09,0x00,0x00, RR+10,N1+20,    0,    0, /*164 sra r2,#25 */
 0244, 0xb2,0x09,0x00,0x00, RB+10,N2+30,    0,    0, /*165 srab rh2,#25 */
 0244, 0xb3,0x0d,0x00,0x00, RL+10,N3+20,    0,    0, /*166 sral rr2,#25 */
 0244, 0xb3,0x01,0x00,0x00, RR+10,N1+20,    0,    0, /*167 srl r2,#25 */
 0244, 0xb2,0x01,0x00,0x00, RB+10,N2+30,    0,    0, /*168 srlb rh2,#25 */
 0244, 0xb3,0x05,0x00,0x00, RL+10,N3+20,    0,    0, /*169 srll rr2,#25 */
 0222, 0x83,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /*170 sub r3,r2 */
 0022, 0x03,0x00,0x00,0x00, RR+14,II+10,    0,    0, /*170 sub r3,@r2 */
 0024, 0x03,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /*170 sub r3,#04567 */
 0024, 0x43,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /*170 sub r3,4567(r2) */
 0024, 0x43,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /*170 sub r3,4567 */
 0222, 0x82,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*171 subb rh3,rh2 */
 0022, 0x02,0x00,0x00,0x00, RB+14,II+10,    0,    0, /*171 subb rh3,@r2 */
 0024, 0x02,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /*171 subb rh3,#045 */
 0024, 0x42,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /*171 subb rh3,4567(r2) */
 0024, 0x42,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /*171 subb rh3,4567 */
 0222, 0x92,0x00,0x00,0x00, RL+14,RL+10,    0,    0, /*172 subl rr2,rr2 */
 0022, 0x12,0x00,0x00,0x00, RL+14,II+10,    0,    0, /*172 subl rr2,@r2 */
 0026, 0x12,0x00,0x00,0x00, RL+14,X4+20,    0,    0, /*172 subl rr2, #0022b */
 0024, 0x52,0x00,0x00,0x00, RL+14,AA+20,PR+10,    0, /*172 subl rr2,4567(r2) */
 0024, 0x52,0x00,0x00,0x00, RL+14,AA+20,    0,    0, /*172 subl rr2,4567 */
 0222, 0xaf,0x00,0x00,0x00, CC+14,RR+10,    0,    0, /*173 tcc ule,r2 */
 0222, 0xae,0x00,0x00,0x00, CC+14,RB+10,    0,    0, /*174 tccb ule,rh2 */
 0222, 0x8d,0x04,0x00,0x00, RR+10,    0,    0,    0, /*175 test r2 */
 0022, 0x0d,0x04,0x00,0x00, II+10,    0,    0,    0, /*175 test @r2 */
 0024, 0x4d,0x04,0x00,0x00, AA+20,PR+10,    0,    0, /*175 test 4567(r2) */
 0024, 0x4d,0x04,0x00,0x00, AA+20,    0,    0,    0, /*175 test 4567 */
 0222, 0x8c,0x04,0x00,0x00, RB+10,    0,    0,    0, /*176 testb rh2 */
 0022, 0x0c,0x04,0x00,0x00, II+10,    0,    0,    0, /*176 testb @r2 */
 0024, 0x4c,0x04,0x00,0x00, AA+20,PR+10,    0,    0, /*176 testb 4567(r2) */
 0024, 0x4c,0x04,0x00,0x00, AA+20,    0,    0,    0, /*176 testb 4567 */
 0222, 0x9c,0x08,0x00,0x00, RL+10,    0,    0,    0, /*177 testl rr2 */
 0022, 0x1c,0x08,0x00,0x00, II+10,    0,    0,    0, /*177 testl @r2 */
 0024, 0x5c,0x08,0x00,0x00, AA+20,PR+10,    0,    0, /*177 testl 4567(r2) */
 0024, 0x5c,0x08,0x00,0x00, AA+20,    0,    0,    0, /*177 testl 4567 */
 0244, 0xb8,0x08,0x00,0x00, II+10,II+30,RR+24,    0, /*178 trdb @r2,@r6,r5 */
 0244, 0xb8,0x0c,0x00,0x00, II+10,II+30,RR+24,    0, /*179 trdrb @r2,@r6,r5 */
 0244, 0xb8,0x00,0x00,0x00, II+10,II+30,RR+24,    0, /*180 trib @r2,@r6,r5 */
 0244, 0xb8,0x04,0x00,0x00, II+10,II+30,RR+24,    0, /*181 trirb @r2,@r6,r5 */
 0244, 0xb8,0x0a,0x00,0x00, II+10,II+30,RR+24,    0, /*182 trtdb @r2,@r6,r5 */
 0244, 0xb8,0x0e,0x00,0x0e, II+10,II+30,RR+24,    0, /*183 trtdrb @r2,@r6,r5 */
 0244, 0xb8,0x02,0x00,0x00, II+10,II+30,RR+24,    0, /*184 trtib @r2,@r6,r5 */
 0244, 0xb8,0x06,0x00,0x0e, II+10,II+30,RR+24,    0, /*185 trtirb @r2,@r6,r5 */
 0222, 0x8d,0x06,0x00,0x00, RR+10,    0,    0,    0, /*186 tset r2 */
 0022, 0x0d,0x06,0x00,0x00, II+10,    0,    0,    0, /*186 tset @r2 */
 0024, 0x4d,0x06,0x00,0x00, AA+20,PR+10,    0,    0, /*186 tset 4567(r2) */
 0024, 0x4d,0x06,0x00,0x00, AA+20,    0,    0,    0, /*186 tset 4567 */
 0222, 0x8c,0x06,0x00,0x00, RB+10,    0,    0,    0, /*187 tsetb rh2 */
 0022, 0x0c,0x06,0x00,0x00, II+10,    0,    0,    0, /*187 tsetb @r2 */
 0024, 0x4c,0x06,0x00,0x00, AA+20,PR+10,    0,    0, /*187 tsetb 4567(r2) */
 0024, 0x4c,0x06,0x00,0x00, AA+20,    0,    0,    0, /*187 tsetb 4567 */
 0222, 0x89,0x00,0x00,0x00, RR+14,RR+10,    0,    0, /*188 xor r3,r2 */
 0022, 0x09,0x00,0x00,0x00, RR+14,II+10,    0,    0, /*188 xor r3,@r2 */
 0024, 0x09,0x00,0x00,0x00, RR+14,X2+20,    0,    0, /*188 xor r3,#04567 */
 0024, 0x49,0x00,0x00,0x00, RR+14,AA+20,PR+10,    0, /*188 xor r3,4567(r2) */
 0024, 0x49,0x00,0x00,0x00, RR+14,AA+20,    0,    0, /*188 xor r3,4567 */
 0222, 0x88,0x00,0x00,0x00, RB+14,RB+10,    0,    0, /*189 xorb rh3,rh2 */
 0022, 0x08,0x00,0x00,0x00, RB+14,II+10,    0,    0, /*189 xorb rh3,@r2 */
 0024, 0x08,0x00,0x00,0x00, RB+14,X1+20,    0,    0, /*189 xorb rh3,#045 */
 0024, 0x48,0x00,0x00,0x00, RB+14,AA+20,PR+10,    0, /*189 xorb rh3,4567(r2) */
 0024, 0x48,0x00,0x00,0x00, RB+14,AA+20,    0,    0, /*189 xorb rh3,4567 */
 0000, 0x00,0x00,0x00,0x00,     0,    0,    0,    0, /*	PHEW! */
};

/*
********	The reserved opcodes table
*/

static const __flash char * const __flash opcodes[] = {
 FSTR(".word"),	/* 01:   1,  0,  0 */
 FSTR("adc"),		/* 02: */
 FSTR("adcb"),		/* 03:  32,  0,  0 */
 FSTR("add"),		/* 04:  12,  0,  0 */
 FSTR("addb"),		/* 05:  17,  0,  0 */
 FSTR("addl"),		/* 06:   3,  0,  0 */
 FSTR("and"),		/* 07:  20,  0,  0 */
 FSTR("andb"),		/* 08:   9,  0,  0 */
 FSTR("bit"),		/* 09:   1,  0,  0 */
 FSTR("bitb"),		/* 0a:  29,  0,  0 */
 FSTR("call"),		/* 0b:   1,  0,  0 */
 FSTR("calr"),		/* 0c:   1,  0,  0 */
 FSTR("clr"),		/* 0d:   1,  0,  0 */
 FSTR("clrb"),		/* 0e:   1,  0,  0 */
 FSTR("com"),		/* 0f:   1,  0,  0 */
 FSTR("comb"),		/* 10:   1,  0,  0 */
 FSTR("comflg"),	/* 11:   1,  0,  0 */
 FSTR("cp"),		/* 12:  29,  0,  0 */
 FSTR("cpb"),		/* 13:   3,  0,  0 */
 FSTR("cpl"),		/* 14:   2,  0,  0 */
 FSTR("cpd"),		/* 15:  28,  0,  0 */
 FSTR("cpdb"),		/* 16:  28,  0,  0 */
 FSTR("cpdr"),		/* 17:   6,  0,  0 */
 FSTR("cpdrb"),	/* 18:   6,  0,  0 */
 FSTR("cpi"),		/* 19:   1,  0,  0 */
 FSTR("cpib"),		/* 1a:   2,  0,  0 */
 FSTR("cpir"),		/* 1b:   1,  0,  0 */
 FSTR("cpirb"),	/* 1c:   1,  0,  0 */
 FSTR("cpsd"),		/* 1d:  10,  0,  0 */
 FSTR("cpsdb"),	/* 1e:   7,  0,  0 */
 FSTR("cpsdr"),	/* 1f:   2,  0,  0 */
 FSTR("cpsdrb"),	/* 20:   1,  0,  0 */
 FSTR("cpsi"),		/* 21:   1,  0,  0 */
 FSTR("cpsib"),	/* 22:   1,  0,  0 */
 FSTR("cpsir"),	/* 23:   1,  0,  0 */
 FSTR("cpsirb"),	/* 24:   1,  0,  0 */
 FSTR("dab"),		/* 25:   1,  0,  0 */
 FSTR("dec"),		/* 26:  22,  0,  0 */
 FSTR("decb"),		/* 27:  29,  0,  0 */
 FSTR("di"),		/* 28:   3,  0,  0 */
 FSTR("div"),		/* 29:   1,  0,  0 */
 FSTR("divl"),		/* 2a:   1,  0,  0 */
 FSTR("djnz"),		/* 2b:   1,  0,  0 */
 FSTR("dbjnz"),	/* 2c:   1,  0,  0 */
 FSTR("ei"),		/* 2d:   1,  0,  0 */
 FSTR("ex"),		/* 2e:   1,  0,  0 */
 FSTR("exb"),		/* 2f:   1,  0,  0 */
 FSTR("exts"),		/* 30:   1,  0,  0 */
 FSTR("extsb"),	/* 31:   1,  0,  0 */
 FSTR("extsl"),	/* 32:   1,  0,  0 */
 FSTR("halt"),		/* 33:  20,  0,  0 */
 FSTR("in"),		/* 34:  14,  0,  0 */
 FSTR("inb"),		/* 35: 311,  0,  0 */
 FSTR("inc"),		/* 36:  24,  0,  0 */
 FSTR("incb"),		/* 37:   6,  0,  0 */
 FSTR("ind"),		/* 38:   1,  0,  0 */
 FSTR("indb"),		/* 39:   1,  0,  0 */
 FSTR("indr"),		/* 3a:   1,  0,  0 */
 FSTR("indrb"),	/* 3b:   1,  0,  0 */
 FSTR("ini"),		/* 3c:   6,  0,  0 */
 FSTR("inib"),		/* 3d:   6,  0,  0 */
 FSTR("inir"),		/* 3e:   6,  0,  0 */
 FSTR("inirb"),	/* 3f:  10,  0,  0 */
 FSTR("iret"),		/* 40:  28,  0,  0 */
 FSTR("jp"),		/* 41:  28,  0,  0 */
 FSTR("jr"),		/* 42:   6,  0,  0 */
 FSTR("ld"),		/* 43:   6,  0,  0 */
 FSTR("ldb"),		/* 44:   2,  0,  0 */
 FSTR("ldl"),		/* 45:   1,  0,  0 */
 FSTR("lda"),		/* 46:  17,  0,  0 */
 FSTR("ldar"),		/* ?? */
 FSTR("ldctl"),	/* 47:  22,  0,  0 */
 FSTR("ldctlb"),	/* 48:   1,  0,  0 */
 FSTR("ldd"),		/* 49:   1,  0,  0 */
 FSTR("lddb"),		/* 4a:   1,  0,  0 */
 FSTR("lddr"),		/* 4b:   1,  0,  0 */
 FSTR("lddrb"),	/* 4c:   1,  0,  0 */
 FSTR("ldi"),		/* 4d:   1,  0,  0 */
 FSTR("ldib"),		/* 4e:   1,  0,  0 */
 FSTR("ldir"),		/* 4f:   1,  0,  0 */
 FSTR("ldirb"),	/* 50:   1,  0,  0 */
 FSTR("ldk"),		/* 51:   8,  0,  0 */
 FSTR("ldm"),		/* 52:   1,  0,  0 */
 FSTR("ldps"),		/* 53:   9,  0,  0 */
 FSTR("ldr"),		/* 54:   1,  0,  0 */
 FSTR("ldrb"),		/* 55:   3,  0,  0 */
 FSTR("ldrl"),		/* 56:   9,  0,  0 */
 FSTR("mbit"),		/* 57:   1,  0,  0 */
 FSTR("mreq"),		/* 58:   1,  0,  0 */
 FSTR("mres"),		/* 59:   1,  0,  0 */
 FSTR("mset"),		/* 5a:   1,  0,  0 */
 FSTR("mult"),		/* 5b:   3,  0,  0 */
 FSTR("multl"),	/* 5c:   1,  0,  0 */
 FSTR("neg"),		/* 5d:   3,  0,  0 */
 FSTR("negb"),		/* 5e:   1,  0,  0 */
 FSTR("nop"),		/* 5f:   1,  0,  0 */
 FSTR("or"),		/* 60:   3,  0,  0 */
 FSTR("orb"),		/* 61:   1,  0,  0 */
 FSTR("otdr"),		/* 62:   3,  0,  0 */
 FSTR("otdrb"),	/* 63:   1,  0,  0 */
 FSTR("otir"),		/* 64:   1,  0,  0 */
 FSTR("otirb"),	/* 65:   1,  0,  0 */
 FSTR("out"),		/* 66:  29,  0,  0 */
 FSTR("outb"),		/* 67:   1,  0,  0 */
 FSTR("outd"),		/* 68:   1,  0,  0 */
 FSTR("outdb"),	/* 69:   3,  0,  0 */
 FSTR("outi"),		/* 6a:   3,  0,  0 */
 FSTR("outib"),	/* 6b:   3,  0,  0 */
 FSTR("pop"),		/* 6c:   3,  0,  0 */
 FSTR("popl"),		/* 6d:  17,  0,  0 */
 FSTR("push"),		/* 6e:  12,  0,  0 */
 FSTR("pushl"),	/* 6f:   3,  0,  0 */
 FSTR("res"),		/* 70:   1,  0,  0 */
 FSTR("resb"),		/* 71:  17,  0,  0 */
 FSTR("resflg"),	/* 72:   0,  0,  0 */
 FSTR("ret"),		/* 73:   0,  0,  0 */
 FSTR("rl"),		/* 74:   0,  0,  0 */
 FSTR("rlb"),		/* 75:   0,  0,  0 */
 FSTR("rlc"),		/* 76:   0,  0,  0 */
 FSTR("rlcb"),		/* 77:   0,  0,  0 */
 FSTR("rldb"),		/* 78:   0,  0,  0 */
 FSTR("rr"),		/* 79:   0,  0,  0 */
 FSTR("rrb"),		/* 7a:   0,  0,  0 */
 FSTR("rrc"),		/* 7b:   0,  0,  0 */
 FSTR("rrcb"),		/* 7c:   0,  0,  0 */
 FSTR("rrdb"),		/* 7d:   0,  0,  0 */
 FSTR("sbc"),		/* 7e:   0,  0,  0 */
 FSTR("sbcb"),		/* 7f:   0,  0,  0 */
 FSTR("sc"),		/* 80:   0,  0,  0 */
 FSTR("sda"),		/* 81:   0,  0,  0 */
 FSTR("sdab"),		/* 82:   0,  0,  0 */
 FSTR("sdal"),		/* 83:   0,  0,  0 */
 FSTR("sdl"),		/* 84:   0,  0,  0 */
 FSTR("sdlb"),		/* 85:   0,  0,  0 */
 FSTR("sdll"),		/* 86:   0,  0,  0 */
 FSTR("set"),		/* 87:   0,  0,  0 */
 FSTR("setb"),		/* 88:   0,  0,  0 */
 FSTR("setflg"),	/* 89:   0,  0,  0 */
 FSTR("sin"),		/* 8a:   0,  0,  0 */
 FSTR("sinb"),		/* 8b:   0,  0,  0 */
 FSTR("sind"),		/* 8c:   0,  0,  0 */
 FSTR("sindb"),	/* 8d:   0,  0,  0 */
 FSTR("sindr"),	/* 8e:   0,  0,  0 */
 FSTR("sindrb"),	/* 8f:   0,  0,  0 */
 FSTR("sini"),		/* 90:   0,  0,  0 */
 FSTR("sinib"),	/* 91:   0,  0,  0 */
 FSTR("sinir"),	/* 92:   0,  0,  0 */
 FSTR("sinirb"),	/* 93:   0,  0,  0 */
 FSTR("sla"),		/* 94:   0,  0,  0 */
 FSTR("slab"),		/* 95:   0,  0,  0 */
 FSTR("slal"),		/* 96:   0,  0,  0 */
 FSTR("sll"),		/* 97:   0,  0,  0 */
 FSTR("sllb"),		/* 98:   0,  0,  0 */
 FSTR("slll"),		/* 99:   0,  0,  0 */
 FSTR("sotdr"),	/* 9a:   0,  0,  0 */
 FSTR("sotdrb"),	/* 9b:   0,  0,  0 */
 FSTR("sotir"),	/* 9c:   0,  0,  0 */
 FSTR("sotirb"),	/* 9d:   0,  0,  0 */
 FSTR("sout"),		/* 9e:   0,  0,  0 */
 FSTR("soutb"),	/* 9f:   0,  0,  0 */
 FSTR("soutd"),	/* a0:   0,  0,  0 */
 FSTR("soutdb"),	/* a1:   0,  0,  0 */
 FSTR("souti"),	/* a2:   0,  0,  0 */
 FSTR("soutib"),	/* a3:   0,  0,  0 */
 FSTR("sra"),		/* a4:   0,  0,  0 */
 FSTR("srab"),		/* a5:   0,  0,  0 */
 FSTR("sral"),		/* a6:   0,  0,  0 */
 FSTR("srl"),		/* a7:   0,  0,  0 */
 FSTR("srlb"),		/* a8:   0,  0,  0 */
 FSTR("srll"),		/* a9:   0,  0,  0 */
 FSTR("sub"),		/* aa:   0,  0,  0 */
 FSTR("subb"),		/* ab:   0,  0,  0 */
 FSTR("subl"),		/* ac:   0,  0,  0 */
 FSTR("tcc"),		/* ad:   0,  0,  0 */
 FSTR("tccb"),		/* ae:   0,  0,  0 */
 FSTR("test"),		/* af:   0,  0,  0 */
 FSTR("testb"),	/* b0:   0,  0,  0 */
 FSTR("testl"),	/* b1:   0,  0,  0 */
 FSTR("trdb"),		/* b2:   0,  0,  0 */
 FSTR("trdrb"),	/* b3:   0,  0,  0 */
 FSTR("trib"),		/* b4:   0,  0,  0 */
 FSTR("trirb"),	/* b5:   0,  0,  0 */
 FSTR("trtdb"),	/* b6:   0,  0,  0 */
 FSTR("trtdrb"),	/* b7:   0,  0,  0 */
 FSTR("trtib"),	/* b8:   0,  0,  0 */
 FSTR("trtirb"),	/* b9:   0,  0,  0 */
 FSTR("tset"),		/* ba:   0,  0,  0 */
 FSTR("tsetb"),	/* bb:   0,  0,  0 */
 FSTR("xor"),		/* bc:   0,  0,  0 */
 FSTR("xorb"),		/* bd:   0,  0,  0 */
 FSTR(""),		/* be:   0,  0,  0 */
};

/*
********	The reserved operands
*/

static const __flash struct symbol oprands[] = {
 0,	0,	"",	/*   0 */
 RR,	0,	"r0",	/*   1 */
 RR,	1,	"r1",	/*   2 */
 RR,	2,	"r2",	/*   3 */
 RR,	3,	"r3",	/*   4 */
 RR,	4,	"r4",	/*   5 */
 RR,	5,	"r5",	/*   6 */
 RR,	6,	"r6",	/*   7 */
 RR,	7,	"r7",	/*   8 */
 RR,	8,	"r8",	/*   9 */
 RR,	9,	"r9",	/*  10 */
 RR,	10,	"r10",	/*  11 */
 RR,	11,	"r11",	/*  12 */
 RR,	12,	"r12",	/*  13 */
 RR,	13,	"r13",	/*  14 */
 RR,	14,	"r14",	/*  15 */
 RR,	15,	"r15",	/*  16 */
 RB,	0,	"rh0",	/*  17 */
 RB,	1,	"rh1",	/*  18 */
 RB,	2,	"rh2",	/*  19 */
 RB,	3,	"rh3",	/*  20 */
 RB,	4,	"rh4",	/*  21 */
 RB,	5,	"rh5",	/*  22 */
 RB,	6,	"rh6",	/*  23 */
 RB,	7,	"rh7",	/*  24 */
 RB,	8,	"rl0",	/*  25 */
 RB,	9,	"rl1",	/*  26 */
 RB,	10,	"rl2",	/*  27 */
 RB,	11,	"rl3",	/*  28 */
 RB,	12,	"rl4",	/*  29 */
 RB,	13,	"rl5",	/*  30 */
 RB,	14,	"rl6",	/*  31 */
 RB,	15,	"rl7",	/*  32 */
 RL,	0,	"rr0",	/*  33 */
 RL,	1,	"rr1?",	/*  34 */
 RL,	2,	"rr2",	/*  35 */
 RL,	3,	"rr3?",	/*  36 */
 RL,	4,	"rr4",	/*  37 */
 RL,	5,	"rr5?",	/*  38 */
 RL,	6,	"rr6",	/*  39 */
 RL,	7,	"rr7?",	/*  40 */
 RL,	8,	"rr8",	/*  41 */
 RL,	9,	"rr9?",	/*  42 */
 RL,	10,	"rr10",	/*  43 */
 RL,	11,	"rr11?",	/*  44 */
 RL,	12,	"rr12",	/*  45 */
 RL,	13,	"rr13?",	/*  46 */
 RL,	14,	"rr14",	/*  47 */
 RL,	15,	"rr15?",	/*  48 */
 RQ,	0,	"rq0",	/*  49 */
 RQ,	4,	"rq4",	/*  50 */
 RQ,	8,	"rq8",	/*  51 */
 RQ,	12,	"rq12",	/*  52 */
  0,	0,	"X",	/*  53 */
  0,	0,	"X",	/*  54 */
  0,	0,	"X",	/*  55 */
  0,	0,	"X",	/*  56 */
  0,	0,	"Y",	/*  57 */
 II,	1,	"@r1",	/*  58 */
 II,	2,	"@r2",	/*  59 */
 II,	3,	"@r3",	/*  60 */
 II,	4,	"@r4",	/*  61 */
 II,	5,	"@r5",	/*  62 */
 II,	6,	"@r6",	/*  63 */
 II,	7,	"@r7",	/*  64 */
 II,	8,	"@r8",	/*  65 */
 II,	9,	"@r9",	/*  66 */
 II,	10,	"@r10",	/*  67 */
 II,	11,	"@r11",	/*  68 */
 II,	12,	"@r12",	/*  69 */
 II,	13,	"@r13",	/*  70 */
 II,	14,	"@r14",	/*  71 */
 II,	15,	"@r15",	/*  72 */
  0,	0,	"Y",	/*  73 */
 IL,	2,	"@rr2",	/*  74 */
 IL,	4,	"@rr4",	/*  75 */
 IL,	6,	"@rr6",	/*  76 */
 IL,	8,	"@rr8",	/*  77 */
 IL,	10,	"@rr10",	/*  78 */
 IL,	12,	"@rr12",	/*  79 */
 IL,	14,	"@rr14",	/*  80 */
  0,	0,	"Y",	/*  81 */
  0,	0,	"Y",	/*  82 */
  0,	0,	"Y",	/*  83 */
  0,	0,	"Y",	/*  84 */
  0,	0,	"Y",	/*  85 */
  0,	0,	"Y",	/*  86 */
  0,	0,	"Y",	/*  87 */
  0,	0,	"Y",	/*  88 */
  0,	0,	"Y",	/*  89 */
 PR,	1,	"(r1)",	/*  90 */
 PR,	2,	"(r2)",	/*  91 */
 PR,	3,	"(r3)",	/*  92 */
 PR,	4,	"(r4)",	/*  93 */
 PR,	5,	"(r5)",	/*  94 */
 PR,	6,	"(r6)",	/*  95 */
 PR,	7,	"(r7)",	/*  96 */
 PR,	8,	"(r8)",	/*  97 */
 PR,	9,	"(r9)",	/*  98 */
 PR,	10,	"(r10)",	/*  99 */
 PR,	11,	"(r11)",	/* 100 */
 PR,	12,	"(r12)",	/* 101 */
 PR,	13,	"(r13)",	/* 102 */
 PR,	14,	"(r14)",	/* 103 */
 PR,	15,	"(r15)",	/* 104 */
  0,	0,	"Y",	/* 105 */
 PL,	2,	"(rr2)",	/* 106 */
 PL,	4,	"(rr4)",	/* 107 */
 PL,	6,	"(rr6)",	/* 108 */
 PL,	8,	"(rr8)",	/* 109 */
 PL,	10,	"(rr10)",	/* 110 */
 PL,	12,	"(rr12)",	/* 111 */
 PL,	14,	"(rr14)",	/* 112 */
 SR,	0,	"cr0?",	/* 113 */
 SR,	1,	"cr1?",	/* 114 */
 SR,	2,	"fcw",	/* 115 */
 SR,	3,	"refresh",	/* 116 */
 SR,	4,	"psapseg",	/* 117 */
 SR,	5,	"psapoff",	/* 118 */
 SR,	6,	"nspseg",	/* 119 */
 SR,	7,	"nspoff",	/* 120 */
 CC,	0,	"Y",	/* 121 */
 CC,	1,	"lt",	/* 122 */
 CC,	2,	"le",	/* 123 */
 CC,	3,	"ule",	/* 124 */
 CC,	4,	"ov",	/* 125 */
 CC,	5,	"mi",	/* 126 */
 CC,	6,	"z",	/* 127 */
 CC,	7,	"cy",	/* 128 */
 CC,	8,	" ",	/* 129 */
 CC,	9,	"ge",	/* 130 */
 CC,	10,	"gt",	/* 131 */
 CC,	11,	"ugt",	/* 132 */
 CC,	12,	"nov",	/* 133 */
 CC,	13,	"pl",	/* 134 */
 CC,	14,	"nz",	/* 135 */
 CC,	15,	"nc",	/* 136 */
 CC,	4,	"pe",	/* 137 */
 CC,	6,	"eq",	/* 138 */
 CC,	7,	"ult",	/* 139 */
 CC,	12,	"po",	/* 140 */
 CC,	14,	"ne",	/* 141 */
 CC,	15,	"uge",	/* 142 */
  0,	0,	"???????",	/* 143 */
 VI,	1,	"nvi",	/* 144 */
 VI,	2,	"vi",	/* 145 */
 VI,	3,	"vi,nvi",	/* 146 */
 UR,	1,	"flags",	/* 147 */
  0,	0,	""	/* 148 */
};


/*

**	*************************************************
**	*						*
**	*	S U B R O U T I N E S   F O R   Z 8 K	*
**	*						*
**	*************************************************
**
*/


/*
********	Memory access routines
*/

UWORD (*ldintp) (LONG addr) = NULL;


static WORD ldint (LONG addr)
{
  if (ldintp)
  {
#if CACHED
    struct cache *pp = NULL;
    for (struct cache *p = op_cache; p < &op_cache[LENGTH(op_cache)]; ++p)
    {
      if (p->valid)
      {
        if (p->addr == addr)
        {
          return p->data;
        }
      }
      else if (!pp)
      {
        pp = p;
      }
    };

    if (!pp)
    {
      pp = &op_cache[LENGTH(op_cache)-1];
    }
    pp->valid = 1;
    pp->addr = addr;
    pp->data = ldintp (addr);
    return pp->data;
#else
    return ldintp (addr);
#endif
  };

  return 0;
}


static BYTE ldchr (LONG addr)
{
  const WORD w = ldint (addr & ~1ul);
  return (addr & 1ul) ? w & 0xFF : w >> 8;
}




/*
************************************************************************
**
********	deasm (base,lptr)	- standard call to the disassembler
** R	base=	a pointer to memory of where to disassemble binary, it is
**		a pointer to unsigned characters.
** W	lptr=	is a pointer to a host provided buffer < 40 chars that deasm
**		will use for output, it will be '\0' terminated without a '\n'.
**
** This function only dispatches to the following disassembler subroutines:
** first:	"difind" to scan for an ops[] index for a matching binary
**		instruction template at "base".
** second:	"disbin" to encode the binary half of the matched opcode
**		string into "iscan" (ex "1234: 56 78 9A ").
** third:	"disop" to encode and concatenate the nmeumonic half of
**		the matched opcode after "iscan" (ex "ld a,b").
**
** deasm will return a pointer to the found indexed item of ops[], which is
** an array of structures "op". Error conditions can only return as ".byte ~"
** To bump "base" to the next binary address, call "getnext" with the index
** returned from "deasm".
**
**
*/

int deasm (LONG base, char *s)
{
	int	opnum;

	error = 0;
        lptr = s;
#if CACHED
        for (struct cache *p = op_cache; p < &op_cache[LENGTH(op_cache)]; ++p)
        {
          p->valid = 0;
        };
#endif
	opnum= deasm_find (base);		/* score and match binary */
	if (error)
          return (0);

	disop (base,opnum);		        /* encode the nmemonic half */
	if (error)
          return (0);

	return (opnum);
}

/*

********	deasm_find (base)	- search for a binary match for disassembler
** R	base=	a pointer to memory of where to disassemble binary, it is
**		a pointer to unsigned characters.
**
** This function scans all of the templates on ops[] and by using the
** template control words, searches for a match. If none are found, then
** a zero is returned, which in "ops[]" is a pointer to a ".byte" opcode.
** no error codes are returned, except 0 for not found.
**
*/

int deasm_find (LONG base)
{
	BYTE	m[MAXF],b[MAXF];
	int	opnum;
	int	opbytes;

	opcode= 0;
	for (opnum=1; ops[opnum].opc; opnum++)	{
		if (ops[opnum].opc & 0200)	opcode++;
		opbytes= (ops[opnum].opc >> 3) & 07;

		for (args=0; args < MAXA; args++)	{
			m[args]= ldchr (base+args);
			b[args]= ops[opnum].opb[args];
		}
		for (args=0; args < MAXA; args++)	{
			swinit (opnum,args);
			if (!opsa)
                          break;

			switch (fldtype)	{	/* in difind */
			case X4:
				m[offset+3]= 00;
				m[offset+2]= 00;
			case X2:
				m[offset+1]= 00;
			case X1:
				m[offset]= 00;
				break;
			case II:
			case IR:
			case IL:
			case BB:
			case BR:
			case BL:
			case PP:
			case PR:
			case PL:
				if ((bitnum==0) && !(m[offset] & 0xF0)) {
                                  opbytes= 0;
                                  break;
                                }
				if ((bitnum==4) && !(m[offset] & 0x0F))	{
                                  opbytes= 0;
                                  break;
                                }
			case I0:
			case P0:
			case B4:
			case RB:
			case RR:
			case RL:
			case RQ:
			case CC:
			case CF:
			case ID:
				if (bitnum == 0) m[offset]&= 0x0F;
				if (bitnum == 4) m[offset]&= 0xF0;
				break;
			case SR:
				m[offset]&= 0xF8;
			case UR:
				break;
			case D1:
				m[offset]&= 0x80;
                                break;
			case D2:
				m[offset]= 00;	
                                break;
			case D3:
				m[offset]&= 0xF0; m[offset+1]= 0;
                                break;
			case D4:
			case D5:
				m[offset]= m[offset+1]= 0;	
                                break;
			case AA:
				if (bitnum == 0) m[offset]= m[offset+1]= 00;
				break;
			case VI:
				m[offset] &= 0xFC;
				break;
			case BN:
				m[offset] &= 0xFD;
				break;
			case N1:
			case N3:
				if ((m[offset] & 0x80) == 0)	opbytes=0;
				m[offset]= 0;	m[offset+1]= 0;
				break;
			case N2:
				if ((m[offset] & 0x80) == 0)	opbytes=0;
				m[offset]= 0;
				break;
			case P1:
			case P3:
				if ((m[offset] & 0x80) != 0)	opbytes=0;
				m[offset]= 0;	m[offset+1]= 0;
				break;
			case P2:
				if ((m[offset] & 0x80) != 0)	opbytes=0;
				m[offset]= 0;
				break;
			default:
				break;
			}
		}

		if (!opbytes)
                  continue;
		if (m[0] != b[0])
                  continue;
		if (opbytes == 1)
                  return (opnum);
		if (m[1] != b[1])
                  continue;
		if (opbytes == 2)
                  return (opnum);
		if (m[2] != b[2])
                  continue;
		if (opbytes == 3)
                  return (opnum);
		if (m[3] != b[3])
                  continue;
		if (opbytes == 4)
                  return (opnum);
	}
	opcode=opnum= 0;
	return (opnum);
}

/*

********	disop (base,opnum,lptr)	- encode nmemonic half into ascii
** R	base=	a pointer to memory of where to disassemble binary, it is
**		a pointer to unsigned characters.
** R	opnum=	an index within "ops[]", which is an array of structures "op"
**		That has all of the necessary template data for the opcode.
**		opnum is the value returned by difind (or asfind).
** W	lptr=	is a pointer to a host provided buffer < 40 chars that deasm
**		will use for output, it will be '\0' terminated without a '\n'.
**
** This subroutine computes the length of the opcode, and encodes a string
** of the nmeumonic for ops[opnum].
** The string in encoded into "lptr", and is '\0' terminated without a '\n'.
** No error conditions returned.
** (ex if base=1234, and base=00, lptr will return "nop").
**
*/

static void disop (LONG base, int opnum)
{
	LONG    boff;
	int	field,fldctr;
	int	topnum;

	opcode= 0;
	for (topnum=1; (topnum <= opnum) && ops[topnum].opc; topnum++)
		if (ops[topnum].opc & 0200)	opcode++;

	fldctr=0;
	lptr += sprintf_P (lptr, PSTR("%S "), opcodes[opcode]);
	for (args= 0; args < MAXA; args++)	{
		swinit (opnum,args);
		if (!opsa)
                  break;
		if (args && fldtype != PR && fldtype != D4 && fldtype != CF)
			lptr += sprintf_P (lptr, PSTR(","));
		boff= base + offset;

		switch (fldtype)	{	/* in disop */
		case XBWD:
		case XBOF:
		case B3:
			tvalue= (ldchr(boff) >> (5-bitnum)) & 0x07;
			lptr += sprintf_P (lptr, PSTR("%01lx"), tvalue);
			break;
		case ID:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0F;
			lptr += sprintf_P (lptr, PSTR("#%lx"), tvalue+1);
			break;
		case B4:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0F;
			lptr += sprintf_P (lptr, PSTR("#%01lx"), tvalue);
			break;
		case X1:
			tvalue= ldchr(boff) & 0xFF;
			lptr += sprintf_P (lptr, PSTR("#%02lx"), tvalue);
                        break;
		case X2:
			tvalue= ldint(boff);
			lptr += sprintf_P (lptr, PSTR("#%04lx"), tvalue);
                        break;
		case X4:
			tvalue= ldint(boff);
			lptr += sprintf_P (lptr, PSTR(" #%04lx"), tvalue);
			tvalue= ldint(boff+2);
			lptr += sprintf_P (lptr, PSTR("%04lx"), tvalue);
                        break;
		case IR:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0F;
			field= typfind(II,(int)tvalue);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case RB:
		case RR:
		case II:
		case PP:
		case PR:
		case CC:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0F;
			field= typfind(fldtype, tvalue);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case CF:
			ivalue= 7-args;
			if ((ldchr(boff) & (1 << ivalue)) == 0)	break;
			field= typfind(CC,ivalue);
			if (fldctr++)	lptr += sprintf_P (lptr, PSTR(","));
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case BB:
		case BR:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0F;
			field= typfind(RR,(int)tvalue);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case SR:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x07;
			field= typfind(fldtype,(int)tvalue);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case UR:
			field= typfind(fldtype,1);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case RQ:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0C;
			field= typfind(fldtype,(int)tvalue);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case RL:
		case IL:
		case PL:
			tvalue= (ldchr(boff) >> (4-bitnum)) & 0x0E;
			field= typfind(fldtype,(int)tvalue);
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case D1:
			ivalue= base + 2 - 2 * (ldchr(boff) & 0x7F);
			lptr += sprintf_P (lptr, PSTR("%04x"), ivalue);
                        break;
		case D2:
			ivalue= base + 2 + 2 * ((ldchr(boff) << 8) >> 8);
			lptr += sprintf_P (lptr, PSTR("%04x"), ivalue);
                        break;
		case D3:
			ivalue= base + 2 - 2 * ((ldint(boff) << 4) >> 4);
			lptr += sprintf_P (lptr, PSTR("%04x"), ivalue);
                        break;
		case D4:
			ivalue= ldint(boff);
			lptr += sprintf_P (lptr, PSTR("(#%04x)"), ivalue);
                        break;
		case D5:
		case AA:
			ivalue= (ldint(boff) & 0xFFFF);
			lptr += sprintf_P (lptr, PSTR("%04x"), ivalue);
                        break;
		case VI:
			if (args)
                          break;
			field= typfind(fldtype,(int)(ldchr(boff) & 0x03));
			lptr += sprintf_P (lptr, PSTR("%S"), oprands[field].uname);
			break;
		case BN:
			ivalue= ((ldchr(boff) >> 1) & 0x01) + 1;
			lptr += sprintf_P (lptr, PSTR("#%01x"), ivalue);
			break;
		case P1:
		case P3:
			ivalue= ldint(boff) & 0x1F;
			lptr += sprintf_P (lptr, PSTR("#%d"), ivalue);
			break;
		case P2:
			ivalue= ldchr(boff) & 0x7;
			lptr += sprintf_P (lptr, PSTR("#%d"), ivalue);
			break;
		case N1:
		case N3:
			ivalue= -ldint(boff) & 0x1F;
			lptr += sprintf_P (lptr, PSTR("#%d"), ivalue);
			break;
		case N2:
			ivalue= -ldchr(boff) & 0x1F;
			lptr += sprintf_P (lptr, PSTR("#%d"), ivalue);
			break;
		default:
			error=44;
                        break;
		}
		if (error)
                  break;
		types[args+1]= fldtype;
		values[args+1]= tvalue;
	}
}


/*

************************************************************************
**
********	deasm_getnext (opnum)	- compute length of opcode in bytes.
** R	opnum=	an index within "ops[]", which is an array of structures "op"
**		That has all of the necessary template data for the opcode.
**		opnum is the value returned by difind (or asfind).
**
** This subroutine computes the length of the opcode, if opnum=0 indicating
** an unknown opcode, the return vaule is a length of 1.
** If you have an opnum from either "asfind" or "difind", you can bump the
** base to point to the next opcode by typing base+= getnext(opnum).
**
*/

int deasm_getnext( int opnum)
{
	return ops[opnum].opc & 07;
}

/*
********	typfind (v)	- find the oprands[] index for a type
**
*/

static int typfind (int t, int v)
{
	int	i;
	for (i= 1; *oprands[i].uname != '\0'; i++)	{
		if (oprands[i].utype != t)	continue;
		if (oprands[i].uvalue != v)	continue;
		return (i);
	}
	return (0);
}


static void swinit (int opnum, int field)
{
	opsa= ops[opnum].opa[field];
	fldtype= opsa & 0xFF00;
	bitoff=  opsa & 0x00FF;
	offset= bitoff / 10;
	bitnum= bitoff - (offset * 10);
	ttype= types[field+1];
	tvalue= values[field+1];
	ivalue= tvalue;
}
/********	END	********/

