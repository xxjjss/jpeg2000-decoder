/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/
#include <string.h>
#include "dec_stru.h"

#include <assert.h>
#include <stdio.h>
#include "dec_t1.h"
#include "debug.h"

#if _WINDOWS
#include <xmmintrin.h>
#include <emmintrin.h>
const __m128i MAXVAL_128  = _mm_set1_epi32(0x7fffffff);
const __m128i SIGNBIT_128 = _mm_set1_epi32(0x80000000);
#endif

#define		BMI_DEQ_FIX_MUL(x, y)					((static_cast<BMI_INT_64>(x) * static_cast<BMI_INT_64>(y)) >> 13)


#if GPU_T1_TESTING
#include "cudaInterface.h"
#endif
#include "bmi_block_decoder.h"
FILE *file_mq_a2;
FILE *file_mq_c2;
// 
// extern unsigned char lut_ctx_lllh[256];
// extern unsigned char lut_ctx_hl[256];
// extern unsigned char lut_ctx_hh[256];

extern int tot_entry;
extern char cb_lut[96];


/**************************************************************************************/
/* static                                dec_cb                                       */
/**************************************************************************************/
#if !SOFTWARE_MULTI_THREAD

#if SOFTWARE_MULTI_THREAD
static int dec_cb(stru_dec_param *dec_param, stru_opt *s_opt,
                  stru_scratch_buffer *scratch_buffer,
                  stru_subband *cur_band, stru_code_block *cur_cb, 
                  unsigned char *lut_ctx, int threadIndex);

static int dec_cb(stru_dec_param *dec_param, stru_opt *s_opt,
				  stru_scratch_buffer *scratch_buffer,
				  stru_subband *cur_band, stru_code_block *cur_cb, 
				  unsigned char *lut_ctx, int threadIndex)
#else	// SOFTWARE_MULTI_THREAD
static int dec_cb(stru_dec_param *dec_param, stru_opt *s_opt,
				  stru_scratch_buffer *scratch_buffer,
				  stru_subband *cur_band, stru_code_block *cur_cb, 
				  unsigned char *lut_ctx);

static int dec_cb(stru_dec_param *dec_param, stru_opt *s_opt,
				  stru_scratch_buffer *scratch_buffer,
				  stru_subband *cur_band, stru_code_block *cur_cb, 
				  unsigned char *lut_ctx)
#endif

