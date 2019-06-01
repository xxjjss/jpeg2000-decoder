/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#if defined(WIN32)
  #include <windows.h>
#endif
#include "debug.h"
#include "codec_datatype.h"
#include "codec_base.h"
#include <string.h>
#include "dec_stru.h"

static int dec_check_param1(stru_dec_param *dec_param);
static int dec_check_param2(stru_dec_param *dec_param);
//static void param_switch(stru_dec_param *dec_param, stru_jp2_syntax *s_jp2file, int comp_id);
// static int dec_reset_opt(stru_dec_param *dec_param, stru_opt *s_opt,
//                          stru_jp2_syntax *s_jp2file, int resolution,
//                          int tx0, int ty0, int width, int height, int tile_idx);
// static int dec_reset_persis(stru_persis_buffer *persis_buffer);


/**************************************************************************************/
/* static                        dec_check_param1                                     */
/**************************************************************************************/
static int dec_check_param1(stru_dec_param *dec_param)
{
  int rc = 0;

  if( dec_param->width <= 0 || dec_param->tile_width <= 0 ||
      dec_param->height <= 0 || dec_param->tile_height <= 0)
    rc = -1;

  if( dec_param->format != RAW && dec_param->format != RGB &&
      dec_param->format != YUV444 && dec_param->format != YUV422 &&
      dec_param->format != YUV420 && dec_param->format != BAYERGRB &&
	  dec_param->format != COMPONENT4)
    rc = -1;

  if(dec_param->bitdepth <= 0 || dec_param->bitdepth > 12)
    rc = -1;

  if(dec_param->layer < 1)
    rc = -1;

  if(dec_param->dwt_level < 0 || dec_param->dwt_level > MAX_DWT_LEVEL)
    rc = -1;

  if( dec_param->progression != LRCP && dec_param->progression != RLCP &&
      dec_param->progression != RPCL && dec_param->progression != PCRL &&
      dec_param->progression != CPRL)
    rc = -1;

//   if( dec_param->cb_width != 16 && dec_param->cb_width != 32 &&
//       dec_param->cb_width != 64)
//     rc = -1;

  if( dec_param->cb_height != 16 && dec_param->cb_height != 32 &&
      dec_param->cb_height != 64)
    rc = -1;

  if( dec_param->prec_width != 1 << int_log2(dec_param->prec_width) ||
      int_log2(dec_param->prec_width) < 1 || int_log2(dec_param->prec_width) > 15)
    rc = -1;

  if( dec_param->prec_height != 1 << int_log2(dec_param->prec_height) ||
      int_log2(dec_param->prec_height) < 1 || int_log2(dec_param->prec_height) > 15)
    rc = -1;
  if(dec_param->tile_width > dec_param->width)
    rc = -1;

  if(dec_param->tile_height > dec_param->height)
    rc = -1;

#if defined(ASIC)    // constraint to ASIC version
  if(dec_param->cb_width == 64)
    rc = -1;

  if(dec_param->cb_height == 64)
    rc = -1;

  if(dec_param->configuration & (CONF_BYPASS | CONF_PTERM | CONF_SEGSYM))
    rc = -1;

  if(~(dec_param->configuration | ~(CONF_RESET | CONF_TERMALL | CONF_CAUSAL)))
    rc = -1;
#else
  dec_param->configuration &= ~CONF_VIDEO;
#endif

  // not supported yet
//  if(dec_param->configuration & CONF_DWT97)
//    rc = -1;

  return rc;
}

/**************************************************************************************/
/* static                        dec_check_param2                                     */
/**************************************************************************************/
static int dec_check_param2(stru_dec_param *dec_param)
{
  int rc = 0;
  if( dec_param->mempool[0] == NULL || dec_param->pool_size[0] <= 0 ||
      dec_param->mempool[1] == NULL || dec_param->pool_size[1] <= 0)
  {
	  rc = -1;
  }

  if(dec_param->input == NULL || dec_param->in_size <= 0)
  {
	  rc = -1;
  }
#if !SOFTWARE_MULTI_THREAD
  {
	  int comp_num, num_component = ( dec_param->format == RAW)       ? 1 :
		  ( dec_param->format == RGB ||
		  dec_param->format == YUV444 ||
		  dec_param->format == YUV422 ||
		  dec_param->format == YUV420 ||
		  dec_param->format == BAYERGRB)  ? 3 : MAX_COMPONENT;

	  for(comp_num = 0; comp_num < num_component; ++comp_num)
		  if(dec_param->output[comp_num] == NULL)
		  {
			  rc = -1;
		  }
  }
#endif
  return rc;
}

