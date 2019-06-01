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
#include "codec_datatype.h"
/**************************************************************************************/
/*                                    dec_info                                        */
/**************************************************************************************/
#if SOFTWARE_MULTI_THREAD
int dec_info( stru_dec_param *dec_param, int pipe_use, int tx0, int ty0,
              int *siz_s_opt, int *siz_persis_buffer, int *siz_extra, int thread_num)
#else	// SOFTWARE_MULTI_THREAD
int dec_info( stru_dec_param *dec_param, int pipe_use, int tx0, int ty0,
			 int *siz_s_opt, int *siz_persis_buffer, int *siz_extra)
#endif	// SOFTWARE_MULTI_THREAD
{

#if !SOFTWARE_MULTI_THREAD
	int conf_mct = !!(dec_param->configuration & CONF_MCT);
	int conf_dwt97 = !!(dec_param->configuration & CONF_DWT97);
	int max_len;
#endif

  int cb_width, cb_height;
  int prec_width, prec_height;
  int num_band, num_prec, num_tree, num_node, num_cb, num_pass;
  int pass_per_block, cnt_s_opt, cnt_persis_buffer;
  int comp_num, num_component = ( dec_param->format == RAW)       ? 1 :
                                ( dec_param->format == RGB ||
                                  dec_param->format == YUV444 ||
                                  dec_param->format == YUV422 ||
                                  dec_param->format == YUV420 ||
                                  dec_param->format == BAYERGRB)  ? 3 : MAX_COMPONENT;
  int tx1 = tx0 + dec_param->tile_width;
  int ty1 = ty0 + dec_param->tile_height;
  int XRsiz[MAX_COMPONENT], YRsiz[MAX_COMPONENT];

  int tcx0, tcx1, tcy0, tcy1;
  int trx0, trx1, try0, try1;
  int tbx0, tbx1, tby0, tby1;
  int tpx0, tpx1, tpy0, tpy1;
  int tppx0, tppx1, tppy0, tppy1;
  int tpcbx0, tpcbx1, tpcby0, tpcby1;

  int nprec_bw, nprec_bh, ncb_pw, ncb_ph, x0b, y0b;
  int reso_num, level_num, band_num, ph_num, pw_num, last_band;
#if !SOFTWARE_MULTI_THREAD
  int siz_pass_fpga_buffer= 0, siz_cb_fpga = 0;
#endif
  int siz_pass_buffer = 0;
  int siz_cb_ctx = 0, siz_cb_data = 0;
  int siz_subband = 0, siz_precinct = 0;
  int siz_tagtree = 0, siz_tagtree_node = 0, siz_codeblock = 0, siz_codingpass = 0;
  int siz_subband2, siz_precinct2, siz_tagtree2, siz_tagtree_node2, siz_codeblock2, siz_codingpass2;

#if ! SOFTWARE_MULTI_THREAD
  int siz_ll, siz_hl, siz_lh, siz_hh, siz_vert, siz_inp, siz_out;
  int nrows, dwt_ext, nbwidth_ext, nbhigh_h, nblow_h, nbhigh_v, nblow_v;
  int siz_s_pipe, siz_dwt_proc_buffer;
  int siz_mct_g_buffer, siz_mct_r_buffer, siz_dwt_tmp_buffer;
  int siz_y, siz_uv, siz_dwt_buffer[MAX_COMPONENT];
#endif
  if(pipe_use) {
	  cnt_s_opt = CNT_S_OPT;
	  cnt_persis_buffer = CNT_PERSIS_BUF;
  } 
  else 
  {
    cnt_s_opt = 1;
    cnt_persis_buffer = 1;
  }
  pass_per_block = (dec_param->roi_shift + 8 * sizeof(int) - 2) * 3 + 1;

  // off chip
  // s_opt
  for(comp_num = 0; comp_num < num_component; ++comp_num) {

    YRsiz[comp_num] = ( (dec_param->format == YUV420 || dec_param->format == BAYERGRB) &&
                        comp_num) ? 2 : 1;
    XRsiz[comp_num] = ( (dec_param->format == YUV422 || dec_param->format == YUV420) &&
                        comp_num) ? 2 : 1;

    tcx0 = ceil_ratio(tx0, XRsiz[comp_num]);
    tcy0 = ceil_ratio(ty0, YRsiz[comp_num]);
    tcx1 = ceil_ratio(tx1, XRsiz[comp_num]);
    tcy1 = ceil_ratio(ty1, YRsiz[comp_num]);

    num_prec = 0, num_cb = 0;  num_node = 0;
    for(reso_num = dec_param->dwt_level; reso_num >= 0; --reso_num) {
      level_num = dec_param->dwt_level + 1 - ((reso_num) ? reso_num : 1);

      if(reso_num) {
        band_num = 1;
        last_band = 4;

        prec_width = dec_param->prec_width >> 1;
        prec_height = dec_param->prec_height >> 1;

        trx0 = ceil_ratio(tcx0, (1 << (level_num - 1)));
        try0 = ceil_ratio(tcy0, (1 << (level_num - 1)));
        trx1 = ceil_ratio(tcx1, (1 << (level_num - 1)));
        try1 = ceil_ratio(tcy1, (1 << (level_num - 1)));

        tpx0 = floor_ratio(trx0, (prec_width << 1));
        tpy0 = floor_ratio(try0, (prec_height << 1));
        tpx1 = ceil_ratio(trx1, (prec_width << 1));
        tpy1 = ceil_ratio(try1, (prec_height << 1));
      } else {
        band_num = 0;
        last_band = 1;

        prec_width = dec_param->prec_width >> 1;
        prec_height = dec_param->prec_height >> 1;

        if(dec_param->prec_width == 32768)    prec_width = 32768;
        if(dec_param->prec_height == 32768)   prec_height = 32768;

        trx0 = ceil_ratio(tcx0, (1 << level_num));
        try0 = ceil_ratio(tcy0, (1 << level_num));
        trx1 = ceil_ratio(tcx1, (1 << level_num));
        try1 = ceil_ratio(tcy1, (1 << level_num));

        tpx0 = floor_ratio(trx0, prec_width);
        tpy0 = floor_ratio(try0, prec_height);
        tpx1 = ceil_ratio(trx1, prec_width);
        tpy1 = ceil_ratio(try1, prec_height);
      }

      nprec_bw = (trx1 <= trx0) ? 0 : tpx1 - tpx0;
      nprec_bh = (try1 <= try0) ? 0 : tpy1 - tpy0;
      if(nprec_bw == 0 || nprec_bh == 0)  continue;

      cb_width = (dec_param->cb_width < prec_width) ? dec_param->cb_width : prec_width;
      cb_height = (dec_param->cb_height < prec_height) ? dec_param->cb_height : prec_height;

      for(; band_num < last_band; ++band_num) {
        num_prec += nprec_bw * nprec_bh;
        x0b = (band_num == BAND_HL || band_num == BAND_HH) ? 1 : 0;
        y0b = (band_num == BAND_LH || band_num == BAND_HH) ? 1 : 0;

        tbx0 = ceil_ratio(tcx0 - x0b * (1 << (level_num - 1)), (1 << level_num));
        tby0 = ceil_ratio(tcy0 - y0b * (1 << (level_num - 1)), (1 << level_num));
        tbx1 = ceil_ratio(tcx1 - x0b * (1 << (level_num - 1)), (1 << level_num));
        tby1 = ceil_ratio(tcy1 - y0b * (1 << (level_num - 1)), (1 << level_num));

        for(ph_num = 0; ph_num < nprec_bh; ++ph_num) {
          for(pw_num = 0; pw_num < nprec_bw; ++pw_num) {
            tppx0 = (tpx0 + pw_num) * prec_width;
            tppy0 = (tpy0 + ph_num) * prec_height;
            tppx1 = tppx0 + prec_width;
            tppy1 = tppy0 + prec_height;

            if(tbx0 > tppx0)  tppx0 = tbx0;
            if(tby0 > tppy0)  tppy0 = tby0;
            if(tbx1 < tppx1)  tppx1 = tbx1;
            if(tby1 < tppy1)  tppy1 = tby1;

            tpcbx0 = floor_ratio(tppx0, cb_width);
            tpcby0 = floor_ratio(tppy0, cb_height);
            tpcbx1 = ceil_ratio(tppx1, cb_width);
            tpcby1 = ceil_ratio(tppy1, cb_height);

            ncb_pw = (tppx1 <= tppx0) ? 0 : tpcbx1 - tpcbx0;
            ncb_ph = (tppy1 <= tppy0) ? 0 : tpcby1 - tpcby0;
            if(ncb_pw == 0 || ncb_ph == 0)  continue;

            num_node += tagtree_num_node(ncb_pw, ncb_ph);
            num_cb += ncb_pw * ncb_ph;
          }
        }
      }
    }

    num_node <<= 1;
    num_pass = num_cb * pass_per_block;
    num_band = 1 + dec_param->dwt_level * 3;
    num_tree = num_prec << 1;

    siz_subband2      = (num_band * sizeof(stru_subband) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_precinct2     = (num_prec * sizeof(stru_precinct) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_tagtree2      = (num_tree * sizeof(stru_tagtree_node) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_tagtree_node2 = (num_node * sizeof(stru_tagtree_node) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_codeblock2    = (num_cb * sizeof(stru_code_block) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_codingpass2   = (num_pass * sizeof(stru_coding_pass) + DSP_ALIGN) & ~DSP_ALIGN;

    siz_subband += siz_subband2;
    siz_precinct += siz_precinct2;
    siz_tagtree += siz_tagtree2;
    siz_tagtree_node += siz_tagtree_node2;
    siz_codeblock += siz_codeblock2;
    siz_codingpass += siz_codingpass2;
  }

#if !SOFTWARE_MULTI_THREAD
  // persis_buffer
  // siz_y/siz_uv: pixel size without bitdepth
  siz_y = dec_param->tile_width * dec_param->tile_height;
  switch(dec_param->format) {
  case RAW :
    siz_uv = 0;
    break;
  case BAYERGRB :
    siz_uv = (((dec_param->tile_height + 1) >> 1) * dec_param->tile_width) << 1;
    break;
  case YUV422 :
    siz_uv = (((dec_param->tile_width + 1) >> 1) * dec_param->tile_height) << 1;
    break;
  case YUV420 :
    siz_uv = (((dec_param->tile_width + 1) >> 1) * ((dec_param->tile_height + 1) >> 1)) << 1;
    break;
  default :                   // RGB, YUV444
    siz_uv = siz_y;
    break;
  }

  siz_dwt_buffer[0] = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  for(comp_num = 1; comp_num < num_component; ++comp_num)
    siz_dwt_buffer[comp_num] = (siz_uv * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;

  // scratch_buffer
  if(conf_mct) {
    siz_mct_g_buffer = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_mct_r_buffer = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  } else {
    siz_mct_g_buffer = 0;
    siz_mct_r_buffer = 0;
  }

  siz_dwt_tmp_buffer = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;

#if defined(ASIC)
  // off chip
  siz_pass_fpga_buffer = (MQ_FPGA_SIZE * sizeof(unsigned int) + DSP_ALIGN) & ~DSP_ALIGN;

  // on chip
  siz_cb_fpga = (cb_width * cb_height * sizeof(short) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_cb_fpga <<= 1;
#else
  // off chip
  siz_pass_buffer = (dec_param->cb_width * dec_param->height * sizeof(unsigned int)) << 1;
  siz_pass_buffer = (siz_pass_buffer * sizeof(unsigned char) + DSP_ALIGN) & ~DSP_ALIGN;

  // on chip
  siz_cb_ctx = (cb_width + 2) * (cb_height + 2);
  siz_cb_ctx = (siz_cb_ctx * sizeof(unsigned short) + DSP_ALIGN) & ~DSP_ALIGN;

  // on chip
  siz_cb_data = cb_width * cb_height;
  siz_cb_data = (siz_cb_data * sizeof(unsigned int) + DSP_ALIGN) & ~DSP_ALIGN;
#endif

  // on chip

  // cache alignment : inp, inp_g, inp_b, prev_verth/vert_h/vert_l
  // chche alignment : out_ll, out_hl, out_lh, out_hh, out
  dwt_ext = (conf_dwt97) ? 4 : 2;                 // dwt boundary extension
  nrows = (conf_dwt97) ? (MIN_DWT_NROWS << 1) : MIN_DWT_NROWS;
  nbwidth_ext = MAX_BDWT_WIDTH + (dwt_ext << 1);
  if(nbwidth_ext > dec_param->tile_width) {       // not enough for dwt extension
    nbwidth_ext = dec_param->tile_width;          // adjust the width of dwt block
  }

  nbhigh_h = (nbwidth_ext + 1) >> 1;    // size of horzental high in one dwt block
  nblow_h = (nbwidth_ext + 1) >> 1;     // size of horzental low in one dwt block
  nbhigh_v = (nrows + 1) >> 1;          // size of vertical high in one dwt block
  nblow_v = (nrows + 1) >> 1;           // size of vertical low in one dwt block

  siz_ll = (nblow_v * nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_hl = (nblow_v * nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_lh = (nbhigh_v * nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_hh = (nbhigh_v * nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_vert = (4 * nbwidth_ext * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_out = ((nrows + 1) * nbwidth_ext * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  siz_inp = siz_ll + siz_hl + siz_lh + siz_hh;

  max_len = siz_vert + ((siz_inp + (1 + (conf_mct << 1)) * siz_out) << 1);
  siz_dwt_proc_buffer = (max_len + DSP_ALIGN) & ~DSP_ALIGN;

  // off chip
  siz_s_pipe = (sizeof(stru_pipe) + DSP_ALIGN) & ~DSP_ALIGN;

  // s_opt * cnt_s_opt
  *siz_s_opt = (siz_subband + siz_precinct + siz_tagtree +
                siz_tagtree_node + siz_codeblock + siz_codingpass) * cnt_s_opt;

  // persis_buffer * cnt_persis_buffer
  *siz_persis_buffer = 0;
  for(comp_num = 0; comp_num < num_component; ++comp_num)
    *siz_persis_buffer += siz_dwt_buffer[comp_num];
  *siz_persis_buffer *= cnt_persis_buffer;
  // extra buffer * 1
  *siz_extra =  siz_mct_g_buffer + siz_mct_r_buffer + siz_dwt_tmp_buffer +
                siz_pass_fpga_buffer + siz_pass_buffer + siz_s_pipe;

  dec_param->pool_size[1] = siz_cb_ctx + siz_cb_data +
                            siz_cb_fpga + siz_dwt_proc_buffer;
#else	//  SOFTWARE_MULTI_THREAD
	// s_opt * cnt_s_opt
	*siz_s_opt = (sizeof(stru_opt) + siz_subband + siz_precinct + siz_tagtree +
				  siz_tagtree_node + siz_codeblock + siz_codingpass) * cnt_s_opt;

	siz_pass_buffer = (dec_param->cb_width * dec_param->height * sizeof(unsigned int)) << 1;
	siz_pass_buffer = (siz_pass_buffer * sizeof(unsigned char) + DSP_ALIGN) & ~DSP_ALIGN;

#if SOFTWARE_MULTI_THREAD
	siz_cb_ctx = (cb_width + 3) * (((cb_height + 3)>>2) + 2) + 1;
	if (siz_cb_ctx < 1600)
	{
		siz_cb_ctx = 1600;
	}
	siz_cb_ctx = siz_cb_ctx * sizeof(int_32);

	// on chip
	siz_cb_data = ((cb_height + 3) & ~3) * cb_width;
	if (siz_cb_data < 4096) 
	{
		siz_cb_data = 4096;
	}
	siz_cb_data = siz_cb_data * sizeof(int_32);
#else
	// on chip
	siz_cb_ctx = (cb_width + 2) * (cb_height + 2);
	siz_cb_ctx = (siz_cb_ctx * sizeof(unsigned short) + DSP_ALIGN) & ~DSP_ALIGN;

	// on chip
	siz_cb_data = cb_width * cb_height;
	siz_cb_data = (siz_cb_data * sizeof(unsigned int) + DSP_ALIGN) & ~DSP_ALIGN;
#endif



  *siz_extra =  siz_pass_buffer  * thread_num + sizeof(stru_pipe);

  dec_param->pool_size[1] = (siz_cb_ctx + siz_cb_data) * thread_num; ;

#endif	//  SOFTWARE_MULTI_THREAD

  return 0;
}

/**************************************************************************************/
/*                                 dec_init_pipe                                      */
/**************************************************************************************/
#if SOFTWARE_MULTI_THREAD
int dec_init_pipe(stru_dec_param *dec_param, int thread_num)
#else	// SOFTWARE_MULTI_THREAD
int dec_init_pipe(stru_dec_param *dec_param)
#endif	// SOFTWARE_MULTI_THREAD
{
#if !SOFTWARE_MULTI_THREAD
	int siz_mct_g_buffer, siz_mct_r_buffer, siz_dwt_tmp_buffer;
	int siz_y, siz_uv, siz_dwt_buffer[MAX_COMPONENT];
	stru_persis_buffer *persis_buffer;
	int siz_s_pipe, siz_cb_fpga, siz_dwt_proc_buffer, comp_num, cnt_persis_buffer, siz_s_opt;
	int conf_mct = !!(dec_param->configuration & CONF_MCT)
	int num_component = ( dec_param->format == RAW)       ? 1 :
		( dec_param->format == RGB ||
		dec_param->format == YUV444 ||
		dec_param->format == YUV422 ||
		dec_param->format == YUV420 ||
		dec_param->format == BAYERGRB)  ? 3 : MAX_COMPONENT;
#endif
	stru_opt *s_opt;
  int rc = 0;

  int cb_width = dec_param->cb_width;
  int cb_height = dec_param->cb_height;
  int max_len, cnt_s_opt;
  int siz_cb_ctx, siz_cb_data ;
  int /*siz_pass_fpga_buffer, */siz_pass_buffer;

  stru_pipe *s_pipe;
//   stru_dspbios *s_dspbios;
  stru_scratch_buffer *scratch_buffer;

  unsigned char *mempool[2];
  mempool[0] = (unsigned char *)dec_param->mempool[0];
  mempool[1] = (unsigned char *)dec_param->mempool[1];

  // off chip
  // fill s_pipe
#if !SOFTWARE_MULTI_THREAD
  siz_s_pipe = (sizeof(stru_pipe) + DSP_ALIGN) & ~DSP_ALIGN;
  s_pipe = (stru_pipe *)mempool[0];
  mempool[0] += siz_s_pipe;
  s_pipe->pipe_use = !!(dec_param->configuration & CONF_VIDEO);
  if(s_pipe->pipe_use) {
	  cnt_s_opt = CNT_S_OPT;
	  cnt_persis_buffer = CNT_PERSIS_BUF;
  } else {
	  cnt_s_opt = 1;
	  cnt_persis_buffer = 1;
  }


  // fill persis_buffer * cnt_persis_buffer
  // siz_y/siz_uv: pixel size without bitdepth
  siz_y = dec_param->tile_width * dec_param->tile_height;
  switch(dec_param->format) {
  case RAW :
	  siz_uv = 0;
	  break;
  case BAYERGRB :
	  siz_uv = (((dec_param->tile_height + 1) >> 1) * dec_param->tile_width) << 1;
	  break;
  case YUV422 :
	  siz_uv = (((dec_param->tile_width + 1) >> 1) * dec_param->tile_height) << 1;
	  break;
  case YUV420 :
	  siz_uv = (((dec_param->tile_width + 1) >> 1) * ((dec_param->tile_height + 1) >> 1)) << 1;
	  break;
  default :                   // RGB, YUV444
	  siz_uv = siz_y;
	  break;
  }
  siz_dwt_buffer[0] = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  for(comp_num = 1; comp_num < num_component; ++comp_num)
	  siz_dwt_buffer[comp_num] = (siz_uv * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;

  for(persis_buffer = &s_pipe->persis_buffer[0];
	  cnt_persis_buffer; --cnt_persis_buffer, ++persis_buffer) {
		  persis_buffer->dwt_buffer[0] = (int *)mempool[0];
		  mempool[0] += siz_dwt_buffer[0];
		  for(comp_num = 1; comp_num < num_component; ++comp_num) {
			  persis_buffer->dwt_buffer[comp_num] = (int *)mempool[0];
			  mempool[0] += siz_dwt_buffer[comp_num];
		  }
  }
  scratch_buffer = &s_pipe->scratch_buffer;

  // fill scratch_buffer
  if(conf_mct) {
    siz_mct_g_buffer = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_mct_r_buffer = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
  } else {
    siz_mct_g_buffer = 0;
    siz_mct_r_buffer = 0;
  }

  scratch_buffer->mct_g_buffer = (int *)mempool[0];
  mempool[0] += siz_mct_g_buffer;

  scratch_buffer->mct_r_buffer = (int *)mempool[0];
  mempool[0] += siz_mct_r_buffer;


  siz_dwt_tmp_buffer = (siz_y * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->dwt_tmp_buffer = (int *)mempool[0];
  mempool[0] += siz_dwt_tmp_buffer;

#if defined(ASIC)
  // off chip
  siz_pass_fpga_buffer = (MQ_FPGA_SIZE * sizeof(unsigned int) + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->pass_fpga_buffer = (unsigned int *)mempool[0];
  mempool[0] += siz_pass_fpga_buffer;

  // on chip
  siz_cb_fpga = (cb_width * cb_height * sizeof(short) + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->cb_fpga[0] = (short *)mempool[1];
  scratch_buffer->cb_fpga[1] = (short *)(mempool[1] + siz_cb_fpga);
  siz_cb_fpga <<= 1;
  mempool[1] += siz_cb_fpga;
#else
  // off chip
  siz_pass_buffer = (dec_param->cb_width * dec_param->height * sizeof(unsigned int)) << 1;
  siz_pass_buffer = (siz_pass_buffer * sizeof(unsigned char) + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->pass_buffer = (unsigned char *)mempool[0];
  mempool[0] += siz_pass_buffer;

  // on chip
  siz_cb_ctx = (cb_width + 2) * (cb_height + 2);
  siz_cb_ctx = (siz_cb_ctx * sizeof(unsigned short) + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->cb_ctx = (unsigned short *)mempool[1];
  mempool[1] += siz_cb_ctx;

  // on chip
  siz_cb_data = cb_width * cb_height;
  siz_cb_data = (siz_cb_data * sizeof(unsigned int) + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->cb_data = (unsigned int *)mempool[1];
  mempool[1] += siz_cb_data;
#endif

  // on chip
  max_len = ((unsigned char *)dec_param->mempool[1] + dec_param->pool_size[1]) - mempool[1];
  siz_dwt_proc_buffer = (max_len + DSP_ALIGN) & ~DSP_ALIGN;

  scratch_buffer->dwt_proc_size = siz_dwt_proc_buffer;
  scratch_buffer->dwt_proc_buffer = (int *)mempool[1];
  mempool[1] += siz_dwt_proc_buffer;
#else	// !SOFTWARE_MULTI_THREAD 
  s_pipe = (stru_pipe *)mempool[0];
  mempool[0] += sizeof(stru_pipe);
  s_pipe->pipe_use = !!(dec_param->configuration & CONF_VIDEO);
  if(s_pipe->pipe_use) {
	  cnt_s_opt = CNT_S_OPT * dec_param->tile_num;
  } else {
	  cnt_s_opt = dec_param->tile_num;
  }

	scratch_buffer = &s_pipe->scratch_buffer;

	scratch_buffer->pass_buffer = (unsigned char *)mempool[0];
	siz_pass_buffer = (dec_param->cb_width * dec_param->height * sizeof(unsigned int)) << 1;
	siz_pass_buffer = (siz_pass_buffer * sizeof(unsigned char) + DSP_ALIGN) & ~DSP_ALIGN;
	scratch_buffer->size_pass_buf = siz_pass_buffer;
	siz_pass_buffer *= thread_num;
	mempool[0] += siz_pass_buffer;

#if SOFTWARE_MULTI_THREAD

	scratch_buffer->cb_ctx = (int_32 *)mempool[1];
	siz_cb_ctx = (cb_width + 3) * (((cb_height + 3)>>2) + 2) + 1;
	if (siz_cb_ctx < 1600)
	{
		siz_cb_ctx = 1600;
	}
	siz_cb_ctx = siz_cb_ctx * sizeof(int_32);
	scratch_buffer->size_ctx_buf = siz_cb_ctx;
	siz_cb_ctx *= thread_num;
	mempool[1] += siz_cb_ctx;

	scratch_buffer->cb_data = (int_32 *)mempool[1];
	siz_cb_data = ((cb_height + 3) & ~3) * cb_width;
	if (siz_cb_data < 4096) siz_cb_data = 4096;
	siz_cb_data = siz_cb_data * sizeof(int_32);
	scratch_buffer->size_data_buf = siz_cb_data;
	siz_cb_data *= thread_num;
	mempool[1] += siz_cb_data;

#else

	scratch_buffer->cb_ctx = (unsigned short *)mempool[1];
	siz_cb_ctx = (cb_width + 2) * (cb_height + 2);
	siz_cb_ctx = (siz_cb_ctx * sizeof(unsigned short) + DSP_ALIGN) & ~DSP_ALIGN;
	scratch_buffer->size_ctx_buf = siz_cb_ctx;
	siz_cb_ctx *= thread_num;
	mempool[1] += siz_cb_ctx;

	scratch_buffer->cb_data = (unsigned int *)mempool[1];
	siz_cb_data = cb_width * cb_height;
	siz_cb_data = (siz_cb_data * sizeof(unsigned int) + DSP_ALIGN) & ~DSP_ALIGN;
	scratch_buffer->size_data_buf = siz_cb_data;
	siz_cb_data *= thread_num;
	mempool[1] += siz_cb_data;
#endif
	max_len = (int)(((unsigned char *)dec_param->mempool[1] + dec_param->pool_size[1]) - mempool[1]);

#endif	// !SOFTWARE_MULTI_THREAD

  // on chip

#if defined(ASIC)    // fill s_dspbios
  s_dspbios = &s_pipe->s_dspbios;

  s_dspbios->sem_t2_start = bmi_SEM_create(0);
  s_dspbios->sem_dwt_start = bmi_SEM_create(0);
  s_dspbios->sem_t1_cb_start = bmi_SEM_create(0);
  s_dspbios->sem_t1_mq_start = bmi_SEM_create(0);

  s_dspbios->sem_t2_done = bmi_SEM_create(0);
  s_dspbios->sem_dwt_done = bmi_SEM_create(0);
  s_dspbios->sem_t1_cb_done = bmi_SEM_create(0);
  s_dspbios->sem_t1_mq_done = bmi_SEM_create(0);

  s_dspbios->sem_fpga_cb = bmi_SEM_create(0);
  s_dspbios->sem_fpga_mq = bmi_SEM_create(0);

  // priority order: t1_cb > t1_mq > dwt > t2
  s_dspbios->tsk_dec_t2 = bmi_TSK_create(tsk_dec_t2, s_pipe, 6, 4096);
  s_dspbios->tsk_dec_dwt = bmi_TSK_create(tsk_dec_dwt, s_pipe, 7, 4096);
  s_dspbios->tsk_dec_t1_mq = bmi_TSK_create(tsk_dec_t1_mq, s_pipe, 8, 4096);
  s_dspbios->tsk_dec_t1_cb = bmi_TSK_create(tsk_dec_t1_cb, s_pipe, 9, 4096);

  if( s_dspbios->sem_t2_start == NULL || s_dspbios->sem_dwt_start == NULL ||
      s_dspbios->sem_t1_cb_start == NULL || s_dspbios->sem_t1_mq_start == NULL ||
      s_dspbios->sem_t2_done == NULL || s_dspbios->sem_dwt_done == NULL ||
      s_dspbios->sem_t1_cb_done == NULL || s_dspbios->sem_t1_mq_done == NULL ||
      s_dspbios->sem_fpga_cb == NULL || s_dspbios->sem_fpga_mq == NULL ||
      s_dspbios->tsk_dec_t2 == NULL || s_dspbios->tsk_dec_dwt == NULL ||
      s_dspbios->tsk_dec_t1_mq == NULL || s_dspbios->tsk_dec_t1_cb == NULL) {
    rc = -1;
  }
#endif

  // according to the way we allocate memory,
  // the remaining should be able to be divided by cnt_s_opt
#if SOFTWARE_MULTI_THREAD
  s_pipe->s_opt = (stru_opt * )mempool[0];
  {
	  int i = 0;
	  for(; i < cnt_s_opt; ++i)
	  {
		  s_opt = (stru_opt * )mempool[0];
		  s_opt->t2_scan_component = 0;
		  s_opt->t2_scan_layer = 0;
		  s_opt->t2_scan_resolution = 0;

		  s_opt->pointer_holder = mempool[0] + sizeof(stru_opt);
		  mempool[0] += dec_param->size_opt;
	  }
  }
#else
  siz_s_opt = ( dec_param->pool_size[0] + (unsigned char *)dec_param->mempool[0] -
                  mempool[0]) / cnt_s_opt;

  // starting address for s_opt
  for(s_opt = &s_pipe->s_opt[0]; cnt_s_opt; --cnt_s_opt, ++s_opt) {
    s_opt->pointer_holder = mempool[0];
    mempool[0] += siz_s_opt;
  }
#endif

  // active s_opt for dwt/t1/t2
  s_pipe->idx_opt_dwt = 0;
  s_pipe->idx_opt_t1 = 0;
  s_pipe->idx_opt_t2 = 0;

  // active persis_buffer for dwt/t1/t2
  s_pipe->idx_persis_dwt = 0;
  s_pipe->idx_persis_t1 = 0;
  s_pipe->idx_persis_t2 = 0;

  s_pipe->pipe_pause = 3;

  return rc;
}

/**************************************************************************************/
/*                                 dec_init_opt                                       */
/**************************************************************************************/
int dec_init_opt(stru_dec_param *dec_param, stru_opt *s_opt, int tx0, int ty0)
{
  int pass_per_block;
  int ll_width, ll_height;
  int cb_width, cb_height;
  int prec_width, prec_height;
  int comp_num, num_component = ( dec_param->format == RAW)       ? 1 :
                                ( dec_param->format == RGB ||
                                  dec_param->format == YUV444 ||
                                  dec_param->format == YUV422 ||
                                  dec_param->format == YUV420 ||
                                  dec_param->format == BAYERGRB)    ? 3 : MAX_COMPONENT;
  int tx1 = tx0 + dec_param->tile_width;
  int ty1 = ty0 + dec_param->tile_height;

  int tcx0, tcx1, tcy0, tcy1;
  int trx0, trx1, try0, try1;
  int tbx0, tbx1, tby0, tby1;
  int tpx0, tpx1, tpy0, tpy1;
  int tcbx0, tcbx1, tcby0, tcby1;
  int tppx0, tppx1, tppy0, tppy1;
  int tpcbx0, tpcbx1, tpcby0, tpcby1;
  int tbcbx0, tbcbx1, tbcby0, tbcby1;

  int nprec_bw, nprec_bh, ncb_bw, ncb_bh, ncb_pw, ncb_ph, x0b, y0b;
  int reso_num, level_num, band_num, ph_num, pw_num, cbh_num, cbw_num;
  int num_prec, num_cb, num_node, num_pass, last_band, idx_band;

  int siz_subband2, siz_precinct2, siz_codeblock2, siz_codingpass2, siz_tagtree_node2, siz_tagtree2;

  stru_tile_component *cur_comp;
  stru_subband *cur_band;
  stru_precinct *cur_prec;
  stru_tagtree *cur_tree;
  stru_tagtree_node *cur_node;
  stru_code_block *cur_cb;
  stru_coding_pass *cur_pass;

  unsigned char *tile_data_memory = (unsigned char *)s_opt->pointer_holder;

  pass_per_block = (dec_param->roi_shift + 8 * sizeof(int) - 2) * 3 + 1;

  s_opt->roi_shift      = dec_param->roi_shift;
  s_opt->num_component  = num_component;

  s_opt->conf_dwt97     = !!(dec_param->configuration & CONF_DWT97);
  s_opt->conf_mct       = !!(dec_param->configuration & CONF_MCT);
  s_opt->conf_quant     = !!(dec_param->configuration & CONF_QUANT);
  s_opt->conf_dwti53    = s_opt->conf_quant && (!s_opt->conf_dwt97);

  // off chip
  // fill component for s_opt
  for(cur_comp = s_opt->component, comp_num = 0;
      comp_num < num_component; ++comp_num, ++cur_comp) {

    s_opt->YRsiz[comp_num] = ((dec_param->format == YUV420 || dec_param->format == BAYERGRB) &&
                              comp_num) ? 2 : 1;
    s_opt->XRsiz[comp_num] = ((dec_param->format == YUV422 || dec_param->format == YUV420) &&
                              comp_num) ? 2 : 1;

    tcx0 = ceil_ratio(tx0, s_opt->XRsiz[comp_num]);
    tcy0 = ceil_ratio(ty0, s_opt->YRsiz[comp_num]);
    tcx1 = ceil_ratio(tx1, s_opt->XRsiz[comp_num]);
    tcy1 = ceil_ratio(ty1, s_opt->YRsiz[comp_num]);

    num_prec = 0, num_cb = 0;  num_node = 0;
    for(reso_num = dec_param->dwt_level; reso_num >= 0; --reso_num) {
      level_num = dec_param->dwt_level + 1 - ((reso_num) ? reso_num : 1);

      if(reso_num) {
        band_num = 1;
        last_band = 4;

        prec_width = dec_param->prec_width >> 1;
        prec_height = dec_param->prec_height >> 1;

        trx0 = ceil_ratio(tcx0, (1 << (level_num - 1)));
        try0 = ceil_ratio(tcy0, (1 << (level_num - 1)));
        trx1 = ceil_ratio(tcx1, (1 << (level_num - 1)));
        try1 = ceil_ratio(tcy1, (1 << (level_num - 1)));

        tpx0 = floor_ratio(trx0, (prec_width << 1));
        tpy0 = floor_ratio(try0, (prec_height << 1));
        tpx1 = ceil_ratio(trx1, (prec_width << 1));
        tpy1 = ceil_ratio(try1, (prec_height << 1));

        cur_comp->width[level_num - 1] = trx1 - trx0;
        cur_comp->height[level_num - 1] = try1 - try0;
      } else {
        band_num = 0;
        last_band = 1;

        prec_width = dec_param->prec_width >> 1;
        prec_height = dec_param->prec_height >> 1;

        if(dec_param->prec_width == 32768)    prec_width = 32768;
        if(dec_param->prec_height == 32768)   prec_height = 32768;

        trx0 = ceil_ratio(tcx0, (1 << level_num));
        try0 = ceil_ratio(tcy0, (1 << level_num));
        trx1 = ceil_ratio(tcx1, (1 << level_num));
        try1 = ceil_ratio(tcy1, (1 << level_num));

        tpx0 = floor_ratio(trx0, prec_width);
        tpy0 = floor_ratio(try0, prec_height);
        tpx1 = ceil_ratio(trx1, prec_width);
        tpy1 = ceil_ratio(try1, prec_height);
      }

      nprec_bw = (trx1 <= trx0) ? 0 : tpx1 - tpx0;
      nprec_bh = (try1 <= try0) ? 0 : tpy1 - tpy0;
      if(nprec_bw == 0 || nprec_bh == 0)  continue;

      cb_width = (dec_param->cb_width < prec_width) ? dec_param->cb_width : prec_width;
      cb_height = (dec_param->cb_height < prec_height) ? dec_param->cb_height : prec_height;

      for(; band_num < last_band; ++band_num) {
        num_prec += nprec_bw * nprec_bh;
        x0b = (band_num == BAND_HL || band_num == BAND_HH) ? 1 : 0;
        y0b = (band_num == BAND_LH || band_num == BAND_HH) ? 1 : 0;

        tbx0 = ceil_ratio(tcx0 - x0b * (1 << (level_num - 1)), (1 << level_num));
        tby0 = ceil_ratio(tcy0 - y0b * (1 << (level_num - 1)), (1 << level_num));
        tbx1 = ceil_ratio(tcx1 - x0b * (1 << (level_num - 1)), (1 << level_num));
        tby1 = ceil_ratio(tcy1 - y0b * (1 << (level_num - 1)), (1 << level_num));

        for(ph_num = 0; ph_num < nprec_bh; ++ph_num) {
          for(pw_num = 0; pw_num < nprec_bw; ++pw_num) {
            tppx0 = (tpx0 + pw_num) * prec_width;
            tppy0 = (tpy0 + ph_num) * prec_height;
            tppx1 = tppx0 + prec_width;
            tppy1 = tppy0 + prec_height;

            if(tbx0 > tppx0)  tppx0 = tbx0;
            if(tby0 > tppy0)  tppy0 = tby0;
            if(tbx1 < tppx1)  tppx1 = tbx1;
            if(tby1 < tppy1)  tppy1 = tby1;

            tpcbx0 = floor_ratio(tppx0, cb_width);
            tpcby0 = floor_ratio(tppy0, cb_height);
            tpcbx1 = ceil_ratio(tppx1, cb_width);
            tpcby1 = ceil_ratio(tppy1, cb_height);

            ncb_pw = (tppx1 <= tppx0) ? 0 : tpcbx1 - tpcbx0;
            ncb_ph = (tppy1 <= tppy0) ? 0 : tpcby1 - tpcby0;
            if(ncb_pw == 0 || ncb_ph == 0)  continue;

            num_node += tagtree_num_node(ncb_pw, ncb_ph);
            num_cb += ncb_pw * ncb_ph;
          }
        }
      }
    }

    num_node <<= 1;
    num_pass = num_cb * pass_per_block;

    cur_comp->num_band = 1 + dec_param->dwt_level * 3;
    cur_comp->num_prec  = num_prec;
    cur_comp->num_tree  = num_prec << 1;
    cur_comp->num_node = num_node;
    cur_comp->num_cb = num_cb;
    cur_comp->num_pass = num_pass;

    siz_subband2      = (cur_comp->num_band * sizeof(stru_subband) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_precinct2     = (cur_comp->num_prec * sizeof(stru_precinct) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_tagtree2      = (cur_comp->num_tree * sizeof(stru_tagtree_node) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_tagtree_node2 = (cur_comp->num_node * sizeof(stru_tagtree_node) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_codeblock2    = (cur_comp->num_cb * sizeof(stru_code_block) + DSP_ALIGN) & ~DSP_ALIGN;
    siz_codingpass2   = (cur_comp->num_pass * sizeof(stru_coding_pass) + DSP_ALIGN) & ~DSP_ALIGN;

    cur_band = cur_comp->band = (stru_subband *)tile_data_memory;
    tile_data_memory += siz_subband2;

    cur_prec = cur_comp->precinct = (stru_precinct *)tile_data_memory;
    tile_data_memory += siz_precinct2;

    cur_tree = cur_comp->tree = (stru_tagtree *)tile_data_memory;
    tile_data_memory += siz_tagtree2;

    cur_node = cur_comp->node = (stru_tagtree_node *)tile_data_memory;
    tile_data_memory += siz_tagtree_node2;

    cur_cb = cur_comp->codeblock = (stru_code_block *)tile_data_memory;
    tile_data_memory += siz_codeblock2;

    cur_pass = cur_comp->pass = (stru_coding_pass *)tile_data_memory;
    tile_data_memory += siz_codingpass2;


    // setup code block and percinct structure by the order of LL, HL, LH, HH
    // setup structures
    idx_band = 0;

    for(reso_num = 0; reso_num <= dec_param->dwt_level; ++reso_num) {
      level_num = dec_param->dwt_level + 1 - ((reso_num) ? reso_num : 1);

      if(reso_num) {
        band_num = 1;
        last_band = 4;

        prec_width = dec_param->prec_width >> 1;
        prec_height = dec_param->prec_height >> 1;

        trx0 = ceil_ratio(tcx0, (1 << (level_num - 1)));
        try0 = ceil_ratio(tcy0, (1 << (level_num - 1)));
        trx1 = ceil_ratio(tcx1, (1 << (level_num - 1)));
        try1 = ceil_ratio(tcy1, (1 << (level_num - 1)));

        tpx0 = floor_ratio(trx0, (prec_width << 1));
        tpy0 = floor_ratio(try0, (prec_height << 1));
        tpx1 = ceil_ratio(trx1, (prec_width << 1));
        tpy1 = ceil_ratio(try1, (prec_height << 1));
      } else {
        band_num = 0;
        last_band = 1;

        prec_width = dec_param->prec_width >> 1;
        prec_height = dec_param->prec_height >> 1;

        if(dec_param->prec_width == 32768)    prec_width = 32768;
        if(dec_param->prec_height == 32768)   prec_height = 32768;

        trx0 = ceil_ratio(tcx0, (1 << level_num));
        try0 = ceil_ratio(tcy0, (1 << level_num));
        trx1 = ceil_ratio(tcx1, (1 << level_num));
        try1 = ceil_ratio(tcy1, (1 << level_num));

        tpx0 = floor_ratio(trx0, prec_width);
        tpy0 = floor_ratio(try0, prec_height);
        tpx1 = ceil_ratio(trx1, prec_width);
        tpy1 = ceil_ratio(try1, prec_height);
      }

      nprec_bw = (trx1 <= trx0) ? 0 : tpx1 - tpx0;
      nprec_bh = (try1 <= try0) ? 0 : tpy1 - tpy0;

      cb_width = (dec_param->cb_width < prec_width) ? dec_param->cb_width : prec_width;
      cb_height = (dec_param->cb_height < prec_height) ? dec_param->cb_height : prec_height;

      for(; band_num < last_band; ++band_num, ++cur_band) {
        x0b = (band_num == BAND_HL || band_num == BAND_HH) ? 1 : 0;
        y0b = (band_num == BAND_LH || band_num == BAND_HH) ? 1 : 0;

        tbx0 = ceil_ratio(tcx0 - x0b * (1 << (level_num - 1)), (1 << level_num));
        tby0 = ceil_ratio(tcy0 - y0b * (1 << (level_num - 1)), (1 << level_num));
        tbx1 = ceil_ratio(tcx1 - x0b * (1 << (level_num - 1)), (1 << level_num));
        tby1 = ceil_ratio(tcy1 - y0b * (1 << (level_num - 1)), (1 << level_num));

        ll_width = ceil_ratio(tcx1, (1 << level_num)) - ceil_ratio(tcx0, (1 << level_num));
        ll_height = ceil_ratio(tcy1, (1 << level_num)) - ceil_ratio(tcy0, (1 << level_num));

#if SOFTWARE_MULTI_THREAD
		cur_band->width = (band_num == BAND_LL || band_num == BAND_LH) ? ll_width : (ceil_ratio(tcx1, (1 << (level_num-1))) - ceil_ratio(tcx0, (1 << (level_num-1))) - ll_width) ;
		cur_band->height =  (band_num == BAND_HL || band_num == BAND_HH) ? ll_height : (ceil_ratio(tcy1, (1 << (level_num-1))) - ceil_ratio(tcy0, (1 << (level_num-1))) - ll_height) ;
#endif 
        tbcbx0 = floor_ratio(tbx0, cb_width);
        tbcby0 = floor_ratio(tby0, cb_height);
        tbcbx1 = ceil_ratio(tbx1, cb_width);
        tbcby1 = ceil_ratio(tby1, cb_height);

     
		ncb_bw = (tbx1 <= tbx0) ? 0 : tbcbx1 - tbcbx0;
        ncb_bh = (tby1 <= tby0) ? 0 : tbcby1 - tbcby0;

        cur_band->ncb_bw = ncb_bw;
        cur_band->ncb_bh = ncb_bh;
        cur_band->codeblock = cur_cb;

        for(cbh_num = 0; cbh_num < ncb_bh; ++cbh_num) {
          for(cbw_num = 0; cbw_num < ncb_bw; ++cbw_num) {
            tcbx0 = (tbcbx0 + cbw_num) * cb_width;
            tcby0 = (tbcby0 + cbh_num) * cb_height;
            tcbx1 = tcbx0 + cb_width;
            tcby1 = tcby0 + cb_height;

            cur_cb->x0 = (tcbx0 > tbx0) ? tcbx0 : tbx0;
            cur_cb->y0 = (tcby0 > tby0) ? tcby0 : tby0;
            cur_cb->width = (tcbx1 < tbx1) ? tcbx1 : tbx1;
            cur_cb->height = (tcby1 < tby1) ? tcby1 : tby1;
            cur_cb->width -= cur_cb->x0;
            cur_cb->height -= cur_cb->y0;
            cur_cb->x0 -= tbx0;
            cur_cb->y0 -= tby0;
            cur_cb->x0 += ll_width * x0b;
            cur_cb->y0 += ll_height * y0b;
            cur_cb->pass = cur_pass;
            cur_pass += pass_per_block;
// #if _DEBUG
// 			{
// 				static int c = 0;
// 				printf("[%d] Init cb pos [%d:%d] size [%d:%d], pass %x\n",++c,  cur_cb->x0, cur_cb->y0, cur_cb->width, cur_cb->height, (void *)cur_cb->pass);
// 			}
// #endif          
			++cur_cb;
          }
        }

        cur_band->trx0 = trx0;
        cur_band->try0 = try0;
        cur_band->nprec_bw = nprec_bw;
        cur_band->nprec_bh = nprec_bh;
        cur_band->precinct = cur_prec;

        num_node = 0;
        for(ph_num = 0; ph_num < nprec_bh; ++ph_num) {
          for(pw_num = 0; pw_num < nprec_bw; ++pw_num) {
            tppx0 = (tpx0 + pw_num) * prec_width;
            tppy0 = (tpy0 + ph_num) * prec_height;
            tppx1 = tppx0 + prec_width;
            tppy1 = tppy0 + prec_height;

            if(tbx0 > tppx0)  tppx0 = tbx0;
            if(tby0 > tppy0)  tppy0 = tby0;
            if(tbx1 < tppx1)  tppx1 = tbx1;
            if(tby1 < tppy1)  tppy1 = tby1;

            tpcbx0 = floor_ratio(tppx0, cb_width);
            tpcby0 = floor_ratio(tppy0, cb_height);
            tpcbx1 = ceil_ratio(tppx1, cb_width);
            tpcby1 = ceil_ratio(tppy1, cb_height);

            ncb_pw = (tppx1 <= tppx0) ? 0 : tpcbx1 - tpcbx0;
            ncb_ph = (tppy1 <= tppy0) ? 0 : tpcby1 - tpcby0;

            tpcbx0 -= tbcbx0;
            tpcby0 -= tbcby0;

            cur_prec->resolution = reso_num;
            cur_prec->subband = band_num;

            cur_prec->ncb_pw = ncb_pw;
            cur_prec->ncb_ph = ncb_ph;

            cur_prec->ncb_bw = ncb_bw;
            cur_prec->ncb_bw0 = tpcbx0;
            cur_prec->ncb_bh0 = tpcby0;
            cur_prec->codeblock = cur_band->codeblock + tpcbx0 + tpcby0 * ncb_bw;

            cur_prec->incltree = cur_tree;
            ++cur_tree;
            cur_prec->imsbtree = cur_tree;
            ++cur_tree;

            cur_prec->num_node = tagtree_num_node(ncb_pw, ncb_ph);
            cur_prec->inclnode = cur_node;
            cur_node += cur_prec->num_node;
            cur_prec->imsbnode = cur_node;
            cur_node += cur_prec->num_node;
            num_node += (cur_prec->num_node << 1);
            ++cur_prec;
          }
        }

        cur_band->num_node = num_node;
        cur_band->incltree = cur_band->precinct->incltree;
        cur_band->imsbtree = cur_band->precinct->imsbtree;
        cur_band->inclnode = cur_band->precinct->inclnode;
        cur_band->imsbnode = cur_band->precinct->imsbnode;

        cur_band->resolution = reso_num;
        cur_band->subband = band_num;
        cur_band->cnt = 0;

/*
        if(!s_opt->conf_quant){
          int p, n, lgs, gain, stepsize;
          static const double dwt_norms_real[4][10] = {
	          {1.000, 1.965, 4.177, 8.403, 16.90, 33.84, 67.69, 135.3, 270.6, 540.9},
	          {2.022, 3.989, 8.355, 17.04, 34.27, 68.63, 137.3, 274.6, 549.0},
	          {2.022, 3.989, 8.355, 17.04, 34.27, 68.63, 137.3, 274.6, 549.0},
	          {2.080, 3.865, 8.307, 17.18, 34.71, 69.59, 139.3, 278.6, 557.2}
          };

          if(s_opt->conf_dwt97 || s_opt->conf_dwti53) {
            gain = 0;
            stepsize = (1 << 13) / dwt_norms_real[band_num][dec_param->dwt_level - reso_num];
          } else {
            gain = (!band_num) ? 0 : (band_num == 3) ? 2 : 1;
            stepsize = 1 << (13 - s_opt->conf_mct);
          }

          lgs = int_log2(stepsize);
          p =  lgs - 13;
          n = lgs - 11;

          cur_band->expn = dec_param->bitdepth + gain - p;
          cur_band->mant = (n > 0 ? stepsize >> n : stepsize << -n) & 0x7ff;
          cur_band->numbps = cur_band->expn + s_opt->guard_bit - 1;

          // real stepsize : delta = pow(2, p) * (1 + mant/(pow(2, 11))
          // stepsize = pow(2, 13) / delta
          // stepsize = pow(2, 26) / (pow(2, 13) * delta)
          // stepsize = pow(2, 26) / (pow(2, lgs) + (pow(2, n) * mant))

          if(n > 0) {
            cur_band->stepsize = (1 << 26) / ((1 << lgs) + (cur_band->mant << n));
          } else {
            cur_band->stepsize = (1 << 26) / ((1 << lgs) + (cur_band->mant >> -n));
          }
        } else {
          cur_band->expn = dec_param->quant[idx_band] >> 16;
          cur_band->mant = dec_param->quant[idx_band] & 0xffff;
          cur_band->numbps = cur_band->expn + s_opt->guard_bit - 1;
        }
*/
      }
    }
  }

  return 0;
}