{
  int cb_width = cur_cb->width;
  int cb_height = cur_cb->height;
  int roi_bit_plan = 30 - cur_cb->missing_bit_plan;

  unsigned short *cb_ctx = scratch_buffer->cb_ctx + cb_width + 3;
  unsigned int *cb_data = scratch_buffer->cb_data;

#if SOFTWARE_MULTI_THREAD
  cb_ctx += (scratch_buffer->size_ctx_buf * threadIndex) / sizeof(short);
  cb_data += (scratch_buffer->size_data_buf * threadIndex) / sizeof(int);
#endif

  unsigned char *buf[MAX_CODINGPASS + 1];

  unsigned short sym, sym2;
  int term_idx = 0, ext[2], cumlen;
  int pass_idx, pass_num = cur_cb->num_pass, bypass = 0;
  int mbp = roi_bit_plan;

  stru_coding_pass *pass1, *pass2;
  stru_mqd mqd;

  // copy mq data from separate layers to pass_buffer
  cumlen = 0;
  pass1 = cur_cb->pass;
  pass2 = pass1 + pass1->num_t2layer - 1;
  buf[0] = scratch_buffer->pass_buffer;
#if SOFTWARE_MULTI_THREAD
  buf[0] += scratch_buffer->size_pass_buf * threadIndex;
#endif	
  while(pass_num) {
    if(!pass1->buffer)    return -1;    // error resilient
    memcpy(buf[0] + cumlen, pass1->buffer, pass2->cumlen - cumlen);
    cumlen = pass2->cumlen;
    pass_num -= pass1->num_t2layer;
    pass1 = pass2 + 1;
    pass2 += pass1->num_t2layer;
  }

  // save pass cumlen on each terminate point to termlen[]
  pass1 = cur_cb->pass;
  if(s_opt->conf_termall) 
  {
    for(pass_num = 0; pass_num < cur_cb->num_pass; ++pass_num, ++pass1) 
	{
      buf[++term_idx] = buf[0] + pass1->cumlen;
    }
  } 
  else if(!s_opt->conf_bypass) 
  {
    buf[++term_idx] = buf[0] + (pass1 + cur_cb->num_pass - 1)->cumlen;
  }
  else 
  {
    for(pass_num = 0; pass_num < cur_cb->num_pass; ++pass_num, ++pass1) 
	{
      if( pass_num != cur_cb->num_pass - 1 && (pass_num < 9 || pass_num % 3 == 1))  continue;
      buf[++term_idx] = buf[0] + pass1->cumlen;
    }
  }

  // decode pass 1/2/3
  pass_num = 0;
  term_idx = 0;
  ext[0] = buf[0][0];   ext[1] = buf[0][1];
  while(pass_num < 3 * roi_bit_plan - 2) 
  {
    if(pass_num == cur_cb->num_pass)
	{
		break;
	}
// 	if(pass_num == 1)  break;
    pass_idx = pass_num % 3;

#if defined(B_FPGA_MQ_TB)
{
  fprintf(file_mq_a2, "pass %d\n", pass_num);
  fprintf(file_mq_c2, "pass %d\n", pass_num);
}
#endif

    if(!pass_num || s_opt->conf_reset)
	{
		mqd_reset(&mqd);
	}

    if(pass_idx == 1 && bypass) 
	{
      // force terminate
      buf[term_idx][0] = ext[0];    buf[term_idx][1] = ext[1];
	  mqd_init_raw(&mqd, buf[term_idx]);
	  ++term_idx;
      ext[0] = buf[term_idx][0];    ext[1] = buf[term_idx][1];
      buf[term_idx][0] = 0xff;      buf[term_idx][1] = 0xff;
      dec_t1_p1_raw(&mqd, cb_data, cb_ctx, cb_width, cb_height, mbp, s_opt->conf_causal);
      mqd_flush_raw(&mqd, s_opt->conf_termall);
    }
	else if(pass_idx == 1) 
	{
      if(s_opt->conf_termall) 
	  {
        // force terminate
        buf[term_idx][0] = ext[0];    buf[term_idx][1] = ext[1];
		mqd_init(&mqd, buf[term_idx]);
		++term_idx;
        ext[0] = buf[term_idx][0];    ext[1] = buf[term_idx][1];
        buf[term_idx][0] = 0xff;      buf[term_idx][1] = 0xff;
      }
      dec_t1_p1(&mqd, cb_data, cb_ctx, cb_width, cb_height, mbp, s_opt->conf_causal, lut_ctx);
      mqd_flush(&mqd, s_opt->conf_termall, s_opt->conf_pterm);
    }
	else if(pass_idx == 2 && bypass) 
	{
      if(s_opt->conf_termall) 
	  {
        // force terminate
        buf[term_idx][0] = ext[0];    buf[term_idx][1] = ext[1];
		mqd_init_raw(&mqd, buf[term_idx]);
		++term_idx;
        ext[0] = buf[term_idx][0];    ext[1] = buf[term_idx][1];
        buf[term_idx][0] = 0xff;      buf[term_idx][1] = 0xff;
      }
      dec_t1_p2_raw(&mqd, cb_data, cb_ctx, cb_width, cb_height, mbp);
      mqd_flush_raw(&mqd, 1);
    }
	else if(pass_idx == 2) 
	{
      if(s_opt->conf_termall) 
	  {
        // force terminate
        buf[term_idx][0] = ext[0];    buf[term_idx][1] = ext[1];
		mqd_init(&mqd, buf[term_idx]);
		++term_idx;
        ext[0] = buf[term_idx][0];    ext[1] = buf[term_idx][1];
        buf[term_idx][0] = 0xff;      buf[term_idx][1] = 0xff;
      }
      dec_t1_p2(&mqd, cb_data, cb_ctx, cb_width, cb_height, mbp);
      mqd_flush(&mqd, s_opt->conf_termall, s_opt->conf_pterm);
    } 
	else
	{
      if(!pass_num || s_opt->conf_termall || bypass) 
	  {
        // force terminate
        buf[term_idx][0] = ext[0];    buf[term_idx][1] = ext[1];
		mqd_init(&mqd, buf[term_idx]);
		++term_idx;
        ext[0] = buf[term_idx][0];    ext[1] = buf[term_idx][1];
        buf[term_idx][0] = 0xff;      buf[term_idx][1] = 0xff;
      }
      dec_t1_p3(&mqd, cb_data, cb_ctx, cb_width, cb_height, mbp, s_opt->conf_causal, lut_ctx);
      if(s_opt->conf_segsym) 
	  {
        mqd_dec((&mqd), (&sym),  CTX_UNIFORM);    sym <<= 3;
        mqd_dec((&mqd), (&sym2), CTX_UNIFORM);    sym += sym2 << 2;
        mqd_dec((&mqd), (&sym2), CTX_UNIFORM);    sym += sym2 << 1;
        mqd_dec((&mqd), (&sym2), CTX_UNIFORM);    sym += sym2;
        // assert (sym == 0x0A)
      }
      if(pass_num == 9)     bypass = s_opt->conf_bypass;
      mqd_flush(&mqd, s_opt->conf_termall || bypass, s_opt->conf_pterm);
    }

    if(!pass_idx)
	{
		--mbp;
	}
    ++pass_num;
    
#if defined(B_FPGA_TB)
    if( pass_num >= s_opt->max_num_pass &&
	      cur_band->resolution == dec_param->dwt_level - 1 &&
        cur_band->subband != BAND_LL) {
      break;
    }
#endif    
  }

//  buf[term_idx][0] = ext[0];    buf[term_idx++][1] = ext[1];

  if(roi_bit_plan && !s_opt->conf_termall) {
    if(bypass && pass_idx == 1)   mqd_flush_raw(&mqd, 1);
    else if(!bypass)              mqd_flush(&mqd, 1, s_opt->conf_pterm);
  }

  return 0;
}