/**************************************************************************************/
/* static                           dec_reset_opt                                     */
/**************************************************************************************/
/*static*/ int dec_reset_opt(stru_dec_param *dec_param, stru_opt *s_opt,
                         stru_jp2_syntax *s_jp2file, int resolution,
                         int tx0, int ty0, int width, int height, int tile_idx)
{
  // fill anything not filled by BMI_dec_info, and reset everything else
//   stru_jpc_qcd *s_qcd = &s_jp2file->s_jp2stream.s_qcd;

  int comp_num, band_num, cb_num;
  stru_tile_component *cur_comp;
  stru_code_block *cur_cb;
  stru_subband *cur_band;
	int i;

//   memcpy(s_opt->qcd_quant_step, s_qcd->quant_step, (1 + 3 * MAX_DWT_LEVEL) * sizeof(unsigned int));
//   s_opt->qcd_guard_bit      = s_qcd->guard_bit;
  for (i = 0; i < MAX_COMPONENT; ++i)
  {
	  if (s_jp2file->s_jp2stream.s_qcd[i].enable)
	  {
		  memcpy(s_opt->qcc_quant_step[i], s_jp2file->s_jp2stream.s_qcd[i].quant_step, (1 + 3 * MAX_DWT_LEVEL) * sizeof(unsigned int));
		  s_opt->qcc_guard_bit[i]      = s_jp2file->s_jp2stream.s_qcd[i].guard_bit;
	  }
	  else	// use qcd
	  {
		  memcpy(s_opt->qcc_quant_step[i], s_jp2file->s_jp2stream.s_qcd[0].quant_step, (1 + 3 * MAX_DWT_LEVEL) * sizeof(unsigned int));
		  s_opt->qcc_guard_bit[i]      = s_jp2file->s_jp2stream.s_qcd[0].guard_bit;
	  }
  }

  // reset code block/coding pass/num_roi_rect
  for( comp_num = 0; comp_num  < s_opt->num_component; ++comp_num) {
		  cur_comp = &s_opt->component[comp_num];
    for(cur_band = cur_comp->band, band_num = cur_comp->num_band;
        band_num; --band_num, ++cur_band) {

		cur_band->numbps =  s_opt->qcc_quant_step[comp_num][cur_comp->num_band - band_num] +
					s_opt->qcc_guard_bit[comp_num] - 1;

      for(cur_cb = cur_band->codeblock, cb_num = cur_band->ncb_bw * cur_band->ncb_bh;
        cb_num; --cb_num, ++cur_cb) {
        cur_cb->has_t2pass = 
        cur_cb->num_t2pass = 
        cur_cb->idx_t2pass = 
        cur_cb->missing_bit_plan = 
        cur_cb->num_pass = 0;
        cur_cb->lblock = 3;

        cur_cb->pass->layer = -1;
        cur_cb->pass->num_t2layer = 1;
        cur_cb->pass->cumlen = 0;
        cur_cb->pass->buffer = NULL;
      }
    }
  }

  // reset configuration
  s_opt->conf_bypass    = !!(dec_param->configuration & CONF_BYPASS);
  s_opt->conf_reset     = !!(dec_param->configuration & CONF_RESET);
  s_opt->conf_termall   = !!(dec_param->configuration & CONF_TERMALL);
  s_opt->conf_causal    = !!(dec_param->configuration & CONF_CAUSAL);
  s_opt->conf_pterm     = !!(dec_param->configuration & CONF_PTERM);
  s_opt->conf_segsym    = !!(dec_param->configuration & CONF_SEGSYM);
  s_opt->conf_sop       = !!(dec_param->configuration & CONF_SOP);
  s_opt->conf_eph       = !!(dec_param->configuration & CONF_EPH);
  s_opt->conf_jp2file   = !!(dec_param->configuration & CONF_JP2FILE);
  s_opt->conf_vhdltb    = !!(dec_param->configuration & CONF_VHDLTB);

  // reset global variables
  s_opt->tx0 = tx0;
  s_opt->ty0 = ty0;
  s_opt->tx1 = tx0 + width;
  s_opt->ty1 = ty0 + height;
  s_opt->tile_idx = tile_idx;
  s_opt->reso_thumb = resolution;

  return 0;
}

