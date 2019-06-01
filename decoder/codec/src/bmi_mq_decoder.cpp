
#include "bmi_mq_decoder.h"
#include "codec.h"

#if BMI_BLOCK_DECODER

#include "bmi_t1_macro.h"
#include "testclock.h"
#ifndef _FINAL
#define SET_INITED			// mInit = true
#define ASSERT_NOT_INITED	// BMI_ASSERT(!mInit)
#else
#define SET_CHECKOUT_MARK
#define REL_CHECKOUT_MARK
#define ASSERT_CHECKOUT
#define ASSERT_NOT_CHECKOUT
#define SET_INITED
#define ASSERT_NOT_INITED
#endif




/*****************************************************************************/
/* STATIC                  initialize_transition_table                       */
/*****************************************************************************/



mq_env_trans bmi_mq_transition_table::env_trans_table[94];
mq_env_state * bmi_mq_transition_table::sOrigianl_state = NULL;

static void initialize_transition_table()
{

	// refer to page 328-329
	int Sigma_mps[47] =
	{ 1, 2, 3, 4, 5,38, 7, 8, 9,10,11,12,13,29,15,16,17,18,19,20,21,22,23,24,
	25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,45,46};
	int Sigma_lps[47] =
	{ 1, 6, 9,12,29,33, 6,14,14,14,17,18,20,21,14,14,15,16,17,18,19,19,20,21,
	22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,46};

	for (int_32 n=0; n < 94; n++)
	{
		int_32 s, Sigma;

		s = n & 1;
		Sigma = n >> 1;

		// Build MPS transition state first.
		Sigma = Sigma_mps[n>>1];
		INIT_STATE(bmi_mq_transition_table::env_trans_table[n].mps, Sigma, s);
		// Now build the LPS transition state.
		Sigma = Sigma_lps[n>>1];
		if ((mq_decoder_state_table[n>>1] == 0x5601) && (Sigma != 46))
		{
			s ^= 1;
		}
		INIT_STATE(bmi_mq_transition_table::env_trans_table[n].lps, Sigma, s);
	}
}

// void * mq_env_state::mOrigianl_state = NULL;
static void initialize_original_state_template()
{

	BMI_ASSERT(bmi_mq_transition_table::sOrigianl_state == NULL);
	mq_env_state * states = new mq_env_state[18];
	for (int n=1; n < 18; n++)
	{
		// Init to (sigma 0 , sign 0)
		states[n]._p_sign = 0x560100;
		states[n].transition = bmi_mq_transition_table::env_trans_table;
	}
	INIT_STATE(states[STATE_SIG_PROP_START], 4, 0);
	INIT_STATE(states[STATE_RUN_START], 3, 0);
	bmi_mq_transition_table::sOrigianl_state = states;
}

//////////////////////////////////////////////////////////////////////////
//	Use static instance to initialize a mq transition table
//////////////////////////////////////////////////////////////////////////

// static void initialize_transition_table();

static class bmi_mq_decoder_local_init 
{
	// use a static class instance to make sure it will be called once when user load this dll
public: bmi_mq_decoder_local_init()
		{ 
			initialize_transition_table(); 
			initialize_original_state_template();
		}
		~bmi_mq_decoder_local_init()
		{
			SAFE_DELETE_ARRAY(bmi_mq_transition_table::sOrigianl_state);
		}
} A_INSTANCE_USE_FOR_INITIALIZE;

//////////////////////////////////////////////////////////////////////////
//		member functions for mq_env_state
//////////////////////////////////////////////////////////////////////////
#if !USE_FAST_MACROS && !ENABLE_ASSEMBLY_OPTIMIZATION
void mq_env_state::init(int Sigma, int s)
{
	BMI_ASSERT((Sigma >= 0) && (Sigma <= 46) /*&& (s == (s&1))*/);
 	_p_sign = ((mq_decoder_state_table[Sigma])<<8) | s;
	transition = bmi_mq_transition_table::env_trans_table + ((Sigma<<1) + s);
}
#endif

