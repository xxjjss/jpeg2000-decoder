#ifndef	_ASM_HEADER_H_
#define _ASM_HEADER_H_

#if !INTEL_ASSEMBLY_64
#include "platform.h"
#endif


#if INTEL_ASSEMBLY_64

struct	bmi_mq_decoder
{
	int A; 
	int C; 
	int t;
	int T;
	int S; // Number of synthesized FF's

	unsigned char *buf_next;

};
#else

#endif

struct mq_env_state
{

	int	_p_sign;
	void * transition;
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


#endif // _ASM_HEADER_H_
