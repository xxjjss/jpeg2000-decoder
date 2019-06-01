/*******************************************************************
 *
 * Copyright (C) 2009 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 *******************************************************************/

// bmi_dwt_gpu_kernel.c
// - these are the device routines included in bmi_dwt_gpu.cu
// - assume all the needed includes are in bmi_dwt_gpu.cu
#if _WINDOWS
#include "..\include\gpu_setting.h"
#else
#include "gpu_setting.h"
#endif
//////////////////////////////////////////////////////////////////////
//
// shared storage declarations
//
//////////////////////////////////////////////////////////////////////

#ifndef BMI_INT_64
#if _MAC_OS || _LINUX
	typedef		long BMI_INT_64;
#else
	typedef		_int64	  BMI_INT_64;
#endif
#endif

#if __DEVICE_EMULATION__
#else
#endif



texture<int, 1, cudaReadModeElementType>	tc_tex_int;
// texture<short, 1, cudaReadModeElementType>	tc_tex_short;
texture<float, 1, cudaReadModeElementType>	tc_tex_float;


// these typedefs enable the templates below
typedef int		int_32;
typedef short	int_16;
typedef int		r53;
typedef float	i97;


extern __shared__ char SharedMemory[];

//////////////////////////////////////////////////////////////////////
//
// load/store templates
//
//////////////////////////////////////////////////////////////////////

// texture fetch prototype
template<class T> __device__ static T bmi_tex_fetch(int coord);

// int specialization
template<> __device__ static int
bmi_tex_fetch<int_32>(int coord) { return tex1Dfetch(tc_tex_int, coord); }

// float specialization
template<> __device__ static float
bmi_tex_fetch<float>(int coord) { return tex1Dfetch(tc_tex_float, coord); }



template<class T1, class T2>
__device__ static void
bmi_idwt_load(T1 *shared_row, int in_offset, const int pitch, const int len_lo,
			  const T1 lo_gain, const T1 hi_gain)
{
	T1 *low_even;
	T1 *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	int blk_off_x = IDWT_TILE_WIDTH * blockIdx.x + in_offset;
	int blk_off_y = IDWT_TILE_WIDTH * blockIdx.y;
	int m_base = (blk_off_y + threadIdx.y) * pitch;

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) {
		T1 * sample1, * sample2;
		int tmp_sample1, tmp_sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;
		int address = m_base + blk_off_x + idx;
		int mask, right_shift;
		if (sizeof(T2) == 2)	// short
		{
			if (threadIdx.x & 0x01)
			{
				mask = 0xffff0000;		// higher 16 bits
				right_shift = 16;
			}
			else				
			{
				mask = 0xffff;			// lower 16 bits
				right_shift = 0;
			}
			address >>= 1;
		}
		tmp_sample1 = bmi_tex_fetch<int>(address);
		if (sizeof(T2) == 2)	// short
		{
			tmp_sample1 = (short)((tmp_sample1 & mask)>>right_shift);	// only take the lower short
			address = m_base + blk_off_x + idx + len_lo;
			if (threadIdx.x & 0x01)
			{
				mask = 0xffff0000;		// higher 16 bits
				right_shift = 16;
			}
			else				
			{
				mask = 0xffff;			// lower 16 bits
				right_shift = 0;
			}
			address >>= 1;
		}
		else
		{
			address += len_lo;
		}
		tmp_sample2 = bmi_tex_fetch<int>(address);
		if (sizeof(T2) == 2)	// short
		{
			tmp_sample2 = (short)((tmp_sample2 & mask)>>right_shift);	// only take the lower short
		}

		sample1 = (T1 *)(& tmp_sample1);
		sample2 = (T1 *)(& tmp_sample2);

		low_even[idx] = *sample1 * lo_gain;
		high_odd[idx] = *sample2 * hi_gain;

	}
}

#if GPU_W9X7_FLOAT
__device__ static void
bmi_idwt_load_i97_float(float *shared_row, int in_offset, const int pitch, const int len_lo,
						const float coef1, const float coef2, int isFloat1, int isFloat2)
{
	union textureIn
	{
		int iVal;
		float fVal;
	} readtexture;
	float *low_even;
	float *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	int blk_off_x = IDWT_TILE_WIDTH * blockIdx.x + in_offset;
	int blk_off_y = IDWT_TILE_WIDTH * blockIdx.y;
	int m_base = (blk_off_y + threadIdx.y) * pitch;

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) {
		// 		float sample1, sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;
		if (isFloat1)
		{
			low_even[idx] = bmi_tex_fetch<float>(m_base + blk_off_x + idx) * coef1;
		}
		else
		{
			readtexture.fVal = bmi_tex_fetch<float>(m_base + blk_off_x + idx);
			readtexture.iVal = (readtexture.iVal & 0x7fffffff )* ((readtexture.iVal>>31) * 2 + 1);
			low_even[idx] = readtexture.iVal * coef1;
		}
		if (isFloat2)
		{
			high_odd[idx]  =  bmi_tex_fetch<float>(m_base + blk_off_x + idx + len_lo) * coef2;
		}
		else
		{
			readtexture.fVal = bmi_tex_fetch<float>(m_base + blk_off_x + idx + len_lo);
			readtexture.iVal = (readtexture.iVal & 0x7fffffff )* ((readtexture.iVal>>31) * 2 + 1);
			high_odd[idx] = readtexture.iVal * coef2;
		}
	}
}

__device__ static
void bmi_idwt_store_float(float *out, int pitch, float *shared_row,
					const int width, const int height, const int par)
{
	float *low_even;
	float *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	// read out the shared memory odd/even results; swap x/y to transpose
	float sample1, sample2;
	if (par ^ (threadIdx.y & 0x1)) {
		sample1 = high_odd[threadIdx.y >> 1];
		sample2 = high_odd[(IDWT_TILE_WIDTH + threadIdx.y) >> 1];
	} else {
		sample1 = low_even[threadIdx.y >> 1];
		sample2 = low_even[(IDWT_TILE_WIDTH + threadIdx.y) >> 1];
	}

	// do the writes; double the y dimension because other blocks are writing
	// out 2x samples
	int m_base, idx_y;
	int blk_off_x = IDWT_TILE_WIDTH * blockIdx.y;
	int blk_off_y = (IDWT_TILE_WIDTH << 1) * blockIdx.x;
	int idx_x     = blk_off_x + threadIdx.x;

	idx_y  = blk_off_y + threadIdx.y;
	m_base = idx_y * pitch;
	if ((idx_x < height) && (idx_y < width)) {
			out[m_base + idx_x] = sample1;
	}

	idx_y  += IDWT_TILE_WIDTH;
	m_base += IDWT_TILE_WIDTH * pitch;
	if ((idx_x < height) && (idx_y < width)) {
			out[m_base + idx_x] = sample2;
	}
}

__device__ static void
bmi_idwt_lift_step_even_float(float *shared_row, const int len, const int par, const float scale)
{
	float  *low_even;
	float  *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) 
	{
		float  sample1, sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;

		// set up some line flags
		bool start = ((IDWT_TILE_WIDTH * blockIdx.x + idx) <= 0);
		bool end   = ((IDWT_TILE_WIDTH * blockIdx.x + idx) >= (BMI_CEILDIV2(len)-1));

		// build up the even output sample
		if (!par) {
			// even parity
			// first sample
			if (start) {
				// mirror
				sample1 = high_odd[idx];
			} else {
				sample1 = high_odd[idx-1];
			}
			// second sample
			if (end && (len & 0x1)) {
				// mirror
				sample2 = sample1;
			} else {
				sample2 = high_odd[idx];
			}
		} else {
			// odd parity
			// first sample
			sample1 = high_odd[idx];
			// second sample
			if (end) {
				// mirror
				sample2 = sample1;
			} else {
				sample2 = high_odd[idx+1];
			}
		}

		low_even[idx] -= (sample1 + sample2) * scale;
	}
}