/**************************************************************************************/
/* static                        dec_reset_persis                                     */
/**************************************************************************************/
// static int dec_reset_persis(stru_persis_buffer *persis_buffer)
// {
//   return 0;
// }

/**************************************************************************************/
/*                            BMI_dec_thumbnail                                       */
/**************************************************************************************/
BMI_LIB_API BMI::Status BMI_dec_thumbnail(stru_dec_param *dec_param, void *output[MAX_COMPONENT],
                                  int resolution, int use_j2k_header)
{
//   int w, h, width, height, tile_width, tile_height;
//   int tile_idx, tiling = 0;
  int rc;

  stru_pipe *s_pipe = (stru_pipe *)dec_param->mempool[0];
//   stru_opt *s_opt_t2, *s_opt_dwt = &s_pipe->s_opt[s_pipe->idx_opt_dwt];
#if !SOFTWARE_MULTI_THREAD
  stru_dspbios *s_dspbios = &s_pipe->s_dspbios;
#endif
  stru_jp2_syntax *s_jp2file = &s_pipe->s_jp2file;
  stru_stream *stream = &s_pipe->stream;
//   stru_dec_param dec_param2;

  // check input parameter: resolution
  if(resolution < 1 || resolution > dec_param->dwt_level)  
  {
	  return BMI::PARSE_ERROR_INPUTFILE_ERROR;
  }
 
#if !SOFTWARE_MULTI_THREAD
  memset(output, (int)NULL, MAX_COMPONENT * sizeof(void *));
#endif

  // if still in pipeline...
  if(dec_param) {
    memset(s_jp2file, 0, sizeof(stru_jp2_syntax));
    rc = dec_check_param2(dec_param);
    if(rc)   
	{
		return BMI::PARSE_ERROR_INVALID_INPUT;
	}
    rc = dec_read_header(dec_param, s_jp2file, stream, use_j2k_header);
    if(rc)    
	{
		return BMI::PARSE_ERROR_INPUTFILE_ERROR;
	}
  }

  if(s_pipe->pipe_pause) {
    if(s_pipe->pipe_use)    --s_pipe->pipe_pause;
    else                    s_pipe->pipe_pause = 0;
  }

//   width = dec_param->width;     tile_width = dec_param->tile_width;
//   height = dec_param->height;   tile_height = dec_param->tile_height;
//   if(width != tile_width || height != tile_height)  tiling = 1;

  // make a local copy of dec_param
//   memcpy(&dec_param2, dec_param, sizeof(stru_dec_param));



#if defined(WIN32)
  try {
#endif
#if !SOFTWARE_MULTI_THREAD
	 rc =  dec_t2(s_pipe, use_j2k_header);
#else
	  rc = dec_t2(dec_param, resolution ,use_j2k_header);
#endif
	if(rc)    
	{
		WRITE_LOG(("T2 error, dec_t2 return code [%d]\n", rc));
		return BMI::PARSE_ERROR_INPUTFILE_ERROR;
	}

#if defined(WIN32)
  } 
  catch(BMI::Status code) 
  {
	  return code;
  }
  catch(...)
  {

	  BMI_ASSERT(0);
  }
#endif


#if SOFTWARE_MULTI_THREAD
//   unsigned char * opt_memory = (unsigned char *)s_pipe->s_opt;
#endif
//   for(tile_idx = 0, h = 0; h < height; h += tile_height) {
//     for(w = 0; w < width; w += tile_width, ++tile_idx) {
//       if(tile_idx) {      // update everything
//         dec_param2.tile_width = (tile_width < width - w) ? tile_width : width - w;
//         dec_param2.tile_height = (tile_height < height - h) ? tile_height : height - h;
//       }

#if SOFTWARE_MULTI_THREAD
// 	  s_opt_t2 = (stru_opt *)opt_memory;
//       dec_init_opt(&dec_param2, s_opt_t2, w, h);
// 	  opt_memory += dec_param->size_opt;
// 
//       // for a new tile, have to reset s_opt for dwt, and persis_buffer for t1
//       dec_reset_opt(&dec_param2, s_opt_t2, s_jp2file, resolution, w, h,
//                     dec_param2.tile_width, dec_param2.tile_height, tile_idx);
#else
	  // dec_param changed, init s_opt
	  s_opt_t2 = &s_pipe->s_opt[s_pipe->idx_opt_t2];
	  if(tiling)    dec_init_opt(&dec_param2, s_opt_t2, w, h, stru_jp2_syntax *s_jp2file);

	  // for a new tile, have to reset s_opt for dwt, and persis_buffer for t1
	  dec_reset_opt(&dec_param2, s_opt_t2, s_jp2file, resolution, w, h,
		  dec_param2.tile_width, dec_param2.tile_height, tile_idx);
//       dec_reset_persis(&s_pipe->persis_buffer[s_pipe->idx_persis_t1]);
#endif

#if defined(B_FPGA_TB)
      if(tile_idx == 0) {
        fpga_tb_config(s_opt_t2);
        fpga_tb_open(dec_param, s_opt_t2);
      }
#endif

#if defined(ASIC_T1)
      if(s_pipe->pipe_use) {          // pipeline version, 3 frame latency
        // have to use this sequence by now...
        bmi_SEM_post(s_dspbios->sem_t1_cb_start);
        bmi_SEM_post(s_dspbios->sem_t1_mq_start);
        bmi_SEM_post(s_dspbios->sem_dwt_start);
        bmi_SEM_post(s_dspbios->sem_t2_start);

        bmi_SEM_pend(s_dspbios->sem_t1_cb_done);
        bmi_SEM_pend(s_dspbios->sem_t1_mq_done);
        bmi_SEM_pend(s_dspbios->sem_dwt_done);
        bmi_SEM_pend(s_dspbios->sem_t2_done);
      } else {                          // non pipe version, 1 frame latency
        bmi_SEM_post(s_dspbios->sem_t2_start);
        bmi_SEM_pend(s_dspbios->sem_t2_done);

        bmi_SEM_post(s_dspbios->sem_t1_cb_start);
        bmi_SEM_post(s_dspbios->sem_t1_mq_start);
        bmi_SEM_pend(s_dspbios->sem_t1_cb_done);
        bmi_SEM_pend(s_dspbios->sem_t1_mq_done);

        bmi_SEM_post(s_dspbios->sem_dwt_start);
        bmi_SEM_pend(s_dspbios->sem_dwt_done);
      }
#else

#if !SOFTWARE_MULTI_THREAD
#if defined(WIN32)
		try {
			dec_t1(s_pipe);
		} catch(...) {
		}
#else
		dec_t1(s_pipe);
#endif

		dec_dwt(s_pipe);
#endif	// !SOFTWARE_MULTI_THREAD
#endif
//     }
//   }

//   if(!s_pipe->pipe_pause) {
//     memcpy(output, s_opt_dwt->dec_param.output, MAX_COMPONENT * sizeof(void *));
//   }

#if defined(B_FPGA_TB)
  fpga_tb_close(s_pipe);
#endif

  return BMI::PARSING;
}

