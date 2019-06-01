#include "asm_header.h"
#include "bmi_t1_macro.h"
#include "codec.h"

#if	ENABLE_ASSEMBLY_OPTIMIZATION && BMI_BLOCK_DECODER

#include	"bmi_mq_decoder.h"
#include "dec_t1.h"


#define	 CODER_CHECK_OUT_ASM(CODER)	\
	int coder_A, coder_C, coder_D, coder_t, coder_T, coder_S;	\
	unsigned char * coder_buf_next;								\
	coder_A = (CODER).A;\
	coder_C = (CODER).C;\
	coder_D = (CODER).D;\
	coder_t = (CODER).t;\
	coder_T = (CODER).T;\
	coder_S = (CODER).S;\
	coder_buf_next = (CODER).buf_next;

#define		CODER_CHECK_IN_ASM(CODER)		\
	(CODER).A = coder_A;					\
	(CODER).C = coder_C;					\
	(CODER).D = coder_D;					\
	(CODER).t = coder_t;					\
	(CODER).T = coder_T;					\
	(CODER).S = coder_S;					\
	(CODER).buf_next = coder_buf_next;

#if INTEL_ASSEMBLY_32
// Fill_lsbs
// EBX : won't changed
// EAX. ECX  ESI : won't touch
// EDX : coder.t
// EDI : coder.C
#define BMI_RENORM_ASM(x)							\
__asm MOV EDX,coder_t								\
__asm renorm_loop_##x:								\
__asm TEST EDX,EDX									\
__asm JNZ skip_fill_lsbs_##x						\
__asm PUSH EBX										\
__asm MOV EDX,coder_T								\
__asm CMP EDX,0xFF									\
__asm JNZ no_ff_##x									\
__asm MOV EBX,coder_buf_next						\
__asm MOVZX EDX,byte ptr [EBX]						\
__asm CMP EDX,0x8F									\
__asm JLE T_le8F_##x								\
__asm MOV EDX,0xff									\
__asm ADD EDI, EDX									\
__asm MOV coder_T,EDX								\
__asm INC coder_S									\
__asm MOV EDX,8										\
__asm JMP fill_lsbs_done_##x						\
__asm T_le8F_##x:									\
__asm ADD EDX,EDX									\
__asm MOV coder_T,EDX								\
__asm ADD EDI, EDX									\
__asm INC EBX										\
__asm MOV coder_buf_next, EBX						\
__asm MOV EDX,7										\
__asm JMP fill_lsbs_done_##x						\
__asm no_ff_##x:									\
__asm MOV EBX,coder_buf_next						\
__asm MOVZX EDX,byte ptr [EBX]						\
__asm MOV coder_T,EDX								\
__asm ADD EDI, EDX									\
__asm INC EBX										\
__asm MOV coder_buf_next, EBX						\
__asm MOV EDX, 8									\
__asm fill_lsbs_done_##x:							\
__asm POP EBX										\
__asm skip_fill_lsbs_##x:							\
__asm ADD EAX,EAX									\
__asm ADD EDI,EDI									\
__asm DEC EDX										\
__asm TEST EAX,0x00800000							\
__asm JZ renorm_loop_##x							\
__asm MOV coder_t,EDX								\

// state : EBX
// return sym in ECX
// EAX, EDI usable
// ESI no changed
// EDX: coder.D
// 
#define BMI_MQ_DECODE_ASM							\
__asm MOV EAX, coder_A								\
__asm MOV EDI, coder_C								\
__asm ADD EAX, EDX									\
__asm ADD EDI, EDX									\
__asm MOV EDX, [EBX]								\
__asm JL	LSI_selected							\
__asm AND EDX, 0xffff00								\
__asm CMP EAX, EDX									\
__asm JGE USIM										\
__asm XOR ECX, 1									\
__asm MOV EDX, [EBX + 4]							\
__asm MOVQ MM0, [EDX + 8]							\
__asm MOVQ [EBX], MM0								\
__asm BMI_RENORM_ASM(0)								\
__asm JMP state_transition_done						\
__asm USIM:											\
__asm MOV EDX, dword ptr[EBX+4]						\
__asm MOVQ MM0, [EDX]								\
__asm MOVQ [EBX], MM0								\
__asm BMI_RENORM_ASM(1)								\
__asm JMP state_transition_done						\
__asm LSI_selected :								\
__asm AND EDX, 0xffff00								\
__asm ADD EDI, EDX									\
__asm CMP EAX, EDX									\
__asm MOV EAX, EDX									\
__asm JGE LSIL										\
__asm MOV EDX, dword ptr[EBX+4]						\
__asm MOVQ MM0, [EDX]								\
__asm MOVQ [EBX], MM0								\
__asm BMI_RENORM_ASM(2)								\
__asm JMP state_transition_done						\
__asm LSIL:											\
__asm XOR ECX, 1									\
__asm MOV EDX, [EBX + 4]							\
__asm MOVQ MM0, [EDX + 8]							\
__asm MOVQ [EBX], MM0								\
__asm BMI_RENORM_ASM(3)								\
__asm state_transition_done:						\
__asm MOV EDX,EAX									\
__asm SUB EDX,0x00800000							\
__asm CMP EDI,EDX									\
__asm CMOVL EDX,EDI									\
__asm SUB EAX,EDX									\
__asm SUB EDI,EDX									\
__asm MOV coder_C,EDI								\
__asm MOV coder_A,EAX								\
__asm ret											\


void bmi_t1_decode_pass0_asm32(bmi_mq_decoder & coder, mq_env_state states[],
							   int p, unsigned char * lutContext,int *samples,	 int *contextWord, int width, int num_stripes )
{
	int one_and_half = (1<<p) + ((1<<p)>>1); 

	width <<= 2;

	int sp_offset0 = (samples - contextWord) << 2;
	int sp_offset1 = sp_offset0 + width;
	int sp_offset2 = sp_offset1 + width;
	int sp_offset3 = sp_offset2 + width;
	int sp_stripe_adjust = width*3 - (DECODE_CWROD_ALIGN<<2);
	int * current_strip_end;

	CODER_CHECK_OUT_ASM(coder)

	__asm
	{
		MOV EDX, coder_D
		MOV ESI, contextWord
		MOV EAX, num_stripes
stribe_loop:
		PUSH EAX
		MOV EDI, width	
		ADD EDI, ESI		// ESI -> EDI : range of current stripe
		MOV current_strip_end, EDI

column_loop:
			MOV EAX, [ESI]		// cword = *(contextWord) 
			TEST EAX, EAX
			JNZ row0

skip_loop:
				ADD ESI, 12
				MOV ECX, [ESI]
				TEST ECX, ECX
				JZ skip_loop
// no_skip:
				SUB ESI, 8
				CMP ESI, current_strip_end
				JNZ column_loop
				JMP next_stripe
row0:
				TEST EAX, 0x80010
				JNZ row1
				TEST EAX, 0x1ef
				JZ row1
					AND EAX, 0x1ef
					MOV EBX, lutContext
					MOVZX EBX, byte ptr[EBX + EAX]
					MOV ECX, states
					LEA EBX, [ECX + 8 * EBX]

					MOV ECX, [EBX]		// _p_sign
					SUB EDX, ECX		// D -= _p_sign
					AND ECX, 1			// _sign
					ADD EDX, ECX		// correct D by adding back _sign
					JGE	row0_decode_in_cdp
					CALL pass0_mq_decode
row0_decode_in_cdp:
					MOV EAX, [ESI]	// restore cword
					TEST ECX, ECX
					JNZ row0_decode_sign
						OR EAX, 0x200000	// cword |= CONTEXT_PI; 
						MOV [ESI], EAX		//	 cword changed, store back
						JMP row1
row0_decode_sign:
						MOV EBX, EAX
						AND EAX, 0x400082	// // sym = cword & SIG_PRO_CHECKING_MASK1 | cword & (SIGMA_SELF>>3);;
						AND EBX, 0x40000
						SAR EBX, 2
						OR EAX, EBX
						MOV ECX, [ESI-4]
						AND ECX, 0x80010
						SAR ECX, 1
						OR EAX, ECX
						MOV ECX, dword ptr[ESI+4]
						AND ECX, 0x80010
						SHL ECX, 1
						OR EAX, ECX			// sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1)
						MOV ECX, EAX
						SAR ECX, 0eh
						OR EAX, ECX			// sym = (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
						SAR EAX, 1			// sym >>= 1

						AND EAX, 0xff
						LEA EBX, lut_ctx_sign
						MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
						MOV EAX, ECX
						AND EAX, 0xfffffffe
						AND ECX, 1							// val & 1
						MOV EBX, states
						LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);

						MOV EAX, [EBX]		// _p_sign
						SUB EDX, EAX		// D -= _p_sign
						AND EAX, 1			// _sign
						XOR ECX, EAX
						ADD EDX, EAX		// correct D by adding back _sign
						JGE	row0_decode_sign_in_cdp
						CALL pass0_mq_decode
row0_decode_sign_in_cdp:
						OR  dword ptr[ESI - 4], 0x20		// contextWord[-1] |= (SIGMA_CR<<0)
						OR  dword ptr[ESI + 4], 0x08		// contextWord[ 1] |= (SIGMA_CL<<0);
						MOV EBX, ESI
						SUB EBX, width
						OR dword ptr[EBX - 16], 0x20000		// contextWord[-width - DECODE_CWROD_ALIGN-1] |=	(SIGMA_BR << 9);
						OR dword ptr[EBX - 8], 0x8000		// contextWord[-width - DECODE_CWROD_ALIGN+1] |=	(SIGMA_BL << 9);

						SHL ECX, 19
						MOV EAX, [ESI]		 // restore cword
						OR EAX, ECX
						OR EAX, 0x200010	// cword |= (SIGMA_SELF) | (CONTEXT_PI) | (sym << CONTEXT_FIRST_CHI_POS);
						MOV [ESI], EAX		// save cword
						SHL ECX, 12			// sym <<= 31
						OR dword ptr[EBX - 12], 0x10000		
						OR dword ptr[EBX - 12], ECX			// contextWord[-width - DECODE_CWROD_ALIGN    ] |=	(SIGMA_BC << 9) | (sym << CONTEXT_NEXT_CHI_POS );

						ADD ECX, one_and_half
						MOV EBX, sp_offset0 //sp_offset_ptr
						MOV [ESI + EBX], ECX				// sp[0] : contextWord[sp_offset] = (sym<<31) + one_and_half;

row1:
				TEST EAX, 0x400080
				JNZ row2
				TEST EAX, 0xf78
				JZ row2
					AND EAX, 0xf78
					SAR EAX, 3
					MOV EBX, lutContext
					MOVZX EBX, byte ptr[EBX + EAX]		
					MOV ECX, states
					LEA EBX, [ECX + 8 * EBX]		// state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW1)>>3];

					MOV ECX, [EBX]		// _p_sign
					SUB EDX, ECX		// D -= _p_sign
					AND ECX, 1			// _sign
					ADD EDX, ECX		// correct D by adding back _sign
					JGE	row1_decode_in_cdp
					CALL pass0_mq_decode
row1_decode_in_cdp:
					MOV EAX, [ESI]	// restore cword
					TEST ECX, ECX
					JNZ row1_decode_sign
						OR EAX, 0x1000000	// cword |= CONTEXT_PI; 
						MOV [ESI], EAX		//	 cword changed, store back
						JMP row2
row1_decode_sign:
						
// 						MOV EBX, EAX
						AND EAX, 0x2080410	// sym = cword & (SIG_PRO_CHECKING_MASK0 | SIG_PRO_CHECKING_MASK2);
						MOV ECX, dword ptr[ESI-4]
						AND ECX, 0x400080
						SAR ECX, 1
						OR EAX, ECX
						MOV ECX, dword ptr[ESI+4]
						AND ECX, 0x400080
						SHL ECX, 1
						OR EAX, ECX			// // sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK1) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK1) << 1)
						MOV ECX, EAX
						SAR ECX, 0eh
						OR EAX, ECX			// sym = (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
						SAR EAX, 4			// sym >>= 4

						AND EAX, 0xff
						LEA EBX, lut_ctx_sign
						MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
						MOV EAX, ECX
						AND EAX, 0xfffffffe
						AND ECX, 1							// val & 1
						MOV EBX, states
						LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);
	
						MOV EAX, [EBX]		// _p_sign
						SUB EDX, EAX		// D -= _p_sign
						AND EAX, 1			// _sign
						XOR ECX, EAX
						ADD EDX, EAX		// correct D by adding back _sign
						JGE	row1_decode_sign_in_cdp
						CALL pass0_mq_decode
row1_decode_sign_in_cdp:

						OR  dword ptr[ESI - 4], 0x100		// contextWord[-1] |= (SIGMA_CR<<3)
						OR  dword ptr[ESI + 4], 0x40		// contextWord[ 1] |= (SIGMA_CL<<3);

						MOV EAX, [ESI]	// restore cword
						SHl ECX, 22
						OR EAX, 0x1000080
						OR EAX, ECX							// cword |= (SIGMA_SELF<<3) | (CONTEXT_PI<<3) | (sym<<(CONTEXT_FIRST_CHI_POS+3));
						MOV [ESI], EAX
						SHL ECX, 9
						ADD ECX, one_and_half				// sp[1] = (sym<<31) + one_and_half;
						MOV EBX, sp_offset1
						MOV [ESI + EBX], ECX				// sp[1] : contextWord[sp_offset1] = (sym<<31) + one_and_half;

row2:
				TEST EAX, 0x2000400
				JNZ row3
				TEST EAX, 0x7bc0
				JZ row3
						AND EAX, 0x7bc0
						SAR EAX, 6
						MOV EBX, lutContext
						MOVZX EBX, byte ptr[EBX + EAX]		
						MOV ECX, states
						LEA EBX, [ECX + 8 * EBX]		//	state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW2)>>6];

						MOV ECX, [EBX]		// _p_sign
						SUB EDX, ECX		// D -= _p_sign
						AND ECX, 1			// _sign
						ADD EDX, ECX		// correct D by adding back _sign
						JGE	row2_decode_in_cdp
						CALL pass0_mq_decode
row2_decode_in_cdp:
						MOV EAX, [ESI]	// restore cword
						TEST ECX, ECX
						JNZ row2_decode_sign
							OR EAX, 0x8000000	// cword |= CONTEXT_PI; 
							MOV [ESI], EAX		//	 cword changed, store back
							JMP row3
row2_decode_sign:
// 							MOV EBX, EAX
							AND EAX, 0x10402080		// sym = cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
							MOV ECX, dword ptr[ESI-4]
							AND ECX, 0x2000400
							SAR ECX, 1
							OR EAX, ECX
							MOV ECX, dword ptr[ESI+4]
							AND ECX, 0x2000400
							SHL ECX, 1
							OR EAX, ECX			//// sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK2) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK2) << 1)
							MOV ECX, EAX
							SAR ECX, 0eh
							OR EAX, ECX			// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
							SAR EAX, 7			// sym >>= 7

							AND EAX, 0xff
							LEA EBX, lut_ctx_sign
							MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
							MOV EAX, ECX
							AND EAX, 0xfffffffe
							AND ECX, 1							// val & 1
							MOV EBX, states
							LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);

							MOV EAX, [EBX]		// _p_sign
							SUB EDX, EAX		// D -= _p_sign
							AND EAX, 1			// _sign
							XOR ECX, EAX
							ADD EDX, EAX		// correct D by adding back _sign
							JGE	row2_decode_sign_in_cdp
							CALL pass0_mq_decode
row2_decode_sign_in_cdp:
							OR  dword ptr[ESI - 4], 0x800		// contextWord[-1] |= (SIGMA_CR<<6)
							OR  dword ptr[ESI + 4], 0x200		// contextWord[ 1] |= (SIGMA_CL<<6);

							MOV EAX, [ESI]	// restore cword
							SHl ECX, 25
							OR EAX, 0x8000400
							OR EAX, ECX							// word |= (SIGMA_SELF<<6) | (CONTEXT_PI<<6) | (sym<<(CONTEXT_FIRST_CHI_POS+6));
							MOV [ESI], EAX
							SHL ECX, 6
							ADD ECX, one_and_half				
							MOV EBX, sp_offset2
							MOV [ESI + EBX], ECX				// sp : contextWord[sp_offset + width_by2] = (sym<<31) + one_and_half;

row3:
					TEST EAX, 0x10002000
					JNZ next_column
					TEST EAX, 0x3de00
					JZ next_column
						AND EAX, 0x3de00
						SAR EAX, 9
						MOV EBX, lutContext
						MOVZX EBX, byte ptr[EBX + EAX]		
						MOV ECX, states
						LEA EBX, [ECX + 8 * EBX]		//	state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW3)>>9];

						MOV ECX, [EBX]		// _p_sign
						SUB EDX, ECX		// D -= _p_sign
						AND ECX, 1			// _sign
						ADD EDX, ECX		// correct D by adding back _sign
						JGE	row3_decode_in_cdp
						CALL pass0_mq_decode
row3_decode_in_cdp:
						MOV EAX, [ESI]	// restore cword
						TEST ECX, ECX
						JNZ row3_decode_sign
						OR EAX, 0x40000000	// cword |= CONTEXT_PI; 
						MOV [ESI], EAX		//	 cword changed, store back
						JMP next_column
row3_decode_sign:
						AND EAX, 0x82010400			// sym = cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
						MOV ECX, [ESI-4]
						AND ECX, 0x10002000
						SAR ECX, 1
						OR EAX, ECX
						MOV ECX, dword ptr[ESI+4]
						AND ECX, 0x10002000
						SHL ECX, 1
						OR EAX, ECX			// sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1)
						MOV ECX, EAX
						SAR ECX, 0eh
						OR EAX, ECX			// sym = (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
						SAR EAX, 10			// sym >>= 10

						AND EAX, 0xff
						LEA EBX, lut_ctx_sign
						MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
						MOV EAX, ECX
						AND EAX, 0xfffffffe
						AND ECX, 1							// val & 1
						MOV EBX, states
						LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);

						MOV EAX, [EBX]		// _p_sign
						SUB EDX, EAX		// D -= _p_sign
						AND EAX, 1			// _sign
						XOR ECX, EAX
						ADD EDX, EAX		// correct D by adding back _sign
						JGE	row3_decode_sign_in_cdp
						CALL pass0_mq_decode
row3_decode_sign_in_cdp:
						OR  dword ptr[ESI - 4], 0x4000		// contextWord[-1] |= (SIGMA_CR<<9)
						OR  dword ptr[ESI + 4], 0x1000		// contextWord[ 1] |= (SIGMA_CL<<9);
						MOV EBX, ESI
						ADD EBX, width
						OR dword ptr[EBX + 8 ], 4		// contextWord[width + DECODE_CWROD_ALIGN -1] |= SIGMA_TR; 
						OR dword ptr[EBX + 16], 1		//	contextWord[width + DECODE_CWROD_ALIGN +1] |= SIGMA_TL;

						SHL ECX, 18
						OR dword ptr[EBX + 12], 2		
						OR dword ptr[EBX + 12], ECX		//	contextWord[width + DECODE_CWROD_ALIGN   ] |= SIGMA_TC | (sym<<CONTEXT_PREV_CHI_POS);

						MOV EAX, [ESI]		 // restore cword
						SHL ECX, 10			// sym<<(CONTEXT_FIRST_CHI_POS+9)
						OR EAX, ECX
						OR EAX, 0x40002000	// cword |= (SIGMA_SELF) | (CONTEXT_PI) | (sym<<(CONTEXT_FIRST_CHI_POS+9));
						MOV [ESI], EAX		// save cword

						SHL ECX, 3			// sym <<= 31
						ADD ECX, one_and_half
						MOV EBX, sp_offset3
						MOV [ESI + EBX], ECX				// sp[0] : ccontextWord[sp_offset + width_by3] = (sym<<31) + one_and_half;

next_column:
			ADD ESI, 4
			CMP ESI, current_strip_end
			JNZ column_loop

next_stripe:
		ADD ESI, 12		// next strip
		MOV EBX, sp_stripe_adjust
		ADD [sp_offset0], EBX
		ADD [sp_offset1], EBX
		ADD [sp_offset2], EBX
		ADD [sp_offset3], EBX
		POP EAX
		DEC EAX
		JNZ stribe_loop
		JMP proc_return

pass0_mq_decode:
		BMI_MQ_DECODE_ASM

proc_return:
		MOV coder_D, EDX
		EMMS

	}

	CODER_CHECK_IN_ASM(coder)

}


