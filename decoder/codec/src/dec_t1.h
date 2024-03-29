
#ifndef	_BMI_DEC_T1_H_
#define _BMI_DEC_T1_H_
// bit context table index bits 7 to 0, neighbours:
// 7 6 5
// 4 x 3
// 2 1 0
#include "bmi_jp2dec.h"
#if SOFTWARE_MULTI_THREAD
static unsigned char lut_ctx_lllh[512] = {
	0, 1, 3, 3, 1, 2, 3, 3, 5, 6, 7, 7, 6, 6, 7, 7,
	0, 1, 3, 3, 1, 2, 3, 3, 5, 6, 7, 7, 6, 6, 7, 7,
	5, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	5, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	1, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	1, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	1, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	1, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	2, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	2, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8
};

static unsigned char lut_ctx_hl[512] = {
	0, 1, 5, 6, 1, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	0, 1, 5, 6, 1, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	1, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	1, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	5, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	5, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	1, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	1, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	2, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	2, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8
};

static unsigned char lut_ctx_hh[512] = {
	0, 3, 1, 4, 3, 6, 4, 7, 1, 4, 2, 5, 4, 7, 5, 7,
	0, 3, 1, 4, 3, 6, 4, 7, 1, 4, 2, 5, 4, 7, 5, 7,
	1, 4, 2, 5, 4, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	1, 4, 2, 5, 4, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	3, 6, 4, 7, 6, 8, 7, 8, 4, 7, 5, 7, 7, 8, 7, 8,
	3, 6, 4, 7, 6, 8, 7, 8, 4, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	1, 4, 2, 5, 4, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	1, 4, 2, 5, 4, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	2, 5, 2, 5, 5, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	2, 5, 2, 5, 5, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	5, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	5, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	3, 6, 4, 7, 6, 8, 7, 8, 4, 7, 5, 7, 7, 8, 7, 8,
	3, 6, 4, 7, 6, 8, 7, 8, 4, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	6, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	6, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	5, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	5, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8
};
#else
static unsigned char lut_ctx_lllh[256] = {
	0, 1, 3, 3, 1, 2, 3, 3, 5, 6, 7, 7, 6, 6, 7, 7,
	5, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	1, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	1, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	2, 2, 3, 3, 2, 2, 3, 3, 6, 6, 7, 7, 6, 6, 7, 7,
	6, 6, 7, 7, 6, 6, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
	3, 3, 4, 4, 3, 3, 4, 4, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8
};

static unsigned char lut_ctx_hl[256] = {
	0, 1, 5, 6, 1, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	1, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	5, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	1, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	2, 2, 6, 6, 2, 2, 6, 6, 3, 3, 7, 7, 3, 3, 7, 7,
	3, 3, 7, 7, 3, 3, 7, 7, 4, 4, 7, 7, 4, 4, 7, 7,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	6, 6, 8, 8, 6, 6, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8,
	7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8, 7, 7, 8, 8
};

static unsigned char lut_ctx_hh[256] = {
	0, 3, 1, 4, 3, 6, 4, 7, 1, 4, 2, 5, 4, 7, 5, 7,
	1, 4, 2, 5, 4, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	3, 6, 4, 7, 6, 8, 7, 8, 4, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	1, 4, 2, 5, 4, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	2, 5, 2, 5, 5, 7, 5, 7, 2, 5, 2, 5, 5, 7, 5, 7,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	5, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	3, 6, 4, 7, 6, 8, 7, 8, 4, 7, 5, 7, 7, 8, 7, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	6, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	4, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	5, 7, 5, 7, 7, 8, 7, 8, 5, 7, 5, 7, 7, 8, 7, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8,
	7, 8, 7, 8, 8, 8, 8, 8, 7, 8, 7, 8, 8, 8, 8, 8
};
#endif