// BMI_LIB_API void BMI_get_context_table(const unsigned char ** lllh, const unsigned char ** hl, const unsigned char ** hh)
// {
// 	dec_get_context_table(lllh, hl, hh);
// }

/**************************************************************************************/
/*                                       BMI_dec1                                     */
/**************************************************************************************/
BMI_LIB_API int BMI_dec(stru_dec_param *dec_param, void *output[MAX_COMPONENT], int use_j2k_header)
{
  return BMI_dec_thumbnail(dec_param, output, dec_param->dwt_level, use_j2k_header);
}

/**************************************************************************************/
/*                                BMI_dec_info                                        */
/**************************************************************************************/
#if SOFTWARE_MULTI_THREAD
BMI_LIB_API int BMI_dec_info(stru_dec_param *dec_param,  int thread_num)
#else	// SOFTWARE_MULTI_THREAD
BMI_LIB_API int BMI_dec_info(stru_dec_param *dec_param)
#endif	// SOFTWARE_MULTI_THREAD
{
  int siz1 = 0, siz2 = 0, siz3 = 0, siz4 = 0;
  int siz_s_opt, siz_persis_buffer, siz_extra;
  int comp_num, w, h, width, height, tile_width, tile_height;
  int pipe_use;
  int rc;

  stru_dec_param dec_param2;

  stru_stream stream;
  stru_jp2_syntax s_jp2file;
  s_jp2file.s_jp2stream.s_m_poc.num_poc = 0;
  s_jp2file.s_jp2stream.s_t_poc.num_poc = 0;
  stru_jp2_colr *s_colr = &s_jp2file.s_jp2h.s_colr;
  stru_jpc_siz *s_siz   = &s_jp2file.s_jp2stream.s_siz;
  stru_jpc_rgn *s_rgn   = &s_jp2file.s_jp2stream.s_rgn;

  stru_jpc_cod *s_cod   = s_jp2file.s_jp2stream.s_cod;
  
  dec_param->pool_size[0] = 0;
  dec_param->pool_size[1] = 0;
  memset(&s_jp2file, 0, sizeof(stru_jp2_syntax));

  if(dec_param->input) {    // fill dec_param from bit-stream

#if defined(WIN32)
try {
    if(dec_read_header(dec_param, &s_jp2file, &stream, 1))
      return -1;
} catch(...) {
  return -1;
}
#else
    if(dec_read_header(dec_param, &s_jp2file, &stream, 1))
      return -1;
#endif

    // double check
    if( s_jp2file.exists_jp2 && s_colr->meth == 1 && s_colr->enmu_colr_space != 16 &&
        s_colr->enmu_colr_space != 17 && s_colr->enmu_colr_space != 18)
      return -1;

    if(s_siz->num_component < 1 || s_siz->num_component > MAX_COMPONENT)
      return -1;

    for(comp_num = 1; comp_num < s_siz->num_component; ++comp_num) {
      if(s_siz->bitdepth[comp_num] != s_siz->bitdepth[comp_num - 1])
        return -1;

      if(s_rgn->roi_shift[comp_num] != s_rgn->roi_shift[comp_num - 1])
        return -1;
    }

    // fill dec_param
    switch(s_cod[0].progression) {
      case 0  :   dec_param->progression = LRCP;    break;
      case 1  :   dec_param->progression = RLCP;    break;
      case 2  :   dec_param->progression = RPCL;    break;
      case 3  :   dec_param->progression = PCRL;    break;
      case 4  :   dec_param->progression = CPRL;    break;
      default :   dec_param->progression = UNKNOWN_PROGRESSION;      break;
    }

	dec_param->num_components = s_siz->num_component;

    switch(s_siz->num_component) {
    case 1  :
      if(s_siz->XRsiz[0] == 1 && s_siz->YRsiz[0] == 1)
        dec_param->format = RAW;
      else
        return -1;
      break;

    case 3  :
      if(s_siz->XRsiz[0] == 1 && s_siz->XRsiz[1] == 2 && s_siz->XRsiz[2] == 2 &&
        s_siz->YRsiz[0] == 1 && s_siz->YRsiz[1] == 2 && s_siz->YRsiz[2] == 2) {
        dec_param->format = YUV420;
      } else if(s_siz->XRsiz[0] == 1 && s_siz->XRsiz[1] == 2 && s_siz->XRsiz[2] == 2 &&
              s_siz->YRsiz[0] == 1 && s_siz->YRsiz[1] == 1 && s_siz->YRsiz[2] == 1) {
        dec_param->format = YUV422;
      } else if(s_siz->XRsiz[0] == 1 && s_siz->XRsiz[1] == 1 && s_siz->XRsiz[2] == 1 &&
              s_siz->YRsiz[0] == 1 && s_siz->YRsiz[1] == 2 && s_siz->YRsiz[2] == 2) {
        dec_param->format = BAYERGRB;
      } else if(s_siz->XRsiz[0] == 1 && s_siz->XRsiz[1] == 1 && s_siz->XRsiz[2] == 1 &&
              s_siz->YRsiz[0] == 1 && s_siz->YRsiz[1] == 1 && s_siz->YRsiz[2] == 1) {
        if(s_colr->enmu_colr_space == 16)         dec_param->format = RGB;
        else if(s_colr->enmu_colr_space == 18)    dec_param->format = YUV444;
        else if(s_cod[0].mct == 1)                  dec_param->format = RGB;
        else                                      dec_param->format = YUV444;
      } else {
        return -1;
      }
      break;

	case 4  :
      dec_param->format = COMPONENT4;
	  break;
	  
    default :     return -1;
    }

    dec_param->width        = s_siz->width;
    dec_param->height       = s_siz->height;
    dec_param->bitdepth     = s_siz->bitdepth[0];
    dec_param->tile_width   = s_siz->tile_width;
    dec_param->tile_height  = s_siz->tile_height;

    dec_param->cb_width     = s_cod[0].cb_width;
    dec_param->cb_height    = s_cod[0].cb_height;

    dec_param->layer        = s_cod[0].layer;
    dec_param->dwt_level    = s_cod[0].dwt_level;
    dec_param->roi_shift    = s_rgn->roi_shift[0];

	dec_param->prec_width   = s_cod[0].prec_width[dec_param->dwt_level];
    dec_param->prec_height  = s_cod[0].prec_height[dec_param->dwt_level];


    if(s_cod[0].bypass)           dec_param->configuration |= CONF_BYPASS;
    if(s_cod[0].reset)            dec_param->configuration |= CONF_RESET;
    if(s_cod[0].termall)          dec_param->configuration |= CONF_TERMALL;
    if(s_cod[0].causal)           dec_param->configuration |= CONF_CAUSAL;
    if(s_cod[0].pterm)            dec_param->configuration |= CONF_PTERM;
    if(s_cod[0].segsym)           dec_param->configuration |= CONF_SEGSYM;
    if(s_cod[0].sop)              dec_param->configuration |= CONF_SOP;
    if(s_cod[0].eph)              dec_param->configuration |= CONF_EPH;
    if(s_cod[0].dwt97)            dec_param->configuration |= CONF_DWT97;
    if(s_cod[0].mct)              dec_param->configuration |= CONF_MCT;
    if(s_cod[0].dwt_i53)          dec_param->configuration |= CONF_QUANT;
    if(s_jp2file.exists_jp2)      dec_param->configuration |= CONF_JP2FILE;
  }

  rc = dec_check_param1(dec_param);
  if(rc)    return rc;

  pipe_use = !!(dec_param->configuration & CONF_VIDEO);

  width = dec_param->width;     tile_width = dec_param->tile_width;
  height = dec_param->height;   tile_height = dec_param->tile_height;

  // make a local copy of dec_param
  memcpy(&dec_param2, dec_param, sizeof(stru_dec_param));

  for(h = 0; h < height; h += tile_height) {
    for(w = 0; w < width; w += tile_width) {
      if(w || h) {      // update everything
        dec_param2.tile_width = (tile_width < width - w) ? tile_width : width - w;
        dec_param2.tile_height = (tile_height < height - h) ? tile_height : height - h;
      }

#if SOFTWARE_MULTI_THREAD
      dec_info( &dec_param2, pipe_use, w, h, &siz_s_opt, &siz_persis_buffer, &siz_extra, thread_num);
	  siz2 = 0;
#else
	  dec_info( &dec_param2, pipe_use, w, h, &siz_s_opt, &siz_persis_buffer, &siz_extra);
	  if(siz2 < siz_persis_buffer)        siz2 = siz_persis_buffer;
#endif
      // find the max on-off chip pool_size among all tiles
      if(siz1 < siz_s_opt)                siz1 = siz_s_opt;
      if(siz3 < siz_extra)                siz3 = siz_extra;
      if(siz4 < dec_param2.pool_size[1])  siz4 = dec_param2.pool_size[1];
    }
  }
#if SOFTWARE_MULTI_THREAD
  dec_param->size_opt = siz1;
  dec_param->tile_num = ceil_ratio(dec_param->width, dec_param->tile_width) * ceil_ratio(dec_param ->height, dec_param ->tile_height);//((dec_param->width + dec_param->tile_width - 1) / dec_param->tile_width) * ((dec_param ->height + dec_param->tile_height - 1) / dec_param->tile_height);
  dec_param->pool_size[0] = siz1 * dec_param->tile_num + siz2 + siz3;
#else
  dec_param->pool_size[0] = siz1 + siz2 + siz3;
#endif
  dec_param->pool_size[1] = siz4;

  return 0;
}