void bmi_t1_decode_pass1_asm32(bmi_mq_decoder & coder, mq_env_state states[],
							   int p,								int *samples,	 int *contexts, int width, int num_stripes )
{
	mq_env_state * state_run_start = &states[STATE_RUN_START];

	int lsb = 1<<p;
	int half_lsb = lsb >>1; 
	int last_plane = lsb << 2;

	width <<= 2;

	int sp_offset0 = (samples - contexts) << 2;
	int sp_offset1 = sp_offset0 + width;
	int sp_offset2 = sp_offset1 + width;
	int sp_offset3 = sp_offset2 + width;
	int sp_stripe_adjust = width*3 - (DECODE_CWROD_ALIGN<<2);
	int * current_strip_end;

	states += STATE_MAG_START;

	CODER_CHECK_OUT_ASM(coder)

	__asm
	{

		MOV EDX, coder_D
		MOV ESI, contexts
		MOV EAX, num_stripes
stribe_loop:
		PUSH EAX
		MOV EDI, width	
		ADD EDI, ESI		// ESI -> EDI : range of current stripe
		MOV current_strip_end, EDI

column_loop:
		MOV EAX, [ESI]		// cword = *(contextWord) 
		TEST EAX, 0x24900000
		JNZ row0

skip_loop:
			ADD ESI, 8
			MOV ECX, [ESI]
			TEST ECX, ECX
			JZ skip_loop

			SUB ESI, 4
			CMP ESI, current_strip_end
			JNZ column_loop
			JMP next_stripe
row0:
			TEST EAX, 0x100000
			JZ row1
			TEST EAX, 0x10
			JZ row1			// cword & MAG_REF_CHECKING_MASK0) == MAG_REF_CHECKING_MASK0)  ?
				MOV ECX, sp_offset0
				MOV ECX, [ESI + ECX]		// val = sp[0]];
				PUSH ECX					// push val
				AND ECX, 0x7fffffff
				MOV EBX, states
				CMP ECX, last_plane
				JGE row0_2nd_mag
					TEST EAX, 0x1ef			// if (cword & NEIGHBOR_SIGMA_ROW0)
					JZ row0_mq_decode
					ADD EBX, 8
					JMP row0_mq_decode
row0_2nd_mag:
					ADD EBX, 16
row0_mq_decode:
					MOV ECX, [EBX]		// _p_sign
					SUB EDX, ECX		// D -= _p_sign
					AND ECX, 1			// _sign
					ADD EDX, ECX		// correct D by adding back _sign
					JGE	row0_mq_decode_done
					CALL pass1_mq_decode
row0_mq_decode_done:
				POP EBX		// val
				TEST ECX, ECX
				JNZ row0_deq_unchanged
				XOR EBX, lsb
row0_deq_unchanged:
				OR EBX, half_lsb
				MOV ECX, sp_offset0
				MOV [ESI + ECX], EBX
				MOV EAX, [ESI]		// cword = *(contextWord) 

row1:
			TEST EAX, 0x800000
			JZ row2
			TEST EAX, 0x80
			JZ row2			// cword & MAG_REF_CHECKING_MASK1) == MAG_REF_CHECKING_MASK1)  ?
				MOV ECX, sp_offset1
				MOV ECX, [ESI + ECX]		// val = contextWord[sp_offset+width];
				PUSH ECX					// push val
				AND ECX, 0x7fffffff
				MOV EBX, states
				CMP ECX, last_plane
				JGE row1_2nd_mag
				TEST EAX, 0xf78			// if (cword & NEIGHBOR_SIGMA_ROW1)
				JZ row1_mq_decode
				ADD EBX, 8
				JMP row1_mq_decode
row1_2nd_mag:
				ADD EBX, 16
row1_mq_decode:
				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row1_mq_decode_done
				CALL pass1_mq_decode
row1_mq_decode_done:
				POP EBX		// val
				TEST ECX, ECX
				JNZ row1_deq_unchanged
				XOR EBX, lsb
row1_deq_unchanged:
				OR EBX, half_lsb
				MOV ECX, sp_offset1
				MOV [ESI + ECX], EBX
				MOV EAX, [ESI]		// cword = *(contextWord) 


row2:
			TEST EAX, 0x4000000
			JZ row3
			TEST EAX, 0x400
			JZ row3			// cword & MAG_REF_CHECKING_MASK2) == MAG_REF_CHECKING_MASK2)  ?
				MOV ECX, sp_offset2
				MOV ECX, [ESI + ECX]		// val = contextWord[sp_offset+width+width];
				PUSH ECX					// push val
				AND ECX, 0x7fffffff
				MOV EBX, states
				CMP ECX, last_plane
				JGE row2_2nd_mag
				TEST EAX, 0x7bc0			// if (cword & NEIGHBOR_SIGMA_ROW3)
				JZ row2_mq_decode
				ADD EBX, 8
				JMP row2_mq_decode
row2_2nd_mag:
				ADD EBX, 16
row2_mq_decode:
				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row2_mq_decode_done
				CALL pass1_mq_decode
row2_mq_decode_done:
				POP EBX		// val
				TEST ECX, ECX
				JNZ row2_deq_unchanged
				XOR EBX, lsb
row2_deq_unchanged:
				OR EBX, half_lsb
				MOV ECX, sp_offset2
				MOV [ESI + ECX], EBX
				MOV EAX, [ESI]		// cword = *(contextWord) 

row3:
			TEST EAX, 0x20000000
			JZ next_column
			TEST EAX, 0x2000
			JZ next_column			// cword & MAG_REF_CHECKING_MASK3) == MAG_REF_CHECKING_MASK3)  ?
				MOV ECX, sp_offset3
				MOV ECX, [ESI + ECX]		// val = contextWord[sp_offset+width+width];
				PUSH ECX					// push val
				AND ECX, 0x7fffffff
				MOV EBX, states
				CMP ECX, last_plane
				JGE row3_2nd_mag
				TEST EAX, 0x3de00			// if (cword & NEIGHBOR_SIGMA_ROW3)
				JZ row3_mq_decode
				ADD EBX, 8
				JMP row3_mq_decode
row3_2nd_mag:
				ADD EBX, 16
row3_mq_decode:
				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row3_mq_decode_done
				CALL pass1_mq_decode
row3_mq_decode_done:
				POP EBX		// val
				TEST ECX, ECX
				JNZ row3_deq_unchanged
				XOR EBX, lsb
row3_deq_unchanged:
				OR EBX, half_lsb
				MOV ECX, sp_offset3
				MOV [ESI + ECX], EBX

next_column:
		ADD ESI, 4
		CMP ESI, current_strip_end
		JNZ column_loop

next_stripe:
		ADD ESI, 12		// next strip
		MOV EBX, sp_stripe_adjust
		ADD [sp_offset0], EBX
		ADD [sp_offset1], EBX
		ADD [sp_offset2], EBX
		ADD [sp_offset3], EBX
		POP EAX
		DEC EAX
		JNZ stribe_loop
		JMP proc_return

pass1_mq_decode:
		BMI_MQ_DECODE_ASM

proc_return:
		MOV coder_D, EDX
		EMMS

	}
	CODER_CHECK_IN_ASM(coder)
}


void bmi_t1_decode_pass2_asm32(bmi_mq_decoder & coder, mq_env_state states[],
							   int bit_plane, unsigned char * lutContext,int *samples,	 int *contexts, int width, int num_stripes )
{


	mq_env_state * state_run_start = &states[STATE_RUN_START];

	int one_and_half = (1<<bit_plane) + ((1<<bit_plane)>>1); 

	width <<= 2;

	int sp_offset0 = (samples - contexts) << 2;
	int sp_offset1 = sp_offset0 + width;
	int sp_offset2 = sp_offset1 + width;
	int sp_offset3 = sp_offset2 + width;

	int sp_stripe_adjust = width*3 - (DECODE_CWROD_ALIGN<<2);
	int * current_strip_end;

	CODER_CHECK_OUT_ASM(coder)

	__asm	{

		MOV EDX, coder_D
		MOV ESI, contexts
		MOV EAX, num_stripes
stribe_loop:
		PUSH EAX
		MOV EDI, width	
		ADD EDI, ESI		// ESI -> EDI : range of current stripe
		MOV current_strip_end, EDI

column_loop:
 		MOV EAX, [ESI]		// cword = *(contextWord) 
		TEST EAX, EAX
		JNZ row0
			MOV EBX, state_run_start
// 			MOV ECX, [ESI + 12]
// 			TEST ECX, ECX
// 			JNZ no_skip
// 			MOV ECX, [EBX]	// _p_sign
// 			TEST ECX, 1
// 			JNZ no_skip
// 				// skip four stripe columns 
// 				SHL ECX, 10		// states[STATE_RUN_START]._p_sign<<2
// 				MOV EAX, EDX
// // 				SAR EAX, 8
// 				SUB EAX, ECX
// 				JL no_skip
// 					// skip 4 colimns
// 					MOV EDX, EAX
// 					AND EDX, 0xffffff00
// 					ADD ESI, 16
// 					CMP ESI, current_strip_end
// 					JNZ	column_loop
// 					JMP next_stripe
//				
//no_skip:
			MOV ECX, [EBX]		// _p_sign
			SUB EDX, ECX		// D -= _p_sign
			AND ECX, 1			// _sign
			ADD EDX, ECX		// correct D by adding back _sign
			JGE	mq_decode_done
			CALL pass2_mq_decode
mq_decode_done:
			TEST ECX, ECX
			JZ next_column
//////////////////////////////////////////////////////////////////////////
// 				 pass2_mq_cdb_decode_2_bits
				XOR ECX,ECX
				MOV EBX,coder_C
				MOV EAX, coder_A

				SUB EDX,0x00560100 
				JGE decode_bit_0 
				/* Non-CDP decoding for the first bit */ 
					ADD EAX,EDX		// A+= D
					ADD EBX,EDX		// C += D
					JL bit_1_LSI
					/* Upper sub-interval selected */
						CMP EAX,0x00560100 
						JGE bit_1_renorm
							MOV ECX,2
							JMP bit_1_renorm
bit_1_LSI: 
						/* Lower sub-interval selected */
					ADD EBX,0x00560100		//  C+= 0x560100
				CMP EAX,0x00560100 
				MOV EAX,0x00560100 
				JL bit_1_renorm 
				MOV ECX,2

bit_1_renorm: 
			MOV EDI,coder_t
bit_1_renorm_once:
			/* Come back here repeatedly until we have shifted enough bits out. */ 
			TEST EDI,EDI 
				JNZ skip_bit_1_fill_lsbs
				/* Begin `fill_lsbs' procedure */
				MOV EDX,coder_T 
				CMP EDX,0xFF 
				JNZ bit_1_ne_ff 
				MOV EDI,coder_buf_next
				MOVZX EDX,byte ptr [EDI] 
			CMP EDX,0x8F 
				JLE bit_1_T_le8F 
				/* Reached a termination marker.Don't update `store_var' */
				MOV EDX,0xFF  // coder_T
				INC coder_S
				MOV EDI,8 /* New value of t */ 
				JMP bit_1_fill_lsbs_done
bit_1_T_le8F:
			/* No termination marker, but bit stuff required */
			ADD EDX,EDX
				INC EDI
				MOV coder_buf_next, EDI
				MOV EDI,7 /* New value of t. */
				JMP bit_1_fill_lsbs_done

bit_1_ne_ff:
			MOV EDI,coder_buf_next
				MOVZX EDX,byte ptr [EDI] 
			INC EDI
				MOV coder_buf_next, EDI
				MOV EDI, 8
bit_1_fill_lsbs_done: 
			ADD EBX,EDX 
				MOV coder_T,EDX /* Save `temp' */ 

skip_bit_1_fill_lsbs: 
			ADD EAX,EAX 
				ADD EBX,EBX 
				DEC EDI
				TEST EAX,0x00800000 
				JZ bit_1_renorm_once
				MOV coder_t,EDI /* Save t */ 


				/* Renormalization is complete.Recompute D and save A and C. */
				MOV EDX,EAX 
				SUB EDX,0x00800000 /* Subtract MQCODER_A_MIN */
				CMP EBX,EDX
				CMOVL EDX,EBX /* Move C to EDX if C < A-MQCODER_A_MIN */ 
				SUB EAX,EDX /* Subtract D from A */
				SUB EBX,EDX /* Subtract D from C */


				/************************************************************************/ 
decode_bit_0:
			SUB EDX,0x00560100 
				JGE bit_1_finished 
				ADD EAX,EDX // A += D
				ADD EBX,EDX // B += D
				JL bit_0_LSI 
				/* Upper sub-interval selected */
				CMP EAX,0x00560100 
				JGE bit_0_renorm
				INC ECX
				JMP bit_0_renorm
bit_0_LSI: 
			/* Lower sub-interval selected */
			ADD EBX,0x00560100 
				CMP EAX,0x00560100 
				MOV EAX,0x00560100 
				JL bit_0_renorm 
				INC ECX
bit_0_renorm: 
			MOV EDI,coder_t
bit_0_renorm_once:
			TEST EDI,EDI 
				JNZ skip_bit_0_fill_lsbs
				MOV EDX,coder_T 
				CMP EDX,0xFF 
				JNZ run_bit0_no_ff 
				MOV EDI,coder_buf_next
				MOVZX EDX,byte ptr [EDI] 
			CMP EDX,0x8F 
				JLE bit_0_T_le8F 
				MOV EDX,0xFF 
				MOV EDI,coder_S
				INC EDI
				MOV coder_S,EDI
				MOV EDI,8 /* New value of t */ 
				JMP bit_0_fill_lsbs_done
bit_0_T_le8F:
			ADD EDX,EDX
				INC EDI
				MOV coder_buf_next,EDI 
				MOV EDI,7 /* New value of t. */
				JMP bit_0_fill_lsbs_done
run_bit0_no_ff:
			MOV EDI,coder_buf_next
				MOVZX EDX,byte ptr [EDI]
			INC EDI
				MOV coder_buf_next,EDI 
				MOV EDI,8 
bit_0_fill_lsbs_done: 
			ADD EBX,EDX
				MOV coder_T,EDX
skip_bit_0_fill_lsbs: 
			ADD EAX,EAX
				ADD EBX,EBX
				DEC EDI

				TEST EAX,0x00800000
				JZ bit_0_renorm_once
				MOV coder_t,EDI
				MOV EDX,EAX 
				SUB EDX,0x00800000
				CMP EBX,EDX
				CMOVL EDX,EBX 
				SUB EAX,EDX
				SUB EBX,EDX
bit_1_finished:
			MOV coder_C,EBX
				MOV coder_A,EAX

//////////////////////////////////////////////////////////////////////////
				MOV EAX, [ESI]
				TEST ECX, ECX
				JZ row0_significant
				JP row3_significant // Parity means there are an even number of 1's (run=3)
				TEST ECX,1
				JNZ row1_significant
				JMP row2_significant
row0:
			TEST EAX, 0x280010
			JNZ row1
				AND EAX, 0x1ef
				MOV EBX, lutContext
				MOVZX EBX, byte ptr[EBX + EAX]
				MOV ECX, states
				LEA EBX, [ECX + 8 * EBX]


				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row0_decode_in_cdp
				CALL pass2_mq_decode
row0_decode_in_cdp:
				MOV EAX, [ESI]
				TEST ECX, ECX
				JZ row1
row0_significant:
					SAR EAX, 1
					MOV ECX, EAX
					AND EAX, 0x200041//0x400082	//sym = cword & SIG_PRO_CHECKING_MASK1;		sym |= cword & (SIGMA_SELF>>3);

					AND ECX, 0x20000//0x40000
					SAR ECX, 2
					OR EAX, ECX				// sym |= (cword & CONTEXT_CHI_PREV)>>2;

					MOV ECX, dword ptr[ESI-4]		// previous context
					AND ECX, 0x80010
					SAR ECX, 2
					OR EAX, ECX				//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1

					MOV ECX, [ESI+4]		// next context 
					AND ECX, 0x80010
					OR EAX, ECX				// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1

					MOV ECX, EAX
					SAR ECX, 14
					OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));

					AND EAX, 0xff

					MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
					MOV EBX, ECX							// val
					AND ECX, 1								// val & 1
					SAR EBX, 1
					MOV EAX, states
					LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state

					MOV EAX, [EBX]		// _p_sign
					SUB EDX, EAX		// D -= _p_sign
					AND EAX, 1			// _sign	
					XOR ECX, EAX
					ADD EDX, EAX		// correct D by adding back _sign
					JGE	row0_sig_decode_in_cdp
					CALL pass2_mq_decode
row0_sig_decode_in_cdp:
					MOV EBX, ESI
					SUB EBX, width
					OR dword ptr[EBX - 16], 0x20000		// contextWord[-width - DECODE_CWROD_ALIGN-1] |=	(SIGMA_BR << 9);
					OR dword ptr[EBX - 8], 0x8000		// contextWord[-width - DECODE_CWROD_ALIGN+1] |=	(SIGMA_BL << 9);
					OR dword ptr[ESI - 4], 0x20					// contextWord[-1] |= (SIGMA_CR<<0);
					OR dword ptr[ESI + 4], 0x08					// contextWord[ 1] |= (SIGMA_CL<<0);

					SHL  ECX, 19
 					MOV EAX, [ESI]		// restore cword
					OR EAX, ECX
					OR EAX, 0x10		// cword |= (SIGMA_SELF) | (sym << CONTEXT_FIRST_CHI_POS);
					MOV [ESI], EAX

					SHL ECX, 12			// sym << 31
					OR dword ptr[EBX - 12], 0x10000		
					OR dword ptr[EBX - 12], ECX		// contextWord[-width - DECODE_CWROD_ALIGN  ] |=	(SIGMA_BC << 9) | (sym << CONTEXT_NEXT_CHI_POS ); 

					ADD ECX, one_and_half
					MOV EBX, sp_offset0
					MOV [ESI + EBX], ECX

row1:
			TEST EAX, 0x1400080
			JNZ row2
				AND EAX, 0xf78
				SAR EAX, 3
				MOV EBX, lutContext
				MOVZX EBX, byte ptr[EBX+EAX]
				MOV ECX, states
				LEA EBX, [ECX + 8 * EBX]

				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row1_decode_in_cdp
				CALL pass2_mq_decode
row1_decode_in_cdp:
				MOV EAX, [ESI]
				TEST ECX, ECX
				JZ row2
row1_significant:
					AND EAX, 0x2080410		//sym = cword & (SIG_PRO_CHECKING_MASK0 | SIG_PRO_CHECKING_MASK2);

					MOV ECX, [ESI-4]
					AND ECX, 0x400080
					SAR ECX, 1
					OR EAX, ECX		//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK1) >> 1;

					MOV ECX, [ESI+4]
					AND ECX, 0x400080
					SHL ECX, 1
					OR EAX, ECX		// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK1) << 1;

					MOV ECX, EAX
					SAR ECX, 14
					OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
					SAR EAX, 4				// sym >>= 4
					
					AND EAX, 0xff
					MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
					MOV EBX, ECX							// val
					AND ECX, 1								// val & 1
					SAR EBX, 1
					MOV EAX, states
					LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state	//	state_ref = states  + STATE_SIGN_START + (val>>1);

					MOV EAX, [EBX]		// _p_sign
					SUB EDX, EAX		// D -= _p_sign
					AND EAX, 1			// _sign	
					XOR ECX, EAX
					ADD EDX, EAX		// correct D by adding back _sign
					JGE	row1_sig_decode_in_cdp
					CALL pass2_mq_decode
row1_sig_decode_in_cdp:

					OR dword ptr[ESI - 4], 0x100					// contextWord[-1] |= (SIGMA_CR<<3);
					OR dword ptr[ESI + 4], 0x40					// contextWord[ 1] |= (SIGMA_CL<<);

					MOV EAX, [ESI]		// restore cword

					SHL  ECX, 22
					OR EAX, ECX
					OR EAX, 0x80		// cword |= (SIGMA_SELF<<3) | (sym<<(CONTEXT_FIRST_CHI_POS+3));
					MOV [ESI], EAX

					SHL ECX, 9			// sym << 31
					ADD ECX, one_and_half
					MOV EBX, sp_offset1
					MOV [ESI + EBX], ECX

row2:
			TEST EAX, 0xA000400
			JNZ row3
				AND EAX, 0x7bc0
				SAR EAX, 6
				MOV EBX, lutContext
				MOVZX EBX, byte ptr[EBX+EAX]
				MOV ECX, states
				LEA EBX, [ECX + 8 * EBX]

				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row2_decode_in_cdp
				CALL pass2_mq_decode
row2_decode_in_cdp:
				MOV EAX, [ESI]
				TEST ECX, ECX
				JZ row3
