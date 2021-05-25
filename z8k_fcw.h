/* z8k_fcw.h */


#ifndef Z8K_FCW_H
#define Z8K_FCW_H


#define FCW_SEGMENTED   0b1000000000000000      /* segmented mode */
#define FCW_SYSTEM      0b0100000000000000      /* system mode */
#define FCW_EPA         0b0010000000000000      /* epu connected */
#define FCW_VIE         0b0001000000000000      /* vectored interrupt enable */
#define FCW_NVIE        0b0000100000000000      /* non vectored interrupt enable */
#define FCW_C           0b0000000010000000      /* carry flag */
#define FCW_Z           0b0000000001000000      /* zero flag */
#define FCW_S           0b0000000000100000      /* sign flag */
#define FCW_PV          0b0000000000010000      /* parity/overflow flag */
#define FCW_DA          0b0000000000001000      /* decimal adjust */
#define FCW_H           0b0000000000000100      /* half carry */


#endif