__device__ static void
bmi_idwt_lift_step_odd_float(float *shared_row, const int len, const int par, const float scale)
{
	float *low_even;
	float *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) {
		float sample1, sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;

		// set up some line flags
		bool start = ((IDWT_TILE_WIDTH * blockIdx.x + idx) <= 0);
		bool end   = ((IDWT_TILE_WIDTH * blockIdx.x + idx) >= (BMI_CEILDIV2(len)-1));

		// again, build the sample up piece by piece.
		if (!par) {
			// even parity
			sample1 = low_even[idx];
			// second sample
			if (end) {
				// mirror
				sample2 = low_even[idx];
			} else {
				sample2 = low_even[idx+1];
			}
		} else {
			// odd parity
			// first sample
			if (start) {
				// mirror
				sample1 = low_even[idx];
			} else {
				sample1 = low_even[idx-1];
			}
			// second sample
			if (end && (len & 0x1)) {
				// mirror
				sample2 = sample1;
			} else {
				sample2 = low_even[idx];
			}
		}

		high_odd[idx] -= (sample1 + sample2) * scale;
	}
}


#endif

//////////////////////////////////////////////////////////////////////
//
// lifting templates
//
//////////////////////////////////////////////////////////////////////


// even lifting function prototype
template<class T> __device__ static void
bmi_idwt_lift_even(T *result, T s1, T s2, T scale);
// specializations
template<> __device__ static void
bmi_idwt_lift_even<int_32>(int *result, int s1, int s2, int scale)
{
	*result -= ((s1 + s2 + 2) >> 2);
}

template<> __device__ static void
bmi_idwt_lift_even<i97>(float *result, float s1, float s2, float scale)
{
	*result -= (s1 + s2) * scale;
}

// odd lifting function prototype
template<class T> __device__ static void
bmi_idwt_lift_odd(T *result, T s1, T s2, T scale);
// specializations
template<> __device__ static void
bmi_idwt_lift_odd<r53>(int *result, int s1, int s2, int scale)
{
	*result += ((s1 + s2) >> 1);
}
template<> __device__ static void
bmi_idwt_lift_odd<i97>(float *result, float s1, float s2, float scale)
{
	*result -= (s1 + s2) * scale;
}


__device__ static void
bmi_idwt_load_i97(float *shared_row, int in_offset, const int pitch, const int len_lo,
				  const float lo_gain, const float hi_gain)
{
	float *low_even;
	float *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	int blk_off_x = IDWT_TILE_WIDTH * blockIdx.x + in_offset;
	int blk_off_y = IDWT_TILE_WIDTH * blockIdx.y;
	int m_base = (blk_off_y + threadIdx.y) * pitch;

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) {
		float sample1, sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;
		sample1 = (float) bmi_tex_fetch<int>(m_base + blk_off_x + idx);
		sample2 = (float) bmi_tex_fetch<int>(m_base + blk_off_x + idx + len_lo);
		low_even[idx] = sample1 * lo_gain;
		high_odd[idx] = sample2 * hi_gain;
	}
}


template<class T1, class T2>
__device__ static
void bmi_idwt_store(T1 *out, int pitch, T2 *shared_row,
					const int width, const int height, const int par,
					const T2 lo_gain, const T2 hi_gain)
{
	T2 *low_even;
	T2 *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	// read out the shared memory odd/even results; swap x/y to transpose
	T1 sample1, sample2;
	if (par ^ (threadIdx.y & 0x1)) {
		sample1 = (T1) (high_odd[threadIdx.y >> 1] * hi_gain);
		sample2 = (T1) (high_odd[(IDWT_TILE_WIDTH + threadIdx.y) >> 1] * hi_gain);
	} else {
		sample1 = (T1) (low_even[threadIdx.y >> 1] * lo_gain);
		sample2 = (T1) (low_even[(IDWT_TILE_WIDTH + threadIdx.y) >> 1] * lo_gain);
	}

	// do the writes; double the y dimension because other blocks are writing
	// out 2x samples
	int m_base, idx_y;
	int blk_off_x = IDWT_TILE_WIDTH * blockIdx.y;
	int blk_off_y = (IDWT_TILE_WIDTH << 1) * blockIdx.x;
	int idx_x     = blk_off_x + threadIdx.x;

	idx_y  = blk_off_y + threadIdx.y;
	m_base = idx_y * pitch;
	if ((idx_x < height) && (idx_y < width)) {
		out[m_base + idx_x] = sample1;
	}

	idx_y  += IDWT_TILE_WIDTH;
	m_base += IDWT_TILE_WIDTH * pitch;
	if ((idx_x < height) && (idx_y < width)) {
		out[m_base + idx_x] = sample2;
	}
}

//
// step across row segment for the even samples
//
template<class T>
__device__ static void
bmi_idwt_lift_step_even(T *shared_row, const int len, const int par, const T scale)
{
	T *low_even;
	T *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) 
	{
		T sample1, sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;

		// set up some line flags
		bool start = ((IDWT_TILE_WIDTH * blockIdx.x + idx) <= 0);
		bool end   = ((IDWT_TILE_WIDTH * blockIdx.x + idx) >= (BMI_CEILDIV2(len)-1));

		// build up the even output sample
		if (!par) {
			// even parity
			// first sample
			if (start) {
				// mirror
				sample1 = high_odd[idx];
			} else {
				sample1 = high_odd[idx-1];
			}
			// second sample
			if (end && (len & 0x1)) {
				// mirror
				sample2 = sample1;
			} else {
				sample2 = high_odd[idx];
			}
		} else {
			// odd parity
			// first sample
			sample1 = high_odd[idx];
			// second sample
			if (end) {
				// mirror
				sample2 = sample1;
			} else {
				sample2 = high_odd[idx+1];
			}
		}
		// lift...!
		bmi_idwt_lift_even<T>(&low_even[idx], sample1, sample2, scale);
	}
}


//
// step across row segment for the odd samples
//
template<class T>
__device__ static void
bmi_idwt_lift_step_odd(T *shared_row, const int len, const int par, const T scale)
{
	T *low_even;
	T *high_odd;
	low_even = &shared_row[IDWT_LO_BASE];
	high_odd = &shared_row[IDWT_HI_BASE];

	for (int step = 0; step < (IDWT_TILE_WIDTH << 1); step += IDWT_TILE_WIDTH) {
		T sample1, sample2;
		int idx = step + threadIdx.x - IDWT_TILE_EXT;

		// set up some line flags
		bool start = ((IDWT_TILE_WIDTH * blockIdx.x + idx) <= 0);
		bool end   = ((IDWT_TILE_WIDTH * blockIdx.x + idx) >= (BMI_CEILDIV2(len)-1));

		// again, build the sample up piece by piece.
		if (!par) {
			// even parity
			sample1 = low_even[idx];
			// second sample
			if (end) {
				// mirror
				sample2 = low_even[idx];
			} else {
				sample2 = low_even[idx+1];
			}
		} else {
			// odd parity
			// first sample
			if (start) {
				// mirror
				sample1 = low_even[idx];
			} else {
				sample1 = low_even[idx-1];
			}
			// second sample
			if (end && (len & 0x1)) {
				// mirror
				sample2 = sample1;
			} else {
				sample2 = low_even[idx];
			}
		}
		// lift...!
		bmi_idwt_lift_odd<T>(&high_odd[idx], sample1, sample2, scale);
	}
}

//////////////////////////////////////////////////////////////////////
//
// reversible 5/3 integer IDWT kernel
//
//////////////////////////////////////////////////////////////////////
__global__ static void 
bmi_idwt_r53_1d_gpu_row_int32(int_32 *out, int in_offset, const int o_pitch, const int i_pitch,
						const int r_len, const int r_len_lo, const int r_par,
						const int c_len)
{
	// the shared memory is laid out in each row like this:
	// [0..7][8  ..  23][24..31][32..39][40  ..  55][56..63][64]
	// lo ext low samp   lo ext  hi ext  high samp   hi ext  pad
	__shared__ r53 idwt_shared[IDWT_TILE_WIDTH][IDWT_TILE_WIDTH*4+1];

	//
	// FIXME:  need to add the case for length of 1
	//
	if (r_len == 1) {
		return;
	}
	

	// load values into the shared memory
	bmi_idwt_load<r53,int_32>(idwt_shared[threadIdx.y], in_offset, i_pitch, r_len_lo, 1, 1);
	// wait for loads to finish
	__syncthreads();

	// first lifting step
	// even parity:  lo[idx] -= ((hi[idx-1] + hi[idx] + 2) >> 2)
	// odd parity:   lo[idx] -= ((hi[idx] + hi[idx+1] + 2) >> 2)
	bmi_idwt_lift_step_even<r53>(idwt_shared[threadIdx.y], r_len, r_par, 1);
	// wait for updates to finish
	__syncthreads();

	// second lifting step
	// even parity:  hi[idx] += ((lo[idx] + lo[idx+1]) >> 1)
	// odd parity:   hi[idx] += ((lo[idx-1] + lo[idx]) >> 1)
	bmi_idwt_lift_step_odd<r53>(idwt_shared[threadIdx.y], r_len, r_par, 1);
	// wait for updates to finish
	__syncthreads();

	// store values to global memory
	bmi_idwt_store<int_32,r53>(out, o_pitch, idwt_shared[threadIdx.x],
		r_len, c_len, r_par, 1, 1);
}

