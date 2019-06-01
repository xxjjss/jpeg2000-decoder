#ifndef INTEL_ASSEMBLY_64
#define INTEL_ASSEMBLY_64	1
#endif
/*#include "platform.h"*/
#include "asm_header.h"
#include "bmi_t1_macro.h"

extern "C"
{


#define		CHECK_OUT_CODER_ASM64(coder)		\
	register int reg_A, reg_C, reg_t, reg_T, reg_S;	\
	register unsigned char * reg_buf_next = (coder).buf_next;	\
	reg_A = (coder).A;					\
	reg_C = (coder).C;					\
	reg_t = (coder).t;					\
	reg_T = (coder).T;					\
	reg_S = (coder).S;					

#define		CHECK_IN_CODER_ASM64(coder)		\
	(coder).A = reg_A;					\
	(coder).C = reg_C;					\
	(coder).t = reg_t;					\
	(coder).T = reg_T;					\
	(coder).S = reg_S;					\
	(coder).buf_next = reg_buf_next;

#define		CODER_VAL(coder,x)		reg_##x

#define MQCODER_A_MIN 0x800000 // ((int_32)(1<<15)) Lower bound for A , left shift 8 bits


#define FILL_LSBS(coder)			\
{									\
	if (CODER_VAL(coder, T) == 0xff )				\
	{								\
		if ( *CODER_VAL(coder, buf_next) > 0x8f)		\
		{							\
			CODER_VAL(coder, t) = 8;\
			++CODER_VAL(coder, S);	\
		}							\
		else						\
		{							\
			CODER_VAL(coder, T) = *(CODER_VAL(coder, buf_next)++)<<1;	\
			CODER_VAL(coder, t) = 7;\
		}							\
	}								\
	else							\
	{								\
		CODER_VAL(coder, T) = *(CODER_VAL(coder, buf_next)++);			\
		CODER_VAL(coder, t) = 8;					\
	}								\
	CODER_VAL(coder, C) += CODER_VAL(coder, T);		\
}	


#define		RENORMALIZE_ONCE(coder)	\
{									\
	if (!CODER_VAL(coder, t))		\
	{								\
		FILL_LSBS(coder);			\
	}								\
	CODER_VAL(coder, A)<<=1;		\
	CODER_VAL(coder, C)<<=1;		\
	--CODER_VAL(coder, t);			\
}

#define MQ_DECODE_ASM64(coder, state, ret)					\
{														\
	ret = (state)._p_sign & 1;		/* set to mps first*/	\
	register int _p = (state)._p_sign & 0xffffff00;		\
	CODER_VAL(coder,A) -= _p;						\
	if (CODER_VAL(coder,C) < _p)					\
	{ /* Lower sub-interval selected */				\
		if (CODER_VAL(coder,A) < _p)				\
		{ /* Conditional exchange; MPS */			\
			(state) = reinterpret_cast<mq_env_trans *>((state).transition)->mps;		\
		}											\
		else										\
		{ /* LPS */									\
			ret ^= 1;								\
			(state) = reinterpret_cast<mq_env_trans *>((state).transition)->lps;		\
		}											\
		CODER_VAL(coder,A) = _p;					\
		do											\
		{											\
			RENORMALIZE_ONCE(coder);				\
		} while (CODER_VAL(coder,A) < MQCODER_A_MIN);	\
	}												\
	else											\
	{ /* Upper sub-interval selected; A < A_min. */	\
		CODER_VAL(coder,C) -= _p;					\
		if (CODER_VAL(coder,A) < MQCODER_A_MIN)		\
		{											\
			if (CODER_VAL(coder,A) < _p)			\
			{ /* Conditional exchange; LPS */		\
				ret ^= 1;							\
				(state) = reinterpret_cast<mq_env_trans *>((state).transition)->lps;		\
			}										\
			else									\
			{ /* MPS */								\
				(state) = reinterpret_cast<mq_env_trans *>((state).transition)->mps;		\
			}										\
			do										\
			{										\
				RENORMALIZE_ONCE(coder);			\
			} while (CODER_VAL(coder,A) < MQCODER_A_MIN);	\
		}											\
	}												\
}

// it's only for using to generate the asm pattern
// All meanningful code should be in modify and create in the asm file 
	void bmi_t1_decode_pass0_asm64(bmi_mq_decoder &coder, mq_env_state states[],
		int p, unsigned char * lutContext,int *sp,	 int *contextWord, int width, int num_stripes, unsigned char * lut_ctx_sign_asm64)
	{
		
		CHECK_OUT_CODER_ASM64(coder);

		register int cword;
		register int sym;
		register int val;
		register mq_env_state *state_ref;
		int one_and_half = (1<<p) + ((1<<p)>>1);
		int width_by2=width+width, width_by3=width_by2+width;
		int c;

	//for (; num_stripes > 0; num_stripes--, contextWord += DECODE_CWROD_ALIGN, sp += width_by3)
		do
		{
		//	for (int c=width; c > 0; c--, sp++, contextWord++)
			c = width;
			do
			{
				if (*contextWord == 0)
				{ 
					// skip over pixels with all-0 neighbourhoods
					while (*(contextWord += 3) == 0)
					{
						c-=3;
						sp+=3;
					};
					contextWord-=2;
					--c;
					++sp;
					continue;
				}
				cword = *contextWord;

					// check all 4 rows 
					// When at lease one neighbor is significant and itself is un-significant
					// Implement the decode pass 0 in page 342
					if ((cword & NEIGHBOR_SIGMA_ROW0) && !(cword & SIG_PRO_CHECKING_MASK0))
					{ 
						// First row, row 0
						state_ref = states + STATE_SIG_PROP_START + lutContext[cword & NEIGHBOR_SIGMA_ROW0];
						MQ_DECODE_ASM64(coder, *state_ref, sym);
						if (!sym)
						{ 
							cword |= CONTEXT_PI; 
						}
						else
						{

							//////////////////////////////////////////////////////////////////////////
							//		decode_sign_comments:
							//////////////////////////////////////////////////////////////////////////
							// Decode sign bit
							// Processing refer to page 249
							// Chi_V
							sym = cword & SIG_PRO_CHECKING_MASK1;										// set bits 7,22, row 1
							sym |= cword & (SIGMA_SELF>>3);												// Set prev Sigma
							sym |= (cword & CONTEXT_CHI_PREV)>>2;										// Set prev CHI and move down 2 bits to 16 to make all sigma/chi gap is 3
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1;						// set bits 3, 18(right)
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1;						// set bits 5, 20(left)
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS));				// Interleave chi & sigma, move 16, 18,20,22 down to 2,4,6,8
							sym >>= 1; // shift down to make it range start from 0						// Move bits 1-8 to 0-7 bits
							val = lut_ctx_sign_asm64[sym & 0x000000FF];										// look into context sign table
							state_ref = states  + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.
							// Above lines is Decode-Sign() processing, refer to page 342

							// refer to page 342 : sigma[j] <- 1
							// The first row we need to change the 3 context words above, set sigma to 1
							// Current context is the ButtomRight context of the TopLeft context
							contextWord[-width - DECODE_CWROD_ALIGN - 1] |=	(SIGMA_BR << 9);		// set the sigma_BR of the TL pixel
							contextWord[-width - DECODE_CWROD_ALIGN    ] |=	(SIGMA_BC << 9) | (sym << CONTEXT_NEXT_CHI_POS ); // Also need to set the next chi bit
							contextWord[-width - DECODE_CWROD_ALIGN + 1] |=	(SIGMA_BL << 9);		// set the sigma_BL of the TR pixel
							contextWord[-1] |= (SIGMA_CR<<0);	// set the sigma_CR of the left pixel
							contextWord[ 1] |= (SIGMA_CL<<0);	// set the sigma_CL of the right pixel

							cword |= (SIGMA_SELF) | (CONTEXT_PI) | (sym << CONTEXT_FIRST_CHI_POS);
							sp[0] = (sym<<31) + one_and_half;

						}
					}
					if ((cword & NEIGHBOR_SIGMA_ROW1) && !(cword & SIG_PRO_CHECKING_MASK1))
					{ 
						// Second row; row 1
						state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW1)>>3];
						MQ_DECODE_ASM64(coder, *state_ref, sym);
						if (!sym)
						{
							cword |= (CONTEXT_PI<<3); 
						}
						else
						{
							// Decode sign bit
							// refer to decode_sign_comments: in this file
							// Chi_V
							sym = cword & (SIG_PRO_CHECKING_MASK0 | SIG_PRO_CHECKING_MASK2);
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK1) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK1) << 1;
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS)); // Interleave chi & sigma
							sym >>= 4; // shift down to make it range start from 0
							val = lut_ctx_sign_asm64[sym & 0x000000FF];
							state_ref = states + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.

							// refer to page 342 : sigma[j] <- 1
							contextWord[-1] |= (SIGMA_CR<<3);
							contextWord[ 1]  |= (SIGMA_CL<<3);

							cword |= (SIGMA_SELF<<3) | (CONTEXT_PI<<3) | (sym<<(CONTEXT_FIRST_CHI_POS+3));
							sp[width] = (sym<<31) + one_and_half;

						}
					}
					if ((cword & NEIGHBOR_SIGMA_ROW2) && !(cword & SIG_PRO_CHECKING_MASK2))
					{ 
						// Third row 
						state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW2)>>6];
						MQ_DECODE_ASM64(coder, *state_ref, sym);
						if (!sym)
						{ 
							cword |= (CONTEXT_PI<<6);
						}
						else
						{
							// Decode sign bit
							// refer to decode_sign_comments: in this file
							// Chi_V
							sym = cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK2) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK2) << 1;
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS-1-SIGMA_SELF_POS)); // Interleave chi & sigma
							sym >>= 7; // shift down to make it range start from 0
							val = lut_ctx_sign_asm64[sym & 0x000000FF];
							state_ref = states + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.

							// refer to page 342 : sigma[j] <- 1
							contextWord[-1] |= (SIGMA_CR<<6);
							contextWord[1]  |= (SIGMA_CL<<6);

							cword |= (SIGMA_SELF<<6) | (CONTEXT_PI<<6) | (sym << (CONTEXT_FIRST_CHI_POS+6));
							sp[width_by2] = (sym<<31) + one_and_half;
						}

					}
					if ((cword & NEIGHBOR_SIGMA_ROW3) && !(cword & SIG_PRO_CHECKING_MASK3))
					{ 
						// Fourth row
						state_ref = states + STATE_SIG_PROP_START + lutContext[(cword & NEIGHBOR_SIGMA_ROW3)>>9];
						MQ_DECODE_ASM64(coder, *state_ref, sym);
						if (!sym)
						{ 
							cword |= (CONTEXT_PI<<9); 
						}
						else
						{
							// Decode sign bit

							// refer to decode_sign_comments: in this file
							// Chi_V
							sym = cword & (SIG_PRO_CHECKING_MASK2 | SIG_PRO_CHECKING_MASK_NEXT);
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK3) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK3) << 1;
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS-1-SIGMA_SELF_POS)); // Interleave chi & sigma
							sym >>= 10; // shift down to make it range start from 0


							val = lut_ctx_sign_asm64[sym & 0x000000FF];
							state_ref = states + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.

							// The forth row, need to change the 3 contexts below
							contextWord[width + DECODE_CWROD_ALIGN -1] |= SIGMA_TR;
							contextWord[width + DECODE_CWROD_ALIGN   ] |= SIGMA_TC | (sym<<CONTEXT_PREV_CHI_POS);
							contextWord[width + DECODE_CWROD_ALIGN +1] |= SIGMA_TL;

							contextWord[-1] |= (SIGMA_CR<<9);
							contextWord[1]  |= (SIGMA_CL<<9);
							cword |= (SIGMA_SELF<<9) | (CONTEXT_PI<<9) | (sym<<(CONTEXT_FIRST_CHI_POS+9));
							sp[width_by3] = (sym<<31) + one_and_half;
						}

					}

				*contextWord = cword;
				--c; 
				++sp;
				++contextWord;
			}		while(c > 0);
			--num_stripes;
			contextWord += DECODE_CWROD_ALIGN;
			sp += width_by3;
		}while(num_stripes > 0);

		CHECK_IN_CODER_ASM64(coder);
	}


	void bmi_t1_decode_pass1_asm64(bmi_mq_decoder &coder, mq_env_state states[],
		int p,								int *sp,	 int *contextWord, int width, int num_stripes )

	{


		CHECK_OUT_CODER_ASM64(coder);
		register int cword;
		register int sym;
		register int val;

		register mq_env_state * state_ref;
		int half_lsb = (1<<p)>>1;
		int  width_by2=width+width, width_by3=width_by2+width;
		int c;
		states += STATE_MAG_START;

	// 	for (; num_stripes > 0; num_stripes--, contextWord += DECODE_CWROD_ALIGN, sp += width_by3)
		do
		{
			c = width;
	//		for (int c=width; c > 0; c--, sp++, contextWord++)
			do
			{
				if ((*contextWord & ((CONTEXT_MU)|(CONTEXT_MU<<3)|(CONTEXT_MU<<6)|(CONTEXT_MU<<9))) == 0)
				{ // Invoke speedup trick to skip over runs of all-0 neighbourhoods
					while (*(contextWord += 2) == 0)
					{
						c-=2;
						sp+=2;
					};
					--contextWord;
					--c;
					++sp;
					continue;
					}
					else
					{
						cword = *contextWord;
						if ((cword & MAG_REF_CHECKING_MASK0) == MAG_REF_CHECKING_MASK0) 
							// The first row
						{
							val = sp[0];
							sym = (val & MAX_INT32_WITHOUT_SIGN) >> p;
							state_ref = states;
							if (sym < 4)
							{
								if (cword & NEIGHBOR_SIGMA_ROW0)
									state_ref++;
							}
							else
							{
								state_ref += 2;
							}
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							val ^= ((1-sym)<<p);
							val |= half_lsb;
							sp[0] = val;
						}
						if ((cword & MAG_REF_CHECKING_MASK1) == MAG_REF_CHECKING_MASK1) 
							// 2nd row
						{ 
							val = sp[width];
							sym = (val & MAX_INT32_WITHOUT_SIGN) >> p;
							state_ref = states;
							if (sym < 4)
							{
								if (cword & NEIGHBOR_SIGMA_ROW1)
									state_ref++;
							}
							else
							{
								state_ref += 2;
							}
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							val ^= ((1-sym)<<p);
							val |= half_lsb;
							sp[width] = val;
						}
						if ((cword & MAG_REF_CHECKING_MASK2) == MAG_REF_CHECKING_MASK2) 
							// 3rd row
						{ 
							val = sp[width_by2];
							sym = (val & MAX_INT32_WITHOUT_SIGN) >> p;
							state_ref = states;
							if (sym < 4)
							{
								if (cword & NEIGHBOR_SIGMA_ROW2)
									state_ref++;
							}
							else
							{
								state_ref += 2;
							}
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							val ^= ((1-sym)<<p);
							val |= half_lsb;
							sp[width_by2] = val;
						}
						if ((cword & MAG_REF_CHECKING_MASK3) == MAG_REF_CHECKING_MASK3) 
							// The forth row
						{ 
							val = sp[width_by3];
							sym = (val & MAX_INT32_WITHOUT_SIGN) >> p;
							state_ref = states;
							if (sym < 4)
							{
								if (cword & NEIGHBOR_SIGMA_ROW3)
								{
									state_ref++;
								}
							}
							else
							{
								state_ref += 2;
							}
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							val ^= ((1-sym)<<p);
							val |= half_lsb;
							sp[width_by3] = val;
						}
					}
				--c;
				++sp;
				++contextWord;
			}		while(c > 0);
			--num_stripes;
			contextWord += DECODE_CWROD_ALIGN;
			sp += width_by3;
		}while(num_stripes > 0);

		CHECK_IN_CODER_ASM64(coder);

	}

	void bmi_t1_decode_pass2_asm64(bmi_mq_decoder &coder, mq_env_state states[],
		int p, unsigned char * lut_ctx,int *sp,	 int *contextWord, int width, int num_stripes, const unsigned char * lut_ctx_sign , mq_env_trans * tableBase, void * tmpTransition)
	{
		CHECK_OUT_CODER_ASM64(coder);
		register int cword;
		register int sym;
		register int val;
		register int run_length;
		register mq_env_state *state_ref;
		int one_and_half = (1<<p) + ((1<<p)>>1);
		int width_by2=width<<1, width_by3=width_by2+width;
		int c;
	//	for (; num_stripes > 0; num_stripes--, contextWord += DECODE_CWROD_ALIGN, sp += width_by3)
		do
		{
			c = width;
	// 		for (int c=width; c > 0; c--, sp++, contextWord++)
			do
				{
					run_length = -1;
					if (*contextWord == 0)
					{ 
						MQ_DECODE_ASM64(coder, states[STATE_RUN_START], sym);
						if (!sym)
						{
					--c;
					++sp;
					++contextWord;
							continue;
						}
						else
						{
							mq_env_state tmpRunState; 
							// 					tmpRunState.init(46,0);
							tmpRunState._p_sign = 0x560100;
						tmpRunState.transition = tmpTransition;
							MQ_DECODE_ASM64(coder, tmpRunState, run_length);
							MQ_DECODE_ASM64(coder, tmpRunState, val);
							run_length = (run_length<<1) + val;
							sym = 0;
						}
					}
					cword = *contextWord;
					if (!(cword & CLEANUP_CHECKING_MASK0) || run_length == 0)
					{ 
						// The first row
						if (run_length == -1)
						{
							state_ref = states + STATE_SIG_PROP_START+lut_ctx[cword & NEIGHBOR_SIGMA_ROW0];
							MQ_DECODE_ASM64(coder, *state_ref, sym);
						}
						if (sym || run_length == 0)
						{
							run_length = -1;

							// Decode sign bit
							// Processing refer to page 249
							// refer to decode_sign_comments: in this file
							// Chi_V
							// 					sym = cword & (SIG_PRO_CHECKING_MASK_PREV | SIG_PRO_CHECKING_MASK1) ;	
							sym = cword & SIG_PRO_CHECKING_MASK1;										// set bits 7,22
							sym |= cword & (SIGMA_SELF>>3);
							sym |= (cword & CONTEXT_CHI_PREV)>>2;
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK0) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK0) << 1;

							sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS)); 
							sym >>= 1; // shift down to make it range start from 0
							val = lut_ctx_sign[sym & 0x000000FF];
							state_ref = states  + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.
							// Above lines is Decode-Sign() processing, refer to page 342

							// refer to page 342 : sigma[j] <- 1
							// The first row we need to change the 3 context words above, set sigma to 1
							// Current context is the ButtomRight context of the TopLeft context
							contextWord[-width - DECODE_CWROD_ALIGN-1] |=	(SIGMA_BR << 9);
							contextWord[-width - DECODE_CWROD_ALIGN  ] |=	(SIGMA_BC << 9) | (sym << CONTEXT_NEXT_CHI_POS ); // Also need to set the next chi bit
							contextWord[-width - DECODE_CWROD_ALIGN+1] |=	(SIGMA_BL << 9);
							contextWord[-1] |= (SIGMA_CR<<0);
							contextWord[ 1] |= (SIGMA_CL<<0);

							cword |= (SIGMA_SELF) /*| (CONTEXT_PI) */| (sym << CONTEXT_FIRST_CHI_POS);
							sp[0] = (sym<<31) + one_and_half;
						}
					}
					if (!(cword & CLEANUP_CHECKING_MASK1) || run_length == 1)
					{ 
						// The second row
						if (run_length == -1)
						{
							state_ref = states+STATE_SIG_PROP_START+lut_ctx[(cword & NEIGHBOR_SIGMA_ROW1) >>3 ];
							MQ_DECODE_ASM64(coder, *state_ref, sym);
						}

						if (sym || run_length == 1)
						{

							run_length = -1;

							// Decode sign bit
							// refer to decode_sign_comments: in this file
							// Chi_V
							sym = cword & (SIG_PRO_CHECKING_MASK0 | SIG_PRO_CHECKING_MASK2);
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK1) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK1) << 1;
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS - 1 - SIGMA_SELF_POS)); // Interleave chi & sigma
							sym >>= 4; // shift down to make it range start from 0
							val = lut_ctx_sign[sym & 0x000000FF];
							state_ref = states + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.

							// refer to page 342 : sigma[j] <- 1
							contextWord[-1] |= (SIGMA_CR<<3);
							contextWord[ 1]  |= (SIGMA_CL<<3);

							cword |= (SIGMA_SELF<<3) /*| (CONTEXT_PI<<3) */| (sym<<(CONTEXT_FIRST_CHI_POS+3));
							sp[width] = (sym<<31) + one_and_half;

						}
					}
					if (!(cword & CLEANUP_CHECKING_MASK2) || run_length == 2)
					{ // Process third row of stripe column (row 2)
						if (run_length == -1)
						{
							state_ref = states+STATE_SIG_PROP_START+lut_ctx[(cword & NEIGHBOR_SIGMA_ROW2)>>6];
							MQ_DECODE_ASM64(coder, *state_ref, sym);
						}
						if (sym || run_length == 2)
						{
							run_length = -1;

							// Decode sign bit
							// refer to decode_sign_comments: in this file
							// Chi_V
							sym = cword & (SIG_PRO_CHECKING_MASK1 | SIG_PRO_CHECKING_MASK3);
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK2) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK2) << 1;
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS-1-SIGMA_SELF_POS)); // Interleave chi & sigma
							sym >>= 7; // shift down to make it range start from 0
							val = lut_ctx_sign[sym & 0x000000FF];
							state_ref = states + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.

							// refer to page 342 : sigma[j] <- 1
							contextWord[-1] |= (SIGMA_CR<<6);
							contextWord[1]  |= (SIGMA_CL<<6);

							cword |= (SIGMA_SELF<<6) /*| (CONTEXT_PI<<6)*/ | (sym << (CONTEXT_FIRST_CHI_POS+6));
							sp[width_by2] = (sym<<31) + one_and_half;
						}
					}
					if (!(cword & CLEANUP_CHECKING_MASK3) || run_length == 3)
					{ 
						// The fourth row
						if (run_length == -1)
						{
							state_ref = states+STATE_SIG_PROP_START+lut_ctx[(cword & NEIGHBOR_SIGMA_ROW3)>>9];
							MQ_DECODE_ASM64(coder, *state_ref, sym);
						}
						if (sym || run_length == 3)
						{

							// Decode sign bit
							// refer to decode_sign_comments: in this file
							// Chi_V
							sym = cword & (SIG_PRO_CHECKING_MASK2 | SIG_PRO_CHECKING_MASK_NEXT);
							// Chi_H
							sym |= (contextWord[-1] & SIG_PRO_CHECKING_MASK3) >> 1;
							sym |= (contextWord[ 1] & SIG_PRO_CHECKING_MASK3) << 1;
							sym |= (sym >> (CONTEXT_FIRST_CHI_POS-1-SIGMA_SELF_POS)); // Interleave chi & sigma
							sym >>= 10; // shift down to make it range start from 0


							val = lut_ctx_sign[sym & 0x000000FF];
							state_ref = states + STATE_SIGN_START + (val>>1);
							MQ_DECODE_ASM64(coder, *state_ref, sym);
							sym ^= (val & 1); // Sign bit recovered in LSB.

							// The forth row, need to change the 3 contexts below
							contextWord[width + DECODE_CWROD_ALIGN -1] |= SIGMA_TR;
							contextWord[width + DECODE_CWROD_ALIGN   ] |= SIGMA_TC | (sym<<CONTEXT_PREV_CHI_POS);
							contextWord[width + DECODE_CWROD_ALIGN +1] |= SIGMA_TL;

							contextWord[-1] |= (SIGMA_CR<<9);
							contextWord[1]  |= (SIGMA_CL<<9);
							cword |= (SIGMA_SELF<<9) /*| (CONTEXT_PI<<9)*/ | (sym<<(CONTEXT_FIRST_CHI_POS+9));
							sp[width_by3] = (sym<<31) + one_and_half;

						}
					}
					cword |= (cword << (CONTEXT_MU_POS - SIGMA_SELF_POS)) &
						((CONTEXT_MU<<0)|(CONTEXT_MU<<3)|(CONTEXT_MU<<6)|(CONTEXT_MU<<9));
					cword &= ~((CONTEXT_PI<<0)|(CONTEXT_PI<<3)|(CONTEXT_PI<<6)|(CONTEXT_PI<<9));
					*contextWord = cword;
				--c;
				++sp;
				++contextWord;

			} while (c > 0);
			--num_stripes;
			contextWord += DECODE_CWROD_ALIGN;
			sp += width_by3;
		}	while(num_stripes > 0);

		CHECK_IN_CODER_ASM64(coder);
	}
};

