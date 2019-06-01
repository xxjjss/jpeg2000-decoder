/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#include "dec_stru.h"
#include "debug.h"


int tot_entry;
char cb_lut[96];

/**************************************************************************************/
/*                                BMI_dec_isr                                         */
/**************************************************************************************/
BMI_LIB_API int BMI_dec_isr(stru_dec_param *dec_param, int fpga_ifr)
{
  return 0;
}

/**************************************************************************************/
/*                                   dec_dwt                                          */
/**************************************************************************************/
#if !SOFTWARE_MULTI_THREAD
int dec_dwt(stru_pipe *s_pipe)
{
  stru_persis_buffer *persis_buffer = &s_pipe->persis_buffer[s_pipe->idx_persis_dwt];
  stru_scratch_buffer *scratch_buffer = &s_pipe->scratch_buffer;
  stru_opt *s_opt = &s_pipe->s_opt[s_pipe->idx_opt_dwt];
  stru_dec_param *dec_param = &s_opt->dec_param;

  int comp_num;

#if defined(B_STAT)
  LgUns t0, t1, cnt_per_us = bmi_CLK_countspms()/1000;
  t0 = bmi_CLK_gethtime();
#endif

  for(comp_num = 0; comp_num < s_opt->num_component; ++comp_num) {
    if(s_opt->conf_dwti53) 
      dec_dwt_i53( dec_param, s_opt, persis_buffer, scratch_buffer, comp_num);
    else     
      dec_dwt_53( dec_param, s_opt, persis_buffer, scratch_buffer, comp_num);
  }

  if(s_pipe->pipe_use) {
    s_pipe->idx_opt_dwt = ++s_pipe->idx_opt_dwt % 3;
    s_pipe->idx_persis_dwt = !s_pipe->idx_persis_dwt;
  }

#if defined(B_STAT)
  t1 = bmi_CLK_gethtime() - t0;
  LOG_printf(&trace, "dwt %d us", t1 / cnt_per_us);
#endif

  return 0;
}

/**************************************************************************************/
/*                                      dec_t1                                        */
/**************************************************************************************/
int dec_t1(stru_pipe *s_pipe)
{
  stru_persis_buffer *persis_buffer = &s_pipe->persis_buffer[s_pipe->idx_persis_t1];
  stru_scratch_buffer *scratch_buffer = &s_pipe->scratch_buffer;
  stru_opt *s_opt = &s_pipe->s_opt[s_pipe->idx_opt_t1];
  stru_dec_param *dec_param = &s_opt->dec_param;

  int comp_num;

#if defined(B_STAT)
  LgUns t0, t1, cnt_per_us = bmi_CLK_countspms()/1000;
  t0 = bmi_CLK_gethtime();
#endif

  for(comp_num = 0; comp_num < s_opt->num_component; ++comp_num)
    dec_t1_comp(dec_param, s_opt, persis_buffer, scratch_buffer, comp_num);

  if(s_pipe->pipe_use) {
    s_pipe->idx_opt_t1 = ++s_pipe->idx_opt_t1 % 3;
    s_pipe->idx_persis_t1 = !s_pipe->idx_persis_t1;
  }

#if defined(B_STAT)
  t1 = bmi_CLK_gethtime() - t0;
  LOG_printf(&trace, "t1 %d us", t1 / cnt_per_us);
#endif

#if defined(B_FPGA_TB)
  fpga_tb_dump_dwt(dec_param, s_opt, persis_buffer);
  fpga_tb_dump_t1_dec(dec_param, s_opt, persis_buffer);
#endif

  return 0;
}
#endif
/**************************************************************************************/
/*                                       dec_t2                                       */
/**************************************************************************************/