__global__ static void 
bmi_idwt_r53_1d_gpu_row_short16(int_16 *out, int in_offset, const int o_pitch, const int i_pitch,
						const int r_len, const int r_len_lo, const int r_par,
						const int c_len, bool is_horizontal)
{
	// the shared memory is laid out in each row like this:
	// [0..7][8  ..  23][24..31][32..39][40  ..  55][56..63][64]
	// lo ext low samp   lo ext  hi ext  high samp   hi ext  pad
	__shared__ r53 idwt_shared[IDWT_TILE_WIDTH][IDWT_TILE_WIDTH*4+1];

	//
	// FIXME:  need to add the case for length of 1
	//
	if (r_len == 1) {
		return;
	}
	

	if (is_horizontal)
	{
		// Horizontal, load from original data, 16 bits
		// load values into the shared memory
		bmi_idwt_load<r53,int_16>(idwt_shared[threadIdx.y],in_offset,  i_pitch, r_len_lo, 1, 1);
	}
	else
	{
		// Verizontal, load from temp buf, 32 bits
		bmi_idwt_load<r53,int_32>(idwt_shared[threadIdx.y],in_offset,  i_pitch, r_len_lo, 1, 1);
	}
		// wait for loads to finish
	__syncthreads();

	// first lifting step
	// even parity:  lo[idx] -= ((hi[idx-1] + hi[idx] + 2) >> 2)
	// odd parity:   lo[idx] -= ((hi[idx] + hi[idx+1] + 2) >> 2)
	bmi_idwt_lift_step_even<r53>(idwt_shared[threadIdx.y], r_len, r_par, 1);
	// wait for updates to finish
	__syncthreads();

	// second lifting step
	// even parity:  hi[idx] += ((lo[idx] + lo[idx+1]) >> 1)
	// odd parity:   hi[idx] += ((lo[idx-1] + lo[idx]) >> 1)
	bmi_idwt_lift_step_odd<r53>(idwt_shared[threadIdx.y], r_len, r_par, 1);
	// wait for updates to finish
	__syncthreads();

	if (is_horizontal)
	{
		// Horizontal, save to temp buffer, 32 bits
		bmi_idwt_store<int_32,r53>((int *)out, o_pitch, idwt_shared[threadIdx.x],
			r_len, c_len, r_par, 1, 1);
	}
	else
	{
		// Verizontal, save back to original buffer, 16 bits
		bmi_idwt_store<int_16,r53>(out, o_pitch, idwt_shared[threadIdx.x],
			r_len, c_len, r_par, 1, 1);
	}
}

__global__ static void	bmi_i97_dequantize_kernel(
				int * PicData,
				int pitch_pixel,
				int width,
				int abstepsize,
				int x_step	)
{
	int x = threadIdx.x;
	int * data = PicData + threadIdx.y * pitch_pixel + x;

	while (x < width)
	{
		BMI_INT_64 tmp				= (*data & 0x7fe00000)>>21;
		tmp *= (!!(*data & 0x80000000)) * -2 + 1;
		tmp <<= 13;
		tmp *= abstepsize;
		tmp >>= 13;
		*data = (int)tmp;
		
		x+= x_step;
		data += x_step;
	}
}

# if !GPU_W9X7_FLOAT

__global__ static void bmi_i97_dequantize_transfer(
	int * PicData,
	int x_offset, 
	int y_offset,
	int pitch_pixel,
	int width,
	int height,
	int abstepsize,
	int thread_step,
	int shift
	)
{
	int x = x_offset + threadIdx.x;
	int y = blockIdx.x + y_offset;
	int tmp;

	int * data = PicData + y * pitch_pixel + x;
		
	while (x < width + x_offset)
	{
		tmp = (*data & 0x7fffffff)/*>>shift*/;	
		tmp *= (!!(*data & 0x80000000)) * -2 + 1;	// will same as jasper T1 output

		tmp <<= 13;									// Dequantize
		*data = BmiFixMul(tmp, abstepsize);
			
		x+= thread_step;
		data += thread_step;
	}
}
#endif


__global__ static void bmi_dc_shift_kernel(
	int * PicData,
	int * outData, //
	int x_offset, 
	int y_offset,
	int x_offset_out, 
	int y_offset_out,
	int pitch_pixel,
	int pitch_pixel_out,
	int width,
	int thread_step,
	int isFloat,
	int bit_depth
	)
{
	int maxP = (1 << bit_depth) - 1;
	int x = x_offset + threadIdx.x;
	int x_out = x_offset_out + threadIdx.x;
	int * data = PicData + (blockIdx.x + y_offset) * pitch_pixel;
	outData += (blockIdx.x + y_offset_out) * pitch_pixel_out;
	float * dataf = (float *)data;
	int tmp;
		
	while (x < width + x_offset)
	{
		if (isFloat)
		{
			tmp = (int)dataf[x];
		}
		else
		{
			tmp = data[x];	
		}

		tmp += 1 << (bit_depth  - 1);
		tmp = SET_BOUNDARY(0, tmp, maxP);

		outData[x_out] = tmp;
		x += thread_step;
		x_out += thread_step;

	}
}


