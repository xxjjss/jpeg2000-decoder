#pragma once

//////////////////////////////////////////////////////////////////////////
// MACRO relate to expanded context word
//////////////////////////////////////////////////////////////////////////
// contextWords :	refer to fig 17.2 in P448, But the order in fig is CHI-MU-PI
//					here the context word we using in in the order of MU-PI-CHI
// lower Sigma :	bot 0 - 17, a 3*6 band sig indicator, we can use the NEIGHBOR_SIGMA
//					to get the corresponding neighbors' sig flag
// MU (?:  bit 20; 23; 26; 29
// PI (p):  bit 21; 24; 27; 30

// B: Bottom  T: Top  R: Right L: Left C: Center
#define SIGMA_TL		0x01
#define SIGMA_TC		0x02
#define SIGMA_TR		0x04
#define SIGMA_CL		0x08
#define SIGMA_SELF		0x10
#define SIGMA_SELF_POS	4
#define SIGMA_CR		0x20
#define SIGMA_BL		0x40
#define SIGMA_BC		0x80
#define SIGMA_BR		0x100
#define NEIGHBOR_SIGMA_ROW0	0x1ef						//    1 1110 1111				0x1ef	: all neighbors' sigma of a center pixel: TL, TC, TR, CL, CC(0), CR, BL, BC, BR
#define NEIGHBOR_SIGMA_ROW1 (NEIGHBOR_SIGMA_ROW0<<3)	// 1111 0111 1000				0xf78
#define NEIGHBOR_SIGMA_ROW2 (NEIGHBOR_SIGMA_ROW1<<3)	//  111 1011 1100 0000			0x7bc0
#define NEIGHBOR_SIGMA_ROW3 (NEIGHBOR_SIGMA_ROW2<<3)	//   11 1101 1110 0000 0000		0x3de00

#define CONTEXT_MU_POS	20
#define CONTEXT_MU		(0x01 << CONTEXT_MU_POS)	// The first MU bit in context words	0x100000
#define CONTEXT_PI_POS	21
#define CONTEXT_PI		(0x01 << CONTEXT_PI_POS)	// The first PI bit in context words	0x200000

// CHI (x): bit 18(prev); 19; 22; 25; 28; 31(next)
#define CONTEXT_FIRST_CHI_POS		19
#define CONTEXT_CHI		(0x01 << CONTEXT_FIRST_CHI_POS)	// The first CHI bit in context words	0x80000
// #define CONTEXT_CHI_ROW1	(CONTEXT_CHI_ROW0<<3)
// #define CONTEXT_CHI_ROW2	(CONTEXT_CHI_ROW1<<3)
// #define CONTEXT_CHI_ROW3	(CONTEXT_CHI_ROW2<<3)
#define CONTEXT_PREV_CHI_POS		18
#define CONTEXT_CHI_PREV	(0x01<<CONTEXT_PREV_CHI_POS)	// 0x40000
#define CONTEXT_NEXT_CHI_POS		31	
#define CONTEXT_CHI_NEXT	(0x01<<CONTEXT_NEXT_CHI_POS)	// 0x 80000000

#define SIG_PRO_CHECKING_MASK_PREV	(CONTEXT_PREV_CHI_POS | (SIGMA_SELF >>3)) //(SIG_PRO_CHECKING_MASK0>>3)			0x40002
#define SIG_PRO_CHECKING_MASK0	(SIGMA_SELF | CONTEXT_CHI)		// Including the bit SIGMA_SELF and CONTEXT_CHI		0x80010
#define SIG_PRO_CHECKING_MASK1		(SIG_PRO_CHECKING_MASK0<<3)												//		0x400080
#define SIG_PRO_CHECKING_MASK2		(SIG_PRO_CHECKING_MASK1<<3)												//		0x2000400
#define SIG_PRO_CHECKING_MASK3		(SIG_PRO_CHECKING_MASK2<<3)												//		0x10002000
#define SIG_PRO_CHECKING_MASK_NEXT	(SIG_PRO_CHECKING_MASK3<<3)												//		0x80010000
// Meanwhile the CONTEXT_CHI for next row is set on bit31, but after shifting, it has been moved to bit 33. Need to set it back for indexing in Lut table


// MU has been set and the bit samples have become significant need to process Mag_Ref
#define MAG_REF_CHECKING_MASK0		(CONTEXT_MU | SIGMA_SELF)			//		0x100010
#define MAG_REF_CHECKING_MASK1		(MAG_REF_CHECKING_MASK0 << 3)		//		0x800080
#define MAG_REF_CHECKING_MASK2		(MAG_REF_CHECKING_MASK1 << 3)		//		0x4000400
#define MAG_REF_CHECKING_MASK3		(MAG_REF_CHECKING_MASK2 << 3)		//		0x20002000

#define CLEANUP_CHECKING_MASK0		(SIGMA_SELF | CONTEXT_CHI | CONTEXT_PI)	//	0x280010
#define CLEANUP_CHECKING_MASK1		(CLEANUP_CHECKING_MASK0<<3)			//		0x1400080
#define CLEANUP_CHECKING_MASK2		(CLEANUP_CHECKING_MASK1<<3)			//		0xa000400
#define CLEANUP_CHECKING_MASK3		(CLEANUP_CHECKING_MASK2<<3)			//		0x50002000

#define DECODE_CWROD_ALIGN	3
#define MAX_INT32_WITHOUT_SIGN	(int)0x7fffffff



#define STATE_SIG_PROP_START   0 // first state of sig prop (9 states) Refer to page 247 table 8.1
#define STATE_RUN_START    9 // first state of run (1 state) Refer to page 247
#define STATE_SIGN_START  10 // first state of sign (5 states) Refer to page 
#define STATE_MAG_START   15 // first state of mag (3 states) Refer to page 250 table 8.3