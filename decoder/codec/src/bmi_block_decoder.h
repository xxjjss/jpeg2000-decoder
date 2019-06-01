#pragma once
#include "platform.h"
#include "bmi_jp2dec.h"

#if BMI_BLOCK_DECODER
#include "bmi_mq_decoder.h"
#include "dec_stru.h"

//////////////////////////////////////////////////////////////////////////
// T1 functions declare
//////////////////////////////////////////////////////////////////////////

/***********************************************************************************
Function Name:	bmi_t1_decode_pass0
Description:	Implement the T1 pass 0
IN:
coder: MQ decoder, the old status from last pass
States[]	: MQ statte
p: bit-plane
OUT:

***********************************************************************************/
void bmi_t1_decode_pass0(bmi_mq_decoder & coder, mq_env_state states[],
						 int p, unsigned char * lutContext,int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );
void bmi_t1_decode_pass1(bmi_mq_decoder & coder, mq_env_state states[],
						 int p,								int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );
void bmi_t1_decode_pass2(bmi_mq_decoder & coder, mq_env_state states[],
						 int p, unsigned char * lutContext,int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );

#if ENABLE_ASSEMBLY_OPTIMIZATION 

#if INTEL_ASSEMBLY_32
	void bmi_t1_decode_pass0_asm32(bmi_mq_decoder & coder, mq_env_state states[],
		int p, unsigned char * lutContext,int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );
	void bmi_t1_decode_pass1_asm32(bmi_mq_decoder & coder, mq_env_state states[],
		int p,								int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );
	void bmi_t1_decode_pass2_asm32(bmi_mq_decoder & coder, mq_env_state states[],
		int p, unsigned char * lutContext,int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );

#elif INTEL_ASSEMBLY_64
extern "C"
{
	// coder we pass **, because C doesn't support reference directly
	void bmi_t1_decode_pass0_asm64(bmi_mq_decoder &coder, mq_env_state states[],
		int p, unsigned char * lutContext,int *sp,	 int *contextWord, int width, int num_stripes, const unsigned char * lut_ctx_sign_asm64);
	void bmi_t1_decode_pass1_asm64(bmi_mq_decoder &coder, mq_env_state states[],
		int p,								int *sp,	 int *contextWord, int width, int num_stripes );
	void bmi_t1_decode_pass2_asm64(bmi_mq_decoder &coder, mq_env_state states[],
		int p, unsigned char * lutContext,int *sp,	 int *contextWord, int width, int num_stripes, const unsigned char * lut_ctx_sign_asm64, mq_env_trans * tableBase , void * TmpTransition);
};
#elif AT_T_ASSEMBLY
	void bmi_t1_decode_pass0_asmATT(bmi_mq_decoder & coder, mq_env_state states[],
		int p, unsigned char * lutContext,int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );
	void bmi_t1_decode_pass1_asmATT(bmi_mq_decoder & coder, mq_env_state states[],
		int p,								int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );
	void bmi_t1_decode_pass2_asmATT(bmi_mq_decoder & coder, mq_env_state states[],
		int p, unsigned char * lutContext,int_32 *sp,	 int_32 *contextWord, int width, int num_stripes );

#endif
#endif


int	decode_code_block(	bool bypass , int_32 * samples, int_32 * context_words, uint_8 * pass_buf,
					  int numBps, stru_code_block *cur_cb, 	unsigned char *lut_ctx	);


#endif	// BMI_BLOCK_DECODER