//////////////////////////////////////////////////////////////////////
//
// irreversible 9/7 floating-point IDWT kernel
//
//////////////////////////////////////////////////////////////////////
#define BLOCK_X_SIZE 32
__global__ static void 
gou_dwt97_1d_transfer(float * inLowBand, float * inHighBand, float * outDwtResult, int inPitch, int outPitch, 
					  float lowCoef, float highCoef, int isLowBandFloat, int isHighBandFloat,
					  int height, int low_w, int high_w , 
					  int parity,
					  int yStep, int xStep, int lineParts)
{

	float * sharedMem = (float *)SharedMemory;
	float * sharedline = &sharedMem[4 + parity];

	int threadId = BLOCK_X_SIZE * threadIdx.y + threadIdx.x;
	int blockId = yStep * blockIdx.y + blockIdx.x;

//	sharedline += 4 + parity;		// pre_fix
	int threadHandleOffset = (threadId - 1) * 2 - parity;
	int fillOffset = threadHandleOffset;
	int filledWidth = low_w + high_w;

	// count pitch in pixels
	inPitch /= 4;
	outPitch /= 4;

	float * lowBand = inLowBand + blockId * inPitch;
	float * highBand = inHighBand + blockId * inPitch;
	float * dwtResult = outDwtResult + blockId;


	if (lineParts == 1 && blockId <= height)
	{

		// shared memory is enough to held one line 

		// loading and do the step 1 and 2

		int lowLoadOffset; 
		int highLoadOffset;

// 		if (threadId == 0)
// 		{
// 			int m = 0;
// 		}
		while (fillOffset < filledWidth + parity + (low_w == high_w ? 5 : 4))
		{
			if (fillOffset < filledWidth)
			{
				lowLoadOffset = (abs(fillOffset) - parity) / 2;
				highLoadOffset = (abs(fillOffset - 1) - parity) / 2;
			}
			else
			{
				lowLoadOffset = low_w - 1 - (fillOffset - filledWidth + 1) / 2;
				highLoadOffset = high_w - 1 - (fillOffset - filledWidth) / 2;
			}
			if (isLowBandFloat)
			{
				sharedline[fillOffset] = lowBand[lowLoadOffset] * lowCoef;
			}
			else
			{
				sharedline[fillOffset] = (((int *)lowBand)[lowLoadOffset] & 0x7fffffff) * (((((int *)lowBand)[lowLoadOffset])>>31) * 2 + 1) * lowCoef;
			}
			if (isHighBandFloat)
			{
				sharedline[fillOffset - 1] = highBand[highLoadOffset] * highCoef;
			}
			else
			{
// 				int a = ((int *)highBand)[highLoadOffset] & 0x7fffffff;
// 				int b = (((int*)highBand)[highLoadOffset]>>31) * 2 + 1;

				sharedline[fillOffset - 1] = (((int *)highBand)[highLoadOffset] & 0x7fffffff) * ((((int*)highBand)[highLoadOffset]>>31) * 2 + 1) * highCoef;
			}
			fillOffset += xStep * 2;
		} ;
		__syncthreads();

		// step 3 : 
		int offset = threadHandleOffset;
		while (offset < filledWidth + parity + (low_w == high_w ? 5 : 4)) 
		{
			sharedline[offset] = sharedline[offset] - IDWT_DELTA * (sharedline[offset - 1] + sharedline[offset + 1]);
			offset += xStep * 2;
		};
		__syncthreads();

		// step 4;
		offset = threadHandleOffset;
		while (offset < filledWidth + parity + (low_w == high_w ? 5 : 4)) 
		{
			sharedline[offset - 1] = sharedline[offset- 1] - IDWT_GAMMA * (sharedline[offset - 2] + sharedline[offset]);
			offset += xStep * 2;
		} ;
		__syncthreads();

		// step 5 and save back;
		offset = threadHandleOffset;
		while (offset < filledWidth + parity + (low_w == high_w ? 5 : 4)) 
		{
			sharedline[offset] = sharedline[offset] - IDWT_BETA * (sharedline[offset - 1] + sharedline[offset + 1]);
			if (offset >= 0 && offset < filledWidth)
			{
				dwtResult[offset * outPitch] = sharedline[offset];
			}
			offset += xStep * 2;
		} ;
		__syncthreads();

		// step 6 and save back;
		offset = threadHandleOffset;
		while (offset < filledWidth + parity + (low_w == high_w ? 5 : 4)) 
		{
			sharedline[offset - 1] = sharedline[offset- 1] - IDWT_ALPHA * (sharedline[offset - 2] + sharedline[offset]);
			if (offset >= 1 && offset <= filledWidth)
			{
				dwtResult[(offset - 1) * outPitch] = sharedline[offset - 1];
			}
			offset += xStep * 2;
		};
// 		__syncthreads();
// 
// 		// save back
// 		dwtResult += blockId;
// 		if (isFinal)
// 		{
// 			int * result = (int *)dwtResult;
// 			offset = threadHandleOffset;
// 			while (offset < filledWidth + parity + (low_w == high_w ? 5 : 4)) 
// 			{
// 				if (offset >= 0 && offset < filledWidth)
// 				{
// 					result[offset * outPitch] = (int)(sharedline[offset] + 0.5);
// 				}
// 				if (offset >=1 && offset <= filledWidth)
// 				{
// 					result[(offset - 1) * outPitch] = (int)(sharedline[offset - 1] + 0.5);
// 				}
// 				offset += xStep * 2;
// 			} ;
// 		}
// 		else
// 		{
// 			offset = threadHandleOffset;
// 			while (offset <= filledWidth && offset >= 0) 
// 			{
// 				if (offset >= 0 && offset < filledWidth)
// 				{
// 					dwtResult[offset * outPitch] = sharedline[offset];
// 				}
// 				if (offset >=1 && offset <= filledWidth)
// 				{
// 					dwtResult[(offset - 1) * outPitch] = sharedline[offset - 1];
// 				}
// 				offset += xStep * 2;
// 			}
// 		}

	}
	else if (lineParts != 1 && blockId < height * lineParts)
	{
		
		// todo : cut a line into parts
	}
	else
	{
		// do nothing
		lineParts = 1;
	}
// 
// 	int threadBandWidth = ((parity ? high_w : low_w) + lineParts - 1) / lineParts;
// 	int threadBandOffset = threadBandWidth * 2 * blockIdx.z;
// 	int filledWidth = ((threadBandOffset + threadBandWidth * 2 + 1 ) >= low_w + high_w) ? (threadBandWidth * 2 + (low_w == high_w ? 0 : 1)) : threadBandWidth * 2;

	// loading

// 
// 	if (threadBandOffset)
// 	{
// 	}
// 	else
// 	{
// 		
// 		int lowLoadOffset; 
// 		int highLoadOffset;
// 
// 		do 
// 		{
// 			if (fillOffset < filledWidth)
// 			{
// 				lowLoadOffset = (abs(fillOffset) - parity) / 2;
// 				highLoadOffset = (abs(fillOffset - 1) - parity) / 2;
// 			}
// 			else
// 			{
// 
// 			}
// 			sharedline[fillOffset] = lowBand[lowLoadOffset];
// 			sharedline[fillOffset - 1] = highBand[highLoadOffset];
// 			fillOffset += xStep * 2;
// 		} while (fillOffset < threadBandWidth + parity + (low_w == high_w ? 5 : 4));
// 	}


	return;
}


#if GPU_W9X7_FLOAT
__global__ static void 
bmi_idwt_i97_1d_gpu_row_float(float *out, int in_offset, const int o_pitch, const int i_pitch,
							  const int r_len, const int r_len_lo, const int r_par,
							  const int c_len, float coef1, float coef2,
							  int isFloat1, int isFloat2)
{
	// the shared memory is laid out in each row like this:
	// [0..7][8  ..  23][24..31][32..39][40  ..  55][56..63][64]
	// lo ext low samp   lo ext  hi ext  high samp   hi ext  pad
	__shared__ float idwt_shared[IDWT_TILE_WIDTH][IDWT_TILE_WIDTH*4+1];

	//
	// FIXME:  need to add the case for length of 1
	//
	if (r_len == 1) {
		return;
	}

	// load values into the shared memory; build in the appropriate scaling
	bmi_idwt_load_i97_float(idwt_shared[threadIdx.y],in_offset, i_pitch, r_len_lo,
		coef1, coef2, isFloat1, isFloat2);
	// wait for loads to finish
	__syncthreads();

	// first lifting step
	bmi_idwt_lift_step_even_float(idwt_shared[threadIdx.y], r_len, r_par, IDWT_DELTA);
	// wait for updates to finish
	__syncthreads();

	// second lifting step
	bmi_idwt_lift_step_odd_float(idwt_shared[threadIdx.y], r_len, r_par, IDWT_GAMMA);
	// wait for updates to finish
	__syncthreads();

	// third lifting step
	bmi_idwt_lift_step_even_float(idwt_shared[threadIdx.y], r_len, r_par, IDWT_BETA);
	// wait for updates to finish
	__syncthreads();

	// fourth lifting step
	bmi_idwt_lift_step_odd_float(idwt_shared[threadIdx.y], r_len, r_par, IDWT_ALPHA);
	// wait for updates to finish
	__syncthreads();

	// store values to global memory
	bmi_idwt_store_float(out, o_pitch, idwt_shared[threadIdx.x],
		r_len, c_len, r_par);

}
#else
__global__ static void 
bmi_idwt_i97_1d_gpu_row(int *out, int in_offset, const int o_pitch, const int i_pitch,
						const int r_len, const int r_len_lo, const int r_par,
						const int c_len)
{
	// the shared memory is laid out in each row like this:
	// [0..7][8  ..  23][24..31][32..39][40  ..  55][56..63][64]
	// lo ext low samp   lo ext  hi ext  high samp   hi ext  pad
	__shared__ float idwt_shared[IDWT_TILE_WIDTH][IDWT_TILE_WIDTH*4+1];

	//
	// FIXME:  need to add the case for length of 1
	//
	if (r_len == 1) {
		return;
	}

	// load values into the shared memory; build in the appropriate scaling
	bmi_idwt_load_i97(idwt_shared[threadIdx.y],in_offset, i_pitch, r_len_lo,
		IDWT_NORM * IDWT_LO_GAIN, IDWT_NORM * IDWT_HI_GAIN);
	// wait for loads to finish
	__syncthreads();

	// first lifting step
	bmi_idwt_lift_step_even<i97>(idwt_shared[threadIdx.y], r_len, r_par, IDWT_DELTA);
	// wait for updates to finish
	__syncthreads();

	// second lifting step
	bmi_idwt_lift_step_odd<i97>(idwt_shared[threadIdx.y], r_len, r_par, IDWT_GAMMA);
	// wait for updates to finish
	__syncthreads();

	// third lifting step
	bmi_idwt_lift_step_even<i97>(idwt_shared[threadIdx.y], r_len, r_par, IDWT_BETA);
	// wait for updates to finish
	__syncthreads();

	// fourth lifting step
	bmi_idwt_lift_step_odd<i97>(idwt_shared[threadIdx.y], r_len, r_par, IDWT_ALPHA);
	// wait for updates to finish
	__syncthreads();

	// store values to global memory
	bmi_idwt_store<int,i97>(out, o_pitch, idwt_shared[threadIdx.x],
		r_len, c_len, r_par, IDWT_RENORM, IDWT_RENORM);

}
#endif
//////////////////////////////////////////////////////////////////////
//
// irreversible 9/7 floating-point IDWT kernel
//
//////////////////////////////////////////////////////////////////////