#endif
/**************************************************************************************/
/*                                    dec_t1_get_jobs                                     */
/**************************************************************************************/
void dec_get_context_table(const unsigned char ** lllh, const unsigned char ** hl, const unsigned char ** hh)
{
	*lllh = lut_ctx_lllh;
	*hl = lut_ctx_hl;
	*hh = lut_ctx_hh;
}

#if GPU_T1_TESTING

void dec_get_all_context_table(const unsigned char ** lllh, const unsigned char ** hl, const unsigned char ** hh, const unsigned char ** sign)
{
	*lllh = lut_ctx_lllh;
	*hl = lut_ctx_hl;
	*hh = lut_ctx_hh;
	*sign = lut_ctx_sign;
}
#endif //GPU_T1_TESTING


#if SOFTWARE_MULTI_THREAD
int dec_cb_stripe(stru_pipe * s_pipe, int tileId, int comp_num, short band_num, stru_code_block *cur_cb, int number_in_stripe, unsigned char * lut_ctx, int thread_index, unsigned char * resultBuf, int result_buf_pitch )
{

	stru_opt *s_opt = s_pipe->s_opt;
	unsigned char * opt_memory = (unsigned char *)s_pipe->s_opt;
	opt_memory += tileId * s_opt->tile_dec_param.size_opt;

	s_opt = (stru_opt *)opt_memory;

	stru_tile_component *cur_comp = &s_opt->component[comp_num];
	stru_subband * cur_band = cur_comp->band + band_num;
//	stru_dec_param *dec_param = &s_opt->tile_dec_param;
	stru_scratch_buffer * scratch_buffer = &s_pipe->scratch_buffer;
	int * dwt_buffer;
	
#if !SOFTWARE_MULTI_THREAD
	int x, y;
#endif	

	int shift_bit_plan;
	int ret = 0;

	shift_bit_plan = 31 - (s_opt->qcc_guard_bit[comp_num] - 1 +
			s_opt->qcc_quant_step[comp_num][band_num]);

	result_buf_pitch >>= 2;	// count in sizeof(int)

// 	DEBUG_PRINT((">>> tile %d com %d band %d has %d cb %d lines\n",tileId, comp_num,band_num,number_in_stripe, cur_cb->height));

	int x_off = 0;

	for(int j = 0; j < number_in_stripe; ++j, ++cur_cb)
	{

		dwt_buffer = (int * )resultBuf;

#if SOFTWARE_MULTI_THREAD
		/*IN*/int_32 *cb_ctx = scratch_buffer->cb_ctx + (scratch_buffer->size_ctx_buf * thread_index) / sizeof(int_32);
		/*IN*/uint_8 * pass_buf = scratch_buffer->pass_buffer + scratch_buffer->size_pass_buf * thread_index;
		/*OUT*/int_32 *cb_data = scratch_buffer->cb_data + (scratch_buffer->size_data_buf * thread_index) / sizeof(unsigned int);
// 	DEBUG_PAUSE(pass_buf[0]== 0xf8 && pass_buf[1] == 0x8c &&pass_buf[2] == 0xed);
// 		DEBUG_PRINT(("   >>> [%d] : number_passed [%d] size [%d:%d] ",j,cur_cb->num_pass, cur_cb->width, cur_cb->height));
// 		DEBUG_SELECT_PRINT((cur_cb->num_pass),("   passbuf: %02x %02x %02x...\n",cur_cb->pass->buffer[0], cur_cb->pass->buffer[1], cur_cb->pass->buffer[2]), ("\n"));

//  		DEBUG_PAUSE(cur_band->subband == 0 && cur_band->resolution == 0);
		decode_code_block((s_opt->conf_bypass != 0), cb_data, cb_ctx, pass_buf, cur_band->numbps, cur_cb, lut_ctx);
		if (s_opt->conf_dwt97)
		{

			for(int y = 0; y < cur_cb->height; ++y) 
			{
				memcpy((void *)(dwt_buffer + x_off), (void *)cb_data, cur_cb->width * sizeof(int));
				dwt_buffer += result_buf_pitch;	// next line
				cb_data += cur_cb->width;
			}
			
		}
		else
		{

			int x, y;
			register int_32  tmp;
			register int_32 * pixel;
			for(y = 0; y < cur_cb->height; ++y) 
			{
				for( pixel = dwt_buffer + x_off, x = 0;
					x < cur_cb->width; ++x, ++pixel) 
				{
					tmp = *cb_data++;

					if (tmp < 0)
					{
						tmp = -((tmp & INT_32_MAX) >> (31 - cur_band->numbps));
					}
					else
					{
						tmp = tmp >> (31 - cur_band->numbps);
					}
					
					*pixel = tmp;

				}
				dwt_buffer += result_buf_pitch;	// next line
			}
		}

		x_off += cur_cb->width;

#else // SOFTWARE_MULTI_THREAD
		unsigned short *cb_ctx = scratch_buffer->cb_ctx + (scratch_buffer->size_ctx_buf * thread_index) / sizeof(unsigned short);
		unsigned int *cb_data = scratch_buffer->cb_data + (scratch_buffer->size_data_buf * thread_index) / sizeof(unsigned int);
		memset(cb_data, 0, cur_cb->width * cur_cb->height * sizeof(unsigned int));
		memset(cb_ctx, 0, (cur_cb->width + 2) * (cur_cb->height + 2) * sizeof(unsigned short));
		ret = dec_cb( dec_param, s_opt, scratch_buffer, cur_band, cur_cb, lut_ctx, thread_index);
		if (ret != 0)
		{
			break;
		}
		cb_ctx += cur_cb->width + 3;

		for(y = 0; y < cur_cb->height; ++y) 
		{
			for( x = x_off;
				x < x_off + cur_cb->width; ++x, ++cb_ctx) 
			{

				int pixel = *cb_data++;

				if (s_opt->conf_dwt97)
				{
					if (*cb_ctx & MASK_SIGN)
					{
						pixel |= 0x80000000;
					}
				}
				else /*if (s_opt->conf_dwti53)*/
				{
					if(s_opt->roi_shift) 
					{
						if(pixel < (1 << shift_bit_plan))  
						{
							pixel <<= s_opt->roi_shift;
						}
					}
					pixel >>= shift_bit_plan;
					if(*cb_ctx & MASK_SIGN) 
					{
						pixel = -pixel;
					}				
				}


				*(dwt_buffer + x) = pixel;
			}
			dwt_buffer += result_buf_pitch;	// next line
			cb_ctx += 2;
		}
		x_off += cur_cb->width;

#endif // SOFTWARE_MULTI_THREAD

	}
	return ret ;
}
#else