// sign/xor context table index bits 7 to 0, neighbours:
// v0(sign, sig), h0(sign, sig), v1(sig, sign), h1(sig, sign)
//    v0         76
// h0 xx h1   54 xx 32
//    v1         10
const unsigned char lut_ctx_sign[256] = {
#if SOFTWARE_MULTI_THREAD

	 0, 2, 0, 3, 6, 8, 6, 4, 0, 2, 0, 3, 7, 5, 7, 9,  
	 6, 8, 6, 4, 6, 8, 6, 4, 6, 8, 6, 4, 0, 2, 0, 3,  
	 0, 2, 0, 3, 6, 8, 6, 4, 0, 2, 0, 3, 7, 5, 7, 9,  
	 7, 5, 7, 9, 0, 2, 0, 3, 7, 5, 7, 9, 7, 5, 7, 9,  
	 2, 2, 2, 0, 8, 8, 8, 6, 2, 2, 2, 0, 5, 5, 5, 7,  
	 8, 8, 8, 6, 8, 8, 8, 6, 8, 8, 8, 6, 2, 2, 2, 0,  
	 2, 2, 2, 0, 8, 8, 8, 6, 2, 2, 2, 0, 5, 5, 5, 7,  
	 5, 5, 5, 7, 2, 2, 2, 0, 5, 5, 5, 7, 5, 5, 5, 7,  
	 0, 2, 0, 3, 6, 8, 6, 4, 0, 2, 0, 3, 7, 5, 7, 9,  
	 6, 8, 6, 4, 6, 8, 6, 4, 6, 8, 6, 4, 0, 2, 0, 3,  
	 0, 2, 0, 3, 6, 8, 6, 4, 0, 2, 0, 3, 7, 5, 7, 9,  
	 7, 5, 7, 9, 0, 2, 0, 3, 7, 5, 7, 9, 7, 5, 7, 9,  
	 3, 0, 3, 3, 4, 6, 4, 4, 3, 0, 3, 3, 9, 7, 9, 9,  
	 4, 6, 4, 4, 4, 6, 4, 4, 4, 6, 4, 4, 3, 0, 3, 3,  
	 3, 0, 3, 3, 4, 6, 4, 4, 3, 0, 3, 3, 9, 7, 9, 9,  
	 9, 7, 9, 9, 3, 0, 3, 3, 9, 7, 9, 9, 9, 7, 9, 9

#else
	9,  9, 10, 10,  9,  9, 10, 10, 12, 12, 13, 11, 12, 12, 11, 13,
	12, 12, 13, 11, 12, 12, 13, 11, 12, 12, 13, 11,  9,  9, 10, 10,
	9,  9, 10, 10,  9,  9, 10, 10, 12, 12, 13, 11, 12, 12, 11, 13,
	12, 12, 11, 13, 12, 12, 11, 13,  9,  9, 10, 10, 12, 12, 11, 13,
	10, 10, 10,  9, 10, 10, 10,  9, 13, 13, 13, 12, 11, 11, 11, 12,
	13, 13, 13, 12, 13, 13, 13, 12, 13, 13, 13, 12, 10, 10, 10,  9,
	10, 10, 10,  9, 10, 10, 10,  9, 13, 13, 13, 12, 11, 11, 11, 12,
	11, 11, 11, 12, 11, 11, 11, 12, 10, 10, 10,  9, 11, 11, 11, 12,
	9,  9, 10, 10,  9,  9, 10, 10, 12, 12, 13, 11, 12, 12, 11, 13,
	12, 12, 13, 11, 12, 12, 13, 11, 12, 12, 13, 11,  9,  9, 10, 10,
	9,  9, 10, 10,  9,  9, 10, 10, 12, 12, 13, 11, 12, 12, 11, 13,
	12, 12, 11, 13, 12, 12, 11, 13,  9,  9, 10, 10, 12, 12, 11, 13,
	10, 10,  9, 10, 10, 10,  9, 10, 11, 11, 12, 11, 13, 13, 12, 13,
	11, 11, 12, 11, 11, 11, 12, 11, 11, 11, 12, 11, 10, 10,  9, 10,
	10, 10,  9, 10, 10, 10,  9, 10, 11, 11, 12, 11, 13, 13, 12, 13,
	13, 13, 12, 13, 13, 13, 12, 13, 10, 10,  9, 10, 13, 13, 12, 13
#endif
};

const unsigned char lut_ctx_xor[256] = {
	0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1,
	1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1,
	1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1
};

#endif	// _BMI_DEC_T1_H_