/**************************************************************************************/
/*                                  BMI_dec_init                                      */
/**************************************************************************************/
#if SOFTWARE_MULTI_THREAD
BMI_LIB_API int BMI_dec_init(stru_dec_param *dec_param, int use_j2k_header, int thread_num)
#else	// SOFTWARE_MULTI_THREAD
BMI_LIB_API int BMI_dec_init(stru_dec_param *dec_param, int use_j2k_header)
#endif	// SOFTWARE_MULTI_THREAD
{
  // max siz_extra/siz_persis_buffer is on first tile, while max siz_s_opt varies
  // mempool should be allocated for max of all 3 elements, so that
  // if s_opt[CNT_S_OPT] is init-ed in the last step, we can ensure won't have
  // memory overlap when init individual s_opt (ie, tiling, new frame...)

  int rc;
  int cnt_s_opt;
  stru_opt *s_opt;
  stru_pipe *s_pipe = (stru_pipe *)dec_param->mempool[0];
  stru_jp2_syntax *s_jp2file = &s_pipe->s_jp2file;

  stru_stream *stream = &s_pipe->stream;

  // have to parse bit-stream in enc() anyway, so the parsing is skipped here
  // without bit-stream, we are missing: cod->precinct, qcd->guard_bit, qcd->quant_step
  // uncomment this section if we do need above variables for BMI_dec_init()
  memset(s_jp2file, 0, sizeof(stru_jp2_syntax));
  if((!use_j2k_header || dec_param->input) && dec_read_header(dec_param, s_jp2file, stream, use_j2k_header))   return -1;

  // s_pipe is fully valid after dec_init_pipe()
#if SOFTWARE_MULTI_THREAD
  rc = dec_init_pipe(dec_param, thread_num);
#else
  rc = dec_init_pipe(dec_param);
#endif
  if(rc)    return rc;

  cnt_s_opt = (s_pipe->pipe_use) ? CNT_S_OPT : 1;
#if SOFTWARE_MULTI_THREAD
	cnt_s_opt *= dec_param->tile_num;
	{
		int i = 0;
		unsigned char * opt_memeory = (unsigned char *)s_pipe->s_opt;
		for (; i < cnt_s_opt; ++i)
		{
			s_opt = (stru_opt *)opt_memeory;
			s_opt->tile_idx = i;
			opt_memeory += dec_param->size_opt;
		}
	}
#else

  for(s_opt = &s_pipe->s_opt[0]; cnt_s_opt; --cnt_s_opt, ++s_opt) {
    dec_init_opt(dec_param, s_opt, 0, 0, stru_jp2_syntax *s_jp2file);
  }
#endif
  return 0;
}