__global__ static void 
bmi_mct_tile_int32(	
				   int *in_y,
				   int *in_u,
				   int *in_v,
				   int x_off_pixel,
				   int y_off_pixel,
				   int result_x_off_pixels,
				   int result_y_off_pixels,
				   unsigned char * out_pic, 
				   int  in_pitch_pixel,
				   int  out_pitch_pixel,
				   int  in_bit_depth, 
				   int outBitDepth, 
				   int outWordLengthBits,
				   int outCompNum,
				   int  width_in_pixel,
				   int  x_step)
{

	int maxV = (1 << in_bit_depth) - 1;
	int dc_offset = (1 <<(in_bit_depth - 1));
	int offset_pixel = x_off_pixel + y_off_pixel * in_pitch_pixel;
// 	int tmpi;
	int red, green, blue, tmp;


	offset_pixel += blockIdx.x *  in_pitch_pixel  + threadIdx.x;
	in_y += offset_pixel;
	in_u += offset_pixel;
	in_v += offset_pixel;

	offset_pixel = result_x_off_pixels + result_y_off_pixels * out_pitch_pixel;
	offset_pixel += blockIdx.x *  out_pitch_pixel  + threadIdx.x;
	out_pic += offset_pixel * (outCompNum * outWordLengthBits)/8;

	offset_pixel = threadIdx.x;		// offset in current line

	while(offset_pixel < width_in_pixel)
	{

		tmp = *in_y + dc_offset;
		green = tmp - (*in_u  + *in_v)/4;
		red = SET_BOUNDARY(0, (*in_v + green), maxV);
		red = LIFT(red, in_bit_depth, outBitDepth);

		//*in_v += dc_offset;

		blue = SET_BOUNDARY(0, (*in_u + green), maxV);
		blue = LIFT(blue, in_bit_depth, outBitDepth);						// clipping

		//*in_u += dc_offset;
		green = SET_BOUNDARY(0, green, maxV);
		green = LIFT(green, in_bit_depth, outBitDepth);	


		// 
		if (outWordLengthBits == 8)
		{
			unsigned char * out = out_pic;
			*(out++) = blue;
			*(out++) = green;
			*(out++) = red;
			if (outCompNum == 4)
			{
				*(out++) = 0;
			}
		}
		else if (outWordLengthBits == 16)
		{
			uint_16 * out = (uint_16 *)out_pic;
			*(out++) = blue;
			*(out++) = green;
			*(out++) = red;
			if (outCompNum == 4)
			{
				*(out++) = 0;
			}
		}
		else
		{
		}

		in_y += x_step;
		in_u += x_step;
		in_v += x_step;
		out_pic += x_step * (outCompNum * outWordLengthBits)/8;
		offset_pixel += x_step;
	}

}


__global__ static void 
bmi_tile__merge_int32(
					  int * comp0,
					  int * comp1,
					  int * comp2,
					  int * comp3,
					  int x_off_pixel,
					  int y_off_pixel,
					  int result_x_off_pixels,
					  int result_y_off_pixels,
					  unsigned char * out_pic, 
					  int  in_pitch_pixel,
					  int  out_pitch_pixel,
					  int  in_bit_depth, 
					  int outBitDepth, 
					  int outWordLengthBits,
					  int outCompNum,
					  int  width_in_pixel,
					  int  x_step)

{

	int minV = 0, maxV = (1 << in_bit_depth) - 1;
	int offset_pixel = x_off_pixel + y_off_pixel * in_pitch_pixel;
	int tmp;



	offset_pixel += blockIdx.x *  in_pitch_pixel  + threadIdx.x;
	if (comp0) comp0 += offset_pixel;
	if (comp1) comp1 += offset_pixel;
	if (comp2) comp2 += offset_pixel;
	if (comp3) comp3 += offset_pixel;

	offset_pixel = result_x_off_pixels + result_y_off_pixels * out_pitch_pixel;
	offset_pixel += blockIdx.x *  out_pitch_pixel  + threadIdx.x;
	unsigned char * out = out_pic + offset_pixel * (outCompNum * outWordLengthBits)/8;

	offset_pixel = threadIdx.x;		// offset in current line


	while(offset_pixel < width_in_pixel)
	{
		if (outCompNum > 3)
		{
			if (comp3 )
			{
				tmp = *comp3;
				tmp += (1 << (in_bit_depth  - 1));
				tmp = SET_BOUNDARY(minV, tmp, maxV);

				//*comp3 = tmp;
				if (outWordLengthBits == 8)
				{
					tmp = LIFT(tmp, in_bit_depth, 8);
					*out = tmp;
					out++;
				}
				else if (outWordLengthBits == 16)
				{
					tmp = LIFT(tmp, in_bit_depth, 16);
					*((unsigned short *)out) = tmp;
					out += 2;
				}
				else
				{
					// others? 
				}		
				comp3 += x_step;

			}
			else
			{
				if (outWordLengthBits == 8)
				{
					*out = 0;
					out++;
				}
				else if (outWordLengthBits == 16)
				{
					*out = 0;
					out++;
					*out = 0;
					out++;
				}
				else
				{
					// others? 
				}	
			}
		}


		if (comp2)
		{
			tmp = *comp2;
			tmp += (1 << (in_bit_depth))  - 1;
			tmp = SET_BOUNDARY(minV, tmp, 2 * maxV);

			//*comp2 = tmp;
			if (outWordLengthBits == 8)
			{
				tmp = LIFT(tmp, in_bit_depth + 1, 8);
				*out = tmp;
				out++;
			}
			else if (outWordLengthBits == 16)
			{
				tmp = LIFT(tmp, in_bit_depth + 1, 16);
				*((unsigned short *)out) = tmp;
				out += 2;
			}
			else
			{
				// others? 
			}		
			comp2 += x_step;

		}
		else
		{
			if (outWordLengthBits == 8)
			{
				*out = 0;
				out++;
			}
			else if (outWordLengthBits == 16)
			{
				*out = 0;
				out++;
				*out = 0;
				out++;
			}
			else
			{
				// others? 
			}	
		}

		if (comp1)
		{
			tmp = *comp1;
			tmp += (1 << (in_bit_depth))  - 1;
			tmp = SET_BOUNDARY(minV, tmp, 2 * maxV);

			//*comp1 = tmp;
			if (outWordLengthBits == 8)
			{
				tmp = LIFT(tmp, in_bit_depth + 1, 8);
				*out = tmp;
				out++;
			}
			else if (outWordLengthBits == 16)
			{
				tmp = LIFT(tmp, in_bit_depth + 1, 16);
				*((unsigned short *)out) = tmp;
				out += 2;
			}
			else
			{
				// others? 
			}		
			comp1 += x_step;

		}
		else
		{
			if (outWordLengthBits == 8)
			{
				*out = 0;
				out++;
			}
			else if (outWordLengthBits == 16)
			{
				*out = 0;
				out++;
				*out = 0;
				out++;
			}
			else
			{
				// others? 
			}	
		}

		if (comp0)
		{
			tmp = *comp0;
			tmp += (1 << (in_bit_depth  - 1));
			tmp = SET_BOUNDARY(minV, tmp, maxV);

			//*comp0 = tmp;
			if (outWordLengthBits == 8)
			{
				tmp = LIFT(tmp, in_bit_depth, 8);
				*out = tmp;
				out++;
			}
			else if (outWordLengthBits == 16)
			{
				tmp = LIFT(tmp, in_bit_depth, 16);
				*((unsigned short *)out) = tmp;
				out += 2;
			}
			else
			{
				// others? 
			}		

			comp0 += x_step;
		}
		else
		{
			if (outWordLengthBits == 8)
			{
				*out = 0;
				out++;
			}
			else if (outWordLengthBits == 16)
			{
				*out = 0;
				out++;
				*out = 0;
				out++;
			}
			else
			{
				// others? 
			}	

		}

		out += (x_step - 1) * (outCompNum * outWordLengthBits) / 8;
		offset_pixel += x_step;

	}

}