row2_significant:
// 					MOV ECX, EAX
					AND EAX, 0x10402080		//sym = cword &  cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);

					MOV ECX, [ESI-4]
					AND ECX, 0x2000400
					SAR ECX, 1
					OR EAX, ECX		//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK2) >> 1;

					MOV ECX, [ESI+4]
					AND ECX, 0x2000400
					SHL ECX, 1
					OR EAX, ECX		// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK2) << 1;

					MOV ECX, EAX
					SAR ECX, 14
					OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
					SAR EAX, 7				// sym >>= 7

					AND EAX, 0xff
					MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
					MOV EBX, ECX							// val
					AND ECX, 1								// val & 1
					SAR EBX, 1								// val >> 1
					MOV EAX, states
					LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state	//	state_ref = states  + STATE_SIGN_START + (val>>1);

					MOV EAX, [EBX]		// _p_sign
					SUB EDX, EAX		// D -= _p_sign
					AND EAX, 1			// _sign	
					XOR ECX, EAX
					ADD EDX, EAX		// correct D by adding back _sign
					JGE	row2_sig_decode_in_cdp
					CALL pass2_mq_decode
row2_sig_decode_in_cdp:

					OR dword ptr[ESI - 4], 0x800					// contextWord[-1] |= (SIGMA_CR<<3);
					OR dword ptr[ESI + 4], 0x200					// contextWord[ 1] |= (SIGMA_CL<<);

					MOV EAX, [ESI]		// restore cword

					SHL  ECX, 25
					OR EAX, ECX
					OR EAX, 0x400		// cword |= (SIGMA_SELF<<6) | (sym<<(CONTEXT_FIRST_CHI_POS+6));
					MOV [ESI], EAX

					SHL ECX, 6			// sym << 31
					ADD ECX, one_and_half
					MOV EBX, sp_offset2
					MOV [ESI + EBX], ECX

row3:
			TEST EAX, 0x50002000
			JNZ update_context
				AND EAX, 0x3de00
				SAR EAX, 9
				MOV EBX, lutContext
				MOVZX EBX, byte ptr[EBX+EAX]
				MOV ECX, states
				LEA EBX, [ECX + 8 * EBX]


				MOV ECX, [EBX]		// _p_sign
				SUB EDX, ECX		// D -= _p_sign
				AND ECX, 1			// _sign
				ADD EDX, ECX		// correct D by adding back _sign
				JGE	row3_decode_in_cdp
				CALL pass2_mq_decode
row3_decode_in_cdp:
				MOV EAX, [ESI]
				TEST ECX, ECX
				JZ update_context
row3_significant:
					AND EAX, 0x82010400		//sym = cword & (SIG_PRO_CHECKING_MASK2 | SIG_PRO_CHECKING_MASK_NEXT);

					MOV ECX, [ESI-4]
					AND ECX, 0x10002000
					SAR ECX, 1
					OR EAX, ECX		//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK3) >> 1;

					MOV ECX, [ESI+4]
					AND ECX, 0x10002000
					SHL ECX, 1
					OR EAX, ECX		// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK3) << 1;

					MOV ECX, EAX
					SAR ECX, 14
					OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
					SAR EAX, 10				// sym >>= 10

					AND EAX, 0xff
					MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
					MOV EBX, ECX							// val
					AND ECX, 1								// val & 1
					SAR EBX, 1								// val >> 1
					MOV EAX, states
					LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state	//	state_ref = states  + STATE_SIGN_START + (val>>1);

					MOV EAX, [EBX]		// _p_sign
					SUB EDX, EAX		// D -= _p_sign
					AND EAX, 1			// _sign	
					XOR ECX, EAX
					ADD EDX, EAX		// correct D by adding back _sign
					JGE	row3_sig_decode_in_cdp
					CALL pass2_mq_decode
row3_sig_decode_in_cdp:

					MOV EAX, width
					OR dword ptr[ESI + EAX + 8], 0x04			// contextWord[width + DECODE_CWROD_ALIGN -1] |= SIGMA_TR;
					OR dword ptr[ESI + EAX +16], 0x01			// contextWord[width + DECODE_CWROD_ALIGN +1] |= SIGMA_TL;

					SHL ECX, 18									// sym <<= CONTEXT_PREV_CHI_POS
					OR dword ptr[ESI + EAX +12], ECX			// contextWord[width + DECODE_CWROD_ALIGN   ] |= (sym<<CONTEXT_PREV_CHI_POS);
					OR dword ptr[ESI + EAX +12], 0x02			// contextWord[width + DECODE_CWROD_ALIGN   ] |= SIGMA_TC 

					OR dword ptr[ESI - 4], 0x4000				// contextWord[-1] |= (SIGMA_CR<<9);
					OR dword ptr[ESI + 4], 0x1000				// contextWord[ 1] |= (SIGMA_CL<<9);

					MOV EAX, [ESI]		// restore cword

					SHL  ECX, 10		// sym<<(CONTEXT_FIRST_CHI_POS+9) remember sym has shl 18 above
					OR EAX, ECX
					OR EAX, 0x2000		// cword |= (SIGMA_SELF<<9) | (sym<<(CONTEXT_FIRST_CHI_POS+9));

					SHL ECX, 3			// sym << 31
					ADD ECX, one_and_half
					MOV EBX, sp_offset3
					MOV [ESI + EBX], ECX

update_context:
			MOV EBX, EAX
			SHL EAX, 16
			AND EAX, 0x24900000
			OR EBX, EAX			// cword |= (cword << (CONTEXT_MU_POS - SIGMA_SELF_POS)) &	((CONTEXT_MU<<0)|(CONTEXT_MU<<3)|(CONTEXT_MU<<6)|(CONTEXT_MU<<9));
			AND EBX, 0xB6DFFFFF	// cword &= ~((CONTEXT_PI<<0)|(CONTEXT_PI<<3)|(CONTEXT_PI<<6)|(CONTEXT_PI<<9));
			MOV dword ptr[ESI], EBX		// *contextWord = cword;

next_column:
		ADD ESI, 4
		CMP ESI, current_strip_end
		JNZ column_loop

// next_stripe:
		ADD ESI, 12		// next strip
		MOV EBX, sp_stripe_adjust
		ADD [sp_offset0], EBX
		ADD [sp_offset1], EBX
		ADD [sp_offset2], EBX
		ADD [sp_offset3], EBX

		POP EAX
		DEC EAX
		JNZ stribe_loop
		JMP proc_return

pass2_mq_decode:
		BMI_MQ_DECODE_ASM

proc_return:
		MOV coder_D, EDX
		EMMS

	}

	CODER_CHECK_IN_ASM(coder)
}


// above are for assemble win32
#elif AT_T_ASSEMBLY



// 
// // Fill_lsbs
// // EBX : won't changed
// // EAX. ECX  ESI : won't touch
// // EDX : coder.t
// // EDI : coder.C
/*
#define BMI_RENORM_ASM(x)							\
	__asm MOV EDX,coder_t								\
	__asm renorm_loop_##x:								\
	__asm TEST EDX,EDX									\
	__asm JNZ skip_fill_lsbs_##x						\
	__asm PUSH EBX										\
	__asm MOV EDX,coder_T								\
	__asm CMP EDX,0xFF									\
	__asm JNZ no_ff_##x									\
	__asm MOV EBX,coder_buf_next						\
	__asm MOVZX EDX,byte ptr [EBX]						\
	__asm CMP EDX,0x8F									\
	__asm JLE T_le8F_##x								\
	__asm MOV EDX,0xff									\
	__asm ADD EDI, EDX									\
	__asm MOV coder_T,EDX								\
	__asm INC coder_S									\
	__asm MOV EDX,8										\
	__asm JMP fill_lsbs_done_##x						\
	__asm T_le8F_##x:									\
	__asm ADD EDX,EDX									\
	__asm MOV coder_T,EDX								\
	__asm ADD EDI, EDX									\
	__asm INC EBX										\
	__asm MOV coder_buf_next, EBX						\
	__asm MOV EDX,7										\
	__asm JMP fill_lsbs_done_##x						\
	__asm no_ff_##x:									\
	__asm MOV EBX,coder_buf_next						\
	__asm MOVZX EDX,byte ptr [EBX]						\
	__asm MOV coder_T,EDX								\
	__asm ADD EDI, EDX									\
	__asm INC EBX										\
	__asm MOV coder_buf_next, EBX						\
	__asm MOV EDX, 8									\
	__asm fill_lsbs_done_##x:							\
	__asm POP EBX										\
	__asm skip_fill_lsbs_##x:							\
	__asm ADD EAX,EAX									\
	__asm ADD EDI,EDI									\
	__asm DEC EDX										\
	__asm TEST EAX,0x00800000							\
	__asm JZ renorm_loop_##x							\
	__asm MOV coder_t,EDX								\
	*/
// 
// // state : EBX
// // return sym in ECX
// // EAX, EDI usable
// // ESI no changed
// // EDX: coder.D
// // 
/*
#define BMI_MQ_DECODE_ASM							\
	__asm MOV EAX, coder_A								\
	__asm MOV EDI, coder_C								\
	__asm ADD EAX, EDX									\
	__asm ADD EDI, EDX									\
	__asm MOV EDX, [EBX]								\
	__asm JL	LSI_selected							\
	__asm AND EDX, 0xffff00								\
	__asm CMP EAX, EDX									\
	__asm JGE USIM										\
	__asm XOR ECX, 1									\
	__asm MOV EDX, [EBX + 4]							\
	__asm MOVQ MM0, [EDX + 8]							\
	__asm MOVQ [EBX], MM0								\
	__asm BMI_RENORM_ASM(0)								\
	__asm JMP state_transition_done						\
	__asm USIM:											\
	__asm MOV EDX, dword ptr[EBX+4]						\
	__asm MOVQ MM0, [EDX]								\
	__asm MOVQ [EBX], MM0								\
	__asm BMI_RENORM_ASM(1)								\
	__asm JMP state_transition_done						\
	__asm LSI_selected :								\
	__asm AND EDX, 0xffff00								\
	__asm ADD EDI, EDX									\
	__asm CMP EAX, EDX									\
	__asm MOV EAX, EDX									\
	__asm JGE LSIL										\
	__asm MOV EDX, dword ptr[EBX+4]						\
	__asm MOVQ MM0, [EDX]								\
	__asm MOVQ [EBX], MM0								\
	__asm BMI_RENORM_ASM(2)								\
	__asm JMP state_transition_done						\
	__asm LSIL:											\
	__asm XOR ECX, 1									\
	__asm MOV EDX, [EBX + 4]							\
	__asm MOVQ MM0, [EDX + 8]							\
	__asm MOVQ [EBX], MM0								\
	__asm BMI_RENORM_ASM(3)								\
	__asm state_transition_done:						\
	__asm MOV EDX,EAX									\
	__asm SUB EDX,0x00800000							\
	__asm CMP EDI,EDX									\
	__asm CMOVL EDX,EDI									\
	__asm SUB EAX,EDX									\
	__asm SUB EDI,EDX									\
	__asm MOV coder_C,EDI								\
	__asm MOV coder_A,EAX								\
	__asm ret											\
*/