/**************************************************************************************/
/*                                  BMI_dec_exit                                      */
/**************************************************************************************/
BMI_LIB_API int BMI_dec_exit(stru_dec_param *dec_param)
{
#if defined(ASIC)
  // remove all dspbios related stuff
  stru_pipe *s_pipe = (stru_pipe *)dec_param->mempool[0];
  stru_dspbios *s_dspbios = &s_pipe->s_dspbios;

  // delete tsk first
  bmi_TSK_delete(s_dspbios->tsk_dec_dwt);
  bmi_TSK_delete(s_dspbios->tsk_dec_t1_cb);
  bmi_TSK_delete(s_dspbios->tsk_dec_t1_mq);
  bmi_TSK_delete(s_dspbios->tsk_dec_t2);

  // then sem
  bmi_SEM_delete(s_dspbios->sem_fpga_cb);
  bmi_SEM_delete(s_dspbios->sem_fpga_mq);

  // then sem
  bmi_SEM_delete(s_dspbios->sem_t2_done);
  bmi_SEM_delete(s_dspbios->sem_dwt_done);
  bmi_SEM_delete(s_dspbios->sem_t1_cb_done);
  bmi_SEM_delete(s_dspbios->sem_t1_mq_done);

  // then sem
  bmi_SEM_delete(s_dspbios->sem_t2_start);
  bmi_SEM_delete(s_dspbios->sem_dwt_start);
  bmi_SEM_delete(s_dspbios->sem_t1_cb_start);
  bmi_SEM_delete(s_dspbios->sem_t1_mq_start);
#endif

  return 0;
}

#if defined(DEC_LIB_EXPORTS)
  BOOL APIENTRY DllMain( HANDLE hModule,
                         DWORD  ul_reason_for_call,
                         LPVOID lpReserved
             )
  {
      switch (ul_reason_for_call)
    {
      case DLL_PROCESS_ATTACH :
      case DLL_THREAD_ATTACH  :
      case DLL_THREAD_DETACH  :
      case DLL_PROCESS_DETACH :
        break;
      }
      return TRUE;
  }
#endif