__global__ static void 
bmi_tile__merge_float(
					  float * comp0,
					  float * comp1,
					  float * comp2,
					  float * comp3,
					  int x_off_pixel,
					  int y_off_pixel,
					  int result_x_off_pixels,
					  int result_y_off_pixels,
					  unsigned char * out_pic, 
					  int  in_pitch_pixel,
					  int  out_pitch_pixel,
					  int  in_bit_depth, 
					  int outBitDepth, 
					  int outWordLengthBits,
					  int outCompNum,
					  int  width_in_pixel,
					  int  x_step)

 {
 
	 int minV = 0, maxV = (1 << in_bit_depth) - 1;
	 int offset_pixel = x_off_pixel + y_off_pixel * in_pitch_pixel;
	 int tmp;



	 offset_pixel += blockIdx.x *  in_pitch_pixel  + threadIdx.x;
	 if (comp0) comp0 += offset_pixel;
	 if (comp1) comp1 += offset_pixel;
	 if (comp2) comp2 += offset_pixel;
	 if (comp3) comp3 += offset_pixel;

	 offset_pixel = result_x_off_pixels + result_y_off_pixels * out_pitch_pixel;
	 offset_pixel += blockIdx.x *  out_pitch_pixel  + threadIdx.x;
	 unsigned char * out = out_pic + offset_pixel * (outCompNum * outWordLengthBits)/8;

	 offset_pixel = threadIdx.x;		// offset in current line


	 while(offset_pixel < width_in_pixel)
	 {
		 if (outCompNum > 3)
		 {
			 if (comp3 )
			 {
				 tmp = (int)*comp3;
				 tmp += (1 << (in_bit_depth  - 1));
				 tmp = SET_BOUNDARY(minV, tmp, maxV);

				 //*((int *)comp3) = tmp;
				 if (outWordLengthBits == 8)
				 {
					 tmp = LIFT(tmp, in_bit_depth, 8);
					 *out = tmp;
					 out++;
				 }
				 else if (outWordLengthBits == 16)
				 {
					 tmp = LIFT(tmp, in_bit_depth, 16);
					 *((unsigned short *)out) = tmp;
					 out += 2;
				 }
				 else
				 {
					 // others? 
				 }		
				 comp3 += x_step;

			 }
			 else
			 {
				 if (outWordLengthBits == 8)
				 {
					 *out = 0;
					 out++;
				 }
				 else if (outWordLengthBits == 16)
				 {
					 *out = 0;
					 out++;
					 *out = 0;
					 out++;
				 }
				 else
				 {
					 // others? 
				 }	
			 }
		 }


		 if (comp2)
		 {
			 tmp = (int)*comp2;
			 tmp += (1 << (in_bit_depth  - 1));
			 tmp = SET_BOUNDARY(minV, tmp, maxV);

			 //*((int *)comp2) = tmp;
			 if (outWordLengthBits == 8)
			 {
				 tmp = LIFT(tmp, in_bit_depth, 8);
				 *out = tmp;
				 out++;
			 }
			 else if (outWordLengthBits == 16)
			 {
				 tmp = LIFT(tmp, in_bit_depth, 16);
				 *((unsigned short *)out) = tmp;
				 out += 2;
			 }
			 else
			 {
				 // others? 
			 }		
			 comp2 += x_step;

		 }
		 else
		 {
			 if (outWordLengthBits == 8)
			 {
				 *out = 0;
				 out++;
			 }
			 else if (outWordLengthBits == 16)
			 {
				 *out = 0;
				 out++;
				 *out = 0;
				 out++;
			 }
			 else
			 {
				 // others? 
			 }	
		 }

		 if (comp1)
		 {
			 tmp = (int)*comp1;
			 tmp += (1 << (in_bit_depth  - 1));
			 tmp = SET_BOUNDARY(minV, tmp, maxV);

			 //*((int *)comp1) = tmp;
			 if (outWordLengthBits == 8)
			 {
				 tmp = LIFT(tmp, in_bit_depth, 8);
				 *out = tmp;
				 out++;
			 }
			 else if (outWordLengthBits == 16)
			 {
				 tmp = LIFT(tmp, in_bit_depth, 16);
				 *((unsigned short *)out) = tmp;
				 out += 2;
			 }
			 else
			 {
				 // others? 
			 }		
			 comp1 += x_step;

		 }
		 else
		 {
			 if (outWordLengthBits == 8)
			 {
				 *out = 0;
				 out++;
			 }
			 else if (outWordLengthBits == 16)
			 {
				 *out = 0;
				 out++;
				 *out = 0;
				 out++;
			 }
			 else
			 {
				 // others? 
			 }	
		 }

		 if (comp0)
		 {
			 tmp = (int)*comp0;
			 tmp += (1 << (in_bit_depth  - 1));
			 tmp = SET_BOUNDARY(minV, tmp, maxV);

			 //*((int *)comp0) = tmp;
			 if (outWordLengthBits == 8)
			 {
				 tmp = LIFT(tmp, in_bit_depth, 8);
				 *out = tmp;
				 out++;
			 }
			 else if (outWordLengthBits == 16)
			 {
				 tmp = LIFT(tmp, in_bit_depth, 16);
				 *((unsigned short *)out) = tmp;
				 out += 2;
			 }
			 else
			 {
				 // others? 
			 }		

			 comp0 += x_step;
		 }
		 else
		 {
			 if (outWordLengthBits == 8)
			 {
				 *out = 0;
				 out++;
			 }
			 else if (outWordLengthBits == 16)
			 {
				 *out = 0;
				 out++;
				 *out = 0;
				 out++;
			 }
			 else
			 {
				 // others? 
			 }	

		 }
	
		 out += (x_step - 1) * (outCompNum * outWordLengthBits) / 8;
		 offset_pixel += x_step;

	 }

}

__global__ static void 
bmi_tile_one_component_float(
				int * compBuf,
				int x_off_pixel,
				int y_off_pixel,
				int *out_bmp, 
				int  in_pitch_pixel,
				int  out_pitch_pixel,
				int  bit_depth, 
				int  width_in_pixel,
				int  x_step)
 {
 
	int out, tmp, flag;
	int offset_tile_pixel = (y_off_pixel + blockIdx.x) * in_pitch_pixel;
	int offset_in_line = x_off_pixel + threadIdx.x;		// offset in current line
	int minV = 0, maxV = (1 << bit_depth) - 1;
	
	while(offset_in_line <( width_in_pixel + x_off_pixel))
	{
		tmp = BmiFixMul(BmiDoubleToFix(1.0) , *(compBuf + offset_tile_pixel + offset_in_line));		
		flag = (!!(tmp & 0x80000000)) * (-2) + 1;
		tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// rounding and convert to integer values
		tmp >>= 13;
		tmp += 1 << (bit_depth  - 1);								// perform level shift
		out = (SET_BOUNDARY(minV, tmp, maxV)) >> (bit_depth - 8);						// clipping

		// use this color to generate grey picture
		tmp = out;
		tmp |= out << 8;
		tmp |= out << 16;

		*(out_bmp + offset_tile_pixel + offset_in_line) = tmp;
		offset_in_line += x_step;
	}
}