void bmi_t1_decode_pass0_asmATT(bmi_mq_decoder & coder, mq_env_state states[],
								int p, unsigned char * lutContext,int *samples,	 int *contextWord, int width, int num_stripes )
{

	CODER_CHECK_OUT_ASM(coder)

	typedef	struct s_att_pass0_stru
	{
		int *contextWord;	//	0
		int sp_offset0;	// 4
		int sp_offset1;	// 8
		int sp_offset2;	// 12
		int sp_offset3;	// 16
		int sp_stripe_adjust;	// 20
		int * current_strip_end;				// 24
		unsigned char * lutContext;				// 28
		int		lut_ctx_sign;		// 32
		mq_env_state * states;					// 36
		int width;								// 40
		int one_and_half;						// 44

	}	att_pass0;

	width <<= 2;

	att_pass0	pass0_paras;
	att_pass0 * paras = &pass0_paras;

	pass0_paras.contextWord = contextWord;
	pass0_paras.one_and_half = (1<<p) + ((1<<p)>>1); 


	pass0_paras.width = width;
	pass0_paras.sp_offset0 = (samples - contextWord) << 2;
	pass0_paras.sp_offset1 = pass0_paras.sp_offset0 + width;
	pass0_paras.sp_offset2 = pass0_paras.sp_offset1 + width;
	pass0_paras.sp_offset3 = pass0_paras.sp_offset2 + width;

	pass0_paras.sp_stripe_adjust = width*3 - (DECODE_CWROD_ALIGN<<2);

	pass0_paras.lutContext = lutContext;
	pass0_paras.lut_ctx_sign = (int)&lut_ctx_sign;
	pass0_paras.states = states;

	__asm__ volatile (
		"pushl %%ebx;"
		"movl %7, %%eax;"
		"movl %8, %%esi;"
//		"movl 12(%%esi), %%edx;"		//	MOV EDX, coder_D
"stribe_loop:;"
		"pushl %%eax;"				//	PUSH EAX
		"movl 40(%%esi), %%edi;"			//	MOV EDI, width	
		"addl (%%esi), %%edi;"			//	ADD EDI, ESI		// ESI -> EDI : range of current stripe
		"movl %%edi, 24(%%esi);"	//	MOV current_strip_end, EDI
"column_loop:;"
		"movl (%%esi), %%eax;"		
		"movl (%%eax), %%eax;"		//	MOV EAX, [ESI]		// cword = *(contextWord) 
		"testl %%eax, %%eax;"		//	TEST EAX, EAX
 		"jnz row0;"					//	JNZ row0
"skip_loop:;"
 		"addl $12, (%%esi);"		// ADD ESI, 12
		"movl (%%esi), %%ecx;"		
		"movl (%%ecx), %%ecx;"		// MOV ECX, [ESI]
 		"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
		"jz skip_loop;"				// JZ skip_loop
	// no skip
		"subl $8, (%%esi);"			// SUB ESI, 8
		"movl 24(%%esi), %%eax;"
		"cmpl %%eax, (%%esi);"		// CMP ESI, current_strip_end
		"jnz column_loop;"			// JNZ column_loop
 		"jmp next_stripe;"			// JMP next_stripe
"row0:;"
		"testl $0x80010, %%eax;"		// TEST EAX, 0x80010
		"jnz row1;"						// JNZ row1
		"testl $0x1ef, %%eax;"			// TEST EAX, 0x1ef
		"jz row1;"						// JZ row1
		"andl $0x1ef, %%eax;"			// AND EAX, 0x1ef
		"movl 28(%%esi), %%ebx;"		// MOV EBX, lutContext
		"movzx (%%ebx, %%eax), %%ebx;"// MOVZX EBX, byte ptr[EBX + EAX]
		"movl 36(%%esi), %%ecx;"		// MOV ECX, states
		"leal (%%ecx,%%ebx, 8), %%ebx;" // LEA EBX, [ECX + 8 * EBX]

		"movl (%%ebx), %%ecx;"		// MOV ECX, [EBX]	// _p_sign
		"subl %%ecx, %%edx;"		// SUB EDX, ECX		//D -= _p_sign
		"andl $1, %%ecx;"			// AND ECX, 1			// _sign
		"addl %%ecx, %%edx;"		// ADD EDX, ECX	//correct D by adding back _sign
		"jge row0_decode_in_cdp;"	// JGE	row0_decode_in_cdp
		"call pass0_mq_decode;"		// CALL pass0_mq_decode
 
"row0_decode_in_cdp:;"
		"movl (%%esi), %%ebx;"	// MOV EAX, [ESI]	//restore cword
		"movl (%%ebx), %%eax;"
		"testl %%ecx, %%ecx;"	// TEST ECX, ECX
		"jnz row0_decode_sign;"	// JNZ row0_decode_sign
		"orl $0x200000, %%eax;"	// OR EAX, 0x200000	// cword |= CONTEXT_PI; 
		"movl %%eax, (%%ebx);"	//	MOV [ESI], EAX		//	  cword changed, store back
		"jmp row1;"				// JMP row1

"row0_decode_sign:;"
		"movl %%eax, %%ecx;"	// MOV EBX, EAX
		"andl $0x400082, %%eax;"	// AND EAX, 0x400082	// sym = cword & SIG_PRO_CHECKING_MASK1 | cword & (SIGMA_SELF>>3);;
		"andl $0x40000, %%ecx;"		// AND EBX, 0x40000
		"sarl $2, %%ecx;"			// SAR EBX, 2
		"orl %%ecx, %%eax;"			// OR EAX, EBX
		"movl -4(%%ebx), %%ecx;"	//		MOV ECX, [ESI-4]
		"andl $0x80010, %%ecx;"		// AND ECX, 0x80010
		"sarl $1, %%ecx;"			// SAR ECX, 1
		"orl %%ecx, %%eax;"			// OR EAX, ECX
		"movl 4(%%ebx), %%ecx;"		// MOV ECX, dword ptr[ESI+4]
		"andl $0x80010, %%ecx;"		// AND ECX, 0x80010
		"shll $1, %%ecx;"			// SHL ECX, 1
		"orl %%ecx, %%eax;"		// OR EAX, ECX		// sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1)
		"movl %%eax, %%ecx;"	//	MOV ECX, EAX
		"sarl $0x0e, %%ecx;"	// SAR ECX, 0eh
		"orl %%ecx, %%eax;"	// OR EAX, ECX		// sym = (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
		"sarl $1, %%eax;"		// SAR EAX, 1	//	sym >>= 1

		"andl $0xff, %%eax;"	// AND EAX, 0xff
		"movl 32(%%esi), %%ebx;"		// LEA EBX, lut_ctx_sign
		"movzx (%%ebx,%%eax), %%ecx;"		// MOVZX ECX, byte ptr[EBX + EAX]	//	val = lut_ctx_sign[sym & 0x000000FF];
		"movl %%ecx, %%eax;"			// MOV EAX, ECX
		"andl $0xfffffffe, %%eax;"		// AND EAX, 0xfffffffe
		"andl $1, %%ecx;"				//	AND ECX, 1		// val & 1
		"movl 36(%%esi), %%ebx;"			// MOV EBX, states
		"leal 80(%%ebx, %%eax, 4), %%ebx;"		// LEA EBX, [EBX + 4 * EAX + 80]	//	state_ref = states  + STATE_SIGN_START + (val>>1);

		"movl (%%ebx), %%eax;"		// MOV EAX, [EBX]		//_p_sign
		"subl %%eax, %%edx;"		// SUB EDX, EAX		//D -= _p_sign
		"andl $1, %%eax;"			// AND EAX, 1			//	_sign
		"xorl %%eax, %%ecx;"		// XOR ECX, EAX
		"addl %%eax, %%edx;"		// ADD EDX, EAX		// correct D by adding back _sign
		"jge row0_decode_sign_in_cdp;"		// JGE	row0_decode_sign_in_cdp
		"call  pass0_mq_decode;"	// CALL pass0_mq_decode
"row0_decode_sign_in_cdp:;"
		"movl (%%esi), %%ebx;"		// MOV EBX, ESI
		"orl $0x20, -4(%%ebx);"		// OR  dword ptr[ESI - 4], 0x20		//	contextWord[-1] |= (SIGMA_CR<<0)
		"orl $0x08, 4(%%ebx);"		// OR  dword ptr[ESI - 4], 0x20		//	contextWord[ 1] |= (SIGMA_CL<<0);

		"movl (%%ebx), %%eax;"		// MOV EAX, [ESI]		 // restore cword
		"subl 40(%%esi), %%ebx;"		// SUB EBX, width
		"orl $0x20000, -16(%%ebx);"		// OR dword ptr[EBX - 16], 0x20000	// contextWord[-width - DECODE_CWROD_ALIGN-1] |=	(SIGMA_BR << 9);
		"orl $0x8000, -8(%%ebx);"		// OR dword ptr[EBX - 8], 0x8000	// contextWord[-width - DECODE_CWROD_ALIGN+1] |=	(SIGMA_BL << 9);
		"shll $19, %%ecx;"				//	SHL ECX, 19

		"orl %%ecx, %%eax;"			//	OR EAX, ECX
		"orl $0x200010, %%eax;"	// OR EAX, 0x200010	//cword |= (SIGMA_SELF) | (CONTEXT_PI) | (sym << CONTEXT_FIRST_CHI_POS);
		"shll $12, %%ecx;"				// SHL ECX, 12	// sym <<= 31
		"orl $0x10000, -12(%%ebx);"		// OR dword ptr[EBX - 12], 0x10000
		"orl %%ecx, -12(%%ebx);"		// OR dword ptr[EBX - 12], ECX	//contextWord[-width - DECODE_CWROD_ALIGN    ] |=	(SIGMA_BC << 9) | (sym << CONTEXT_NEXT_CHI_POS );

		"movl (%%esi), %%ebx;"
		"movl %%eax, (%%ebx);"	//	MOV [ESI], EAX
		"addl 44(%%esi), %%ecx;"		// ADD ECX, one_and_half
		"addl 4(%%esi), %%ebx;"		// MOV EBX, sp_offset0 //sp_offset_ptr
		"movl %%ecx, (%%ebx);"	//		MOV [ESI + EBX], ECX				// sp[0] : contextWord[sp_offset] = (sym<<31) + one_and_half;

"row1:;"
		"testl $0x400080, %%eax;"	//	TEST EAX, 0x400080
		"jnz row2;"	//			JNZ row2
		"testl $0xf78, %%eax;"	//		TEST EAX, 0xf78
		"jz row2;"		//			JZ row2
		"andl $0xf78, %%eax;"	//			AND EAX, 0xf78
		"sarl $3, %%eax;"		//			SAR EAX, 3
		"movl 28(%%esi), %%ebx;"	//			MOV EBX, lutContext
		"movzx (%%ebx,%%eax), %%ebx;"	//			MOVZX EBX, byte ptr[EBX + EAX]
		"movl 36(%%esi), %%ecx;"	//			MOV ECX, states
		"leal (%%ecx, %%ebx, 8), %%ebx;"	//			LEA EBX, [ECX + 8 * EBX]		// state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW1)>>3];
		"movl (%%ebx), %%ecx;"	//		MOV ECX, [EBX]		// _p_sign
		"subl %%ecx, %%edx;"	//		SUB EDX, ECX		// D -= _p_sign
		"andl $1, %%ecx;"	//			AND ECX, 1			// _sign
		"addl %%ecx, %%edx;"	//			ADD EDX, ECX		// correct D by adding back _sign
		"jge row1_decode_in_cdp;"	//				JGE	row1_decode_in_cdp
		"call pass0_mq_decode;"		//		CALL pass0_mq_decode
"row1_decode_in_cdp:;"
		"movl (%%esi), %%ebx;"
		"movl (%%ebx), %%eax;"	//	MOV EAX, [ESI]	// restore cword
		"testl %%ecx, %%ecx;"	//		TEST ECX, ECX
		"jnz row1_decode_sign;"	//				JNZ row1_decode_sign
		"orl $0x1000000, %%eax;"	//			OR EAX, 0x1000000	// cword |= CONTEXT_PI; 
		"movl %%eax, (%%ebx);"	//			MOV [ESI], EAX		//	 cword changed, store back
		"jmp row2;"	//			JMP row2
"row1_decode_sign:;"
		"andl $0x2080410, %%eax;"	//			AND EAX, 0x2080410	// sym = cword & (SIG_PRO_CHECKING_MASK0 | SIG_PRO_CHECKING_MASK2);
		"movl (%%esi), %%ebx;"
		"movl -4(%%ebx), %%ecx;"	//			MOV ECX, dword ptr[ESI-4]
		"andl $0x400080, %%ecx;"	//		AND ECX, 0x400080
		"sarl $1, %%ecx;"	//			SAR ECX, 1
		"orl %%ecx, %%eax;"	//			OR EAX, ECX
		"movl 4(%%ebx), %%ecx;"	//			MOV ECX, dword ptr[ESI+4]
		"andl $0x400080, %%ecx;"	//			AND ECX, 0x400080
		"shll $1, %%ecx;"	//			SHL ECX, 1
		"orl %%ecx, %%eax;"	//			OR EAX, ECX			// // sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK1) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK1) << 1)
		"movl %%eax, %%ecx;"	//			MOV ECX, EAX
		"sarl $0x0e, %%ecx;"	//			SAR ECX, 0eh
		"orl %%ecx, %%eax;"	//			OR EAX, ECX			// sym = (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
		"sarl $4, %%eax;"	//				SAR EAX, 4			// sym >>= 4

		"andl $0xff, %%eax;"	//			AND EAX, 0xff
		"movl 32(%%esi), %%ebx;"	//				LEA EBX, lut_ctx_sign
		"movzx (%%ebx, %%eax), %%ecx;"	//			MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
		"movl %%ecx, %%eax;"	//		MOV EAX, ECX
		"andl $0xfffffffe, %%eax;"	//			AND EAX, 0xfffffffe
		"andl $1, %%ecx;"	//			AND ECX, 1							// val & 1
		"movl 36(%%esi), %%ebx;"	//			MOV EBX, states
		"leal 80(%%ebx, %%eax, 4), %%ebx;"	//			LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);

		"movl (%%ebx), %%eax;"	//		MOV EAX, [EBX]		// _p_sign
		"subl %%eax, %%edx;"	//		SUB EDX, EAX		// D -= _p_sign
		"andl $1, %%eax;"		//			AND EAX, 1			// _sign
		"xorl %%eax, %%ecx;"	//			XOR ECX, EAX
		"addl %%eax, %%edx;"	//			ADD EDX, EAX		// correct D by adding back _sign
		"jge row1_decode_sign_in_cdp;"	//			JGE	row1_decode_sign_in_cdp
		"call pass0_mq_decode;"	//			CALL pass0_mq_decode
"row1_decode_sign_in_cdp:;"
		"movl (%%esi), %%ebx;"
		"orl $0x100, -4(%%ebx);"		// OR  dword ptr[ESI - 4], 0x100		contextWord[-1] |= (SIGMA_CR<<3)
		"orl $0x40, 4(%%ebx);"		// OR  dword ptr[ESI + 4], 0x40		contextWord[ 1] |= (SIGMA_CL<<3);
		"movl (%%ebx), %%eax;"		//		MOV EAX, [ESI]	// restore cword
		"shll $22, %%ecx;"			//	SHl ECX, 22
		"orl $0x1000080, %%eax;"	//			OR EAX, 0x1000080
		"orl %%ecx, %%eax;"		//			OR EAX, ECX							// cword |= (SIGMA_SELF<<3) | (CONTEXT_PI<<3) | (sym<<(CONTEXT_FIRST_CHI_POS+3));
		"movl %%eax, (%%ebx);"	//			MOV [ESI], EAX
		"shll $9, %%ecx;"	//			SHL ECX, 9
		"addl 44(%%esi), %%ecx;"	//			ADD ECX, one_and_half				// sp[1] = (sym<<31) + one_and_half;
		"addl 8(%%esi), %%ebx;"	
		"movl %%ecx, (%%ebx);"	//			MOV EBX, sp_offset1	
								//MOV [ESI + EBX], ECX				// sp[1] : contextWord[sp_offset1] = (sym<<31) + one_and_half;
"row2:;"
		"testl $0x2000400, %%eax;"		//	TEST EAX, 0x2000400
		"jnz row3;"		//			JNZ row3
		"testl $0x7bc0, %%eax;"	//			TEST EAX, 0x7bc0
		"jz row3;"		//			JZ row3
		"andl $0x7bc0, %%eax;"		//				AND EAX, 0x7bc0
		"sarl $6, %%eax;"		//				SAR EAX, 6
		"movl 28(%%esi), %%ebx;"	//			MOV EBX, lutContext
		"movzx (%%ebx, %%eax), %%ebx;"	//			MOVZX EBX, byte ptr[EBX + EAX]		
		"movl 36(%%esi), %%ecx;"	//		MOV ECX, states
		"leal (%%ecx, %%ebx, 8), %%ebx;"	//				LEA EBX, [ECX + 8 * EBX]		//	state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW2)>>6];
		
		"movl (%%ebx), %%ecx;"		//			MOV ECX, [EBX]		// _p_sign
		"subl %%ecx, %%edx;"	//		SUB EDX, ECX		// D -= _p_sign
		"andl $1, %%ecx;"	//			AND ECX, 1			// _sign
		"addl %%ecx, %%edx;"	//				ADD EDX, ECX		// correct D by adding back _sign
		"jge row2_decode_in_cdp;"	//			JGE	row2_decode_in_cdp
		"call pass0_mq_decode;"		//			CALL pass0_mq_decode
"row2_decode_in_cdp:;"
		"movl (%%esi), %%ebx;"
		"movl (%%ebx), %%eax;"	//		MOV EAX, [ESI]	// restore cword
		"testl %%ecx, %%ecx;"	//			TEST ECX, ECX
		"jnz row2_decode_sign;"	//			JNZ row2_decode_sign
		"orl $0x8000000, %%eax;"	//			OR EAX, 0x8000000	// cword |= CONTEXT_PI; 
		"movl %%eax, (%%ebx);"	//			MOV [ESI], EAX		//	 cword changed, store back
		"jmp row3;"		//			JMP row3
"row2_decode_sign:;"
		"andl $0x10402080, %%eax;"	//			AND EAX, 0x10402080		// sym = cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
		"movl -4(%%ebx), %%ecx;"	//			MOV ECX, dword ptr[ESI-4]
		"andl $0x2000400, %%ecx;"	//			AND ECX, 0x2000400
		"sarl $1, %%ecx;"		//			SAR ECX, 1
		"orl %%ecx, %%eax;"	//			OR EAX, ECX
		"movl 4(%%ebx), %%ecx;"	//			MOV ECX, dword ptr[ESI+4]
		"andl $0x2000400, %%ecx;"	//		AND ECX, 0x2000400
		"shll $1, %%ecx;"	//			SHL ECX, 1
		"orl %%ecx, %%eax;"	//			OR EAX, ECX			//// sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK2) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK2) << 1)
		"movl %%eax, %%ecx;"	//				MOV ECX, EAX
		"sarl $0x0e, %%ecx;"	//				SAR ECX, 0eh
		"orl %%ecx, %%eax;"	//			OR EAX, ECX			// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
		"sarl $7, %%eax;"	//			SAR EAX, 7			// sym >>= 7

		"andl $0xff, %%eax;"	//			AND EAX, 0xff
		"movl 32(%%esi), %%ebx;"	//			LEA EBX, lut_ctx_sign
		"movzx (%%ebx, %%eax), %%ecx;"	//				MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
		"movl %%ecx, %%eax;"		//			MOV EAX, ECX
		"andl $0xfffffffe, %%eax;"		//			AND EAX, 0xfffffffe
		"andl $1, %%ecx;"			//			AND ECX, 1							// val & 1
		"movl 36(%%esi), %%ebx;"	//			MOV EBX, states
		"leal 80(%%ebx, %%eax, 4), %%ebx;"	//			LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);

		"movl (%%ebx), %%eax;"		//		MOV EAX, [EBX]		// _p_sign
		"subl %%eax, %%edx;"	//		SUB EDX, EAX		// D -= _p_sign
		"andl $1, %%eax;"		//				AND EAX, 1			// _sign
		"xorl %%eax, %%ecx;"	//				XOR ECX, EAX
		"addl %%eax, %%edx;"	//			ADD EDX, EAX		// correct D by adding back _sign
		"jge row2_decode_sign_in_cdp;"		//	JGE	row2_decode_sign_in_cdp
		"call pass0_mq_decode;"	//			CALL pass0_mq_decode
"row2_decode_sign_in_cdp:;"
		"movl (%%esi), %%ebx;"
		"orl $0x800, -4(%%ebx);"	//		OR  dword ptr[ESI - 4], 0x800		// contextWord[-1] |= (SIGMA_CR<<6)
		"orl $0x200, 4(%%ebx);"		//				OR  dword ptr[ESI + 4], 0x200		// contextWord[ 1] |= (SIGMA_CL<<6);

		"movl (%%ebx), %%eax;"		//			MOV EAX, [ESI]	// restore cword
		"shll $25, %%ecx;"		//		SHl ECX, 25
		"orl $0x8000400, %%eax;"		//			OR EAX, 0x8000400
		"orl %%ecx, %%eax;"	//				OR EAX, ECX							// word |= (SIGMA_SELF<<6) | (CONTEXT_PI<<6) | (sym<<(CONTEXT_FIRST_CHI_POS+6));
		"movl %%eax, (%%ebx);"	//			MOV [ESI], EAX
		"shll $6, %%ecx;"		//				SHL ECX, 6
		"addl 44(%%esi), %%ecx;"		//			ADD ECX, one_and_half				
		"addl 12(%%esi), %%ebx;"	//			MOV EBX, sp_offset2
		"movl %%ecx, (%%ebx);"	//			MOV [ESI + EBX], ECX				// sp : contextWord[sp_offset + width_by2] = (sym<<31) + one_and_half;

"row3:;"
		"testl $0x10002000, %%eax;"	//	TEST EAX, 0x10002000
		"jnz next_column;"		//			JNZ next_column
		"testl $0x3de00, %%eax;"		//			TEST EAX, 0x3de00
		"jz next_column;"		//				JZ next_column
		"andl $0x3de00, %%eax;"		//				AND EAX, 0x3de00
		"sarl $9, %%eax;"		//				SAR EAX, 9
		"movl 28(%%esi), %%ebx;"	//			MOV EBX, lutContext
		"movzx (%%ebx, %%eax), %%ebx;"		//				MOVZX EBX, byte ptr[EBX + EAX]
		"movl 36(%%esi), %%ecx;"	//		MOV ECX, states
		"leal (%%ecx, %%ebx, 8), %%ebx;"	//			LEA EBX, [ECX + 8 * EBX]		//	state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW3)>>9];

		"movl (%%ebx), %%ecx;"	//		MOV ECX, [EBX]		// _p_sign
		"subl %%ecx, %%edx;"	//			SUB EDX, ECX		// D -= _p_sign
		"andl $1, %%ecx;"		//				AND ECX, 1			// _sign
		"addl %%ecx, %%edx;"	//			ADD EDX, ECX		// correct D by adding back _sign
		"jge row3_decode_in_cdp;"	//				JGE	row3_decode_in_cdp
		"call pass0_mq_decode;"	//			CALL pass0_mq_decode
"row3_decode_in_cdp:;"
		"movl (%%esi), %%ebx;"
		"movl (%%ebx), %%eax;"		//		MOV EAX, [ESI]	// restore cword
		"testl %%ecx, %%ecx;"		//			TEST ECX, ECX
		"jnz row3_decode_sign;"	//				JNZ row3_decode_sign
		"orl $0x40000000, %%eax;"	//				OR EAX, 0x40000000	// cword |= CONTEXT_PI; 
		"movl %%eax, (%%ebx);"	//			MOV [ESI], EAX		//	 cword changed, store back
		"jmp next_column;"	//				JMP next_column
"row3_decode_sign:;"
		"andl $0x82010400, %%eax;"	//		AND EAX, 0x82010400			// sym = cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
		"movl -4(%%ebx), %%ecx;"		//			MOV ECX, [ESI-4]
		"andl $0x10002000, %%ecx;"		//			AND ECX, 0x10002000
		"sarl $1, %%ecx;"		//				SAR ECX, 1
		"orl %%ecx, %%eax;"	//			OR EAX, ECX
		"movl 4(%%ebx), %%ecx;"	//			MOV ECX, dword ptr[ESI+4]
		"andl $0x10002000, %%ecx;"	//			AND ECX, 0x10002000
		"shll $1, %%ecx;"		//			SHL ECX, 1
		"orl %%ecx, %%eax;"		//				OR EAX, ECX			// sym |= ((contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1) | ((contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1)
		"movl %%eax, %%ecx;"		//				MOV ECX, EAX
		"sar $0x0e, %%ecx;"		//				SAR ECX, 0eh
		"orl %%ecx, %%eax;"		//				OR EAX, ECX			// sym = (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS))
		"sarl $10, %%eax;"		//				SAR EAX, 10			// sym >>= 10

		"andl $0xff, %%eax;"	//			AND EAX, 0xff
		"movl 32(%%esi), %%ebx;"	//				LEA EBX, lut_ctx_sign
		"movzx (%%ebx, %%eax), %%ecx;"		//				MOVZX ECX, byte ptr[EBX + EAX]		// val = lut_ctx_sign[sym & 0x000000FF];
		"movl %%ecx, %%eax;"	//			MOV EAX, ECX
		"andl $0xfffffffe, %%eax;"			//			AND EAX, 0xfffffffe
		"andl $1, %%ecx;"	//			AND ECX, 1							// val & 1
		"movl 36(%%esi), %%ebx;"	//				MOV EBX, states
		"leal	80(%%ebx, %%eax, 4), %%ebx;"	//				LEA EBX, [EBX + 4 * EAX + 80]		// state_ref = states  + STATE_SIGN_START + (val>>1);

		"movl (%%ebx), %%eax;"		//		MOV EAX, [EBX]		// _p_sign
		"subl %%eax, %%edx;"	//		SUB EDX, EAX		// D -= _p_sign
		"andl $1, %%eax;"		//				AND EAX, 1			// _sign
		"xorl %%eax, %%ecx;"	//				XOR ECX, EAX
		"addl %%eax, %%edx;"	//			ADD EDX, EAX		// correct D by adding back _sign
		"jge row3_decode_sign_in_cdp;"		//	JGE	row3_decode_sign_in_cdp
		"call pass0_mq_decode;"	//			CALL pass0_mq_decode

"row3_decode_sign_in_cdp:;"
		"movl (%%esi), %%ebx;"	//	MOV EBX, ESI
		"orl $0x4000, -4(%%ebx);"	//	OR  dword ptr[ESI - 4], 0x4000		// contextWord[-1] |= (SIGMA_CR<<9)
		"orl $0x1000, 4(%%ebx);"		//				OR  dword ptr[ESI + 4], 0x1000		// contextWord[ 1] |= (SIGMA_CL<<9);
		"addl 40(%%esi), %%ebx;"		//				ADD EBX, width
		"orl $4, 8(%%ebx);"	//				OR dword ptr[EBX + 8 ], 4		// contextWord[width + DECODE_CWROD_ALIGN -1] |= SIGMA_TR; 
		"orl $1, 16(%%ebx);"	//				OR dword ptr[EBX + 16], 1		//	contextWord[width + DECODE_CWROD_ALIGN +1] |= SIGMA_TL;

		"shll $18, %%ecx;"	//				SHL ECX, 18
		"orl $2, 12(%%ebx);"	//			OR dword ptr[EBX + 12], 2		
		"orl %%ecx, 12(%%ebx);"	//			OR dword ptr[EBX + 12], ECX		//	contextWord[width + DECODE_CWROD_ALIGN   ] |= SIGMA_TC | (sym<<CONTEXT_PREV_CHI_POS);

		"movl (%%esi), %%ebx;"
		"movl (%%ebx), %%eax;"		//			MOV EAX, [ESI]		 // restore cword
		"shll $10, %%ecx;"	//			SHL ECX, 10			// sym<<(CONTEXT_FIRST_CHI_POS+9)
		"orl %%ecx, %%eax;"		//				OR EAX, ECX
		"orl $0x40002000, %%eax;"	//			OR EAX, 0x40002000	// cword |= (SIGMA_SELF) | (CONTEXT_PI) | (sym<<(CONTEXT_FIRST_CHI_POS+9));
		"movl %%eax, (%%ebx);"		//			MOV [ESI], EAX		// save cword

		"shll $3, %%ecx;"	//			SHL ECX, 3			// sym <<= 31
		"addl 44(%%esi), %%ecx;"	//				ADD ECX, one_and_half
		"addl 16(%%esi), %%ebx;"	//				MOV EBX, sp_offset3
		"movl %%ecx, (%%ebx);"		//				MOV [ESI + EBX], ECX				// sp[0] : ccontextWord[sp_offset + width_by3] = (sym<<31) + one_and_half;

"next_column:;"
		"addl $4, (%%esi);"			//			ADD ESI, 4
		"movl (%%esi), %%eax;"
		"cmpl %%eax, 24(%%esi);"	//				CMP ESI, current_strip_end
		"jnz column_loop;"			//			JNZ column_loop

"next_stripe:;"
		"addl $12, (%%esi);"		//		ADD ESI, 12		// next strip

		"movl 20(%%esi), %%ebx;"	//			MOV EBX, sp_stripe_adjust
		"addl %%ebx, 4(%%esi);"
		"addl %%ebx, 8(%%esi);"
		"addl %%ebx, 12(%%esi);"
		"addl %%ebx, 16(%%esi);"
		"popl %%eax;"				//			POP EAX
		"decl %%eax;"				//				DEC EAX
		"jnz stribe_loop;"			//				JNZ stribe_loop
		"jmp proc_return;"			//	JMP proc_return
	
"pass0_mq_decode:;"
 		"movl %1, %%eax;"		//	_asm MOV EAX, coder_A
 		"movl %2, %%edi;"	//	__asm MOV EDI, coder_C
		"addl %%edx, %%eax;"		//	__asm ADD EAX, EDX									
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX								
		"movl (%%ebx), %%edx;"		//	__asm MOV EDX, [EBX]							
		"jl LSI_selected;"			//	__asm JL	LSI_selected							
		"andl $0xffff00, %%edx;"	//	__asm AND EDX, 0xffff00								
		"cmpl %%edx, %%eax;"		//	__asm CMP EAX, EDX									
		"jge USIM;"					//	__asm JGE USIM										
		"xorl $1, %%ecx;"			//	__asm XOR ECX, 1									
		"movl 4(%%ebx), %%edx;"		//	__asm MOV EDX, [EBX + 4]							
		"movq 8(%%edx), %%mm0;"		//	__asm MOVQ MM0, [EDX + 8]							
		"movq %%mm0,(%%ebx);"		//__asm MOVQ [EBX], MM0
//////////////////////////////////////////////////////////////////////////
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_0:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_0;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"		//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_0;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"
		"cmpl $0x8f, %%edx;"		//	__asm CMP EDX,0x8F									
		"jle T_le8F_0;"				//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"		//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"			//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_0;"		//	__asm JMP fill_lsbs_done_##x						
"T_le8F_0:;"						//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"		//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"			//	__asm ADD EDI, EDX									
		"incl %%ebx;"				// __asm INC EBX										
		"movl %%ebx, %6;"		//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"			//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_0;"		//	__asm JMP fill_lsbs_done_##x						
"no_ff_0:;"							//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"				//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"			//	__asm MOV EDX, 8									
"fill_lsbs_done_0:;"				//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"				//	__asm POP EBX										
"skip_fill_lsbs_0:;"				//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"				//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_0;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////

		"jmp state_transition_done;"	//__asm JMP state_transition_done						
"USIM:;"					//__asm USIM:							
		"movl 4(%%ebx), %%edx;"		//__asm MOV EDX, dword ptr[EBX+4]						
		"movq (%%edx), %%mm0;"		//__asm MOVQ MM0, [EDX]								
		"movq %%mm0, (%%ebx);"		//__asm MOVQ [EBX], MM0								
		
//////////////////////////////////////////////////////////////////////////
		//		__asm BMI_RENORM_ASM(1)								
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_1:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_1;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"	//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_1;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"cmpl $0x8f, %%edx;"	//	__asm CMP EDX,0x8F									
		"jle T_le8F_1;"			//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"	//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"	//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"		//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_1;"	//	__asm JMP fill_lsbs_done_##x						
"T_le8F_1:;"					//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"	//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX									
		"incl %%ebx;"			// __asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"		//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_1;"	//	__asm JMP fill_lsbs_done_##x						
"no_ff_1:;"						//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"			//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"		//	__asm MOV EDX, 8									
"fill_lsbs_done_1:;"		//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"		//	__asm POP EBX										
"skip_fill_lsbs_1:;"		//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"			//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_1;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////

		"jmp state_transition_done;"		//__asm JMP state_transition_done						
"LSI_selected:;"		//_asm LSI_selected :								
		"andl $0xffff00, %%edx;"	//__asm AND EDX, 0xffff00								
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"cmpl %%edx, %%eax;"		//__asm CMP EAX, EDX
		"movl %%edx, %%eax;"		//	__asm MOV EAX, EDX									
		"jge LSIL;"				//__asm JGE LSIL										
		"movl 4(%%ebx), %%edx;"		//__asm MOV EDX, dword ptr[EBX+4]						
		"movq (%%edx), %%mm0;"		//	__asm MOVQ MM0, [EDX]								
		"movq %%mm0,(%%ebx);"			//	__asm MOVQ [EBX], MM0								

////////////////////////////////////////////////////////////////////////
		//		__asm BMI_RENORM_ASM(2)								
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_2:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_2;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"		//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_2;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"cmpl $0x8f, %%edx;"	//	__asm CMP EDX,0x8F									
		"jle T_le8F_2;"			//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"	//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"	//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"		//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_2;"	//	__asm JMP fill_lsbs_done_##x						
"T_le8F_2:;"					//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"	//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX									
		"incl %%ebx;"			// __asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"		//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_2;"	//	__asm JMP fill_lsbs_done_##x						
"no_ff_2:;"						//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"			//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"		//	__asm MOV EDX, 8									
"fill_lsbs_done_2:;"		//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"		//	__asm POP EBX										
"skip_fill_lsbs_2:;"		//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"			//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_2;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////

		"jmp state_transition_done;"	//__asm JMP state_transition_done						
"LSIL:;"					//__asm LSIL:											
		"xorl $1, %%ecx;"		//	__asm XOR ECX, 1									
		"movl 4(%%ebx), %%edx;"		//__asm MOV EDX, [EBX + 4]							
		"movq 8(%%edx), %%mm0;"		//	__asm MOVQ MM0, [EDX + 8]							
		"movq %%mm0, (%%ebx);"		//	__asm MOVQ [EBX], MM0								

//////////////////////////////////////////////////////////////////////////
		//		__asm BMI_RENORM_ASM(3)								
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_3:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_3;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"		//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_3;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"cmpl $0x8f, %%edx;"	//	__asm CMP EDX,0x8F									
		"jle T_le8F_3;"			//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"	//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"	//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"		//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_3;"	//	__asm JMP fill_lsbs_done_##x						
"T_le8F_3:;"					//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"	//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX
		"incl %%ebx;"			// __asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"		//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_3;"	//	__asm JMP fill_lsbs_done_##x						
"no_ff_3:;"						//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"			//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"		//	__asm MOV EDX, 8									
"fill_lsbs_done_3:;"		//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"		//	__asm POP EBX										
"skip_fill_lsbs_3:;"		//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"			//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_3;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////
"state_transition_done:;"			// __asm state_transition_done:						
		"movl %%eax, %%edx;"		//__asm MOV EDX,EAX									
		"subl $0x00800000, %%edx;"	//__asm SUB EDX,0x00800000							
		"cmpl %%edx, %%edi;"		//	__asm CMP EDI,EDX									
		"cmovl %%edi, %%edx;"		//__asm CMOVL EDX,EDI									
		"subl %%edx, %%eax;"		//__asm SUB EAX,EDX									
		"subl %%edx, %%edi;"		//	__asm SUB EDI,EDX									
		"movl %%edi, %2;"		//__asm MOV coder_C,EDI
		"movl %%eax, %1;"		//__asm MOV coder_A,EAX
		"ret;"						//__asm ret											
 
"proc_return:;"						//	proc_return:
// 		"movl %%edx, 12(%%esi);"	//		MOV coder_D, EDX
		"emms;"						//				EMMS
		"popl %%ebx;"

		// output
		: "=d"(coder_D),			// %0
		  "=g"(coder_A),			// %1
		  "=g"(coder_C),			// %2
		  "=g"(coder_t),			// %3
		  "=g"(coder_S),			// %4
		  "=g"(coder_T),			// %5
		  "=g"(coder_buf_next)		// %6
		  // input
		: "g"(num_stripes),			// %7
		  "g"(paras),				// %8
		  "0"(coder_D),
		  "1"(coder_A),
		  "2"(coder_C),
		  "3"(coder_t),	
		  "4"(coder_S),
		  "5"(coder_T),
		  "6"(coder_buf_next)
		: "eax", "ecx", "esi", "edi"
	);
 		CODER_CHECK_IN_ASM(coder)

}