//////////////////////////////////////////////////////////////////////////
//		member functions for mq_decoder
//////////////////////////////////////////////////////////////////////////
#if INTEL_ASSEMBLY_64

void FillLsbs(bmi_mq_decoder & coder)
{
	if (coder.T == 0xff )
	{
		if ( *(coder.buf_next) > 0x8f)	// termination marker
		{
			// fill 1, meanwhile temp is already 0xff
			coder.t = 8;
			++coder.S;
		}
		else
		{
			coder.T = *(coder.buf_next++)<<1;
			coder.t = 7;
		}
	}
	else
	{
		coder.T = *(coder.buf_next++);
		coder.t = 8;
	}
	coder.C += coder.T;
}

void  MqDecoderInit(bmi_mq_decoder &coder, uint_8 *buffer, int segment_length, bool bypass)
{
	ASSERT_NOT_INITED;
	SET_INITED;

	// refer to P334 in <jpeg2000>

	//	buf_start = 
	coder.buf_next = buffer;
	buffer[segment_length] = buffer[segment_length+1] = 0xFF; // Force termination
	if (!bypass)
	{
		coder.S = 0;
		coder.T = 0; coder.C = 0; coder.t = 0;
		FillLsbs(coder);
		coder.C <<= coder.t;
		FillLsbs(coder);
		coder.C <<= 7;
		coder.t -= 7;
	}
	else
	{
		BMI_ASSERT(0);
		// todo : implement raw mode when set bypass
	}

	// optimization
	// Add one more status D, refer to page 446
	// Convert A/C to new optinized A/C
	coder.A = MQCODER_A_MIN;
	if (coder.C>=0)	// C>= A - MQCODER_A_MIN
	{
		coder.D = 0;
	}
	else
	{
		coder.D = coder.C;
		coder.A -= coder.D;
		coder.C = 0;
	}
}


#else
void bmi_mq_decoder::Init(uint_8 *buffer, int segment_length, bool bypass)
{
	ASSERT_NOT_INITED;
	SET_INITED;
	
	// refer to P334 in <jpeg2000>

//	buf_start = 
	buf_next = buffer;
	buffer[segment_length] = buffer[segment_length+1] = 0xFF; // Force termination
	if (!bypass)
	{
		S = 0;
		T = 0; C = 0; t = 0;
		FillLsbs();
		C <<= t;
		FillLsbs();
		C <<= 7;
		t -= 7;
	}
	else
	{
		BMI_ASSERT(0);
#pragma	BMI_PRAGMA_TODO_MSG("implement raw mode when set bypass")
	}

	A = MQCODER_A_MIN;

#if ENABLE_ASSEMBLY_OPTIMIZATION 
	D = 0;
#endif`

	return;
}


void bmi_mq_decoder::FillLsbs()
// refer to P334 in <jpeg2000>
{
	
	if (T == 0xff )
	{
		if ( *(buf_next) > 0x8f)	// termination marker
		{
			// fill 1, meanwhile temp is already 0xff
			t = 8;
			++S;
		}
		else
		{
			T = *(buf_next++)<<1;
			t = 7;
		}
	}
	else
	{
		T = *(buf_next++);
		t = 8;
	}
	C += T;
}

void bmi_mq_decoder::RenormalizeOnce()
// refer to P335 in <jpeg2000>
{
	if (!t)
	{
		FillLsbs();
	}
	A<<=1; 
	C<<=1;
	--t;
}

static int mqcoderCounter = 0;