/**************************************************************************************/
/*                                    dec_t1_comp                                     */
/**************************************************************************************/
int dec_t1_comp(stru_dec_param *dec_param, stru_opt *s_opt,
				stru_persis_buffer *persis_buffer,
				stru_scratch_buffer *scratch_buffer, int comp_num)
{
	stru_tile_component *cur_comp = s_opt->component + comp_num;
	stru_code_block *cur_cb;
	stru_subband *cur_band;

	unsigned short *cb_ctx;
	unsigned int *cb_data;
	unsigned char *lut_ctx;
	int *dwt_buffer = persis_buffer->dwt_buffer[comp_num];

	int shift_bit_plan, pixel, x, y, y_off, band_num, cb_num;

	int i, j, m, idx;

	int levMap[5] = {13, 10, 7, 4, 1};
	int cbheight[5] = {0, 0, 0, 0, 0};

	// Compute table for component
	cbheight[0] = dec_param->height;
	for (i = 1; i < dec_param->dwt_level; ++i)
		cbheight[i] = (cbheight[i-1] + 1) >> 1;

	i=0;
	tot_entry = 0;
	while(cbheight[dec_param->dwt_level-1] > 0) {
		if     (!(i & 1))   j = 0;
		else if(!(i & 2))   j = 1;
		else if(!(i & 4))   j = 2;
		else if(!(i & 8))   j = 3;
		else                j = 4;

		i++;

		for(m = 0; m <= j; ++m) {
			if(cbheight[m] > 0) {
				cbheight[m] -= 32;
				cb_lut[tot_entry++] = levMap[m];

				if(m == (dec_param->dwt_level - 1)) {
					cb_lut[tot_entry++] = 0;
					break;
				}
			}
		}
	}

#if 1
	for(m = 0; m < tot_entry; ++m) {
		idx = cb_lut[m];

		for(band_num = 0; band_num < ((idx) ? 3 : 1); ++band_num) {
			int b_num = idx + band_num;
			if(idx)   b_num -= 3 * (5 - dec_param->dwt_level);

			//      cur_band = cur_comp->band + idx + band_num;
			//      if(idx)   cur_band -= 3 * (5 - dec_param->dwt_level);
			cur_band = cur_comp->band + b_num;

			shift_bit_plan = 31 - ( s_opt->guard_bit - 1 +
				s_opt->quant_step[b_num]);

#else
	for(cur_band = cur_comp->band,
		band_num = cur_comp->num_band;
		band_num; --band_num, ++cur_band) {

			shift_bit_plan = 31 - ( s_opt->guard_bit - 1 +
				s_opt->quant_step[cur_comp->num_band - band_num]);

#endif
			if(!cur_band->ncb_bw || !cur_band->ncb_bh)  continue;
			if(cur_band->cnt == cur_band->ncb_bh)       continue; 


			// context table selection
			lut_ctx = (unsigned char *)lut_ctx_lllh;
			if(cur_band->subband == BAND_HL)  lut_ctx = (unsigned char *)lut_ctx_hl;
			if(cur_band->subband == BAND_HH)  lut_ctx = (unsigned char *)lut_ctx_hh;

#if 1
			for(cur_cb = cur_band->codeblock + cur_band->cnt * cur_band->ncb_bw,
				j = 0; j < cur_band->ncb_bw; ++j, ++cur_cb){
#else
			for(cur_cb = cur_band->codeblock, cb_num = cur_band->ncb_bw * cur_band->ncb_bh;
				cb_num; --cb_num, ++cur_cb) {
#endif
					cb_ctx = scratch_buffer->cb_ctx;
					cb_data = scratch_buffer->cb_data;

					memset(cb_data, 0, cur_cb->width * cur_cb->height * sizeof(unsigned int));
					memset(cb_ctx, 0, (cur_cb->width + 2) * (cur_cb->height + 2) * sizeof(unsigned short));
					cb_ctx += cur_cb->width + 3;

#if defined(B_FPGA_MQ_TB)
					{
						static int cnt = 0;
						if(cnt == 0) {
							file_mq_a2  = fopen("\\raw\\t1\\file_c_mq_a2.txt", "wt");
							file_mq_c2  = fopen("\\raw\\t1\\file_c_mq_c2.txt", "wt");
						}
						++cnt;
#if defined(B_FPGA_MQ_TB)
						fprintf(file_mq_a2, "cb %d\n", cnt);
						fprintf(file_mq_c2, "cb %d\n", cnt);
#endif
					}
#endif

					dec_cb( dec_param, s_opt, scratch_buffer, cur_band, cur_cb, lut_ctx);

					// copy cb to cb buffer
					if(s_opt->roi_shift) {
						for(y = cur_cb->y0; y < cur_cb->y0 + cur_cb->height; ++y) {
							for(y_off = y * cur_comp->width[0], x = cur_cb->x0;
								x < cur_cb->x0 + cur_cb->width; ++x, ++cb_ctx) {
									pixel = *cb_data++;
									if(pixel < (1 << shift_bit_plan))   pixel <<= s_opt->roi_shift;
									pixel >>= shift_bit_plan;
									if(*cb_ctx & MASK_SIGN)   pixel = -pixel;
									*(dwt_buffer + y_off + x) = pixel;
							}
							cb_ctx += 2;
						}
					} else {
						for(y = cur_cb->y0; y < cur_cb->y0 + cur_cb->height; ++y) {
							for(y_off = y * cur_comp->width[0], x = cur_cb->x0;
								x < cur_cb->x0 + cur_cb->width; ++x, ++cb_ctx) {
									pixel = (*cb_data++) >> shift_bit_plan;
									if(*cb_ctx & MASK_SIGN)   pixel = -pixel;

									if(s_opt->conf_dwti53) {
										//              pixel = coef_dequantizer(pixel, dec_param->dwt_level, cur_band->resolution, cur_band->subband, cur_band->mant, cur_band->numbps + 1);
									}

									*(dwt_buffer + y_off + x) = pixel;
							}
							cb_ctx += 2;
						}
					}
			}
#if 1
			++cur_band->cnt;
	}
}

for(band_num = 0; band_num < cur_comp->num_band; ++band_num) {
	cur_band = cur_comp->band + band_num;
	cur_band->cnt = 0;
}
#else
	}
#endif

#if defined(B_FPGA_MQ_TB)
	{
		fclose(file_mq_a2);
		fclose(file_mq_c2);
	}
#endif

	return 0;
}

#endif // SOFTWARE_MULTI_THREAD


#if SOFTWARE_MULTI_THREAD

#if GPU_T1_TESTING
int	cpy_cb_passes( uint_8 * pass_buf, short numBps, stru_code_block *cur_cb, unsigned int& length)
{

	int max_original_passes = (numBps - cur_cb->missing_bit_plan ) * 3 - 2; //***UUU passses that are available; but we  may not be decoding all (decode only up to certain layer for example)
	if (max_original_passes <= 0)
	{
		return 0; 
	}
	int p_max = 30 - cur_cb->missing_bit_plan; 
	int num_passes = 3 * p_max - 2; 
	if (num_passes > cur_cb->num_pass) //****UUUU Calculating from p_max and comparing with cur_cb->num_pass
	{									
		num_passes =  cur_cb->num_pass; //***UUU decode only 30 bit planes; exclude the rest of the passes of the codeblock
	}
	if (!num_passes)
	{
		return 0; 
	}

	stru_coding_pass *pass1, *pass2;
	pass1 = cur_cb->pass; //***UUU first pass of the codeblock at the beginning
	pass2 = pass1 + pass1->num_t2layer - 1;//***UUU pass2:last pass in the layer. num_t2layer = number of passes in the current layer from the block
	//***UUU these layers continuous - so ptrs not set to buffs in t2 dec. only the first pass of the layer has a ptr to buff

	/*int segment_passes = max_original_passes; 
	if (segment_passes > num_passes) 
	{
		segment_passes = num_passes ;
	} */
	int passed = num_passes; 

	while(passed) { //***UUUU this copies all the passes of the block from the input stream to the pass buffer
		BMI_ASSERT(pass1->buffer); 
		BMI_ASSERT(pass2->cumlen >= length);
		memcpy(pass_buf + length, pass1->buffer, pass2->cumlen - length);
		length = pass2->cumlen;
		passed -= pass1->num_t2layer;
		pass1 = pass2 + 1; //***UUU the first pass in the next layer - has a pointer to the buffer
		pass2 += pass1->num_t2layer;
	}

return 0;
}
#endif //GPU_T1_TESTING
#endif // SOFTWARE_MULTI_THREAD