void bmi_t1_decode_pass1_asmATT(bmi_mq_decoder &coder, mq_env_state states[],
						 int p, int *samples, int *contexts, int width, int num_stripes )
{

	CODER_CHECK_OUT_ASM(coder)

	typedef struct s_att_pass1_stru
	{
		int *contextWord; //	0
		int sp_offset0; // 4
		int sp_offset1; // 8
		int sp_offset2; // 12
		int sp_offset3; // 16
		int sp_stripe_adjust; // 20
		int * current_strip_end; // 24
		mq_env_state * states; // 28
		int width; // 32
		int lsb; // 36
		int half_lsb; // 40
		int last_plane; // 44

	}att_pass1;

	width <<= 2;
	states += STATE_MAG_START;

	att_pass1 pass1_paras;
	att_pass1 * paras = &pass1_paras;

	pass1_paras.lsb = 1<<p;
	pass1_paras.half_lsb = pass1_paras.lsb >>1;
	pass1_paras.last_plane = pass1_paras.lsb << 2;

	pass1_paras.width = width;
	pass1_paras.contextWord = contexts;
	pass1_paras.sp_offset0 = (samples - contexts) << 2;
	pass1_paras.sp_offset1 = pass1_paras.sp_offset0 + width;
	pass1_paras.sp_offset2 = pass1_paras.sp_offset1 + width;
	pass1_paras.sp_offset3 = pass1_paras.sp_offset2 + width;

	pass1_paras.sp_stripe_adjust = width*3 - (DECODE_CWROD_ALIGN<<2);
	pass1_paras.states = states;

	__asm__ volatile (
			"pushl %%ebx;"
			"movl %7, %%eax;"
			"movl %8, %%esi;"
			//		"movl 12(%%esi), %%edx;"		//	MOV EDX, coder_D
		"stripe_loop_p1:;"
			"pushl %%eax;" //	PUSH EAX
			"movl 32(%%esi), %%edi;" //	MOV EDI, width
			"addl (%%esi), %%edi;" //	ADD EDI, ESI		// ESI -> EDI : range of current stripe
			"movl %%edi, 24(%%esi);" //	MOV current_strip_end, EDI
		"column_loop_p1:;"
			"movl (%%esi), %%eax;" //MOV EAX, [ESI]		// cword = *(contextWord)
			"movl (%%eax), %%eax;"
			"testl $0x24900000, %%eax;" //TEST EAX, 0x24900000
			"jnz row0_p1;"//JNZ row0

		"skip_loop_p1:;"
			"addl $8,(%%esi);" //				ADD ESI, 8
			"movl (%%esi), %%ecx;"//				MOV ECX, [ESI]
			"movl (%%ecx), %%ecx;"
			"testl %%ecx, %%ecx;"//				TEST ECX, ECX
			"jz skip_loop_p1;"//				JZ skip_loop
			//
			"subl $4, (%%esi);"//				SUB ESI, 4
			"movl 24(%%esi), %%eax;"
			"cmpl %%eax, (%%esi);"// CMP ESI, current_strip_end
			"jnz column_loop_p1;"// JNZ column_loop
			"jmp next_stripe_p1;"// JMP next_stripe

		"row0_p1:;"
			"testl $0x100000, %%eax;"//				TEST EAX, 0x100000
			"jz row1_p1;"//				JZ row1
			"testl $0x10, %%eax;"//				TEST EAX, 0x10
			"jz row1_p1;"//				JZ row1			// cword & MAG_REF_CHECKING_MASK0) == MAG_REF_CHECKING_MASK0)  ?
			"movl 4(%%esi), %%ecx;"//					MOV ECX, sp_offset0
			"addl (%%esi), %%ecx;"
			"movl (%%ecx),%%ecx;"//					MOV ECX, [ESI + ECX]		// val = sp[0]];
			"pushl %%ecx;"//					PUSH ECX					// push val
			"andl $0x7fffffff, %%ecx;"//					AND ECX, 0x7fffffff
			"movl 28(%%esi), %%ebx;"//					MOV EBX, states
			"cmpl 44(%%esi), %%ecx;"//					CMP ECX, last_plane
			"jge row0_2nd_mag_p1;"//					JGE row0_2nd_mag
			"testl $0x1ef, %%eax;"//						TEST EAX, 0x1ef			// if (cword & NEIGHBOR_SIGMA_ROW0)
			"jz row0_mq_decode_p1;"//						JZ row0_mq_decode
			"addl $8, %%ebx;"//						ADD EBX, 8
			"jmp row0_mq_decode_p1;"//						JMP row0_mq_decode
		"row0_2nd_mag_p1:;"
			"addl $16, %%ebx;"//						ADD EBX, 16
		"row0_mq_decode_p1:;"
			"movl (%%ebx), %%ecx;"//						MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"//						SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"//						AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"//						ADD EDX, ECX		// correct D by adding back _sign
			"jge row0_mq_decode_done_p1;"//						JGE	row0_mq_decode_done
			"call pass1_mq_decode;"//						CALL pass1_mq_decode
		"row0_mq_decode_done_p1:;"
			"popl %%ebx;"//					POP EBX		// val
			"testl %%ecx, %%ecx;"//					TEST ECX, ECX
			"jnz row0_deq_unchanged_p1;"//					JNZ row0_deq_unchanged
			"xorl 36(%%esi), %%ebx;"//					XOR EBX, lsb
		"row0_deq_unchanged_p1:;"
			"orl 40(%%esi), %%ebx;"//					OR EBX, half_lsb
			"movl 4(%%esi), %%ecx;"//					MOV ECX, sp_offset0
			"addl (%%esi), %%ecx;"
			"movl %%ebx,(%%ecx);"//					MOV [ESI + ECX], EBX
			"movl (%%esi), %%eax;" //MOV EAX, [ESI]		// cword = *(contextWord)
			"movl (%%eax), %%eax;"
			//
		"row1_p1:;"
			"testl $0x800000, %%eax;"//				TEST EAX, 0x800000
			"jz row2_p1;"//				JZ row2

			"testl $0x80, %%eax;"//				TEST EAX, 0x80
			"jz row2_p1;"//				JZ row2			// cword & MAG_REF_CHECKING_MASK1) == MAG_REF_CHECKING_MASK1)  ?
			"movl 8(%%esi), %%ecx;"//					MOV ECX, sp_offset1
			"addl (%%esi), %%ecx;"
			"movl (%%ecx),%%ecx;"//					MOV ECX, [ESI + ECX]
			"pushl %%ecx;"//					PUSH ECX					// push val
			"andl $0x7fffffff, %%ecx;"//					AND ECX, 0x7fffffff
			"movl 28(%%esi), %%ebx;"//					MOV EBX, states
			"cmpl 44(%%esi), %%ecx;"//					CMP ECX, last_plane
			"jge row1_2nd_mag_p1;"//					JGE row1_2nd_mag
			"testl $0xf78, %%eax;"//					TEST EAX, 0xf78			// if (cword & NEIGHBOR_SIGMA_ROW1)
			"jz row1_mq_decode_p1;"//					JZ row1_mq_decode
			"addl $8, %%ebx;"//					ADD EBX, 8
			"jmp row1_mq_decode_p1;"//					JMP row1_mq_decode
		"row1_2nd_mag_p1:;"
			"addl $16, %%ebx;"//					ADD EBX, 16
		"row1_mq_decode_p1:;"
			"movl (%%ebx), %%ecx;"//					MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"//					SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"//					AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"//					ADD EDX, ECX		// correct D by adding back _sign
			"jge row1_mq_decode_done_p1;"//					JGE	row1_mq_decode_done
			"call pass1_mq_decode;"//					CALL pass1_mq_decode
		"row1_mq_decode_done_p1:;"
			"popl %%ebx;"//					POP EBX		// val
			"testl %%ecx, %%ecx;"//					TEST ECX, ECX
			"jnz row1_deq_unchanged_p1;"//					JNZ row1_deq_unchanged
			"xorl 36(%%esi), %%ebx;"//					XOR EBX, lsb
		"row1_deq_unchanged_p1:;"
			"orl 40(%%esi), %%ebx;"//					OR EBX, half_lsb
			"movl 8(%%esi), %%ecx;"//					MOV ECX, sp_offset1
			"addl (%%esi), %%ecx;"
			"movl %%ebx,(%%ecx);"//					MOV [ESI + ECX], EBX
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"//      MOV EAX, [ESI]		// cword = *(contextWord)
			//
			//
		"row2_p1:;"
			"testl $0x4000000, %%eax;"//				TEST EAX, 0x4000000
			"jz row3_p1;"//				JZ row3
			"testl $0x400, %%eax;"//				TEST EAX, 0x400
			"jz row3_p1;"//				JZ row3			// cword & MAG_REF_CHECKING_MASK2) == MAG_REF_CHECKING_MASK2)  ?
			"movl 12(%%esi), %%ecx;"//					MOV ECX, sp_offset2
			"addl (%%esi), %%ecx;"
			"movl (%%ecx),%%ecx;"//					MOV ECX, [ESI + ECX]		// val = contextWord[sp_offset+width+width];
			"pushl %%ecx;"//					PUSH ECX					// push val
			"andl $0x7fffffff, %%ecx;"//					AND ECX, 0x7fffffff
			"movl 28(%%esi), %%ebx;"//					MOV EBX, states
			"cmpl 44(%%esi), %%ecx;"//					CMP ECX, last_plane
			"jge row2_2nd_mag_p1;"//					JGE row2_2nd_mag
			"testl $0x7bc0, %%eax;"//					TEST EAX, 0x7bc0			// if (cword & NEIGHBOR_SIGMA_ROW3)
			"jz row2_mq_decode_p1;"//					JZ row2_mq_decode
			"addl $8, %%ebx;"//					ADD EBX, 8
			"jmp row2_mq_decode_p1;"//					JMP row2_mq_decode
		"row2_2nd_mag_p1:;"
			"addl $16, %%ebx;"//					ADD EBX, 16
		"row2_mq_decode_p1:;"
			"movl (%%ebx), %%ecx;"//					MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"//					SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"//					AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"//					ADD EDX, ECX		// correct D by adding back _sign
			"jge row2_mq_decode_done_p1;"//					JGE	row2_mq_decode_done
			"call pass1_mq_decode;"//					CALL pass1_mq_decode
		"row2_mq_decode_done_p1:;"
			"popl %%ebx;"//					POP EBX		// val
			"testl %%ecx, %%ecx;"//					TEST ECX, ECX
			"jnz row2_deq_unchanged_p1;"//					JNZ row2_deq_unchanged
			"xorl 36(%%esi), %%ebx;"//					XOR EBX, lsb
		"row2_deq_unchanged_p1:;"
			"orl 40(%%esi), %%ebx;"//					OR EBX, half_lsb
			"movl 12(%%esi), %%ecx;"//					MOV ECX, sp_offset2
			"addl (%%esi), %%ecx;"
			"movl %%ebx,(%%ecx);"//					MOV [ESI + ECX], EBX
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"//					MOV EAX, [ESI]		// cword = *(contextWord)
			//
		"row3_p1:;"
			"testl $0x20000000, %%eax;"//				TEST EAX, 0x20000000
			"jz next_column_p1;"//				JZ next_column
			"testl $0x2000, %%eax;"//				TEST EAX, 0x2000
			"jz next_column_p1;"//				JZ next_column			// cword & MAG_REF_CHECKING_MASK3) == MAG_REF_CHECKING_MASK3)  ?
			"movl 16(%%esi), %%ecx;"//					MOV ECX, sp_offset3
			"addl (%%esi), %%ecx;"
			"movl (%%ecx),%%ecx;"//					MOV ECX, [ESI + ECX]		// val = contextWord[sp_offset+width+width];
			"pushl %%ecx;"//					PUSH ECX					// push val
			"andl $0x7fffffff, %%ecx;"//					AND ECX, 0x7fffffff
			"movl 28(%%esi), %%ebx;"//					MOV EBX, states
			"cmpl 44(%%esi), %%ecx;"//					CMP ECX, last_plane
			"jge row3_2nd_mag_p1;"//					JGE row3_2nd_mag
			"testl $0x3de00, %%eax;"//					TEST EAX, 0x3de00			// if (cword & NEIGHBOR_SIGMA_ROW3)
			"jz row3_mq_decode_p1;"//					JZ row3_mq_decode
			"addl $8, %%ebx;"//					ADD EBX, 8
			"jmp row3_mq_decode_p1;"//					JMP row3_mq_decode
		"row3_2nd_mag_p1:;"
			"add $16, %%ebx;"//					ADD EBX, 16
		"row3_mq_decode_p1:;"
			"movl (%%ebx), %%ecx;"//					MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"//					SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"//					AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"//					ADD EDX, ECX		// correct D by adding back _sign
			"jge row3_mq_decode_done_p1;"//					JGE	row3_mq_decode_done
			"call pass1_mq_decode;"//					CALL pass1_mq_decode
		"row3_mq_decode_done_p1:;"
			"popl %%ebx;"//					POP EBX		// val
			"testl %%ecx, %%ecx;"//					TEST ECX, ECX
			"jnz row3_deq_unchanged_p1;"//					JNZ row3_deq_unchanged
			"xorl 36(%%esi), %%ebx;"//					XOR EBX, lsb
		"row3_deq_unchanged_p1:;"
			"orl 40(%%esi), %%ebx;"//					OR EBX, half_lsb
			"movl 16(%%esi), %%ecx;"//					MOV ECX, sp_offset3
			"addl (%%esi), %%ecx;"
			"movl %%ebx,(%%ecx);"//					MOV [ESI + ECX], EBX
			//
		"next_column_p1:;"
			"addl $4, (%%esi);"//			ADD ESI, 4
			"movl 24(%%esi), %%eax;"
			"cmpl %%eax, (%%esi);"//			CMP ESI, current_strip_end
			"jnz column_loop_p1;"//			JNZ column_loop
			//
		"next_stripe_p1:;"
			"addl $12, (%%esi);"//			ADD ESI, 12		// next strip
			"movl 20(%%esi), %%ebx;"//			MOV EBX, sp_stripe_adjust
			"addl %%ebx, 4(%%esi);"//			ADD [sp_offset0], EBX
			"addl %%ebx, 8(%%esi);"//			ADD [sp_offset1], EBX
			"addl %%ebx, 12(%%esi);"//			ADD [sp_offset2], EBX
			"addl %%ebx, 16(%%esi);"//			ADD [sp_offset3], EBX
			"popl %%eax;"//			POP EAX
			"decl %%eax;"//			DEC EAX
			"jnz stripe_loop_p1;"//			JNZ stribe_loop
			"jmp proc_return_p1;"//JMP proc_return


		"pass1_mq_decode:;"
			"movl %1, %%eax;" //	_asm MOV EAX, coder_A
			"movl %2, %%edi;" //	__asm MOV EDI, coder_C
			"addl %%edx, %%eax;" //	__asm ADD EAX, EDX
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"movl (%%ebx), %%edx;" //	__asm MOV EDX, [EBX]
			"jl LSI_selected_p1;" //	__asm JL	LSI_selected_p1
			"andl $0xffff00, %%edx;" //	__asm AND EDX, 0xffff00
			"cmpl %%edx, %%eax;" //	__asm CMP EAX, EDX
			"jge USIM_p1;" //	__asm JGE USIM
			"xorl $1, %%ecx;" //	__asm XOR ECX, 1
			"movl 4(%%ebx), %%edx;" //	__asm MOV EDX, [EBX + 4]
			"movq 8(%%edx), %%mm0;" //	__asm MOVQ MM0, [EDX + 8]
			"movq %%mm0,(%%ebx);" //__asm MOVQ [EBX], MM0
			//////////////////////////////////////////////////////////////////////////
			"movl %3, %%edx;" //	__asm MOV EDX,coder_t
		"renorm_loop_0_p1:;" //	__asm renorm_loop_##x:
			"testl %%edx, %%edx;" //	__asm TEST EDX,EDX
			"jnz skip_fill_lsbs_0_p1;" //	__asm JNZ skip_fill_lsbs_##x
			"pushl %%ebx;" //	__asm PUSH EBX
			"movl %5, %%edx;" //	__asm MOV EDX,coder_T
			"cmpl $0xff, %%edx;" //	__asm CMP EDX,0xFF
			"jnz no_ff_0_p1;" //	__asm JNZ no_ff_##x
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;"
			"cmpl $0x8f, %%edx;" //	__asm CMP EDX,0x8F
			"jle T_le8F_0_p1;" //	__asm JLE T_le8F_##x
			"movl $0xff, %%edx;" //	__asm MOV EDX,0xff
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"incl %4;" //	__asm INC coder_S
			"movl $8, %%edx;" //	__asm MOV EDX,8
			"jmp fill_lsbs_done_0_p1;" //	__asm JMP fill_lsbs_done_##x
		"T_le8F_0_p1:;" //	__asm T_le8F_##x:
			"addl %%edx, %%edx;" //	__asm ADD EDX,EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"incl %%ebx;" // __asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $7, %%edx;" //	__asm MOV EDX,7
			"jmp fill_lsbs_done_0_p1;" //	__asm JMP fill_lsbs_done_##x
		"no_ff_0_p1:;" //	__asm no_ff_##x:
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //__asm ADD EDI, EDX
			"incl %%ebx;" //	__asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $8, %%edx;" //	__asm MOV EDX, 8
		"fill_lsbs_done_0_p1:;" //__asm fill_lsbs_done_##x:
			"popl %%ebx;" //	__asm POP EBX
		"skip_fill_lsbs_0_p1:;" //__asm skip_fill_lsbs_##x:
			"addl %%eax, %%eax;" //	__asm ADD EAX,EAX
			"addl %%edi, %%edi;" //	__asm ADD EDI,EDI
			"decl %%edx;" //__asm DEC EDX
			"testl $0x00800000, %%eax;" //__asm TEST EAX,0x00800000
			"jz renorm_loop_0_p1;" //	__asm JZ renorm_loop_##x
			"movl %%edx,%3;" //__asm MOV coder_t,EDX
			//////////////////////////////////////////////////////////////////////////

			"jmp state_transition_done_p1;" //__asm JMP state_transition_done
		"USIM_p1:;" //__asm USIM:
			"movl 4(%%ebx), %%edx;" //__asm MOV EDX, dword ptr[EBX+4]
			"movq (%%edx), %%mm0;" //__asm MOVQ MM0, [EDX]
			"movq %%mm0, (%%ebx);" //__asm MOVQ [EBX], MM0

			//////////////////////////////////////////////////////////////////////////
			//		__asm BMI_RENORM_ASM(1)
			"movl %3, %%edx;" //	__asm MOV EDX,coder_t
		"renorm_loop_1_p1:;" //	__asm renorm_loop_##x:
			"testl %%edx, %%edx;" //	__asm TEST EDX,EDX
			"jnz skip_fill_lsbs_1_p1;" //	__asm JNZ skip_fill_lsbs_##x
			"pushl %%ebx;" //	__asm PUSH EBX
			"movl %5, %%edx;" //	__asm MOV EDX,coder_T
			"cmpl $0xff, %%edx;" //	__asm CMP EDX,0xFF
			"jnz no_ff_1_p1;" //	__asm JNZ no_ff_##x
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"cmpl $0x8f, %%edx;" //	__asm CMP EDX,0x8F
			"jle T_le8F_1_p1;" //	__asm JLE T_le8F_##x
			"movl $0xff, %%edx;" //	__asm MOV EDX,0xff
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"incl %4;" //	__asm INC coder_S
			"movl $8, %%edx;" //	__asm MOV EDX,8
			"jmp fill_lsbs_done_1_p1;" //	__asm JMP fill_lsbs_done_##x
		"T_le8F_1_p1:;" //	__asm T_le8F_##x:
			"addl %%edx, %%edx;" //	__asm ADD EDX,EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"incl %%ebx;" // __asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $7, %%edx;" //	__asm MOV EDX,7
			"jmp fill_lsbs_done_1_p1;" //	__asm JMP fill_lsbs_done_##x
		"no_ff_1_p1:;" //	__asm no_ff_##x:
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //__asm ADD EDI, EDX
			"incl %%ebx;" //	__asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $8, %%edx;" //	__asm MOV EDX, 8
		"fill_lsbs_done_1_p1:;" //__asm fill_lsbs_done_##x:
			"popl %%ebx;" //	__asm POP EBX
		"skip_fill_lsbs_1_p1:;" //__asm skip_fill_lsbs_##x:
			"addl %%eax, %%eax;" //	__asm ADD EAX,EAX
			"addl %%edi, %%edi;" //	__asm ADD EDI,EDI
			"decl %%edx;" //__asm DEC EDX
			"testl $0x00800000, %%eax;" //__asm TEST EAX,0x00800000
			"jz renorm_loop_1_p1;" //	__asm JZ renorm_loop_##x
			"movl %%edx,%3;" //__asm MOV coder_t,EDX
			//////////////////////////////////////////////////////////////////////////

			"jmp state_transition_done_p1;" //__asm JMP state_transition_done
		"LSI_selected_p1:;" //_asm LSI_selected :
			"andl $0xffff00, %%edx;" //__asm AND EDX, 0xffff00
			"addl %%edx, %%edi;" //__asm ADD EDI, EDX
			"cmpl %%edx, %%eax;" //__asm CMP EAX, EDX
			"movl %%edx, %%eax;" //	__asm MOV EAX, EDX
			"jge LSIL_p1;" //__asm JGE LSIL
			"movl 4(%%ebx), %%edx;" //__asm MOV EDX, dword ptr[EBX+4]
			"movq (%%edx), %%mm0;" //	__asm MOVQ MM0, [EDX]
			"movq %%mm0,(%%ebx);" //	__asm MOVQ [EBX], MM0

			////////////////////////////////////////////////////////////////////////
			//		__asm BMI_RENORM_ASM(2)
			"movl %3, %%edx;" //	__asm MOV EDX,coder_t
		"renorm_loop_2_p1:;" //	__asm renorm_loop_##x:
			"testl %%edx, %%edx;" //	__asm TEST EDX,EDX
			"jnz skip_fill_lsbs_2_p1;" //	__asm JNZ skip_fill_lsbs_##x
			"pushl %%ebx;" //	__asm PUSH EBX
			"movl %5, %%edx;" //	__asm MOV EDX,coder_T
			"cmpl $0xff, %%edx;" //	__asm CMP EDX,0xFF
			"jnz no_ff_2_p1;" //	__asm JNZ no_ff_##x
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"cmpl $0x8f, %%edx;" //	__asm CMP EDX,0x8F
			"jle T_le8F_2_p1;" //	__asm JLE T_le8F_##x
			"movl $0xff, %%edx;" //	__asm MOV EDX,0xff
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"incl %4;" //	__asm INC coder_S
			"movl $8, %%edx;" //	__asm MOV EDX,8
			"jmp fill_lsbs_done_2_p1;" //	__asm JMP fill_lsbs_done_##x
		"T_le8F_2_p1:;" //	__asm T_le8F_##x:
			"addl %%edx, %%edx;" //	__asm ADD EDX,EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"incl %%ebx;" // __asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $7, %%edx;" //	__asm MOV EDX,7
			"jmp fill_lsbs_done_2_p1;" //	__asm JMP fill_lsbs_done_##x
		"no_ff_2_p1:;" //	__asm no_ff_##x:
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //__asm ADD EDI, EDX
			"incl %%ebx;" //	__asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $8, %%edx;" //	__asm MOV EDX, 8
		"fill_lsbs_done_2_p1:;" //__asm fill_lsbs_done_##x:
			"popl %%ebx;" //	__asm POP EBX
		"skip_fill_lsbs_2_p1:;" //__asm skip_fill_lsbs_##x:
			"addl %%eax, %%eax;" //	__asm ADD EAX,EAX
			"addl %%edi, %%edi;" //	__asm ADD EDI,EDI
			"decl %%edx;" //__asm DEC EDX
			"testl $0x00800000, %%eax;" //__asm TEST EAX,0x00800000
			"jz renorm_loop_2_p1;" //	__asm JZ renorm_loop_##x
			"movl %%edx,%3;" //__asm MOV coder_t,EDX
			//////////////////////////////////////////////////////////////////////////

			"jmp state_transition_done_p1;" //__asm JMP state_transition_done
		"LSIL_p1:;" //__asm LSIL:
			"xorl $1, %%ecx;" //	__asm XOR ECX, 1
			"movl 4(%%ebx), %%edx;" //__asm MOV EDX, [EBX + 4]
			"movq 8(%%edx), %%mm0;" //	__asm MOVQ MM0, [EDX + 8]
			"movq %%mm0, (%%ebx);" //	__asm MOVQ [EBX], MM0

			//////////////////////////////////////////////////////////////////////////
			//		__asm BMI_RENORM_ASM(3)
			"movl %3, %%edx;" //	__asm MOV EDX,coder_t
		"renorm_loop_3_p1:;" //	__asm renorm_loop_##x:
			"testl %%edx, %%edx;" //	__asm TEST EDX,EDX
			"jnz skip_fill_lsbs_3_p1;" //	__asm JNZ skip_fill_lsbs_##x
			"pushl %%ebx;" //	__asm PUSH EBX
			"movl %5, %%edx;" //	__asm MOV EDX,coder_T
			"cmpl $0xff, %%edx;" //	__asm CMP EDX,0xFF
			"jnz no_ff_3_p1;" //	__asm JNZ no_ff_##x
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"cmpl $0x8f, %%edx;" //	__asm CMP EDX,0x8F
			"jle T_le8F_3_p1;" //	__asm JLE T_le8F_##x
			"movl $0xff, %%edx;" //	__asm MOV EDX,0xff
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"incl %4;" //	__asm INC coder_S
			"movl $8, %%edx;" //	__asm MOV EDX,8
			"jmp fill_lsbs_done_3_p1;" //	__asm JMP fill_lsbs_done_##x
		"T_le8F_3_p1:;" //	__asm T_le8F_##x:
			"addl %%edx, %%edx;" //	__asm ADD EDX,EDX
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //	__asm ADD EDI, EDX
			"incl %%ebx;" // __asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $7, %%edx;" //	__asm MOV EDX,7
			"jmp fill_lsbs_done_3_p1;" //	__asm JMP fill_lsbs_done_##x
		"no_ff_3_p1:;" //	__asm no_ff_##x:
			"movl %6, %%ebx;" //	__asm MOV EBX,coder_buf_next
			"movzx (%%ebx), %%edx;" //	__asm MOVZX EDX,byte ptr [EBX]
			"movl %%edx, %5;" //	__asm MOV coder_T,EDX
			"addl %%edx, %%edi;" //__asm ADD EDI, EDX
			"incl %%ebx;" //	__asm INC EBX
			"movl %%ebx, %6;" //	__asm MOV coder_buf_next, EBX
			"movl $8, %%edx;" //	__asm MOV EDX, 8
		"fill_lsbs_done_3_p1:;" //__asm fill_lsbs_done_##x:
			"popl %%ebx;" //	__asm POP EBX
		"skip_fill_lsbs_3_p1:;" //__asm skip_fill_lsbs_##x:
			"addl %%eax, %%eax;" //	__asm ADD EAX,EAX
			"addl %%edi, %%edi;" //	__asm ADD EDI,EDI
			"decl %%edx;" //__asm DEC EDX
			"testl $0x00800000, %%eax;" //__asm TEST EAX,0x00800000
			"jz renorm_loop_3_p1;" //	__asm JZ renorm_loop_##x
			"movl %%edx,%3;" //__asm MOV coder_t,EDX
			//////////////////////////////////////////////////////////////////////////
		"state_transition_done_p1:;" // __asm state_transition_done:
			"movl %%eax, %%edx;" //__asm MOV EDX,EAX
			"subl $0x00800000, %%edx;" //__asm SUB EDX,0x00800000
			"cmpl %%edx, %%edi;" //	__asm CMP EDI,EDX
			"cmovl %%edi, %%edx;" //__asm CMOVL EDX,EDI
			"subl %%edx, %%eax;" //__asm SUB EAX,EDX
			"subl %%edx, %%edi;" //	__asm SUB EDI,EDX
			"movl %%edi, %2;" //__asm MOV coder_C,EDI
			"movl %%eax, %1;" //__asm MOV coder_A,EAX
			"ret;"

		"proc_return_p1:;" //	proc_return:
			// 		"movl %%edx, 12(%%esi);"	//		MOV coder_D, EDX
			"emms;" //				EMMS
			"popl %%ebx;"

			// output
			: "=d"(coder_D), // %0
			"=g"(coder_A), // %1
			"=g"(coder_C), // %2
			"=g"(coder_t), // %3
			"=g"(coder_S), // %4
			"=g"(coder_T), // %5
			"=g"(coder_buf_next) // %6
			// input
			: "g"(num_stripes), // %7
			"g"(paras), // %8
			"0"(coder_D),
			"1"(coder_A),
			"2"(coder_C),
			"3"(coder_t),
			"4"(coder_S),
			"5"(coder_T),
			"6"(coder_buf_next)
			: "eax", "ecx", "esi", "edi"
	);
	CODER_CHECK_IN_ASM(coder)
}