int bmi_mq_decoder::Decode(mq_env_state &state)
//refer to P446 <jpeg2000>
{


	int ret = state._p_sign & 1;		/* set to mps first*/
	int_32 _p = state._p_sign & 0xffffff00;
	A -= _p;

	if (C < _p) 
	{ /* Lower sub-interval selected */ 
		if (A < _p)
		{ /* Conditional exchange; MPS */
			state = reinterpret_cast<mq_env_trans *>((state).transition)->mps;
		}
		else
		{ /* LPS */
			ret ^= 1;
			state = reinterpret_cast<mq_env_trans *>((state).transition)->lps;
		}
		A = _p;
		do
		{
			RenormalizeOnce();
		} while (A < MQCODER_A_MIN);
	}		
	else
	{ /* Upper sub-interval selected; A < A_min. */
		C -= _p;
		if (A < MQCODER_A_MIN)
		{
			if (A < _p)
			{ /* Conditional exchange; LPS */
				ret ^= 1;
				state = reinterpret_cast<mq_env_trans *>((state).transition)->lps;
			}
			else
			{ /* MPS */
				state = reinterpret_cast<mq_env_trans *>((state).transition)->mps;
			}			
			do
			{
				RenormalizeOnce();
			} while (A < MQCODER_A_MIN);
		}

	}
	return ret;
}

int bmi_mq_decoder::DecodeInit()
{

	int ret = 0;		/* set to mps first*/

	if (C < 0x560100) 
	{ /* Lower sub-interval selected */ 
		if (A >= (0x560100<<1))
		{ /* LPS */
			ret = 1;
		}
		if (!t)
		{
			FillLsbs();
		}
		C<<=1;
		--t;
		A = 0x560100<<1;
	}		
	else
	{ /* Upper sub-interval selected; A < A_min. */
		C -= 0x560100;
		A -= 0x560100;
		if (A < MQCODER_A_MIN)
		{
			if (A < 0x560100)
			{ /* Conditional exchange; LPS */
				ret = 1;
				do
				{
					RenormalizeOnce();
				} while (A < MQCODER_A_MIN);
			}
			else
			{ /* MPS */

				RenormalizeOnce();

			}			
		}
	}

	return (ret);
}

#endif

// int_32	bmi_mq_decoder::RawDecode()
// {
// 	if (!t--)
// 	{
// 		if (T == 0xff )
// 		{
// 			if ( *(buf_next) > 0x8f)	// termination marker
// 			{
// 				t = 7;
// 			}
// 			else
// 			{
// 				T = *(buf_next++);
// 				t = 6;
// 			}
// 		}
// 		else
// 		{
// 			T = *(buf_next++);
// 			t = 7;
// 		}	
// 	}
// 	return (T>>t)&0x01;
// }
// 
#endif	// BMI_BLOCK_DECODER

#if GPU_T1_TESTING
void dec_get_trans_table(const unsigned char ** table, unsigned int& size)
{

	size = 94 * sizeof(mq_env_trans); 
	*table = (unsigned char*) bmi_mq_transition_table::env_trans_table;

}

 /*//double check the copy to make sure that possible errors here don't propagate as cuda errors which are hard to debug
void comp_table(void *tempBuff)
{
	mq_env_trans* tempPtr = (mq_env_trans*) tempBuff;
	for(unsigned int i = 0; i<94; i++)
	{
		BMI_ASSERT((bmi_mq_transition_table::env_trans_table[i].mps._p == tempPtr[i].mps._p));
		BMI_ASSERT((bmi_mq_transition_table::env_trans_table[i].mps.sign == tempPtr[i].mps.sign));
		BMI_ASSERT((bmi_mq_transition_table::env_trans_table[i].mps.transition == tempPtr[i].mps.transition));
		BMI_ASSERT((bmi_mq_transition_table::env_trans_table[i].lps._p == tempPtr[i].lps._p));
		BMI_ASSERT((bmi_mq_transition_table::env_trans_table[i].lps.sign == tempPtr[i].lps.sign));
		BMI_ASSERT((bmi_mq_transition_table::env_trans_table[i].lps.transition == tempPtr[i].lps.transition));

	}
}  */
#endif //GPU_T1_TESTING