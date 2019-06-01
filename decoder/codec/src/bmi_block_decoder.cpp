#include "bmi_block_decoder.h"
#include "debug.h"
#include "debugtools.h"

#if BMI_BLOCK_DECODER
#include "dec_t1.h"
#include "bmi_t1_macro.h"

#if !ENABLE_ASSEMBLY_OPTIMIZATION
void bmi_t1_decode_pass0(bmi_mq_decoder &coder, mq_env_state states[],
						 int bit_plane, unsigned char * lutContext,int_32 *sp,	 
						 int_32 *contextWord, int width, int num_stripes )
{
	CHECK_OUT_CODER(coder);  //***UUU copy the member variables to local variables, at the end copy back the local variables to update coder
	/*register*/ int_32 cword;
	/*register*/ int_32 sym;
	/*register*/ int_32 val;
	/*register*/ mq_env_state *state_ref;
	int_32 one_and_half = (1<<bit_plane) + ((1<<bit_plane)>>1); //****UUUUXXXX may not work with byte by byte decoding
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
				state_ref = states + STATE_SIG_PROP_START + lutContext[cword & NEIGHBOR_SIGMA_ROW0];//***UUU lut gives out the label index within the primitive
				MQ_DECODE(coder, *state_ref, sym);
				if (!sym) //***UUU not significant just set the pi bit in the cword
				{ 
					cword |= CONTEXT_PI; 
				}
				else //***UUU significant, decode the sign
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
					val = lut_ctx_sign[sym & 0x000000FF];										// look into context sign table
					state_ref = states  + STATE_SIGN_START + (val>>1);
					MQ_DECODE(coder, *state_ref, sym);
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
				MQ_DECODE(coder, *state_ref, sym);
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
					val = lut_ctx_sign[sym & 0x000000FF];
					state_ref = states + STATE_SIGN_START + (val>>1);
					MQ_DECODE(coder, *state_ref, sym);
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
				MQ_DECODE(coder, *state_ref, sym);
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
					val = lut_ctx_sign[sym & 0x000000FF];
					state_ref = states + STATE_SIGN_START + (val>>1);
					MQ_DECODE(coder, *state_ref, sym);
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
				MQ_DECODE(coder, *state_ref, sym);
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


					val = lut_ctx_sign[sym & 0x000000FF];
					state_ref = states + STATE_SIGN_START + (val>>1);
					MQ_DECODE(coder, *state_ref, sym);
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

	CHECK_IN_CODER(coder);

}

