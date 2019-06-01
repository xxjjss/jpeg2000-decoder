#pragma once

#include "platform.h"
#include "bmi_jp2dec.h"

#if BMI_BLOCK_DECODER

#include "debug.h"
 
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif


#if USE_FAST_MACROS
// check out the coder's member and use local variable to increase the performance
// In assembly level coder.A will use the class pointer to index the variable
// Use local variable system can get the variable faster

#define		CHECK_OUT_CODER(coder)		\
register int_32 reg_A, reg_C, reg_t, reg_T, reg_S;	\
register unsigned char * reg_buf_next = (coder).buf_next;	\
	reg_A = (coder).A;					\
	reg_C = (coder).C;					\
	reg_t = (coder).t;					\
	reg_T = (coder).T;					\
	reg_S = (coder).S;					

#define		CHECK_IN_CODER(coder)		\
	(coder).A = reg_A;					\
	(coder).C = reg_C;					\
	(coder).t = reg_t;					\
	(coder).T = reg_T;					\
	(coder).S = reg_S;					\
	(coder).buf_next = reg_buf_next;

#define		CODER_VAL(coder,x)		reg_##x

#else
#define		CHECK_OUT_CODER(coder)
#define		CHECK_IN_CODER(coder)
#define		CODER_VAL(coder, x)		((coder).x)
#endif



//////////////////////////////////////////////////////////////////////////
//	fast macro for mq-decoder
//////////////////////////////////////////////////////////////////////////

#if USE_FAST_MACROS
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

#define MQ_DECODE(coder, state, ret)					\
{														\
	ret = (state)._p_sign & 1;		/* set to mps first*/	\
	int_32 _p = (state)._p_sign & 0xffffff00;		\
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


#define MQ_DECODE_START(coder, ret)		\
{		\
	ret = 0;		/* set to mps first*/	\
	\
	if (CODER_VAL(coder,C) < 0x560100)	\
	{ /* Lower sub-interval selected */		\
		if (CODER_VAL(coder,A) >= (0x560100<<1))	\
		{ 	\
			ret = 1;	\
		}	\
		if (!CODER_VAL(coder,t))	\
		{	\
			FILL_LSBS(coder);	\
		}	\
		CODER_VAL(coder,C)<<=1;	\
		--CODER_VAL(coder,t);	\
		CODER_VAL(coder,A) = 0x560100<<1 ;	\
	}		\
	else	\
	{ /* Upper sub-interval selected; A < A_min. */	\
		CODER_VAL(coder,A) -= 0x560100;	\
		CODER_VAL(coder,C) -= 0x560100;	\
		if (CODER_VAL(coder,A) < MQCODER_A_MIN)	\
		{	\
			if (CODER_VAL(coder,A) < 0x560100)	\
			{ /* Conditional exchange; LPS */	\
				ret = 1;		\
				do	\
				{	\
					RENORMALIZE_ONCE(coder);	\
				} while (CODER_VAL(coder,A) < MQCODER_A_MIN);	\
			}	\
			else	\
			{ /* MPS */	\
				RENORMALIZE_ONCE(coder);	\
			}				\
		}	\
	}	\
}

#define INIT_STATE(state, Sigma, s)			\
{												\
	(state)._p_sign = (mq_decoder_state_table[(Sigma)] << 8 ) | s;	\
	(state).transition = bmi_mq_transition_table::env_trans_table + (((Sigma)<<1) + (s));		\
}

#else
#define INIT_STATE(state, Sigma, sign)			(state).init((Sigma), (sign))
#define FILL_LSBS(x)						FillLsbs()
#define RENORMALIZE_ONCE(x)					RenormalizeOnce()
#define MQ_DECODE(coder, state, ret)		ret = (coder).Decode(state)
#define MQ_DECODE_START(coder, ret)			ret = (coder).DecodeInit()

#define MQ_DECODE_CDP(coder, state, ret)		ret = (coder).DecodeCDP(state)
#endif

#if MQDECODER_WITH_D
#define MQCODER_A_MIN 0x8000 // ((int_32)(1<<15)) Lower bound for A /*register*/.
#else
#define MQCODER_A_MIN 0x800000 // ((int_32)(1<<15)) Lower bound for A , left shift 8 bits
#endif
//////////////////////////////////////////////////////////////////////////

const int_16 mq_decoder_state_table[47] = {
	0x5601, 0x3401, 0x1801, 0x0AC1, 0x0521, 0x0221,
	0x5601, 0x5401, 0x4801, 0x3801, 0x3001, 0x2401, 
	0x1C01, 0x1601, 0x5601, 0x5401, 0x5101, 0x4801, 
	0x3801, 0x3401, 0x3001, 0x2801, 0x2401, 0x2201, 
	0x1C01, 0x1801, 0x1601, 0x1401, 0x1201, 0x1101,
	0x0AC1, 0x09C1, 0x08A1, 0x0521, 0x0441, 0x02A1, 
	0x0221, 0x0141, 0x0111, 0x0085, 0x0049, 0x0025, 
	0x0015, 0x0009, 0x0005, 0x0001, 0x5601 
};



#if ENABLE_ASSEMBLY_OPTIMIZATION 
#include "asm_header.h"
#else
struct mq_env_state
{

	int_32 _p_sign;
//	int_32	sign;

	void * transition;
#if !USE_FAST_MACROS
	void init(int Sigma, int s);
#endif

};

struct mq_env_trans
{
	mq_env_state mps;
	mq_env_state lps;
};

class bmi_mq_transition_table
{
public:
	static mq_env_trans env_trans_table[94]; // See defn of `mqd_transition'
	static mq_env_state * sOrigianl_state;
};
#endif

#if INTEL_ASSEMBLY_64
#include "asm_header.h"
void MqDecoderInit(bmi_mq_decoder & coder, unsigned char *buffer, int length, bool bypass);
void FillLsbs(bmi_mq_decoder & coder);
#else
struct	bmi_mq_decoder
{

	int_32 A; 
	int_32 C;

	uint_32 t;
	int_32 T;

#if ENABLE_ASSEMBLY_OPTIMIZATION 
	int_32 D; 
#endif

	unsigned char *buf_next;
	int_32 S; // Number of synthesized FF's
	
	void Init(unsigned char *buffer, int length, bool bypass);

	int_32	Decode(mq_env_state &state);
	int DecodeInit();

// 	int_32	RawDecode();


private :
	void FillLsbs();
	void RenormalizeOnce();

};
#endif


#endif	//	BMI_BLOCK_DECODER