void bmi_t1_decode_pass2_asmATT(bmi_mq_decoder &coder, mq_env_state states[],
						 int bit_plane,unsigned char * lutContext,
						 int_32 *samples, int_32 *contextWord,
						 int width, int num_stripes)
{


	typedef	struct s_att_pass2_stru
	{
		int *contextWord;	//	0
		int sp_offset0;	// 4
		int sp_offset1;	// 8
		int sp_offset2;	// 12
		int sp_offset3;	// 16
		int sp_stripe_adjust;	// 20
		int * current_strip_end;				// 24

		unsigned char * lutContext;				// 28
		int		lut_ctx_sign;		// 32
		mq_env_state * states;					// 36
		int width;								// 40
		int one_and_half;						// 44
		mq_env_state * state_run_start;			// 48

	}	att_pass2;


	att_pass2	pass2_paras;
	att_pass2 * paras = &pass2_paras;

	pass2_paras.contextWord = contextWord;
	pass2_paras.one_and_half = (1<<bit_plane) + ((1<<bit_plane)>>1); 


	width <<= 2;
	pass2_paras.width = width;
	pass2_paras.sp_offset0 = (samples - contextWord) << 2;
	pass2_paras.sp_offset1 = pass2_paras.sp_offset0 + width;
	pass2_paras.sp_offset2 = pass2_paras.sp_offset1 + width;
	pass2_paras.sp_offset3 = pass2_paras.sp_offset2 + width;

	pass2_paras.sp_stripe_adjust = width*3 - (DECODE_CWROD_ALIGN<<2);

	pass2_paras.lutContext = lutContext;
	pass2_paras.lut_ctx_sign = (int)&lut_ctx_sign;
	pass2_paras.states = states;

	pass2_paras.state_run_start = &states[STATE_RUN_START];
	CODER_CHECK_OUT_ASM(coder)



	__asm__ volatile (
			"pushl %%ebx;"
			//	MOV EDX, coder_D
			"movl %7, %%eax;"			//	MOV EAX, num_stripes
			"movl %8, %%esi;"			// MOV ESI, contexts
"stribe_loop2:;"
			"pushl	%%eax;"				//	PUSH EAX
			"movl 40(%%esi), %%edi;"	//	MOV EDI, width	
			"addl (%%esi), %%edi;"		//	ADD EDI, ESI		// ESI -> EDI : range of current stripe
			"movl %%edi, 24(%%esi);"	//	MOV current_strip_end, EDI
"column_loop2:;"
			"movl (%%esi), %%eax;"		//	MOV EAX, [ESI]		// cword = *(contextWord) 
			"movl (%%eax), %%eax;"
			"testl %%eax, %%eax;"		//	TEST EAX, EAX
			"jnz row0_2;"					//	JNZ row0
			"movl 48(%%esi), %%ebx;"	//	MOV EBX, state_run_start
			"movl (%%ebx), %%ecx;"		//	MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"		//	SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"			//	AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"		//	ADD EDX, ECX		// correct D by adding back _sign
			"jge mq_decode_done;"		//	JGE	mq_decode_done
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"mq_decode_done:;"
			"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
			"jz next_column_2;"			//	JZ next_column
			//////////////////////////////////////////////////////////////////////////
			// 				 pass2_mq_cdb_decode_2_bits
			"xorl %%ecx, %%ecx;"		//	XOR ECX,ECX
			"movl %2, %%ebx;"			//	MOV EBX,coder_C
			"movl %1, %%eax;"			//	MOV EAX, coder_A
			"subl $0x00560100, %%edx;"	//	SUB EDX,0x00560100 
			"jge decode_bit_0;"			//	JGE decode_bit_0 
			/* Non-CDP decoding for the first bit */ 
			"addl %%edx, %%eax;"		//	ADD EAX,EDX		// A+= D
			"addl %%edx, %%ebx;"		//	ADD EBX,EDX		// C += D
			"jl	bit_1_LSI;"				//	JL bit_1_LSI
			/* Upper sub-interval selected */
			"cmpl $0x00560100, %%eax;"	// 	CMP EAX,0x00560100 
			"jge bit_1_renorm;"			//	JGE bit_1_renorm
			"movl $2, %%ecx;"			// 	MOV ECX,2
			"jmp bit_1_renorm;"			//	JMP bit_1_renorm
"bit_1_LSI:;"
			/* Lower sub-interval selected */
			"addl $0x00560100, %%ebx;"	//	ADD EBX,0x00560100		//  C+= 0x560100
			"cmpl $0x00560100, %%eax;"	//	CMP EAX,0x00560100 
			"movl $0x00560100, %%eax;"	//	MOV EAX,0x00560100 
			"jl bit_1_renorm;"			//	JL bit_1_renorm 
			"movl $2, %%ecx;"			//	MOV ECX,2
"bit_1_renorm: ;"
			"movl %3, %%edi;"			//	MOV EDI,coder_t
"bit_1_renorm_once:;"
			/* Come back here repeatedly until we have shifted enough bits out. */ 
			"testl %%edi, %%edi;"		//	TEST EDI,EDI 
			"jnz skip_bit_1_fill_lsbs;"	//	JNZ skip_bit_1_fill_lsbs
			/* Begin `fill_lsbs' procedure */
			"movl %5, %%edx;"			//	MOV EDX,coder_T 
			"cmpl $0xff, %%edx;"			//	CMP EDX,0xFF 
			"jnz bit_1_ne_ff;"			//	JNZ bit_1_ne_ff 
			"movl %6, %%edi;"			//	MOV EDI,coder_buf_next
			"movzx (%%edi), %%edx;"		//	MOVZX EDX,byte ptr [EDI] 
			"cmpl $0x8f, %%edx;"		//	CMP EDX,0x8F 
			"jle bit_1_T_le8F;"			//	JLE bit_1_T_le8F 
			/* Reached a termination marker.Don't update `store_var' */
			"movl $0xff, %%edx;"		//	MOV EDX,0xFF  // coder_T
			"incl %4;"					//	INC coder_S
			"movl $8, %%edi;"			//	MOV EDI,8 /* New value of t */ 
			"jmp bit_1_fill_lsbs_done;"	//	JMP bit_1_fill_lsbs_done
"bit_1_T_le8F:;"
			/* No termination marker, but bit stuff required */
			"addl %%edx, %%edx;"		//	ADD EDX,EDX
			"incl %%edi;"				//	INC EDI
			"movl %%edi, %6;"			//	MOV coder_buf_next, EDI
			"movl $7, %%edi;"			//	MOV EDI,7 /* New value of t. */
			"jmp bit_1_fill_lsbs_done;"	//	JMP bit_1_fill_lsbs_done
"bit_1_ne_ff:;"
			"movl %6, %%edi;"			//	MOV EDI,coder_buf_next
			"movzx (%%edi), %%edx;"		//	MOVZX EDX,byte ptr [EDI] 
			"incl %%edi;"				//	INC EDI
			"movl %%edi, %6;"			//	MOV coder_buf_next, EDI
			"movl $8, %%edi;"			//	MOV EDI, 8
"bit_1_fill_lsbs_done:;"
			"addl %%edx, %%ebx;"		//	ADD EBX,EDX 
			"movl %%edx, %5;"			//	MOV coder_T,EDX /* Save `temp' */ 
"skip_bit_1_fill_lsbs:;"
			"addl %%eax, %%eax;"		//	ADD EAX,EAX 
			"addl %%ebx, %%ebx;"		//	ADD EBX,EBX 
			"decl %%edi;"				//	DEC EDI
			"testl $0x00800000, %%eax;"	//	TEST EAX,0x00800000 
			"jz bit_1_renorm_once;"		//	JZ bit_1_renorm_once
			"movl %%edi, %3;"			//	MOV coder_t,EDI /* Save t */ 

			/* Renormalization is complete.Recompute D and save A and C. */
			"movl %%eax, %%edx;"		//	MOV EDX,EAX 
			"subl $0x00800000, %%edx;"	//	SUB EDX,0x00800000 /* Subtract MQCODER_A_MIN */
			"cmpl %%edx, %%ebx;"		//	CMP EBX,EDX
			"cmovll	%%ebx, %%edx;"		//	CMOVL EDX,EBX /* Move C to EDX if C < A-MQCODER_A_MIN */ 
			"subl %%edx, %%eax;"		//	SUB EAX,EDX /* Subtract D from A */
			"subl %%edx, %%ebx;"		//	SUB EBX,EDX /* Subtract D from C */
			/************************************************************************/ 
"decode_bit_0:;"
			"subl $0x00560100, %%edx;"	//	SUB EDX,0x00560100 
			"jge bit_1_finished;"		//	JGE bit_1_finished 
			"addl %%edx, %%eax;"		//	ADD EAX,EDX // A += D
			"addl %%edx, %%ebx;"		//	ADD EBX,EDX // B += D
			"jl bit_0_LSI;"				//	JL bit_0_LSI 
			/* Upper sub-interval selected */
			"cmpl $0x00560100, %%eax;"	//	CMP EAX,0x00560100 
			"jge bit_0_renorm;"			//	JGE bit_0_renorm
			"incl %%ecx;"				//	INC ECX
			"jmp bit_0_renorm;"			//	JMP bit_0_renorm
"bit_0_LSI:;"
			/* Lower sub-interval selected */
			"addl $0x00560100, %%ebx;"	//	ADD EBX,0x00560100 
			"cmpl $0x00560100, %%eax;"	//	CMP EAX,0x00560100 
			"movl $0x00560100, %%eax;"	//	MOV EAX,0x00560100 
			"jl bit_0_renorm;"			//	JL bit_0_renorm 
			"incl %%ecx;"				//	INC ECX
"bit_0_renorm:"
			"movl %3, %%edi;"			//	MOV EDI,coder_t
"bit_0_renorm_once:"
			"testl %%edi, %%edi;"		//	TEST EDI,EDI 
			"jnz skip_bit_0_fill_lsbs;"	//	JNZ skip_bit_0_fill_lsbs
			"movl %5, %%edx;"			//	MOV EDX,coder_T 
			"cmpl $0xff, %%edx;"		//	CMP EDX,0xFF 
			"jnz run_bit0_no_ff;"		//	JNZ run_bit0_no_ff 
			"movl %6, %%edi;"			//	MOV EDI,coder_buf_next
			"movzx (%%edi), %%edx;"		//	MOVZX EDX,byte ptr [EDI] 
			"cmpl $0x8f, %%edx;"		//	CMP EDX,0x8F 
			"jle bit_0_T_le8F;"			//	JLE bit_0_T_le8F 
			"movl $0xff, %%edx;"		//	MOV EDX,0xFF 
			"movl %4, %%edi;"			//	MOV EDI,coder_S
			"incl %%edi;"				//	INC EDI
			"movl %%edi, %4;"			//	MOV coder_S,EDI
			"movl $8, %%edi;"			//	MOV EDI,8 /* New value of t */ 
			"jmp bit_0_fill_lsbs_done;"	//	JMP bit_0_fill_lsbs_done
"bit_0_T_le8F:;"
			"addl %%edx, %%edx;"		//	ADD EDX,EDX
			"incl %%edi;"				//	INC EDI
			"movl %%edi, %6;"			//	MOV coder_buf_next,EDI 
			"movl $7, %%edi;"			//	MOV EDI,7 /* New value of t. */
			"jmp bit_0_fill_lsbs_done;"	//	JMP bit_0_fill_lsbs_done
"run_bit0_no_ff:;"
			"movl %6, %%edi;"			//	MOV EDI,coder_buf_next
			"movzx (%%edi), %%edx;"		//	MOVZX EDX,byte ptr [EDI]
			"incl %%edi;"				//	INC EDI
			"movl %%edi, %6;"			//	MOV coder_buf_next,EDI 
			"movl $8, %%edi;"			//	MOV EDI,8 
"bit_0_fill_lsbs_done:;"
			"addl %%edx, %%ebx;"		//	ADD EBX,EDX
			"movl %%edx, %5;"			//	MOV coder_T,EDX
"skip_bit_0_fill_lsbs:;"
			"addl %%eax, %%eax;"		//	ADD EAX,EAX
			"addl %%ebx, %%ebx;"		//	ADD EBX,EBX
			"decl %%edi;"				//	DEC EDI
			"testl $0x00800000, %%eax;"	//	TEST EAX,0x00800000
			"jz bit_0_renorm_once;"		//	JZ bit_0_renorm_once
			"movl %%edi, %3;"			//	MOV coder_t,EDI
			"movl %%eax, %%edx;"		//	MOV EDX,EAX 
			"subl $0x00800000, %%edx;"	//	SUB EDX,0x00800000
			"cmpl %%edx, %%ebx;"		//	CMP EBX,EDX
			"cmovll %%ebx, %%edx;"		//	CMOVL EDX,EBX 
			"subl %%edx, %%eax;"		//	SUB EAX,EDX
			"subl %%edx, %%ebx;"		//	SUB EBX,EDX
"bit_1_finished:;"
			"movl %%ebx, %2;"			//	MOV coder_C,EBX
			"movl %%eax, %1;"			//	MOV coder_A,EAX

			//////////////////////////////////////////////////////////////////////////
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"		//	MOV EAX, [ESI]
			"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
			"jz row0_significant;"		//	JZ row0_significant
			"jp row3_significant;"		//	JP row3_significant // Parity means there are an even number of 1's (run=3)
			"testl $1, %%ecx;"			//	TEST ECX,1
			"jnz row1_significant;"		//	JNZ row1_significant
			"jmp row2_significant;"		//	JMP row2_significant
"row0_2:"
			"testl $0x280010, %%eax;"	//	TEST EAX, 0x280010
			"jnz row1_2;"				//	JNZ row1
			"andl $0x1ef, %%eax;"		//	AND EAX, 0x1ef
			"movl 28(%%esi), %%ebx;"	//	MOV EBX, lutContext
			"movzx (%%ebx, %%eax), %%ebx;"	//	MOVZX EBX, byte ptr[EBX + EAX]
			"movl 36(%%esi), %%ecx;"	//	MOV ECX, states
			"leal (%%ecx, %%ebx, 8), %%ebx;"	//	LEA EBX, [ECX + 8 * EBX]
			"movl (%%ebx), %%ecx;"		//	MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"		//	SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"			//	AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"		//	ADD EDX, ECX		// correct D by adding back _sign
			"jge row0_decode_in_cdp_2;"	//	JGE	row0_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row0_decode_in_cdp_2:;"
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"		//	MOV EAX, [ESI]
			"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
			"jz row1_2;"					//	JZ row1
"row0_significant:;"
			"sarl $1, %%eax;"			//	SAR EAX, 1
			"movl %%eax, %%ecx;"		//	MOV ECX, EAX
			"andl $0x200041, %%eax;"	//	AND EAX, 0x200041//0x400082	//sym = cword & SIG_PRO_CHECKING_MASK1;		sym |= cword & (SIGMA_SELF>>3);
			"andl $0x20000, %%ecx;"		//	AND ECX, 0x20000//0x40000
			"sarl $2, %%ecx;"			//	SAR ECX, 2
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (cword & CONTEXT_CHI_PREV)>>2;

			"movl (%%esi), %%ebx;"		
			"movl -4(%%ebx), %%ecx;"	//	MOV ECX, dword ptr[ESI-4]		// previous context
			"andl $0x80010, %%ecx;"		//	AND ECX, 0x80010
			"sarl $2, %%ecx; "			//	SAR ECX, 2
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1
			"movl 4(%%ebx), %%ecx;"		//	MOV ECX, [ESI+4]		// next context 
			"andl $0x80010, %%ecx;"		//	AND ECX, 0x80010
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1
			"movl %%eax, %%ecx;"		//	MOV ECX, EAX
			"sarl $14, %%ecx;"			//	SAR ECX, 14
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
			"andl $0xff, %%eax;"		//	AND EAX, 0xff

			"addl 32(%%esi), %%eax;"
			"movzx (%%eax), %%ecx;"		//	MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
			"movl %%ecx, %%ebx;"		//	MOV EBX, ECX							// val
			"andl $1, %%ecx;"			//	AND ECX, 1								// val & 1
			"sarl $1, %%ebx;"			//	SAR EBX, 1
			"movl 36(%%esi), %%eax;"	//	MOV EAX, states
			"leal 80(%%eax, %%ebx, 8), %%ebx;"	//	LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state
			"movl (%%ebx), %%eax;"		// MOV EAX, [EBX]		// _p_sign
			"subl %%eax, %%edx;"		//	SUB EDX, EAX		// D -= _p_sign
			"andl $1, %%eax;"			//	AND EAX, 1			// _sign	
			"xorl %%eax, %%ecx;"		//	XOR ECX, EAX
			"addl %%eax, %%edx;"		//	ADD EDX, EAX		// correct D by adding back _sign
			"jge row0_sig_decode_in_cdp;"	//	JGE	row0_sig_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row0_sig_decode_in_cdp:;"
			"movl (%%esi), %%ebx;"		//	MOV EBX, ESI
			"movl (%%ebx), %%eax;"		//	MOV EAX, [ESI]		// restore cword

			"orl $0x20,	-4(%%ebx);"		//	OR dword ptr[ESI - 4], 0x20					// contextWord[-1] |= (SIGMA_CR<<0);
			"orl $0x08, 4(%%ebx);"		//	OR dword ptr[ESI + 4], 0x08					// contextWord[ 1] |= (SIGMA_CL<<0);
			"subl 40(%%esi), %%ebx;"	//	SUB EBX, width
			"orl $0x20000, -16(%%ebx);"	//	OR dword ptr[EBX - 16], 0x20000		// contextWord[-width - DECODE_CWROD_ALIGN-1] |=	(SIGMA_BR << 9);
			"orl $0x8000, -8(%%ebx);"	//	OR dword ptr[EBX - 8], 0x8000		// contextWord[-width - DECODE_CWROD_ALIGN+1] |=	(SIGMA_BL << 9);
			"shll $19, %%ecx;"			//	SHL  ECX, 19

			"orl %%ecx, %%eax;"			//	OR EAX, ECX
			"orl $0x10, %%eax;"			//	OR EAX, 0x10		// cword |= (SIGMA_SELF) | (sym << CONTEXT_FIRST_CHI_POS);

			"shll $12, %%ecx;"			//	SHL ECX, 12			// sym << 31
			"orl $0x10000, -12(%%ebx);"	//	OR dword ptr[EBX - 12], 0x10000		
			"orl %%ecx, -12(%%ebx);"	//	OR dword ptr[EBX - 12], ECX		// contextWord[-width - DECODE_CWROD_ALIGN  ] |=	(SIGMA_BC << 9) | (sym << CONTEXT_NEXT_CHI_POS ); 

			"movl (%%esi), %%ebx;"
			"movl %%eax, (%%ebx);"		//	MOV [ESI], EAX
			"addl 44(%%esi), %%ecx;"	//	ADD ECX, one_and_half
			"addl 4(%%esi), %%ebx;"		//	MOV EBX, sp_offset0
			"movl %%ecx, (%%ebx);"		//	MOV [ESI + EBX], ECX
"row1_2:;"
			"testl $0x1400080, %%eax;"	//	TEST EAX, 0x1400080
			"jnz row2_2;"					//	JNZ row2
			"andl $0xf78, %%eax;"		//	AND EAX, 0xf78
			"sarl $3, %%eax;"			//	SAR EAX, 3

			"movl 28(%%esi), %%ebx;"	//	MOV EBX, lutContext
			"movzx (%%ebx, %%eax), %%ebx;"	//	MOVZX EBX, byte ptr[EBX + EAX]
			"movl 36(%%esi), %%ecx;"	//	MOV ECX, states
			"leal (%%ecx, %%ebx, 8), %%ebx;"	//	LEA EBX, [ECX + 8 * EBX]
			"movl (%%ebx), %%ecx;"		//	MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"		//	SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"			//	AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"		//	ADD EDX, ECX		// correct D by adding back _sign
			"jge row1_decode_in_cdp_2;"	//	JGE	row1_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row1_decode_in_cdp_2:;"
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"		//	MOV EAX, [ESI]
			"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
			"jz row2_2;"					//	JZ row2
"row1_significant:;"
			"andl $0x2080410, %%eax;"	//	AND EAX, 0x2080410		//sym = cword & (SIG_PRO_CHECKING_MASK0 | SIG_PRO_CHECKING_MASK2);
			"movl (%%esi), %%ebx;"
			"movl -4(%%ebx), %%ecx;"	//	MOV ECX, [ESI-4]
			"andl $0x400080, %%ecx;"	//	AND ECX, 0x400080
			"sarl $1, %%ecx;"			//	SAR ECX, 1
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (cword & CONTEXT_CHI_PREV)>>1;

			"movl 4(%%ebx), %%ecx;"		//	MOV ECX, [ESI+4]		// next context 
			"andl $0x400080, %%ecx;"	//	AND ECX, 0x400080
			"shll $1, %%ecx;"			//	SHL ECX, 1
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK1) << 1;

			"movl %%eax, %%ecx;"		//	MOV ECX, EAX
			"sarl $14, %%ecx;"			//	SAR ECX, 14
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
			"sarl $4, %%eax;"			//	SAR EAX, 4				// sym >>= 4
			"andl $0xff, %%eax;"		//	AND EAX, 0xff
			"addl 32(%%esi), %%eax;"
			"movzx (%%eax), %%ecx;"		//	MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
			"movl %%ecx, %%ebx;"		//	MOV EBX, ECX							// val

			"andl $1, %%ecx;"			//	AND ECX, 1								// val & 1
			"sarl $1, %%ebx;"			//	SAR EBX, 1
			"movl 36(%%esi), %%eax;"	//	MOV EAX, states
			"leal 80(%%eax, %%ebx, 8), %%ebx;"	//	LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state
			"movl (%%ebx), %%eax;"		// MOV EAX, [EBX]		// _p_sign
			"subl %%eax, %%edx;"		//	SUB EDX, EAX		// D -= _p_sign
			"andl $1, %%eax;"			//	AND EAX, 1			// _sign	
			"xorl %%eax, %%ecx;"		//	XOR ECX, EAX
			"addl %%eax, %%edx;"		//	ADD EDX, EAX		// correct D by adding back _sign
			"jge row1_sig_decode_in_cdp;"	//	JGE	row1_sig_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row1_sig_decode_in_cdp:;"
			"movl (%%esi), %%ebx;"		
			"movl (%%ebx), %%eax;"		//	MOV EAX, [ESI]		// restore cword

			"orl $0x100, -4(%%ebx);"	//	OR dword ptr[ESI - 4], 0x100					// contextWord[-1] |= (SIGMA_CR<<3);
			"orl $0x40, 4(%%ebx);"		//	OR dword ptr[ESI + 4], 0x40					// contextWord[ 1] |= (SIGMA_CL<<);

			"shll $22, %%ecx;"			//	SHL ECX, 22	
			"orl %%ecx, %%eax;"			//	OR EAX, ECX
			"orl $0x80, %%eax;"			//	OR EAX, 0x80	// cword |= (SIGMA_SELF<<3) | (sym<<(CONTEXT_FIRST_CHI_POS+3));
			"movl %%eax, (%%ebx);"		//	MOV [ESI], EAX
			"shll $9, %%ecx;"			// SHL ECX, 9			// sym << 31

			"addl 44(%%esi), %%ecx;"	//	ADD ECX, one_and_half
			"addl 8(%%esi), %%ebx;"		//	MOV EBX, sp_offset1
			"movl %%ecx, (%%ebx);"		//	MOV [ESI + EBX], ECX
"row2_2:;"
			"testl $0xa000400, %%eax;"	//	TEST EAX, 0xa000400
			"jnz row3_2;"					//	JNZ row3
			"andl $0x7bc0, %%eax;"		//	AND EAX, 0x7bc0
			"sarl $6, %%eax;"			//	SAR EAX, 6

			"movl 28(%%esi), %%ebx;"	//	MOV EBX, lutContext
			"movzx (%%ebx, %%eax), %%ebx;"	//	MOVZX EBX, byte ptr[EBX + EAX]
			"movl 36(%%esi), %%ecx;"	//	MOV ECX, states
			"leal (%%ecx, %%ebx, 8), %%ebx;"	//	LEA EBX, [ECX + 8 * EBX]
			"movl (%%ebx), %%ecx;"		//	MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"		//	SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"			//	AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"		//	ADD EDX, ECX		// correct D by adding back _sign
			"jge row2_decode_in_cdp_2;"	//	JGE	row2_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row2_decode_in_cdp_2:;"
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"		//	MOV EAX, [ESI]
			"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
			"jz row3_2;"					//	JZ row3
"row2_significant:;"
			"andl $0x10402080, %%eax;"	//	AND EAX, 0x10402080		//sym = cword &  cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
			"movl (%%esi), %%ebx;"
			"movl -4(%%ebx), %%ecx;"	//	MOV ECX, [ESI-4]
			"andl $0x2000400, %%ecx;"	//	AND ECX, 0x2000400
			"sarl $1, %%ecx;"			//	SAR ECX, 1
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK2) >> 1;

			"movl 4(%%ebx), %%ecx;"		//	MOV ECX, [ESI+4]		// next context 
			"andl $0x2000400, %%ecx;"	//	AND ECX, 0x2000400
			"shll $1, %%ecx;"			//	SHL ECX, 1
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK2) << 1;

			"movl %%eax, %%ecx;"		//	MOV ECX, EAX
			"sarl $14, %%ecx;"			//	SAR ECX, 14
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
			"sarl $7, %%eax;"			//	SAR EAX, 7				// sym >>= 7
			"andl $0xff, %%eax;"		//	AND EAX, 0xff

			"addl 32(%%esi), %%eax;"
			"movzx (%%eax), %%ecx;"		//	MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
			"movl %%ecx, %%ebx;"		//	MOV EBX, ECX							// val
			"andl $1, %%ecx;"			//	AND ECX, 1								// val & 1
			"sarl $1, %%ebx;"			//	SAR EBX, 1
			"movl 36(%%esi), %%eax;"	//	MOV EAX, states
			"leal 80(%%eax, %%ebx, 8), %%ebx;"	//	LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state	//	state_ref = states  + STATE_SIGN_START + (val>>1);

			"movl (%%ebx), %%eax;"		// MOV EAX, [EBX]		// _p_sign
			"subl %%eax, %%edx;"		//	SUB EDX, EAX		// D -= _p_sign
			"andl $1, %%eax;"			//	AND EAX, 1			// _sign	
			"xorl %%eax, %%ecx;"		//	XOR ECX, EAX
			"addl %%eax, %%edx;"		//	ADD EDX, EAX		// correct D by adding back _sign
			"jge row2_sig_decode_in_cdp;"		//	JGE	row2_sig_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row2_sig_decode_in_cdp:;"
			"movl (%%esi), %%ebx;"		
			"movl (%%ebx), %%eax;"		//	MOV EAX, [ESI]		// restore cword
			"orl $0x800, -4(%%ebx);"	//	OR dword ptr[ESI - 4], 0x800				// contextWord[-1] |= (SIGMA_CR<<3);
			"orl $0x200, 4(%%ebx);"		//	OR dword ptr[ESI + 4], 0x200					// contextWord[ 1] |= (SIGMA_CL<<);

			"shll $25, %%ecx;"			//	SHL ECX, 25	
			"orl %%ecx, %%eax;"			//	OR EAX, ECX
			"orl $0x400, %%eax;"			//	OR EAX,0x400		// cword |= (SIGMA_SELF<<6) | (sym<<(CONTEXT_FIRST_CHI_POS+6));
			"movl %%eax, (%%ebx);"		//	MOV [ESI], EAX
			"shll $6, %%ecx;"			// SHL ECX, 6			// sym << 31

			"addl 44(%%esi), %%ecx;"	//	ADD ECX, one_and_half
			"addl 12(%%esi), %%ebx;"		//	MOV EBX, sp_offset2
			"movl %%ecx, (%%ebx);"		//	MOV [ESI + EBX], ECX