void bmi_t1_decode_pass1(bmi_mq_decoder &coder, mq_env_state states[],
						 int bit_plane, int_32 *sp,	 int_32 *contextWord, int width, int num_stripes )
{

	// refer to page 448 and page 343

	CHECK_OUT_CODER(coder);
	/*register*/ int_32 cword;
	/*register*/ int_32 sym;
	/*register*/ int_32 val;

	/*register*/ mq_env_state * state_ref;
	int_32 half_lsb = (1<<bit_plane)>>1;
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
				DEBUG_PAUSE((cword & CONTEXT_MU) && (!(cword & SIGMA_SELF)));
				if ((cword & MAG_REF_CHECKING_MASK0) == MAG_REF_CHECKING_MASK0) 
					// The first row
				{
					val = sp[0]; //***UUUXXX have to change everything here; should we store delayed sigj ?
					sym = (val & MAX_INT32_WITHOUT_SIGN) >> bit_plane;
					state_ref = states;
					if (sym < 4) //***UUUXXX checks whether it became significant in the last plane ?
					{
						if (cword & NEIGHBOR_SIGMA_ROW0)
							state_ref++;
					}
					else
					{
						state_ref += 2;
					}
					MQ_DECODE(coder, *state_ref, sym);
					val ^= ((1-sym)<<bit_plane); //***UUU current bitplane already has 1 for dequant signaling, so this is the best way to keep or to reset it
					val |= half_lsb; //***UUU dequant signaling
					sp[0] = val;
				}
				if ((cword & MAG_REF_CHECKING_MASK1) == MAG_REF_CHECKING_MASK1) 
					// 2nd row
				{ 
					val = sp[width];
					sym = (val & MAX_INT32_WITHOUT_SIGN) >> bit_plane;
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
					MQ_DECODE(coder, *state_ref, sym);
					val ^= ((1-sym)<<bit_plane);
					val |= half_lsb;
					sp[width] = val;
				}
				if ((cword & MAG_REF_CHECKING_MASK2) == MAG_REF_CHECKING_MASK2) 
					// 3rd row
				{ 
					val = sp[width_by2];
					sym = (val & MAX_INT32_WITHOUT_SIGN) >> bit_plane;
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
					MQ_DECODE(coder, *state_ref, sym);
					val ^= ((1-sym)<<bit_plane);
					val |= half_lsb;
					sp[width_by2] = val;
				}
				if ((cword & MAG_REF_CHECKING_MASK3) == MAG_REF_CHECKING_MASK3) 
					// The forth row
				{ 
					val = sp[width_by3];
					sym = (val & MAX_INT32_WITHOUT_SIGN) >> bit_plane;
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
					MQ_DECODE(coder, *state_ref, sym);
					val ^= ((1-sym)<<bit_plane);
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
	CHECK_IN_CODER(coder);

}

void bmi_t1_decode_pass2(bmi_mq_decoder &coder, mq_env_state states[],
						 int bit_plane,unsigned char * lut_ctx,
						 int_32 *sp, int_32 *contextWord,
						 int width, int num_stripes)
{

// Refer to page 343
	CHECK_OUT_CODER(coder);
	/*register*/ int_32 cword;
	/*register*/ int_32 sym;
	/*register*/ int_32 val;
	/*register*/ int_32 run_length;
	/*register*/ mq_env_state *state_ref;
	int_32 one_and_half = (1<<bit_plane) + ((1<<bit_plane)>>1); //****UUUUXXXX may not work with byte by byte decoding
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
// #if MQCODER_ADD_STATUS_D
// 				// Enter the run mode.
// 				if ((contextWord[3] == 0) && !(states[STATE_RUN_START]._p_sign & 1))
// 				{
// 					// we can skip four stripe columns //***UUU the following should be equivalent to checking whether 4 zer0s can be MQ decoded without renormalizing (each zero - a run in a stripe)
// 													//***UUU if renormalizing required, even if the 4 bits are zero, we cannot use the same prob states[STATE_RUN_START]._p in decoding
// 					sym = (coder.D>>8) - (states[STATE_RUN_START]._p_sign<<2); //***UUU This is not same as decoding 4 consecutive mps (0) without renormalizing ?
// 					if (sym >= 0)
// 					{ // Succeeded in skipping 4 columns at once!
// 						coder.D = (sym<<8);
// 						contextWord += 4; c -= 4; sp += 4;
// 						continue;
// 					}
// 				}
// #endif
				MQ_DECODE(coder, states[STATE_RUN_START], sym);
//sym = (coder).Decode(states[STATE_RUN_START]);
				if (!sym) //***UUU no run interrupt: go to next stripe
				{
					--c;
					++sp;
					++contextWord;
					continue;
				}
				else //***UUU run interrupt: decode the run length
				{
// 					mq_env_state tmpRunState; 
// 					tmpRunState._p_sign = 0x560100;
// 					tmpRunState.transition = reinterpret_cast<void *>(&bmi_mq_transition_table::env_trans_table[92]);
// 					MQ_DECODE(coder, tmpRunState, run_length);
// 					MQ_DECODE(coder, tmpRunState, val);
// 					run_length =(run_length<<1) + val;//***UUU two bits decoded and the run length calculated
 					MQ_DECODE_START(coder, run_length);
					MQ_DECODE_START(coder, val);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
					MQ_DECODE(coder, *state_ref, sym);
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
	CHECK_IN_CODER(coder);

}
#endif // ENABLE_ASSEMBLY_OPTIMIZATION

int	decode_code_block(	bool bypass , int_32 * samples, int_32 * context_words, uint_8 * pass_buf,
				 int numBps, stru_code_block *cur_cb, 	unsigned char *lut_ctx	)
{

//#if _DEBUG
//	static int si = 1;
//	static int blki=1;
//	{
//		FILE * fcoder;
//		fopen_s(&fcoder, "c://test//resultasm.txt", "a+");
//		fprintf_s(fcoder, "=========  code block [%04d]\n",blki++);
//		fclose(fcoder);
//	}
//#endif`
	// Get dimensions.
	int num_cols = cur_cb->width;
	int num_rows = cur_cb->height;
	int num_stripes = (num_rows+3)>>2; 
	int num_samples = (num_stripes<<2)*num_cols; //****UUUU last rows may be extra (to have multiple of 4 rows)

	memset(samples,0,(size_t)(num_samples<<2));	
	// Initialize sample to zero

	int max_original_passes = (numBps - cur_cb->missing_bit_plan ) * 3 - 2;
	if (max_original_passes <= 0)
	{
		return 0; // Break out ; nothing to decode.
	}

	// Determine which passes we can decode and where to put them.
	int p_max = 30 - cur_cb->missing_bit_plan; // Index of most significant plane
	int num_passes = 3 * p_max - 2; // 1 plane for dequantization signalling
	if (num_passes > cur_cb->num_pass) //****UUUU Calculating from p_max and comparing with cur_cb->num_pass
	{									// for exrror resilience ??, otherwise cur_cb->num_pass should be correct
		num_passes =  cur_cb->num_pass;
	}
	if (!num_passes)
	{
		return 0; // Break out ; nothing to decode.
	}

	int context_row_gap = num_cols + DECODE_CWROD_ALIGN; //****UUU why three extra context words

	context_words += context_row_gap + 1; //***UUU exclude first row ? to allow the updating operations at block top without explicitly checking for it ?
	memset(context_words-1,0,(size_t)((num_stripes*context_row_gap+1)<<2));
	// initialize context words to zero

	if (num_rows & 3)
	{
		int_32 oob_marker;
		if ((num_rows & 3) == 1) // Last 3 rows of last stripe unoccupied
			oob_marker = (CONTEXT_CHI<<3) | (CONTEXT_CHI<<6) | (CONTEXT_CHI<<9); //***UUU CONTEXT_CHI signals no decode for first row
		else if ((num_rows & 3) == 2) // Last 2 rows of last stripe are empty //***UUU CONTEXT_CHI<<3 signals no decode for second row
			oob_marker = (CONTEXT_CHI << 6) | (CONTEXT_CHI << 9);
		else
			oob_marker = (CONTEXT_CHI << 9);
		int_32 *cp = context_words + (num_stripes-1)*context_row_gap; //***UUUU the last context words row
		for (int coding_pass_type=num_cols; coding_pass_type > 0; coding_pass_type--)
			*(cp++) = oob_marker; //***UUU Set the context words for the last stripe to signal no decode for the required rows
	}
	{ 
		// Initialize the extra context words between lines , set CHI bits
#if ENABLE_ASSEMBLY_OPTIMIZATION
#if INTEL_ASSEMBLY_32
		__asm {
			MOV EDI,	context_words
				MOV EAX,	num_cols
				SHL EAX,	2
				ADD EDI,	EAX
				MOV EAX,	context_row_gap	// Every (num_cols) pixels, set 3 pixel to 0x49200000
				SHL EAX,	2
				MOV EBX,	0x12480000		// CONTEXT_CHI | (CONTEXT_CHI<<3) | (CONTEXT_CHI<<6) | (CONTEXT_CHI<<9)
				MOV ECX,	num_stripes
loop_start:		
			MOV dword ptr [EDI],	EBX
				MOV dword ptr [EDI + 4],	EBX
				MOV dword ptr [EDI + 8],	EBX
				ADD EDI,	EAX
				LOOPNZ	loop_start
		}
#elif AT_T_ASSEMBLY

		int useless0= 0, useless1 = 1, useless2 = 2, useless3 = 3,useless4 = 4,useless5 = 5,useless6 = 5,useless7 = 7,useless8 = 8;
		__asm__("shll $2, %1;"
				"addl %1, %3;"
				"shll $2, %0;"
"loop_start:;"
				"movl $0x12480000, (%3);"
				"movl $0x12480000, 4(%3);"
				"movl $0x12480000, 8(%3);"
				"addl %0, %3;"
				"dec  %2;"
				"jnz	loop_start;"
			:					// output
			: "r" (context_row_gap), "r" (num_cols), "r" (num_stripes) ,  "r" (context_words)       //inputs	
			//:  "memory"          // memory modified  
		);

#else	// INTEL_ASSEMBLY_64
		int_32 oob_marker = CONTEXT_CHI | (CONTEXT_CHI<<3) | (CONTEXT_CHI<<6) | (CONTEXT_CHI<<9);
		int_32 *cp = context_words + num_cols;
		for (int coding_pass_type=num_stripes; coding_pass_type > 0; coding_pass_type--, cp+=context_row_gap)
		{
			cp[0] = cp[1] = cp[2] = oob_marker; // Need 3 OOB words after line //****UUU set extra three context words to signal no decode;  
		}										//***UUU 3 extra places to support efficeint skipping in mag_ref and sig_prof passes
#endif
#else		// ENABLE_ASSEMBLY_OPTIMIZATION
		// C++
		int_32 oob_marker = CONTEXT_CHI | (CONTEXT_CHI<<3) | (CONTEXT_CHI<<6) | (CONTEXT_CHI<<9);
		int_32 *cp = context_words + num_cols;
		for (int coding_pass_type=num_stripes; coding_pass_type > 0; coding_pass_type--, cp+=context_row_gap)
		{
			cp[0] = cp[1] = cp[2] = oob_marker; // Need 3 OOB words after line //****UUU set extra three context words to signal no decode;  
		}										//***UUU 3 extra places to support efficeint skipping in mag_ref and sig_prof passes

#endif
	}


	int bit_plane = (numBps>>11) - 1 - cur_cb->missing_bit_plan;
	if(bit_plane < 0) bit_plane = p_max;
	if(bit_plane == 0) bit_plane = 1;
	int segment_passes = 0; // Num coding passes in current codeword segment.

	bmi_mq_decoder coder;
	mq_env_state states[18];
	memcpy(states, bmi_mq_transition_table::sOrigianl_state, sizeof(mq_env_state)* 18);
	// memcpy is faster than the following init
// 	for (int n=0; n < 18; n++)
// 	{
// 		// Init to (sigma 0 , sign 0)
// 		states[n]._p = 0x5601;
// 		states[n].sign = 0;
//		states[n].transition = bmi_mq_transition_table::env_trans_table;
// 	}
// 	INIT_STATE(states[STATE_SIG_PROP_START], 4, 0);
// 	INIT_STATE(states[STATE_RUN_START], 3, 0);

	for (int codingPassIndex = 0, coding_pass_type = 2 ; /*start decode from pass2  clean up processing*/  
		codingPassIndex < num_passes; codingPassIndex++, coding_pass_type++)
	{
		if (coding_pass_type == 3)
		{
			coding_pass_type=0; 
			bit_plane--; // Move on to next bit-plane.
		} 
		if (segment_passes == 0)
		{ 
			// New segment
			segment_passes = max_original_passes; 
			if ((codingPassIndex + segment_passes) > num_passes)
			{
				segment_passes = num_passes - codingPassIndex;
			}

			stru_coding_pass *pass1, *pass2;
			pass1 = cur_cb->pass; //***UUU first pass of the codeblock at the beginning
			pass2 = pass1 + pass1->num_t2layer - 1; //***UUU pass2:last pass in the layer. num_t2layer = number of passes in the current layer from the block
											//***UUU these layers continuous  - so ptrs not set to buffs in t2 dec. only the first pass of the layer has a ptr to buff
			int length = 0; 

			int passed =  segment_passes;
			while(passed) { //***UUUU this copies all the passes of the block from the input stream to the pass buffer 
				if(!pass1->buffer) return -1;    // error resilient, do nothing , return
				BMI_ASSERT(pass2->cumlen >= length);
				memcpy(pass_buf + length, pass1->buffer, pass2->cumlen - length);
				length = pass2->cumlen;
				passed -= pass1->num_t2layer;
				pass1 = pass2 + 1; //***UUU the first pass in the next layer - has a pointer to the buffer
				pass2 += pass1->num_t2layer;
			}


#if INTEL_ASSEMBLY_64
			MqDecoderInit(coder, pass_buf, length, bypass);
#else
			coder.Init(pass_buf, length, bypass);
#endif
			pass_buf += length;
		}

// DEBUG_PAUSE(si == 1672);
		if (coding_pass_type == 0 )
		{

			if (!bypass)
			{
				// sig_prop
#if ENABLE_ASSEMBLY_OPTIMIZATION 
	#if INTEL_ASSEMBLY_32
				bmi_t1_decode_pass0_asm32(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes );
	#elif INTEL_ASSEMBLY_64
				bmi_t1_decode_pass0_asm64(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes, lut_ctx_sign );
	#elif AT_T_ASSEMBLY
				bmi_t1_decode_pass0_asmATT(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes );
	#else
				assert(0);
	#endif	// 
#else	// c++ code

				bmi_t1_decode_pass0(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes );
#endif // ENABLE_ASSEMBLY_OPTIMIZATION
			}
			else
			{
#pragma	BMI_PRAGMA_TODO_MSG("implement pass0 raw mode")
				BMI_ASSERT(0);
				// todo : implement pass0 raw mode
			}		
		}
		else if (coding_pass_type == 1)
		{
			if (!bypass)
			{
				// mag_ref
#if ENABLE_ASSEMBLY_OPTIMIZATION
	#if INTEL_ASSEMBLY_32
				bmi_t1_decode_pass1_asm32(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, samples, context_words,num_cols,num_stripes );
	#elif INTEL_ASSEMBLY_64
				bmi_t1_decode_pass1_asm64(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, samples, context_words,num_cols,num_stripes );
	#elif AT_T_ASSEMBLY
				bmi_t1_decode_pass1_asmATT(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, samples, context_words,num_cols,num_stripes );
	#else
				assert(0);
	#endif	// 
#else	// c++ code 
				bmi_t1_decode_pass1(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, samples, context_words,num_cols,num_stripes );
#endif	// ENABLE_ASSEMBLY_OPTIMIZATION
			}
			else
			{
				#pragma	BMI_PRAGMA_TODO_MSG("implement pass1 raw mode")
			BMI_ASSERT(0);
			}
		}
		else	// pass2
		{
			// clean up
#if ENABLE_ASSEMBLY_OPTIMIZATION 
	#if INTEL_ASSEMBLY_32
  			bmi_t1_decode_pass2_asm32(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes );
	#elif INTEL_ASSEMBLY_64
			bmi_t1_decode_pass2_asm64(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes, lut_ctx_sign, bmi_mq_transition_table::env_trans_table, (void *)&bmi_mq_transition_table::env_trans_table[92]);
	#elif AT_T_ASSEMBLY
			bmi_t1_decode_pass2_asmATT(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes );
	#else
			assert(0);
	#endif	
#else	// c++ code 
			bmi_t1_decode_pass2(coder, reinterpret_cast<mq_env_state *>(states), bit_plane, lut_ctx,samples, context_words,num_cols,num_stripes );
#endif
		}
//#if _DEBUG
//		{
//			FILE * fcoder;
//			fopen_s(&fcoder, "c://test//resultasm.txt", "a+");
//			int tail = coder.D & 0xff;
//			fprintf_s(fcoder, "[%6d][%3d/%d]\t%x\t%x\t%x\t%d\t%d\t%d\t\tCCR: %08x\n",si++, coding_pass_type,bypass,coder.A+tail,coder.C+tail,coder.D-tail, coder.t,coder.T,coder.S, CHECK_MEMORY_CCR(samples, num_stripes* num_cols * 4));
//			fclose(fcoder);
//		}
//#endif
		segment_passes--;
	}
	return 0;
}


#endif	//	BMI_BLOCK_DECODER