__global__ static void 
bmi_tile_one_component_float_orig(
				int * compBuf,
				int x_off_pixel,
				int y_off_pixel,
				int  in_pitch_pixel,
				int  out_pitch_pixel,
				int  bit_depth, 
				int  width_in_pixel,
				int  x_step)
 {
 
	int tmp, flag;
	int offset_tile_pixel = (y_off_pixel + blockIdx.x) * in_pitch_pixel;
	int offset_in_line = x_off_pixel + threadIdx.x;		// offset in current line
	int minV = 0, maxV = (1 << bit_depth) - 1;
	int * comp;
	
	while(offset_in_line <( width_in_pixel + x_off_pixel))
	{
		comp = compBuf + offset_tile_pixel + offset_in_line;
		tmp = BmiFixMul(BmiDoubleToFix(1.0) , *comp);	//***UUU I don't think we need this fix mul which just loses accuracy	
		flag = (!!(tmp & 0x80000000)) * (-2) + 1;
		tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// rounding and convert to integer values
		tmp >>= 13;
		tmp += 1 << (bit_depth  - 1);								// perform level shift
		*comp = (SET_BOUNDARY(minV, tmp, maxV));						// clipping

		offset_in_line += x_step;
	}
}


__global__ static void 
bmi_tile_one_component_int(
				int component_id,
				int * compBuf,				
				int x_off_pixel,
				int y_off_pixel,
				int *out_bmp, 
				int  in_pitch_pixel,
				int  out_pitch_pixel,
				int  bit_depth, 
				int  width_in_pixel,
				int  x_step)
 {

 	int out, tmp;
 	int offset_tile_pixel = (y_off_pixel + blockIdx.x) * in_pitch_pixel;
 	int offset_in_line = x_off_pixel + threadIdx.x;		// offset in current line
 // 	int mask =( 1<<bit_depth) - 1;
 	int minV = 0, maxV = (1 << bit_depth) - 1;
 	
 	while(offset_in_line <( width_in_pixel + x_off_pixel))
 	{
 		out = (*(compBuf + offset_tile_pixel + offset_in_line));// >> color_position;
		if( component_id == 0)
		{
			out += 1 << (bit_depth  - 1);	
		}
		else
		{
			out += (1 << bit_depth)  - 1;
			out >>= 1;
		}
		out = SET_BOUNDARY(minV, out , maxV ); //*** UUU This may not even be needed
 //		out &= mask;
 		if (bit_depth >= 8)
 		{
 			out >>= (bit_depth - 8);
 		}
 		else
 		{
 			out <<= (8 - bit_depth);
 		}
 		out &= 0x0ff;
 
 
 		// use this color to generate grey picture
 		tmp = out;
 		tmp |= out << 8;
 		tmp |= out << 16;
 
 		*(out_bmp + offset_tile_pixel + offset_in_line) = tmp;
 		offset_in_line += x_step;
 	}
}

__global__ static void 
bmi_tile_one_component_int_orig(
				int component_id,
				int * compBuf,
				int x_off_pixel,
				int y_off_pixel,
				int  in_pitch_pixel,
				int  out_pitch_pixel,
				int  bit_depth, 
				int  width_in_pixel,
				int  x_step)
 {

 	int out;
 	int offset_tile_pixel = (y_off_pixel + blockIdx.x) * in_pitch_pixel;
 	int offset_in_line = x_off_pixel + threadIdx.x;		// offset in current line
 	int minV = 0, maxV = (1 << bit_depth) - 1;
	int * comp;
 	
 	while(offset_in_line <( width_in_pixel + x_off_pixel))
 	{
 		
		comp = compBuf + offset_tile_pixel + offset_in_line;
		out = *comp;
		if( component_id == 0)
		{
			out += 1 << (bit_depth  - 1);	
			out = SET_BOUNDARY(minV, out , maxV ); 
		}
		else
		{
			out += (1 << bit_depth)  - 1;
			out = SET_BOUNDARY(minV, out , (1 << (bit_depth  + 1) - 1)); 
		}	
 
 		*comp = out;
 		offset_in_line += x_step;
 	}
}
__global__ static void 
bmi_get_one_component_from_result(
				int compId,
				int x_off_pixel,
				int y_off_pixel,
				int *out_bmp, 
				int pitch_pixel,
				int width_in_pixel,
				int x_step)
{
	int out, tmp;
	int offset_tile_pixel = (y_off_pixel + blockIdx.x) * pitch_pixel;
	int offset_in_line = x_off_pixel + threadIdx.x;		// offset in current line
	int shift = (2-compId) * 8;
	
	while(offset_in_line <( width_in_pixel + x_off_pixel))
	{
		out = *(out_bmp + offset_tile_pixel + offset_in_line)>>shift;
		out &= 0x0ff;

		// use this color to generate grey picture
		tmp = out;
		tmp |= out << 8;
		tmp |= out << 16;

		*(out_bmp + offset_tile_pixel + offset_in_line) = tmp;
		offset_in_line += x_step;
	}
}

#if GPU_W9X7_FLOAT 
__global__ static void 
bmi_ict_tile_float(	
				   float *in_y,
				   float *in_u,
				   float *in_v,
				   int x_off_pixel,
				   int y_off_pixel,
				   int result_x_off_pixels,
				   int result_y_off_pixels,
				   unsigned char * out_pic, 
				   int  in_pitch_pixel,
				   int  out_pitch_pixel,
				   int  in_bit_depth, 
				   int outBitDepth, 
				   int outWordLengthBits,
				   int outCompNum,
				   int  width_in_pixel,
				   int  x_step)
{

	int minV = 0, maxV = (1 << in_bit_depth) - 1;
	int offset_pixel = x_off_pixel + y_off_pixel * in_pitch_pixel;
	double tmpf;
	int red, green, blue;



	offset_pixel += blockIdx.x *  in_pitch_pixel  + threadIdx.x;
	in_y += offset_pixel;
	in_u += offset_pixel;
	in_v += offset_pixel;

	offset_pixel = result_x_off_pixels + result_y_off_pixels * out_pitch_pixel;
	offset_pixel += blockIdx.x *  out_pitch_pixel  + threadIdx.x;
	out_pic += offset_pixel * (outCompNum * outWordLengthBits)/8;

	offset_pixel = threadIdx.x;		// offset in current line

	while(offset_pixel < width_in_pixel)
	{
		tmpf = *in_y + 1.402 * (*in_v);		// r
		tmpf += (1 << (in_bit_depth  - 1))/* + 0.5*/;								// perform level shift
		red = (int)(SET_BOUNDARY(minV, tmpf, maxV));						// clipping
		red = LIFT(red, in_bit_depth, outBitDepth);

		tmpf = *in_y + (-0.34413) * (*in_u) + (-0.71414) * (*in_v);		// g
		tmpf += (1 << (in_bit_depth  - 1))/* + 0.5*/;
		green = (int)(SET_BOUNDARY(0, tmpf, maxV));
		green = LIFT(green, in_bit_depth, outBitDepth);	


		tmpf = *in_y + (1.772) * (*in_u);		// b
		tmpf +=( 1 << (in_bit_depth  - 1)) /*+ 0.5*/;
		blue = (int)(SET_BOUNDARY(0, tmpf, maxV));
		blue = LIFT(blue, in_bit_depth, outBitDepth);						// clipping


// 
		if (outWordLengthBits == 8)
		{
			unsigned char * out = out_pic;
			*(out++) = blue;
			*(out++) = green;
			*(out++) = red;
			if (outCompNum == 4)
			{
				*(out++) = 0;
			}
		}
		else if (outWordLengthBits == 16)
		{
			uint_16 * out = (uint_16 *)out_pic;
			*(out++) = blue;
			*(out++) = green;
			*(out++) = red;
			if (outCompNum == 4)
			{
				*(out++) = 0;
			}
		}
		else
		{
		}

		// prepare for getOneComponent
		/*
		red = (int)(*in_y) + (1 << (in_bit_depth  - 1));
		*((int *)in_y) = SET_BOUNDARY(minV, red, maxV);		
		green = (int)(*in_u) + (1 << (in_bit_depth  - 1));
		*((int *)in_u) = SET_BOUNDARY(minV, green, maxV);	
		blue = (int)(*in_v) + (1 << (in_bit_depth  - 1));
		*((int *)in_v) = SET_BOUNDARY(minV, blue, maxV); */

		in_y += x_step;
		in_u += x_step;
		in_v += x_step;
		out_pic += x_step * (outCompNum * outWordLengthBits)/8;
		offset_pixel += x_step;
	}
}
#else