"row3_2:;"
			"testl $0x50002000, %%eax;"	//	TEST EAX, 0x50002000
			"jnz update_context;"		//	JNZ update_context
			"andl $0x3de00, %%eax;"		//	AND EAX, 0x3de00
			"sarl $9, %%eax;"			//	SAR EAX, 9

			"movl 28(%%esi), %%ebx;"	//	MOV EBX, lutContext
			"movzx (%%ebx, %%eax), %%ebx;"	//	MOVZX EBX, byte ptr[EBX + EAX]
			"movl 36(%%esi), %%ecx;"	//	MOV ECX, states
			"leal (%%ecx, %%ebx, 8), %%ebx;"	//	LEA EBX, [ECX + 8 * EBX]
			"movl (%%ebx), %%ecx;"		//	MOV ECX, [EBX]		// _p_sign
			"subl %%ecx, %%edx;"		//	SUB EDX, ECX		// D -= _p_sign
			"andl $1, %%ecx;"			//	AND ECX, 1			// _sign
			"addl %%ecx, %%edx;"		//	ADD EDX, ECX		// correct D by adding back _sign
			"jge row3_decode_in_cdp_2;"	//	JGE	row3_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row3_decode_in_cdp_2:;"
			"movl (%%esi), %%eax;"
			"movl (%%eax), %%eax;"		//	MOV EAX, [ESI]
			"testl %%ecx, %%ecx;"		//	TEST ECX, ECX
			"jz update_context;"		//	JZ update_context