#if SOFTWARE_MULTI_THREAD
int dec_t2(stru_dec_param *dec_param, int resolution, int use_j2k_header)
{
#else 
int dec_t2(stru_pipe *s_pipe, int use_j2k_header)
{
	stru_opt *s_opt  = &s_pipe->s_opt[s_pipe->idx_opt_t2];
#endif// !SOFTWARE_MULTI_THREAD

	stru_pipe *s_pipe = (stru_pipe *)dec_param->mempool[0];
	stru_stream *stream = &s_pipe->stream;
	unsigned char * opt_memory = (unsigned char *)s_pipe->s_opt;
	stru_opt *s_opt_t2 = NULL;

#if !SOFTWARE_MULTI_THREAD
	stru_opt *s_opt_dwt = &s_pipe->s_opt[s_pipe->idx_opt_dwt];
	int s_opt_t2_inited = 0;
#endif

	int rc;
	int tile_x, tile_y;
	int tile_width = dec_param->tile_width, tile_height = dec_param->tile_height;
	int width = dec_param->width, height = dec_param->height;
	int tile_num_x = ceil_ratio(width, tile_width);
//	int tile_num_y = ceil_ratio(height, tile_height );

	stru_jp2_syntax *s_jp2file = &s_pipe->s_jp2file;
	stru_jpc_sot *s_sot = &s_jp2file->s_jp2stream.s_sot;
	stru_dec_param  * dec_param2 = NULL;
	// make a local copy of dec_param

	unsigned short last_tile_idx = 0xff;
	while (1)
	{

		rc = dec_read_tile_header(s_jp2file, stream);   // parse tile header
		if (rc == -1)
		{
			// error
			return -1;
		}
		if ( rc == CODE_STREAM_END)
		{
			break;
		}
		BMI_ASSERT(s_sot->tile_idx < 0xFF);

 		if(s_sot->tile_idx != last_tile_idx) 
		{      // update s_opt
			last_tile_idx = s_sot->tile_idx;
			BMI_ASSERT(s_sot->tile_idx < dec_param->tile_num);
			s_opt_t2 = (stru_opt *)(opt_memory + dec_param->size_opt * s_sot->tile_idx);
			dec_param2 = &s_opt_t2->tile_dec_param;
			memcpy(dec_param2, dec_param, sizeof(stru_dec_param));

			tile_x = s_sot->tile_idx % tile_num_x;
			tile_y = s_sot->tile_idx / tile_num_x;

			dec_param2->tile_width = (tile_x + 1) * tile_width > width ? (width - tile_x * tile_width) : tile_width;
			dec_param2->tile_height = (tile_y + 1 )* tile_height > height ? (height - tile_y * tile_height) : tile_height;
			dec_init_opt(dec_param2, s_opt_t2, tile_x * tile_width , tile_y * tile_height);
			  // for a new tile, have to reset s_opt for dwt, and persis_buffer for t1
			dec_reset_opt(dec_param2, s_opt_t2, s_jp2file, resolution, tile_x * tile_width, tile_y * tile_height,
				  dec_param2->tile_width, dec_param2->tile_height, s_sot->tile_idx);
		}

// 		DEBUG_PRINT(("\n=====> Decode t2 tile:%d comp:%d\n", s_sot->tile_idx, s_opt_t2->t2_scan_component));
// 		DEBUG_PAUSE(s_sot->tile_idx != 0);
		rc = dec_progression(dec_param2, s_opt_t2, stream);
		if ( rc == CODE_STREAM_END)
		{
			break;
		}
		else if (rc == CHANGE_TILE || rc == 0)
		{
// 				ostream_seek(stream, s_sot->tile_offset + s_sot->tile_len);
		}
		else
		{
			char msg[100];
			SPRINTF_S((msg, 100, "unknown return code %d when decode progressing [%d]", rc, dec_param2->progression));
			BMI_ASSERT_MSG(0, msg); // unknown return code
			return -1;
		}
	};


// 
// 
// 
// 
// // #if _DEBUG
// //   printf("tile %d  [%d,%d]  :  [%d,%d]\n", s_opt->tile_idx, s_opt->tx0, s_opt->ty0, s_opt->tx1, s_opt->ty1);
// // #endif
// 
//   if(s_pipe->pipe_use) {
//     s_pipe->idx_opt_t2 = ++s_pipe->idx_opt_t2 % 3;
//     s_pipe->idx_persis_t2 = !s_pipe->idx_persis_t2;
//   }
// 
// #if defined(B_STAT)
//   t1 = bmi_CLK_gethtime() - t0;
//   LOG_printf(&trace, "t2 %d us", t1 / cnt_per_us);
// #endif

  return 0;
}

#if defined(B_FPGA_TB)

#include <assert.h>
#include <stdio.h>

FILE *file_img;
FILE *file_dwt_ll[5], *file_dwt_hl[5], *file_dwt_lh[5], *file_dwt_hh[5];

FILE *file_lut_cb;

int fpga_tb_config(stru_opt *s_opt)
{
  s_opt->res_align = 0;
  s_opt->max_num_pass = 43;
  
  return 0;
}

int fpga_tb_open(stru_dec_param *dec_param, stru_opt *s_opt)
{
  if(!s_opt->conf_vhdltb)   return 0;

  file_img  = fopen("\\raw\\file_c_img.txt", "wt");

  file_dwt_ll[0]  = fopen("\\raw\\file_c_dwt_ll_1.txt", "wt");
  file_dwt_ll[1]  = fopen("\\raw\\file_c_dwt_ll_2.txt", "wt");
  file_dwt_ll[2]  = fopen("\\raw\\file_c_dwt_ll_3.txt", "wt");
  file_dwt_ll[3]  = fopen("\\raw\\file_c_dwt_ll_4.txt", "wt");
  file_dwt_ll[4]  = fopen("\\raw\\file_c_dwt_ll_5.txt", "wt");

  file_dwt_hl[0]  = fopen("\\raw\\file_c_dwt_hl_1.txt", "wt");
  file_dwt_hl[1]  = fopen("\\raw\\file_c_dwt_hl_2.txt", "wt");
  file_dwt_hl[2]  = fopen("\\raw\\file_c_dwt_hl_3.txt", "wt");
  file_dwt_hl[3]  = fopen("\\raw\\file_c_dwt_hl_4.txt", "wt");
  file_dwt_hl[4]  = fopen("\\raw\\file_c_dwt_hl_5.txt", "wt");

  file_dwt_lh[0]  = fopen("\\raw\\file_c_dwt_lh_1.txt", "wt");
  file_dwt_lh[1]  = fopen("\\raw\\file_c_dwt_lh_2.txt", "wt");
  file_dwt_lh[2]  = fopen("\\raw\\file_c_dwt_lh_3.txt", "wt");
  file_dwt_lh[3]  = fopen("\\raw\\file_c_dwt_lh_4.txt", "wt");
  file_dwt_lh[4]  = fopen("\\raw\\file_c_dwt_lh_5.txt", "wt");

  file_dwt_hh[0]  = fopen("\\raw\\file_c_dwt_hh_1.txt", "wt");
  file_dwt_hh[1]  = fopen("\\raw\\file_c_dwt_hh_2.txt", "wt");
  file_dwt_hh[2]  = fopen("\\raw\\file_c_dwt_hh_3.txt", "wt");
  file_dwt_hh[3]  = fopen("\\raw\\file_c_dwt_hh_4.txt", "wt");
  file_dwt_hh[4]  = fopen("\\raw\\file_c_dwt_hh_5.txt", "wt");

  file_lut_cb = fopen("\\raw\\file_c_lut_cb.txt", "wt");

  return 0;
}

int fpga_tb_dump_img(stru_dec_param *dec_param, stru_opt *s_opt)
{
  int x, y, pix;

  if(!s_opt->conf_vhdltb)   return 0;

  for(y = 0; y < dec_param->height; ++y) {
    for(x = 0; x < dec_param->width; ++x) {
      if(dec_param->bitdepth <= 8) {
        pix = (*((unsigned char *)dec_param->output[0] + x + y * dec_param->width));
      } else {
        pix = (*((unsigned short *)dec_param->output[0] + x + y * dec_param->width));
      }
      fprintf(file_img, "%08X ", pix);
    }
    fprintf(file_img, "\n");
  }

  return 0;
}

int fpga_tb_dump_dwt(stru_dec_param *dec_param, stru_opt *s_opt, stru_persis_buffer *persis_buffer)
{
  int w0, h0, x, y, level, pix, pix1, pix2;
  stru_tile_component *cur_comp = s_opt->component;

  if(!s_opt->conf_vhdltb)   return 0;

  for(level = 0; level < dec_param->dwt_level; ++level) {
    w0 = (cur_comp->width[level] + 1) / 2;
    h0 = (cur_comp->height[level] + 1) / 2;
    for(y = 0; y < cur_comp->height[level]; ++y) {
      for(x = 0; x < cur_comp->width[level]; ++x) { //
        pix = *(persis_buffer->dwt_buffer[0] + x + y * cur_comp->width[0]);
        pix1 = (s_opt->conf_dwti53) ? (pix + 1) >> 1 : pix;
        pix2 = (s_opt->conf_dwti53) ? (pix + 2) >> 2 : pix;

             if(x <  w0 && y <  h0 && level == dec_param->dwt_level - 1)   fprintf(file_dwt_ll[level], "%08X ", pix);
        else if(x >= w0 && y <  h0)   fprintf(file_dwt_hl[level], "%08X ", pix1);
        else if(x <  w0 && y >= h0)   fprintf(file_dwt_lh[level], "%08X ", pix1);
        else if(x >= w0 && y >= h0)   fprintf(file_dwt_hh[level], "%08X ", pix2);
      }

      if(y < h0) {
        if(level == dec_param->dwt_level - 1)   fprintf(file_dwt_ll[level], "\n");
        fprintf(file_dwt_hl[level], "\n");
      } else {
        fprintf(file_dwt_lh[level], "\n");
        fprintf(file_dwt_hh[level], "\n");
      }
    }
  }

  return 0;
}

int fpga_tb_dump_t1_dec(stru_dec_param *dec_param, stru_opt *s_opt, stru_persis_buffer *persis_buffer)
{
  stru_tile_component *cur_comp = s_opt->component;
  stru_code_block *cur_cb;
  stru_subband *cur_band;

  int m, j, x, y, pix, idx, sign;
  int band_num;

  if(!s_opt->conf_vhdltb)   return 0;

  for(m = 0; m < tot_entry; ++m) {
    idx = cb_lut[m];

    //////// dump lut t1 data
    for(band_num = 0; band_num < ((idx) ? 3 : 1); ++band_num) {
      cur_band = cur_comp->band + idx + band_num;
      if(idx)   cur_band -= 3 * (5 - dec_param->dwt_level);

      if(!cur_band->ncb_bw || !cur_band->ncb_bh)  continue;
      if(cur_band->cnt == cur_band->ncb_bh)       continue; 

      for(cur_cb = cur_band->codeblock + cur_band->cnt * cur_band->ncb_bw,
          j = 0; j < cur_band->ncb_bw; ++j, ++cur_cb){


        //////// dump lut t1 code block
//        fprintf(file_lut_cb, "%08X ", cur_cb->width * cur_cb->height);    // total cb size

        for(y = cur_cb->y0; y < cur_cb->y0 + cur_cb->height; ++y) {
          for(x = cur_cb->x0; x < cur_cb->x0 + cur_cb->width; ++x) {

            pix = *(persis_buffer->dwt_buffer[0] + x + y * cur_comp->width[0]);

            if(s_opt->conf_dwti53) {
//              pixel = coef_dequantizer(pixel, dec_param->dwt_level, cur_band->resolution, cur_band->subband, cur_band->mant, cur_band->numbps + 1);
            }

            if(pix < 0) {
              sign = 0x00008000;
              pix = -pix;
            } else {
              sign = 0x00000000;
            }

            pix = pix << (15 - (cur_band->numbps - cur_cb->missing_bit_plan));
            pix = sign | pix;
            fprintf(file_lut_cb, "%08X ", pix);
          }
        }
        fprintf(file_lut_cb, "\n");
      }
      ++cur_band->cnt;
    }
  }

  for(band_num = 0; band_num < cur_comp->num_band; ++band_num) {
    cur_band = cur_comp->band + band_num;
    cur_band->cnt = 0;
  }

  return 0;
}

int fpga_tb_close(stru_pipe *s_pipe)
{
  stru_opt *s_opt = &s_pipe->s_opt[s_pipe->idx_opt_dwt];
  stru_dec_param *dec_param = &s_opt->dec_param;

  if(!s_pipe->s_opt->conf_vhdltb)   return 0;

  fpga_tb_dump_img(dec_param, s_opt);

  fclose(file_img);

  fclose(file_dwt_ll[0]);
  fclose(file_dwt_ll[1]);
  fclose(file_dwt_ll[2]);
  fclose(file_dwt_ll[3]);
  fclose(file_dwt_ll[4]);

  fclose(file_dwt_hl[0]);
  fclose(file_dwt_hl[1]);
  fclose(file_dwt_hl[2]);
  fclose(file_dwt_hl[3]);
  fclose(file_dwt_hl[4]);

  fclose(file_dwt_lh[0]);
  fclose(file_dwt_lh[1]);
  fclose(file_dwt_lh[2]);
  fclose(file_dwt_lh[3]);
  fclose(file_dwt_lh[4]);

  fclose(file_dwt_hh[0]);
  fclose(file_dwt_hh[1]);
  fclose(file_dwt_hh[2]);
  fclose(file_dwt_hh[3]);
  fclose(file_dwt_hh[4]);

  fclose(file_lut_cb);

  return 0;
}
#endif