__global__ static void 
bmi_ict_tile_int32(	
				int *in_y,
				int *in_u,
				int *in_v,
				int x_off_pixel,
				int y_off_pixel,
				int *out_bmp, 
				int  in_pitch_pixel,
				int  out_pitch_pixel,
				int  bit_depth, 
				int  width_in_pixel,
				int  x_step)
{

	int out, tmp, flag;
	int minV = 0, maxV = (1 << bit_depth) - 1;
	int offset_pixel = x_off_pixel + y_off_pixel * in_pitch_pixel;
	offset_pixel += blockIdx.x *  in_pitch_pixel  + threadIdx.x;
	in_y += offset_pixel;
	in_u += offset_pixel;
	in_v += offset_pixel;
	out_bmp += offset_pixel;

	offset_pixel = threadIdx.x;		// offset in current line

	while(offset_pixel < width_in_pixel)
	{
		tmp = *in_y + BmiFixMul(BmiDoubleToFix(1.402) , *in_v);		// r
		flag = (!!(tmp & 0x80000000)) * (-2) + 1;
		tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// rounding and convert to integer values
		tmp >>= 13;
		tmp += 1 << (bit_depth  - 1);								// perform level shift
		out = (SET_BOUNDARY(minV, tmp, maxV)) >> (bit_depth - 8);						// clipping
		out <<= 8;

		tmp = *in_y + BmiFixMul(BmiDoubleToFix(-0.34413) , *in_u) + BmiFixMul(BmiDoubleToFix(-0.71414) , *in_v);		// g
		flag = (!!(tmp & 0x80000000)) * (-2) + 1;
		tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// convert fix to int
		tmp >>= 13;
		tmp += 1 << (bit_depth  - 1);
		out += (SET_BOUNDARY(minV, tmp, maxV)) >> (bit_depth - 8);
		out <<= 8;

		tmp = *in_y + BmiFixMul(BmiDoubleToFix(1.772) , *in_u);		// b
		flag = (!!(tmp & 0x80000000)) * (-2) + 1;
		tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// convert fix to int
		tmp >>= 13;
		tmp += 1 << (bit_depth  - 1);
		out += (SET_BOUNDARY(minV, tmp, maxV)) >> (bit_depth - 8);

		*out_bmp = out;

		  in_y += x_step;
		  in_u += x_step;
		  in_v += x_step;
		  out_bmp += x_step;
		  offset_pixel += x_step;
	}
}
#endif 

__global__ static void 
bmi_mct_tile_short16(	
				short *in_r,
				short *in_g,
				short *in_b,
				int *out_bmp, 
				int  in_pitch_pixel,
				int  out_pitch_pixel,
				int  bit_depth, 
				int  width_in_pixel,
				int  x_step)
{

	int g, out;
	int offset_pixel = blockIdx.x *  in_pitch_pixel  + threadIdx.x;
	int dc_offset = 0x01 <<(bit_depth - 1);
	int dc_max = (dc_offset << 1) - 1;
	in_r += offset_pixel;
	in_g += offset_pixel;
	in_b += offset_pixel;
	out_bmp += blockIdx.x *  out_pitch_pixel  + threadIdx.x;
	offset_pixel = threadIdx.x;		// offset in current line

	while(offset_pixel < width_in_pixel)
	{
          g = *in_r - ((*in_g + *in_b) >> 2) + dc_offset;
          out = (unsigned char)SET_BOUNDARY(0, (*in_b + g), dc_max);		// r
		  out <<= bit_depth;
		  out += (unsigned char)SET_BOUNDARY(0, g, dc_max);		// + g
		  out <<= bit_depth;
          out += (unsigned char)SET_BOUNDARY(0, (*in_g + g), dc_max);		// + b
 		  *out_bmp = out;

		  in_r += x_step;
		  in_g += x_step;
		  in_b += x_step;
		  out_bmp += x_step;
		  offset_pixel += x_step;
	}

}
// 
// __global__ static void 
// bmi_dc_shift_tile(
// 				int *in_r,
// 				int *in_g,
// 				int *in_b,
// 				int *out_bmp, 
// 				int  in_pitch_pixel,
// 				int  out_pitch_pixel,
// 				int  bit_depth, 
// 				int  width_in_words,
// 				int  x_step)
// {
// 
// 	int out;
// 	int offset_pixel =  blockIdx.x *  in_pitch_pixel  + threadIdx.x;
// 	int dc_offset = 0x01 <<(bit_depth - 1);
// 	int dc_max = (dc_offset << 1) - 1;
// 	in_r += offset_pixel;
// 	in_g += offset_pixel;
// 	in_b += offset_pixel;
// 	out_bmp += blockIdx.x *  out_pitch_pixel  + threadIdx.x;
// 	offset_pixel = threadIdx.x;		// offset in current line
// 
// 	while(offset_pixel < width_in_words)
// 	{
// 		  out = (unsigned char)SET_BOUNDARY(0, (*in_r + dc_offset), dc_max);		// r
// 		  out <<= bit_depth;
// 		  out += (unsigned char)SET_BOUNDARY(0, (*in_g + dc_offset), dc_max);		// g
// 		  out <<= bit_depth;
// 		  out += (unsigned char)SET_BOUNDARY(0, (*in_b + dc_offset), dc_max);		// b
// 		  *out_bmp = out;
// 
// 		  in_r += x_step;
// 		  in_g += x_step;
// 		  in_b += x_step;
// 		  out_bmp += x_step;
// 		  offset_pixel += x_step;
// 	}
// 
// }
// 
// 
// #define XPOSE_TILE_WIDTH	16
// __global__ static void
// bmi_image_transpose(int *out, int o_pitch, int *in, int i_pitch,
// 					int width, int height) {
// 
// 	// setup shared memory
// 	// - 16 by 16 matrix padded at the end to avoid bank conflicts
// 	__shared__ int xpose_tile[XPOSE_TILE_WIDTH][XPOSE_TILE_WIDTH+1];
// 
// 	// index pointers
// 	int x_idx;
// 	int y_idx;
// 	int element;
// 
// 	// calculate read indices
// 	x_idx = (XPOSE_TILE_WIDTH * blockIdx.x) + threadIdx.x;
// 	y_idx = (XPOSE_TILE_WIDTH * blockIdx.y) + threadIdx.y;
// 	element = (i_pitch * y_idx) + x_idx;
// 
// 	// read if within the bounds
// 	// - the matrix is read into shared memory in original order
// 	if ((x_idx < width) && (y_idx < height)) {
// 		xpose_tile[threadIdx.y][threadIdx.x] = in[element];
// 	}
// 
// 	// calculate write indices
// 	// - when reading the submatrix, the thread indices are swapped to perform
// 	//   the transpose, so we use the x and y thread indices to calculate the
// 	//   values of x_idx and y_idx, which then represent the y and x coordinates,
// 	//   respectively, of the original matrix
// 	x_idx = (XPOSE_TILE_WIDTH * blockIdx.y) + threadIdx.x;
// 	y_idx = (XPOSE_TILE_WIDTH * blockIdx.x) + threadIdx.y;
// 	element = (o_pitch * y_idx) + x_idx;
// 
// 	// everyone play catchup
// 	// - move this below the write index calculation to hide the latency of
// 	//   the loads
// 	__syncthreads();
// 
// 	// write if within the bounds
// 	// - transpose the tile by swapping the x/y thread indices into the
// 	//   shared memory
// 	if ((x_idx < height) && (y_idx < width)) {
// 		out[element] = xpose_tile[threadIdx.x][threadIdx.y];
// 	}
// 
// }
// 
// //////////////////////////////////////////////////////////////////////
// //
// // Kernel MemCpy
// //
// //////////////////////////////////////////////////////////////////////
// __global__ static void
// bmi_kernel_memmory_allign(int *src, int src_length, int src_x_step, int src_y_step, int line_length, int line_num, int * dest, int dest_offset_x, int dest_offset_y, int dest_length)
// {
// 
// 	int * in, * out;	// in ==> src  out ==>dest, we are goint to copy dest into src
// 	int row = threadIdx.y;	// copy this line
// 	int col  = threadIdx.x;				// from this col
// 
// 
// 	while(row < line_num)
// 	{
// 		col  = threadIdx.x;
// 		in  = src + row * src_length + col;
// 		out = dest  + (dest_offset_y + row) * dest_length + dest_offset_x + col;
// 		for (; col < line_length; col += src_x_step)
// 		{
// 			*out = *in;
// 			out += src_x_step;
// 			in += src_x_step;
// 		}
// 		row += src_y_step;
// 	}
// 	__syncthreads();
// 
// }