"row3_significant:;"
			"andl $0x82010400, %%eax;"	//	AND EAX, 0x82010400			//sym = cword & (SIG_PRO_CHECKING_MASK2 | SIG_PRO_CHECKING_MASK_NEXT);

			"movl (%%esi), %%ebx;"
			"movl -4(%%ebx), %%ecx;"	//	MOV ECX, [ESI-4]
			"andl $0x10002000, %%ecx;"	//	AND ECX, 0x10002000
			"sarl $1, %%ecx;"			//	SAR ECX, 1
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				//	sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK3) >> 1;

			"movl 4(%%ebx), %%ecx;"		//	MOV ECX, [ESI+4]		// next context 
			"andl $0x10002000, %%ecx;"	//	AND ECX, 0x10002000
			"shll $1, %%ecx;"			//	SHL ECX, 1
			"orl %%ecx, %%eax;"			//	OR EAX, ECX			// sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK3) << 1;

			"movl %%eax, %%ecx;"		//	MOV ECX, EAX
			"sarl $14, %%ecx;"			//	SAR ECX, 14
			"orl %%ecx, %%eax;"			//	OR EAX, ECX				// sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));
			"sarl $10, %%eax;"			//	SAR EAX, 10				// sym >>= 10
			"andl $0xff, %%eax;"		//	AND EAX, 0xff


			"addl 32(%%esi), %%eax;"
			"movzx (%%eax), %%ecx;"		//	MOVZX ECX, byte ptr[lut_ctx_sign + EAX]	// val = lut_ctx_sign[sym & 0x000000FF];
			"movl %%ecx, %%ebx;"		//	MOV EBX, ECX							// val
			"andl $1, %%ecx;"			//	AND ECX, 1								// val & 1
			"sarl $1, %%ebx;"			//	SAR EBX, 1
			"movl 36(%%esi), %%eax;"	//	MOV EAX, states
			"leal 80(%%eax, %%ebx, 8), %%ebx;"	//	LEA EBX,[EAX + 8*EBX + 0x50]		// EBX has pointer to context state	//	state_ref = states  + STATE_SIGN_START + (val>>1);

			"movl (%%ebx), %%eax;"		// MOV EAX, [EBX]		// _p_sign
			"subl %%eax, %%edx;"		//	SUB EDX, EAX		// D -= _p_sign
			"andl $1, %%eax;"			//	AND EAX, 1			// _sign	
			"xorl %%eax, %%ecx;"		//	XOR ECX, EAX
			"addl %%eax, %%edx;"		//	ADD EDX, EAX		// correct D by adding back _sign
			"jge row3_sig_decode_in_cdp;"		//	JGE	row3_sig_decode_in_cdp
			"call pass2_mq_decode;"		//	CALL pass2_mq_decode
"row3_sig_decode_in_cdp:;"
			"movl (%%esi), %%ebx;"		
			"orl $0x4000, -4(%%ebx);"	//	OR dword ptr[ESI - 4], 0x4000			// contextWord[-1] |= (SIGMA_CR<<9);
			"orl $0x1000, 4(%%ebx);"	//	OR dword ptr[ESI + 4], 0x1000		// contextWord[ 1] |= (SIGMA_CL<<9);
			"movl 40(%%esi), %%eax;"	// MOV EAX, width
			"orl $4, 8(%%ebx,%%eax);"	//	OR dword ptr[ESI + EAX + 8], 0x04			// contextWord[width + DECODE_CWROD_ALIGN -1] |= SIGMA_TR;
			"orl $1, 16(%%ebx, %%eax);"	//	OR dword ptr[ESI + EAX +16], 0x01			// contextWord[width + DECODE_CWROD_ALIGN +1] |= SIGMA_TL;
			"shll $18, %%ecx;"			//	SHL ECX, 18	
			"orl %%ecx, 12(%%ebx, %%eax);"	//	OR dword ptr[ESI + EAX +12], ECX			// contextWord[width + DECODE_CWROD_ALIGN   ] |= (sym<<CONTEXT_PREV_CHI_POS);
			"orl $2, 12(%%ebx, %%eax);"	//	OR dword ptr[ESI + EAX +12], 0x02			// contextWord[width + DECODE_CWROD_ALIGN   ] |= SIGMA_TC 
			"movl (%%ebx), %%eax;"		//	MOV EAX, [ESI]		// restore cword
			"shll $10, %%ecx;"			// SHL  ECX, 10		// sym<<(CONTEXT_FIRST_CHI_POS+9) remember sym has shl 18 above
			"orl %%ecx, %%eax;"			//	OR EAX, ECX
			"orl $0x2000, %%eax;"		//	OR EAX,0x2000	// cword |= (SIGMA_SELF<<9) | (sym<<(CONTEXT_FIRST_CHI_POS+9));
			"shll $3, %%ecx;"			// SHL ECX, 3			// sym << 31
			"addl 44(%%esi), %%ecx;"	//	ADD ECX, one_and_half
			"addl 16(%%esi), %%ebx;"	//	MOV EBX, sp_offset3
			"movl %%ecx, (%%ebx);"		//	MOV [ESI + EBX], ECX

"update_context:;"
			"movl %%eax, %%ebx;"		//	MOV EBX, EAX
			"shll $16, %%eax;"			//	SHL EAX, 16
			"andl $0x24900000, %%eax;"	//	AND EAX, 0x24900000
			"orl %%eax, %%ebx;"			//	OR EBX, EAX			// cword |= (cword << (CONTEXT_MU_POS - SIGMA_SELF_POS)) &	((CONTEXT_MU<<0)|(CONTEXT_MU<<3)|(CONTEXT_MU<<6)|(CONTEXT_MU<<9));
			"andl $0xB6DFFFFF, %%ebx;"	//	AND EBX, 0xB6DFFFFF	// cword &= ~((CONTEXT_PI<<0)|(CONTEXT_PI<<3)|(CONTEXT_PI<<6)|(CONTEXT_PI<<9));
			"movl (%%esi), %%eax;"
			"movl %%ebx, (%%eax);"		//	MOV dword ptr[ESI], EBX		// *contextWord = cword;
"next_column_2:"
			"addl $4, (%%esi);"			//	ADD ESI, 4
			"movl (%%esi), %%eax;"			
			"cmpl 24(%%esi), %%eax;"	//	CMP ESI, current_strip_end
			"jnz column_loop2;"			//	JNZ column_loop

			// next_stripe:
			"addl $12, (%%esi);"			//	ADD ESI, 12		// next strip
			"movl 20(%%esi), %%ebx;"		//	MOV EBX, sp_stripe_adjust
			"addl %%ebx,4(%%esi);"			//	ADD [sp_offset0], EBX
			"addl %%ebx, 8(%%esi);"
			"addl %%ebx, 12(%%esi);"
			"addl %%ebx, 16(%%esi);"
			"popl %%eax;"				//			POP EAX
			"decl %%eax;"				//				DEC EAX
			"jnz stribe_loop2;"			//				JNZ stribe_loop
			"jmp proc_return_2;"			//	JMP proc_return

"pass2_mq_decode:"
 		"movl %1, %%eax;"		//	_asm MOV EAX, coder_A
 		"movl %2, %%edi;"	//	__asm MOV EDI, coder_C
		"addl %%edx, %%eax;"		//	__asm ADD EAX, EDX									
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX								
		"movl (%%ebx), %%edx;"		//	__asm MOV EDX, [EBX]							
		"jl LSI_selected_2;"			//	__asm JL	LSI_selected							
		"andl $0xffff00, %%edx;"	//	__asm AND EDX, 0xffff00								
		"cmpl %%edx, %%eax;"		//	__asm CMP EAX, EDX									
		"jge USIM_2;"					//	__asm JGE USIM										
		"xorl $1, %%ecx;"			//	__asm XOR ECX, 1									
		"movl 4(%%ebx), %%edx;"		//	__asm MOV EDX, [EBX + 4]							
		"movq 8(%%edx), %%mm0;"		//	__asm MOVQ MM0, [EDX + 8]							
		"movq %%mm0,(%%ebx);"		//__asm MOVQ [EBX], MM0
//////////////////////////////////////////////////////////////////////////
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_0_2:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_0_2;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"		//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_0_2;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"
		"cmpl $0x8f, %%edx;"		//	__asm CMP EDX,0x8F									
		"jle T_le8F_0_2;"				//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"		//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"			//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_0_2;"		//	__asm JMP fill_lsbs_done_##x						
"T_le8F_0_2:;"						//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"		//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"			//	__asm ADD EDI, EDX									
		"incl %%ebx;"				// __asm INC EBX										
		"movl %%ebx, %6;"		//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"			//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_0_2;"		//	__asm JMP fill_lsbs_done_##x						
"no_ff_0_2:;"							//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"				//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"			//	__asm MOV EDX, 8									
"fill_lsbs_done_0_2:;"				//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"				//	__asm POP EBX										
"skip_fill_lsbs_0_2:;"				//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"				//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_0_2;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////

		"jmp state_transition_done_2;"	//__asm JMP state_transition_done						
"USIM_2:;"					//__asm USIM:							
		"movl 4(%%ebx), %%edx;"		//__asm MOV EDX, dword ptr[EBX+4]						
		"movq (%%edx), %%mm0;"		//__asm MOVQ MM0, [EDX]								
		"movq %%mm0, (%%ebx);"		//__asm MOVQ [EBX], MM0								
		
//////////////////////////////////////////////////////////////////////////
		//		__asm BMI_RENORM_ASM(1)								
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_1_2:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_1_2;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"	//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_1_2;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"cmpl $0x8f, %%edx;"	//	__asm CMP EDX,0x8F									
		"jle T_le8F_1_2;"			//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"	//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"	//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"		//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_1_2;"	//	__asm JMP fill_lsbs_done_##x						
"T_le8F_1_2:;"					//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"	//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX									
		"incl %%ebx;"			// __asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"		//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_1_2;"	//	__asm JMP fill_lsbs_done_##x						
"no_ff_1_2:;"						//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"			//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"		//	__asm MOV EDX, 8									
"fill_lsbs_done_1_2:;"		//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"		//	__asm POP EBX										
"skip_fill_lsbs_1_2:;"		//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"			//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_1_2;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////

		"jmp state_transition_done_2;"		//__asm JMP state_transition_done						
"LSI_selected_2:;"		//_asm LSI_selected :								
		"andl $0xffff00, %%edx;"	//__asm AND EDX, 0xffff00								
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"cmpl %%edx, %%eax;"		//__asm CMP EAX, EDX
		"movl %%edx, %%eax;"		//	__asm MOV EAX, EDX									
		"jge LSIL_2;"				//__asm JGE LSIL										
		"movl 4(%%ebx), %%edx;"		//__asm MOV EDX, dword ptr[EBX+4]						
		"movq (%%edx), %%mm0;"		//	__asm MOVQ MM0, [EDX]								
		"movq %%mm0,(%%ebx);"			//	__asm MOVQ [EBX], MM0								

////////////////////////////////////////////////////////////////////////
		//		__asm BMI_RENORM_ASM(2)								
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_2_2:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_2_2;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"		//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_2_2;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"cmpl $0x8f, %%edx;"	//	__asm CMP EDX,0x8F									
		"jle T_le8F_2_2;"			//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"	//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"	//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"		//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_2_2;"	//	__asm JMP fill_lsbs_done_##x						
"T_le8F_2_2:;"					//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"	//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX									
		"incl %%ebx;"			// __asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"		//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_2_2;"	//	__asm JMP fill_lsbs_done_##x						
"no_ff_2_2:;"						//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"			//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"		//	__asm MOV EDX, 8									
"fill_lsbs_done_2_2:;"		//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"		//	__asm POP EBX										
"skip_fill_lsbs_2_2:;"		//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"			//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_2_2;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////

		"jmp state_transition_done_2;"	//__asm JMP state_transition_done						
"LSIL_2:;"					//__asm LSIL:											
		"xorl $1, %%ecx;"		//	__asm XOR ECX, 1									
		"movl 4(%%ebx), %%edx;"		//__asm MOV EDX, [EBX + 4]							
		"movq 8(%%edx), %%mm0;"		//	__asm MOVQ MM0, [EDX + 8]							
		"movq %%mm0, (%%ebx);"		//	__asm MOVQ [EBX], MM0								

//////////////////////////////////////////////////////////////////////////
		//		__asm BMI_RENORM_ASM(3)								
		"movl %3, %%edx;"		//	__asm MOV EDX,coder_t
"renorm_loop_3_2:;"					//	__asm renorm_loop_##x:	
		"testl %%edx, %%edx;"		//	__asm TEST EDX,EDX									
		"jnz skip_fill_lsbs_3_2;"		//	__asm JNZ skip_fill_lsbs_##x						
		"pushl %%ebx;"				//	__asm PUSH EBX										
		"movl %5, %%edx;"		//	__asm MOV EDX,coder_T
		"cmpl $0xff, %%edx;"		//	__asm CMP EDX,0xFF									
		"jnz no_ff_3_2;"				//	__asm JNZ no_ff_##x									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"cmpl $0x8f, %%edx;"	//	__asm CMP EDX,0x8F									
		"jle T_le8F_3_2;"			//	__asm JLE T_le8F_##x								
		"movl $0xff, %%edx;"	//	__asm MOV EDX,0xff									
		"addl %%edx, %%edi;"	//	__asm ADD EDI, EDX				
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"incl %4;"			//	__asm INC coder_S
		"movl $8, %%edx;"		//	__asm MOV EDX,8										
		"jmp fill_lsbs_done_3_2;"	//	__asm JMP fill_lsbs_done_##x						
"T_le8F_3_2:;"					//	__asm T_le8F_##x:									
		"addl %%edx, %%edx;"	//	__asm ADD EDX,EDX									
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//	__asm ADD EDI, EDX
		"incl %%ebx;"			// __asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $7, %%edx;"		//	__asm MOV EDX,7										
		"jmp fill_lsbs_done_3_2;"	//	__asm JMP fill_lsbs_done_##x						
"no_ff_3_2:;"						//	__asm no_ff_##x:									
		"movl %6, %%ebx;"	//	__asm MOV EBX,coder_buf_next
		"movzx (%%ebx), %%edx;"		//	__asm MOVZX EDX,byte ptr [EBX]
		"movl %%edx, %5;"	//	__asm MOV coder_T,EDX
		"addl %%edx, %%edi;"		//__asm ADD EDI, EDX									
		"incl %%ebx;"			//	__asm INC EBX										
		"movl %%ebx, %6;"	//	__asm MOV coder_buf_next, EBX
		"movl $8, %%edx;"		//	__asm MOV EDX, 8									
"fill_lsbs_done_3_2:;"		//__asm fill_lsbs_done_##x:							
 		"popl %%ebx;"		//	__asm POP EBX										
"skip_fill_lsbs_3_2:;"		//__asm skip_fill_lsbs_##x:							
		"addl %%eax, %%eax;"		//	__asm ADD EAX,EAX									
		"addl %%edi, %%edi;"		//	__asm ADD EDI,EDI									
		"decl %%edx;"			//__asm DEC EDX										
		"testl $0x00800000, %%eax;"	//__asm TEST EAX,0x00800000							
		"jz renorm_loop_3_2;"			//	__asm JZ renorm_loop_##x							
		"movl %%edx,%3;"		//__asm MOV coder_t,EDX
//////////////////////////////////////////////////////////////////////////
"state_transition_done_2:;"			// __asm state_transition_done:						
		"movl %%eax, %%edx;"		//__asm MOV EDX,EAX									
		"subl $0x00800000, %%edx;"	//__asm SUB EDX,0x00800000							
		"cmpl %%edx, %%edi;"		//	__asm CMP EDI,EDX									
		"cmovl %%edi, %%edx;"		//__asm CMOVL EDX,EDI									
		"subl %%edx, %%eax;"		//__asm SUB EAX,EDX									
		"subl %%edx, %%edi;"		//	__asm SUB EDI,EDX									
		"movl %%edi, %2;"		//__asm MOV coder_C,EDI
		"movl %%eax, %1;"		//__asm MOV coder_A,EAX
		"ret;"						//__asm ret											
 						

"proc_return_2:;"						//	proc_return:
		//		MOV coder_D, EDX
		"emms;"						//				EMMS
		"popl %%ebx;"


			// output
			: "=d"(coder_D),		// %0
			"=g"(coder_A),			// %1
			"=g"(coder_C),			// %2
			"=g"(coder_t),			// %3
			"=g"(coder_S),			// %4
			"=g"(coder_T),			// %5
			"=g"(coder_buf_next)	// %6
			// input
			: "g"(num_stripes),		// %7
			"g"(paras),				// %8
			"0"(coder_D),
			"1"(coder_A),
			"2"(coder_C),
			"3"(coder_t),	
			"4"(coder_S),
			"5"(coder_T),
			"6"(coder_buf_next)
			: "eax", "ecx", "esi", "edi"

			);

	CODER_CHECK_IN_ASM(coder)


}

#endif	// ASSEMBLY selection

#endif	// ENABLE_ASSEMBLY_OPTIMIZATION