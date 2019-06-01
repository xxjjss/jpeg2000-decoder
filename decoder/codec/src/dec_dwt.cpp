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

#if !SOFTWARE_MULTI_THREAD

static int dec_dc_shift_mct(int *inp_r, int *inp_g, int *inp_b,
                            int comp_num, int conf_mct, int dc_offset,
                            int width, int irows, int sample_bytes);
static int dec_dc_shift_i53ct(int *inp_r, int *inp_g, int *inp_b,
                              int comp_num, int conf_mct, int dc_offset,
                              int width_ext, int irows, int sample_bytes,
                              int bitdepth, int guard_bit);

static int dec_dwt53_horz(int * restrict out, int *pinl, int *pinh,
                          int even, int width, int dwt_ext, int b_ext);
static int dec_dwti53_horz(int * restrict out, int *pinl, int *pinh,
                           int even, int width, int dwt_ext, int b_ext);

/**************************************************************************************/
/* static                         dec_dc_shift_mct                                    */
/**************************************************************************************/
static int dec_dc_shift_mct(int *inp_r, int *inp_g, int *inp_b,
                            int comp_num, int conf_mct, int dc_offset,
                            int width, int irows, int sample_bytes)
{
  int r, g, b;
  int n = irows * width - 1;
  int max = (dc_offset << 1) - 1;

  int * restrict src_r32 = inp_r;
  int * restrict src_g32 = inp_g;
  int * restrict src_b32 = inp_b;

  unsigned char * restrict dst_r8 = (unsigned char *)inp_r;
  unsigned char * restrict dst_g8 = (unsigned char *)inp_g;
  unsigned char * restrict dst_b8 = (unsigned char *)inp_b;

  unsigned short * restrict dst_r16 = (unsigned short *)inp_r;
  unsigned short * restrict dst_g16 = (unsigned short *)inp_g;
  unsigned short * restrict dst_b16 = (unsigned short *)inp_b;

  if(conf_mct && comp_num < 3) {        // mct
    if(comp_num == 2) {
      // some issue when use (restrict) pointer in mct
      if(sample_bytes == 1) {           // int to char
        for(; n >= 0; --n, ++dst_r8, ++dst_g8, ++dst_b8, ++src_r32, ++src_g32, ++src_b32) {
          g = *src_r32 - ((*src_g32 + *src_b32) >> 2) + dc_offset;
          r = *src_b32 + g;
          b = *src_g32 + g;

          if(r > max)   r = max;    if(r < 0)   r = 0;
          if(g > max)   g = max;    if(g < 0)   g = 0;
          if(b > max)   b = max;    if(b < 0)   b = 0;

          *dst_r8 = (unsigned char)r;
          *dst_g8 = (unsigned char)g;
          *dst_b8 = (unsigned char)b;
        }
      } else {                          // int to short
        for(; n >= 0; --n, ++dst_r16, ++dst_g16, ++dst_b16, ++src_r32, ++src_g32, ++src_b32) {
          g = *src_r32 - ((*src_g32 + *src_b32) >> 2) + dc_offset;
          r = *src_b32 + g;
          b = *src_g32 + g;

          if(r > max)   r = max;    if(r < 0)   r = 0;
          if(g > max)   g = max;    if(g < 0)   g = 0;
          if(b > max)   b = max;    if(b < 0)   b = 0;

          *dst_r16 = (unsigned short)r;
          *dst_g16 = (unsigned short)g;
          *dst_b16 = (unsigned short)b;
        }
      }
    }
  } else {                              // dc shift
    if(sample_bytes == 1) {             // int to char
      for(; n >= 0; --n, ++dst_b8, ++src_b32) {
        b = *src_b32 + dc_offset;
        if(b > max)   b = max;    if(b < 0)   b = 0;
        *dst_b8 = (unsigned char)b;
      }
    } else {                            // int to short
      for(; n >= 0; --n, ++dst_b16, ++src_b32) {
        b = *src_b32 + dc_offset;
        if(b > max)   b = max;    if(b < 0)   b = 0;
        *dst_b16 = (unsigned short)b;
      }
    }
  }

  return 0;
}

/**************************************************************************************/
/* static                             dec_dwt53_horz                                    */
/**************************************************************************************/
static int dec_dwt53_horz(int * restrict out, int *pinl, int *pinh,
                          int even, int width, int dwt_ext, int b_ext)
{
  int ext;
  int cnt = 3;
  int *prev_poutl = out + !even;      // dependent pointers?

  if(width == 1) {
    if(even)    *out = pinl[0];
    else        *out = pinh[0] >> 1;
    return 0;
  }

  if(b_ext & 0x2) {                                                   // begin
    pinl += (dwt_ext >> 1);
    pinh += (dwt_ext >> 1);
    if(even) {
      *out++ = pinl[0] - ((pinh[-1] + pinh[0] + 2) >> 2);
      ++cnt;    ++pinl;
    } else {
      ext = pinl[-1] - ((pinh[-1] + pinh[0] + 2) >> 2);
      prev_poutl = &ext;
    }
  } else if(even) {
    *out++ = pinl[0] - ((pinh[0] + 1) >> 1);
    ++cnt;    ++pinl;
  }

  for(; cnt <= width; cnt += 2, ++pinh, ++pinl, out += 2) {           // middle
    out[1] = pinl[0] - ((pinh[0] + pinh[1] + 2) >> 2);
    out[0] = pinh[0] + ((*prev_poutl + out[1]) >> 1);
    prev_poutl = out + 1;
  }

  if((width & 1) ^ !even) {                                           // end
    if(b_ext & 0x1)   out[1] = pinl[0] - ((pinh[0] + pinh[1] + 2) >> 2);
    else              out[1] = pinl[0] - ((pinh[0] + 1) >> 1);
    *out = pinh[0] + ((*prev_poutl + out[1]) >> 1);
  } else {
    if(b_ext & 0x1)   ext = pinl[0] - ((pinh[0] + pinh[1] + 2) >> 2);
    else              ext = *prev_poutl;
    *out = pinh[0] + ((*prev_poutl + ext) >> 1);
  }

  return 0;
}

/**************************************************************************************/
/*                                        dec_dwt_53                                  */
/**************************************************************************************/
int dec_dwt_53(stru_dec_param *dec_param, stru_opt *s_opt,
               stru_persis_buffer *persis_buffer, stru_scratch_buffer *scratch_buffer,
               int comp_num)
{
  stru_tile_component *cur_comp = s_opt->component + comp_num;

  int thumb_num   = dec_param->dwt_level - s_opt->reso_thumb + 1;
  int thumb_width = ceil_ratio(dec_param->width, (1 << (thumb_num - 1)));
  int thumb_x0    = ceil_ratio(s_opt->tx0, (1 << (thumb_num - 1)));
  int thumb_y0    = ceil_ratio(s_opt->ty0, (1 << (thumb_num - 1)));
  int thumb_tcx0  = ceil_ratio(thumb_x0, s_opt->XRsiz[comp_num]);
  int thumb_tcy0  = ceil_ratio(thumb_y0, s_opt->YRsiz[comp_num]);

  int n, reso_num = dec_param->dwt_level + 1;
  int tbx0_low, tbx1_low, tby0_low, tby1_low, tbx0_high, tbx1_high, tby0_high, tby1_high;
  int width, height, width_low, width_high, height_low, height_high, even_v, even_h;
  int idx = 0, cnt, mct, first_dma, nrows, irows, rrows, ihrows, ilrows, orows, grrows;
  int nbwidth, rbwidth, nbwidth_ext, b_ext;       // used by block based dwt
  int dwt_ext = (s_opt->conf_dwt97) ? 4 : 2;      // dwt boundary extension
  int nblow_h, nbhigh_h, nblow_v, nbhigh_v;       // low/high of bdwt in one dma

  int pitch = ceil_ratio(thumb_width, s_opt->XRsiz[comp_num]);
  int sample_bytes = (dec_param->bitdepth + 7) >> 3;
  int dc_offset = 1 << (dec_param->bitdepth - 1);

  int comp_width = cur_comp->width[0];
  int comp_height = cur_comp->height[0];
  int tcx0 = ceil_ratio(s_opt->tx0, s_opt->XRsiz[comp_num]);
  int tcy0 = ceil_ratio(s_opt->ty0, s_opt->YRsiz[comp_num]);
  int tcx1 = tcx0 + comp_width;
  int tcy1 = tcy0 + comp_height;

  int *vert_l, *vert_h, *prev_vert_h, *prev_horz_l;
  int * restrict pin0, * restrict pin1, * restrict pin2;
  int *poutl, *prev_poutl;            // dependent pointers?
  int *pouth;

  unsigned char * restrict out_buf;
  unsigned char * restrict out_g_buf =  (unsigned char *)dec_param->output[1] +
                                        (thumb_tcx0 + thumb_tcy0 * pitch) * sample_bytes;
  unsigned char * restrict out_r_buf =  (unsigned char *)dec_param->output[0] +
                                        (thumb_tcx0 + thumb_tcy0 * pitch) * sample_bytes;
#if !SOFTWARE_MULTI_THREAD
  int * restrict inp_g_buf = scratch_buffer->mct_g_buffer;
  int * restrict inp_r_buf = scratch_buffer->mct_r_buffer;
#endif // !SOFTWARE_MULTI_THREAD
  int * restrict inp_l_buf, * restrict inp_h_buf;

  int * restrict inp_ll[2], * restrict inp_hl[2];
  int * restrict inp_lh[2], * restrict inp_hh[2], * restrict inp[2];
  int * restrict out[2], * restrict out_g[2], * restrict out_r[2];
  int siz_vert, siz_inp, siz_out, siz_ll, siz_hl, siz_lh, siz_hh;
  int off_out, off_inp_h, off_inp_l, off_out_gr;

  unsigned int id_ll = DAT_XFRID_WAITNONE;
  unsigned int id_out[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_g[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_out_g[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_r[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_out_r[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_ll[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_hl[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_lh[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_hh[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};

  while(--reso_num != thumb_num - 1) {
    tbx0_low  = ceil_ratio(tcx0, (1 << reso_num));
    tby0_low  = ceil_ratio(tcy0, (1 << reso_num));
    tbx1_low  = ceil_ratio(tcx1, (1 << reso_num));
    tby1_low  = ceil_ratio(tcy1, (1 << reso_num));

    tbx0_high = ceil_ratio(tcx0 - (1 << (reso_num - 1)), (1 << reso_num));
    tby0_high = ceil_ratio(tcy0 - (1 << (reso_num - 1)), (1 << reso_num));
    tbx1_high = ceil_ratio(tcx1 - (1 << (reso_num - 1)), (1 << reso_num));
    tby1_high = ceil_ratio(tcy1 - (1 << (reso_num - 1)), (1 << reso_num));

    width_low   = tbx1_low - tbx0_low;
    width_high  = tbx1_high - tbx0_high;
    height_low  = tby1_low - tby0_low;
    height_high = tby1_high - tby0_high;

    width       = width_low + width_high;
    height      = height_low + height_high;

    even_v      = (tby0_high < tby0_low) ? 0 : 1;
    even_h      = (tbx0_high < tbx0_low) ? 0 : 1;

    if(!width || !height)    continue;            // zero length subband, start next dwt loop

    bmi_CACHE_wbInvAllL2(CACHE_WAIT);

    // update variables & pointers
    if(reso_num != thumb_num) {
      pitch = comp_width;
      sample_bytes = sizeof(int);

      inp_l_buf = scratch_buffer->dwt_tmp_buffer;
      out_buf = (unsigned char *)persis_buffer->dwt_buffer[comp_num];

      // copy LL from out_buf to inp_l_buf (dwt_ll_buf)
      for(n = 0; n < height; ++n) {
        bmi_CACHE_wbInvL2_my(out_buf + n * comp_width,
                          (width * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      bmi_CACHE_wbInvL2_my(inp_l_buf,
                        ((width * sizeof(int) * height + DSP_ALIGN) & ~DSP_ALIGN),
                        CACHE_WAIT);
      id_ll = bmi_DAT_copy2d( DAT_2D1D, out_buf, inp_l_buf,
                              width * sizeof(int), height,
                              comp_width * sizeof(int));
    } else {
      if(thumb_num != 1) {    // have to use dwt_tmp_buffer for thunbnail decoding
        inp_l_buf = scratch_buffer->dwt_tmp_buffer;
        out_buf = (unsigned char *)persis_buffer->dwt_buffer[comp_num];

        // copy LL from out_buf to inp_l_buf (dwt_ll_buf)
        for(n = 0; n < height; ++n) {
          bmi_CACHE_wbInvL2_my(out_buf + n * comp_width,
                            (width * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        bmi_CACHE_wbInvL2_my(inp_l_buf,
                          ((width * sizeof(int) * height + DSP_ALIGN) & ~DSP_ALIGN),
                          CACHE_WAIT);
        id_ll = bmi_DAT_copy2d( DAT_2D1D, out_buf, inp_l_buf,
                                width * sizeof(int), height,
                                comp_width * sizeof(int));
      } else {
        inp_l_buf = persis_buffer->dwt_buffer[comp_num];
      }

      if(s_opt->conf_mct && comp_num < 2) {       // g/r component for mct operation
        pitch = width;
        sample_bytes = sizeof(int);

        if(comp_num)    out_buf = (unsigned char *)scratch_buffer->mct_g_buffer;
        else            out_buf = (unsigned char *)scratch_buffer->mct_r_buffer;
      } else {
        pitch = ceil_ratio(thumb_width, s_opt->XRsiz[comp_num]);
        sample_bytes = (dec_param->bitdepth + 7) >> 3;

        out_buf =   (unsigned char *)dec_param->output[comp_num] +
                    (thumb_tcx0 + thumb_tcy0 * pitch) * sample_bytes;
      }
    }

    mct = (s_opt->conf_mct && reso_num == thumb_num && comp_num == 2) ? 1 : 0;

    // first dwt block : init
    nbwidth = MAX_BDWT_WIDTH;
    rbwidth = width;
    b_ext = 0x1;                        // b'01, left extension = 0, right extension = 1

    do {                                // block based dwt
      if(nbwidth + dwt_ext > rbwidth) { // not enough for dwt extension
        nbwidth = rbwidth;              // adjust the width of dwt block
        b_ext &= 0x2;                   // b'x0, right extension = 0;
      }

      nbwidth_ext = nbwidth;
      if(b_ext & 0x2)   nbwidth_ext += dwt_ext;   // left extension
      if(b_ext & 0x1)   nbwidth_ext += dwt_ext;   // right extension

      off_out   = 0;                    // out_buf offset
      off_inp_h = 0;                    // inp_h_buf offset
      off_inp_l = 0;                    // inp_l_buf offset
      off_out_gr = 0;                   // out_gr_buf offset
      first_dma = 1;                    // first DMA
      rrows     = height;               // remaining rows

      // nrows: number of rows for one DMA, have to be able to divided by 4(53) or 8(97)
      // L2_SRAM = sizeof(int) * (2*nrows + 2*3*(nrows+1) + 4) * width  : mct
      // L2_SRAM = sizeof(int) * (2*nrows + 2*(nrows+1) + 4) * width    : otherwise
      // L2_SRAM = sizeof(int) * (8*nrows + 10) * width                 : mct
      // L2_SRAM = sizeof(int) * (4*nrows + 6) * width                  : otherwise
      nrows = ((scratch_buffer->dwt_proc_size - (6 + mct * 4) * nbwidth_ext * sizeof(int)) /
              ((mct + 1) * 4 * nbwidth_ext * sizeof(int))) & ~3;

      // cache alignment : out, out_g, out_r, prev_vert_h/vert_h/vert_l
      // chche alignment : inp_ll, inp_hl, inp_lh, inp_hh, inp
      // siz_vert             : nbwidth_ext
      // siz_out, siz_out_g/b : nbwidth
      // siz_inp              : nbwidth_ext
      for(; nrows; nrows -= 4) {
        nbhigh_h = (nbwidth_ext + !even_h) >> 1;  // size of horzental high in one dwt block
        nblow_h = (nbwidth_ext + even_h) >> 1;    // size of horzental low in one dwt block
        nbhigh_v = (nrows + !even_v) >> 1;        // size of vertical high in one dwt block
        nblow_v = (nrows + even_v) >> 1;          // size of vertical low in one dwt block

        siz_ll = (nblow_v * nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_hl = (nblow_v * nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_lh = (nbhigh_v * nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_hh = (nbhigh_v * nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_vert = (4 * nbwidth_ext * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_out = ((nrows + 1) * nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_inp = siz_ll + siz_hl + siz_lh + siz_hh;

        if( (siz_vert + ((siz_inp + (1 + (mct << 1)) * siz_out) << 1)) <=
            scratch_buffer->dwt_proc_size) {
          break;
        }
      }
      if(s_opt->conf_dwt97)     nrows &= ~7;
      if(nrows > rrows)         nrows = rrows;

      // setup on chip memory partition, ping-pong style
      prev_vert_h = (int *)scratch_buffer->dwt_proc_buffer;
      vert_h = prev_vert_h + nbwidth_ext;
      vert_l = vert_h + nbwidth_ext;
      prev_horz_l = vert_l + nbwidth_ext;
      inp[0] = (int *)((char *)prev_vert_h + siz_vert);
      inp[1] = (int *)((char *)inp[0] + siz_inp);
      out[0] = (int *)((char *)inp[1] + siz_inp);
      out[1] = (int *)((char *)out[0] + siz_out);
      if(mct) {
        out_g[0] = (int *)((char *)out  [1] + siz_out);
        out_g[1] = (int *)((char *)out_g[0] + siz_out);
        out_r[0] = (int *)((char *)out_g[1] + siz_out);
        out_r[1] = (int *)((char *)out_r[0] + siz_out);
      }

      inp_ll[0] = (int *)((char *)inp[0]);
      inp_hl[0] = (int *)((char *)inp_ll[0] + siz_ll);
      inp_lh[0] = (int *)((char *)inp_hl[0] + siz_hl);
      inp_hh[0] = (int *)((char *)inp_lh[0] + siz_lh);

      inp_ll[1] = (int *)((char *)inp[1]);
      inp_hl[1] = (int *)((char *)inp_ll[1] + siz_ll);
      inp_lh[1] = (int *)((char *)inp_hl[1] + siz_hl);
      inp_hh[1] = (int *)((char *)inp_lh[1] + siz_lh);
      inp_h_buf = inp_l_buf + height_low * width;

      // tmp number of rows for one DMA
      // 53: odd: (0-12)-(34-56)..., even: (01-23)-(45-67)...
      // 97: odd: (0-1234)-(5678-9abc)..., even: (0123-4567)-(89ab-cdef)...
      irows = nrows - ((!even_v && rrows > nrows) ? ((s_opt->conf_dwt97) ? 3 : 1) : 0);

      // double buffer DMA
      bmi_DAT_wait(id_ll);                  // done copying LL to new buffer
      if(mct) {
        grrows = irows - (rrows > irows);
        for(n = 0; n < grrows; ++n) {
          bmi_CACHE_wbInvL2_my(inp_g_buf + off_out_gr + n * width,
                            (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        id_inp_g[idx] = bmi_DAT_copy2d( DAT_2D1D, inp_g_buf + off_out_gr, out_g[idx],
                                        nbwidth * sizeof(int), grrows,
                                        width * sizeof(int));
        for(n = 0; n < grrows; ++n) {
          bmi_CACHE_wbInvL2_my(inp_r_buf + off_out_gr + n * width,
                            (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        id_inp_r[idx] = bmi_DAT_copy2d( DAT_2D1D, inp_r_buf + off_out_gr, out_r[idx],
                                        nbwidth * sizeof(int), grrows,
                                        width * sizeof(int));
        off_out_gr += grrows * width;
      }
      for(n = 0; n < (irows + even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + n * width,
                          (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_ll[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_l_buf + off_inp_l, inp_ll[idx],
                                      nblow_h * sizeof(int), ((irows + even_v) >> 1),
                                      width * sizeof(int));
      for(n = 0; n < (irows + even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + width_low + n * width,
                          (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_hl[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_l_buf + off_inp_l + width_low, inp_hl[idx],
                                      nbhigh_h * sizeof(int), ((irows + even_v) >> 1),
                                      width * sizeof(int));
      for(n = 0; n < (irows + !even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + n * width,
                          (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_lh[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_h_buf + off_inp_h, inp_lh[idx],
                                      nblow_h * sizeof(int), ((irows + !even_v) >> 1),
                                      width * sizeof(int));
      for(n = 0; n < (irows + !even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + width_low + n * width,
                          (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_hh[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_h_buf + off_inp_h + width_low, inp_hh[idx],
                                      nbhigh_h * sizeof(int), ((irows + !even_v) >> 1),
                                      width * sizeof(int));
      off_inp_l += (irows >> 1) * width;
      off_inp_h += ((irows + 1) >> 1) * width;

      rrows -= irows;
      orows = irows - !!rrows;          // orows == grrows
      if(nrows > rrows)         nrows = rrows;

      // brings arrays into the cache
//      touch(vert_l,      nbwidth_ext * sizeof(int));
//      touch(vert_h,      nbwidth_ext * sizeof(int));
//      touch(prev_vert_h, nbwidth_ext * sizeof(int));

      do {
  /////////////////////// mct/dwt core
        if(nrows) {
          if(mct) {
            grrows = nrows + (rrows == nrows);
            for(n = 0; n < grrows; ++n) {
              bmi_CACHE_wbInvL2_my(inp_g_buf + off_out_gr + n * width,
                                (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                                CACHE_WAIT);
            }
           id_inp_g[!idx] = bmi_DAT_copy2d(DAT_2D1D, inp_g_buf + off_out_gr, out_g[!idx],
                                           nbwidth * sizeof(int), grrows,
                                           width * sizeof(int));
            for(n = 0; n < grrows; ++n) {
              bmi_CACHE_wbInvL2_my(inp_r_buf + off_out_gr + n * width,
                                (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                                CACHE_WAIT);
            }
           id_inp_r[!idx] = bmi_DAT_copy2d(DAT_2D1D, inp_r_buf + off_out_gr, out_r[!idx],
                                           nbwidth * sizeof(int), grrows,
                                           width * sizeof(int));
          off_out_gr += grrows * width;
          }
          for(n = 0; n < (nrows + 1) >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + n * width,
                              (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_ll[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_l_buf + off_inp_l, inp_ll[!idx],
                                            nblow_h * sizeof(int), ((nrows + 1) >> 1),
                                            width * sizeof(int));
          for(n = 0; n < (nrows + 1) >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + width_low + n * width,
                              (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_hl[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_l_buf + off_inp_l + width_low, inp_hl[!idx],
                                            nbhigh_h * sizeof(int), ((nrows + 1) >> 1),
                                            width * sizeof(int));
          for(n = 0; n < nrows >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + n * width,
                              (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_lh[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_h_buf + off_inp_h, inp_lh[!idx],
                                            nblow_h * sizeof(int), (nrows >> 1),
                                            width * sizeof(int));
          for(n = 0; n < nrows >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + width_low + n * width,
                              (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_hh[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_h_buf + off_inp_h + width_low, inp_hh[!idx],
                                            nbhigh_h * sizeof(int), (nrows >> 1),
                                            width * sizeof(int));
          off_inp_l += ((nrows + 1) >> 1) * width;
          off_inp_h += (nrows >> 1) * width;
        }

        bmi_DAT_wait(id_out[idx]);
        bmi_DAT_wait(id_inp_ll[idx]);
        bmi_DAT_wait(id_inp_hl[idx]);
        bmi_DAT_wait(id_inp_hh[idx]);
        bmi_DAT_wait(id_inp_lh[idx]);
        if(mct) {
          bmi_DAT_wait(id_inp_g[idx]);
          bmi_DAT_wait(id_inp_r[idx]);
          bmi_DAT_wait(id_out_g[idx]);
          bmi_DAT_wait(id_out_r[idx]);
        }

        // dwt
        ihrows = 0;                     // number of high processed in one DMA
        ilrows = 0;                     // number of low  processed in one DMA
        if(!first_dma) {                                              // begin
          cnt = 2;
          pouth = out[idx];
        } else {
          cnt = 3;

          if(even_v) {
            pin0 = vert_l;
            prev_poutl = poutl = out[idx];
            dec_dwt53_horz( vert_l,
                          inp_ll[idx] + ilrows * nblow_h,
                          inp_hl[idx] + ilrows * nbhigh_h,
                          even_h, nbwidth, dwt_ext, b_ext);
            ++ilrows;

            if(irows != 1) {
              dec_dwt53_horz( prev_vert_h,
                            inp_lh[idx] + ihrows * nblow_h,
                            inp_hh[idx] + ihrows * nbhigh_h,
                            even_h, nbwidth, dwt_ext, b_ext);
              ++ihrows;

              for(pin1 = prev_vert_h, n = nbwidth; n; --n, ++poutl, ++pin0, ++pin1) {
                *poutl = *pin0 - ((*pin1 + 1) >> 1);
              }
            } else {                      // one rows only
              for(n = nbwidth; n; --n, ++poutl, ++pin0) {
                *poutl = *pin0;
              }
            }

            ++cnt;
            pouth = poutl;
          } else {
            pouth = out[idx];
            prev_poutl = pouth + nbwidth;
            dec_dwt53_horz( prev_vert_h,
                          inp_lh[idx] + ihrows * nblow_h,
                          inp_hh[idx] + ihrows * nbhigh_h,
                          even_h, nbwidth, dwt_ext, b_ext);
            ++ihrows;
          }
        }

        for(; cnt <= irows; cnt += 2) {                               // middle
          dec_dwt53_horz( vert_h,
                        inp_lh[idx] + ihrows * nblow_h,
                        inp_hh[idx] + ihrows * nbhigh_h,
                        even_h, nbwidth, dwt_ext, b_ext);
          ++ihrows;
          dec_dwt53_horz( vert_l,
                        inp_ll[idx] + ilrows * nblow_h,
                        inp_hl[idx] + ilrows * nbhigh_h,
                        even_h, nbwidth, dwt_ext, b_ext);
          ++ilrows;

          for(pin0 = prev_vert_h, pin1 = vert_l, pin2 = vert_h,
              poutl = pouth + nbwidth, n = nbwidth;
              n; --n, ++pin0, ++pin1, ++pin2, ++poutl, ++pouth, ++prev_poutl) {
            *poutl = *pin1 - ((*pin0 + *pin2 + 2) >> 2);
            *pouth = *pin0 + ((*prev_poutl + *poutl) >> 1);
          }

          prev_poutl = vert_h;
          vert_h = prev_vert_h;
          prev_vert_h = prev_poutl;

          prev_poutl = pouth;
          pouth = poutl;
        }

        if(!nrows) {                                                  // end
          if(irows == 1 && first_dma) {
            if(!even_v) {
              for(pin0 = prev_vert_h, n = nbwidth; n; --n, ++pouth, ++pin0) {
                *pouth = *pin0 >> 1;
              }
            }       // !even_v is done in "begin" section
          } else {
            if((irows & 1) ^ (!even_v && first_dma)) {
              dec_dwt53_horz( vert_l,
                            inp_ll[idx] + ilrows * nblow_h,
                            inp_hl[idx] + ilrows * nbhigh_h,
                            even_h, nbwidth, dwt_ext, b_ext);
              ++ilrows;
              for(pin0 = prev_vert_h, pin1 = vert_l, poutl = pouth + nbwidth, n = nbwidth;
                  n; --n, ++pin0, ++pin1, ++poutl, ++pouth, ++prev_poutl) {
                *poutl = *pin1 - ((*pin0 + 1) >> 1);
                *pouth = *pin0 + ((*prev_poutl + *poutl) >> 1);
              }
            } else {
              for(pin0 = prev_vert_h, n = nbwidth; n; --n, ++pin0, ++pouth, ++prev_poutl) {
                *pouth = *pin0 + *prev_poutl;
              }
            }
          }
        }

        if(reso_num == thumb_num) {                                   // dcshift/mct
          // copy prev_poutl to a tmp buffer
          memcpy(prev_horz_l, prev_poutl, nbwidth * sizeof(int));
          prev_poutl = prev_horz_l;

          // dcshift/mct
          dec_dc_shift_mct( out_r[idx], out_g[idx], out[idx],
                            comp_num, s_opt->conf_mct, dc_offset,
                            nbwidth, orows, sample_bytes);
        }

  /////////////////////// mct/dwt core

        if(mct) {
          for(n = 0; n < orows; ++n) {
            bmi_CACHE_wbInvL2_my(out_g_buf + off_out + n * nbwidth * sample_bytes,
                              (nbwidth * sample_bytes + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_out_g[idx] = bmi_DAT_copy2d( DAT_1D2D, out_g[idx], out_g_buf + off_out,
                                          nbwidth * sample_bytes, orows,
                                          pitch * sample_bytes);
          for(n = 0; n < orows; ++n) {
            bmi_CACHE_wbInvL2_my(out_r_buf + off_out + n * pitch * sample_bytes,
                              (nbwidth * sample_bytes + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_out_r[idx] = bmi_DAT_copy2d( DAT_1D2D, out_r[idx], out_r_buf + off_out,
                                          nbwidth * sample_bytes, orows,
                                          pitch * sample_bytes);
        }
        for(n = 0; n < orows; ++n) {
          bmi_CACHE_wbInvL2_my(out_buf + off_out + n * pitch * sample_bytes,
                            (nbwidth * sample_bytes + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        id_out[idx] = bmi_DAT_copy2d( DAT_1D2D, out[idx], out_buf + off_out,
                                      nbwidth * sample_bytes, orows,
                                      pitch * sample_bytes);
        off_out += orows * pitch * sample_bytes;

        idx = !idx;
        first_dma = 0;
        irows = nrows;
        rrows -= nrows;
        orows = nrows + !rrows;         // orows == grrows
        if(nrows > rrows)         nrows = rrows;
      } while(irows);                   // one dwt block done

      // going to update memory partition, wait till all buf done
      bmi_DAT_wait(id_ll);
      bmi_DAT_wait(id_out[!idx]);
      bmi_DAT_wait(id_inp_ll[idx]);
      bmi_DAT_wait(id_inp_hl[idx]);
      bmi_DAT_wait(id_inp_hh[idx]);
      bmi_DAT_wait(id_inp_lh[idx]);
      if(mct) {
        bmi_DAT_wait(id_inp_g[idx]);
        bmi_DAT_wait(id_inp_r[idx]);
        bmi_DAT_wait(id_out_g[!idx]);
        bmi_DAT_wait(id_out_r[!idx]);
      }

      // update block based dwt
      inp_l_buf += ((nbwidth + even_h) >> 1);
      out_buf   += nbwidth * sample_bytes;
      if(!(b_ext & 0x2)) {
        inp_l_buf   -= (dwt_ext >> 1);
      }

      if(mct) {
        inp_g_buf += nbwidth;
        inp_r_buf += nbwidth;
        out_g_buf += nbwidth * sample_bytes;
        out_r_buf += nbwidth * sample_bytes;
      }

      rbwidth -= nbwidth;
      b_ext |= 0x2;                     // b'1x, left extension = 1

    } while(rbwidth);                   // block based dwt
  }
/*
  // done processing one component
  bmi_DAT_wait(id_ll);                      // done copying LL to new buffer

  for(n = 0; n < 2; ++n) {
    bmi_DAT_wait(id_inp[n]);
    bmi_DAT_wait(id_inp_g[n]);
    bmi_DAT_wait(id_inp_b[n]);
    bmi_DAT_wait(id_out_g[n]);
    bmi_DAT_wait(id_out_b[n]);
    bmi_DAT_wait(id_out_ll[n]);
    bmi_DAT_wait(id_out_hl[n]);
    bmi_DAT_wait(id_out_lh[n]);
    bmi_DAT_wait(id_out_hh[n]);
  }
*/

  return 0;
}

/**************************************************************************************/
/* static                         dec_dc_shift_i53ct                                  */
/**************************************************************************************/
static int dec_dc_shift_i53ct(int *inp_r, int *inp_g, int *inp_b,
                              int comp_num, int conf_mct, int dc_offset,
                              int width, int irows, int sample_bytes,
                              int bitdepth, int guard_bit)
{
  int r, g, b;
  int n = irows * width - 1;
  int max = (dc_offset << 1) - 1;
  int shift = 18 - bitdepth - guard_bit;

  int * restrict src_r32 = inp_r;
  int * restrict src_g32 = inp_g;
  int * restrict src_b32 = inp_b;

  unsigned char * restrict dst_r8 = (unsigned char *)inp_r;
  unsigned char * restrict dst_g8 = (unsigned char *)inp_g;
  unsigned char * restrict dst_b8 = (unsigned char *)inp_b;

  unsigned short * restrict dst_r16 = (unsigned short *)inp_r;
  unsigned short * restrict dst_g16 = (unsigned short *)inp_g;
  unsigned short * restrict dst_b16 = (unsigned short *)inp_b;

  if(conf_mct && comp_num < 3) {        // mct
    if(comp_num == 2) {
      // some issue when use (restrict) pointer in mct
      if(sample_bytes == 1) {           // int to char
        for(; n >= 0; --n, ++dst_r8, ++dst_g8, ++dst_b8, ++src_r32, ++src_g32, ++src_b32) {
          g = *src_r32 - ((*src_g32 + *src_b32) >> 2) + dc_offset;
          r = *src_b32 + g;
          b = *src_g32 + g;

          if(r > max)   r = max;    if(r < 0)   r = 0;
          if(g > max)   g = max;    if(g < 0)   g = 0;
          if(b > max)   b = max;    if(b < 0)   b = 0;

          *dst_r8 = (unsigned char)r;
          *dst_g8 = (unsigned char)g;
          *dst_b8 = (unsigned char)b;
        }
      } else {                          // int to short
        for(; n >= 0; --n, ++dst_r16, ++dst_g16, ++dst_b16, ++src_r32, ++src_g32, ++src_b32) {
          g = *src_r32 - ((*src_g32 + *src_b32) >> 2) + dc_offset;
          r = *src_b32 + g;
          b = *src_g32 + g;

          if(r > max)   r = max;    if(r < 0)   r = 0;
          if(g > max)   g = max;    if(g < 0)   g = 0;
          if(b > max)   b = max;    if(b < 0)   b = 0;

          *dst_r16 = (unsigned short)r;
          *dst_g16 = (unsigned short)g;
          *dst_b16 = (unsigned short)b;
        }
      }
    }
  } else {                              // dc shift
    if(sample_bytes == 1) {             // int to char
      for(; n >= 0; --n, ++dst_b8, ++src_b32) {
        *src_b32 >>= shift;

        b = *src_b32 + dc_offset;
        if(b > max)   b = max;    if(b < 0)   b = 0;

        *dst_b8 = (unsigned char)b;
      }
    } else {                            // int to short
      for(; n >= 0; --n, ++dst_b16, ++src_b32) {
        *src_b32 >>= shift;

        b = *src_b32 + dc_offset;
        if(b > max)   b = max;    if(b < 0)   b = 0;

        *dst_b16 = (unsigned short)b;
      }
    }
  }

  return 0;
}

/**************************************************************************************/
/* static                             dec_dwt53_horz                                    */
/**************************************************************************************/
static int dec_dwti53_horz(int * restrict out, int *pinl, int *pinh,
                           int even, int width, int dwt_ext, int b_ext)
{
  int ext;
  int cnt = 3;
  int *prev_poutl = out + !even;      // dependent pointers?

  if(width == 1) {
    if(even)    *out = pinl[0];
    else        *out = pinh[0] >> 1;
    return 0;
  }

  if(b_ext & 0x2) {                                                   // begin
    pinl += (dwt_ext >> 1);
    pinh += (dwt_ext >> 1);
    if(even) {
      *out++ = pinl[0] - ((pinh[-1] + pinh[0] + 2) >> 2);
      ++cnt;    ++pinl;
    } else {
      ext = pinl[-1] - ((pinh[-1] + pinh[0] + 2) >> 2);
      prev_poutl = &ext;
    }
  } else if(even) {
    *out++ = pinl[0] - ((pinh[0] + 1) >> 1);
    ++cnt;    ++pinl;
  }

  for(; cnt <= width; cnt += 2, ++pinh, ++pinl, out += 2) {           // middle
    out[1] = pinl[0] - ((pinh[0] + pinh[1] + 2) >> 2);
    out[0] = pinh[0] + ((*prev_poutl + out[1] + 1) >> 1);
    prev_poutl = out + 1;
  }

  if((width & 1) ^ !even) {                                           // end
    if(b_ext & 0x1)   out[1] = pinl[0] - ((pinh[0] + pinh[1] + 2) >> 2);
    else              out[1] = pinl[0] - ((pinh[0] + 1) >> 1);
    *out = pinh[0] + ((*prev_poutl + out[1] + 1) >> 1);
  } else {
    if(b_ext & 0x1)   ext = pinl[0] - ((pinh[0] + pinh[1] + 2) >> 2);
    else              ext = *prev_poutl;
    *out = pinh[0] + ((*prev_poutl + ext + 1) >> 1);
  }

  return 0;
}

/**************************************************************************************/
/*                                        dec_dwt_53                                  */
/**************************************************************************************/
int dec_dwt_i53(stru_dec_param *dec_param, stru_opt *s_opt,
                stru_persis_buffer *persis_buffer, stru_scratch_buffer *scratch_buffer,
                int comp_num)
{
  stru_tile_component *cur_comp = s_opt->component + comp_num;

  int thumb_num   = dec_param->dwt_level - s_opt->reso_thumb + 1;
  int thumb_width = ceil_ratio(dec_param->width, (1 << (thumb_num - 1)));
  int thumb_x0    = ceil_ratio(s_opt->tx0, (1 << (thumb_num - 1)));
  int thumb_y0    = ceil_ratio(s_opt->ty0, (1 << (thumb_num - 1)));
  int thumb_tcx0  = ceil_ratio(thumb_x0, s_opt->XRsiz[comp_num]);
  int thumb_tcy0  = ceil_ratio(thumb_y0, s_opt->YRsiz[comp_num]);

  int n, reso_num = dec_param->dwt_level + 1;
  int tbx0_low, tbx1_low, tby0_low, tby1_low, tbx0_high, tbx1_high, tby0_high, tby1_high;
  int width, height, width_low, width_high, height_low, height_high, even_v, even_h;
  int idx = 0, cnt, mct, first_dma, nrows, irows, rrows, ihrows, ilrows, orows, grrows;
  int nbwidth, rbwidth, nbwidth_ext, b_ext;       // used by block based dwt
  int dwt_ext = (s_opt->conf_dwt97) ? 4 : 2;      // dwt boundary extension
  int nblow_h, nbhigh_h, nblow_v, nbhigh_v;       // low/high of bdwt in one dma

  int pitch = ceil_ratio(thumb_width, s_opt->XRsiz[comp_num]);
  int sample_bytes = (dec_param->bitdepth + 7) >> 3;
  int dc_offset = 1 << (dec_param->bitdepth - 1);

  int comp_width = cur_comp->width[0];
  int comp_height = cur_comp->height[0];
  int tcx0 = ceil_ratio(s_opt->tx0, s_opt->XRsiz[comp_num]);
  int tcy0 = ceil_ratio(s_opt->ty0, s_opt->YRsiz[comp_num]);
  int tcx1 = tcx0 + comp_width;
  int tcy1 = tcy0 + comp_height;

  int *vert_l, *vert_h, *prev_vert_h, *prev_horz_l;
  int * restrict pin0, * restrict pin1, * restrict pin2;
  int *poutl, *prev_poutl;            // dependent pointers?
  int *pouth;

  unsigned char * restrict out_buf;
  unsigned char * restrict out_g_buf =  (unsigned char *)dec_param->output[1] +
                                        (thumb_tcx0 + thumb_tcy0 * pitch) * sample_bytes;
  unsigned char * restrict out_r_buf =  (unsigned char *)dec_param->output[0] +
                                        (thumb_tcx0 + thumb_tcy0 * pitch) * sample_bytes;

  int * restrict inp_g_buf = scratch_buffer->mct_g_buffer;
  int * restrict inp_r_buf = scratch_buffer->mct_r_buffer;
  int * restrict inp_l_buf, * restrict inp_h_buf;

  int * restrict inp_ll[2], * restrict inp_hl[2];
  int * restrict inp_lh[2], * restrict inp_hh[2], * restrict inp[2];
  int * restrict out[2], * restrict out_g[2], * restrict out_r[2];
  int siz_vert, siz_inp, siz_out, siz_ll, siz_hl, siz_lh, siz_hh;
  int off_out, off_inp_h, off_inp_l, off_out_gr;

  unsigned int id_ll = DAT_XFRID_WAITNONE;
  unsigned int id_out[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_g[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_out_g[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_r[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_out_r[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_ll[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_hl[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_lh[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};
  unsigned int id_inp_hh[2] = {DAT_XFRID_WAITNONE, DAT_XFRID_WAITNONE};

  while(--reso_num != thumb_num - 1) {
    tbx0_low  = ceil_ratio(tcx0, (1 << reso_num));
    tby0_low  = ceil_ratio(tcy0, (1 << reso_num));
    tbx1_low  = ceil_ratio(tcx1, (1 << reso_num));
    tby1_low  = ceil_ratio(tcy1, (1 << reso_num));

    tbx0_high = ceil_ratio(tcx0 - (1 << (reso_num - 1)), (1 << reso_num));
    tby0_high = ceil_ratio(tcy0 - (1 << (reso_num - 1)), (1 << reso_num));
    tbx1_high = ceil_ratio(tcx1 - (1 << (reso_num - 1)), (1 << reso_num));
    tby1_high = ceil_ratio(tcy1 - (1 << (reso_num - 1)), (1 << reso_num));

    width_low   = tbx1_low - tbx0_low;
    width_high  = tbx1_high - tbx0_high;
    height_low  = tby1_low - tby0_low;
    height_high = tby1_high - tby0_high;

    width       = width_low + width_high;
    height      = height_low + height_high;

    even_v      = (tby0_high < tby0_low) ? 0 : 1;
    even_h      = (tbx0_high < tbx0_low) ? 0 : 1;

    if(!width || !height)    continue;            // zero length subband, start next dwt loop

    bmi_CACHE_wbInvAllL2(CACHE_WAIT);

    // update variables & pointers
    if(reso_num != thumb_num) {
      pitch = comp_width;
      sample_bytes = sizeof(int);

      inp_l_buf = scratch_buffer->dwt_tmp_buffer;
      out_buf = (unsigned char *)persis_buffer->dwt_buffer[comp_num];

      // copy LL from out_buf to inp_l_buf (dwt_ll_buf)
      for(n = 0; n < height; ++n) {
        bmi_CACHE_wbInvL2_my(out_buf + n * comp_width,
                          (width * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      bmi_CACHE_wbInvL2_my(inp_l_buf,
                        ((width * sizeof(int) * height + DSP_ALIGN) & ~DSP_ALIGN),
                        CACHE_WAIT);
      id_ll = bmi_DAT_copy2d( DAT_2D1D, out_buf, inp_l_buf,
                              width * sizeof(int), height,
                              comp_width * sizeof(int));
    } else {
      if(thumb_num != 1) {    // have to use dwt_tmp_buffer for thunbnail decoding
        inp_l_buf = scratch_buffer->dwt_tmp_buffer;
        out_buf = (unsigned char *)persis_buffer->dwt_buffer[comp_num];

        // copy LL from out_buf to inp_l_buf (dwt_ll_buf)
        for(n = 0; n < height; ++n) {
          bmi_CACHE_wbInvL2_my(out_buf + n * comp_width,
                            (width * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        bmi_CACHE_wbInvL2_my(inp_l_buf,
                          ((width * sizeof(int) * height + DSP_ALIGN) & ~DSP_ALIGN),
                          CACHE_WAIT);
        id_ll = bmi_DAT_copy2d( DAT_2D1D, out_buf, inp_l_buf,
                                width * sizeof(int), height,
                                comp_width * sizeof(int));
      } else {
        inp_l_buf = persis_buffer->dwt_buffer[comp_num];
      }

      if(s_opt->conf_mct && comp_num < 2) {       // g/r component for mct operation
        pitch = width;
        sample_bytes = sizeof(int);

        if(comp_num)    out_buf = (unsigned char *)scratch_buffer->mct_g_buffer;
        else            out_buf = (unsigned char *)scratch_buffer->mct_r_buffer;
      } else {
        pitch = ceil_ratio(thumb_width, s_opt->XRsiz[comp_num]);
        sample_bytes = (dec_param->bitdepth + 7) >> 3;

        out_buf =   (unsigned char *)dec_param->output[comp_num] +
                    (thumb_tcx0 + thumb_tcy0 * pitch) * sample_bytes;
      }
    }

    mct = (s_opt->conf_mct && reso_num == thumb_num && comp_num == 2) ? 1 : 0;

    // first dwt block : init
    nbwidth = MAX_BDWT_WIDTH;
    rbwidth = width;
    b_ext = 0x1;                        // b'01, left extension = 0, right extension = 1

    do {                                // block based dwt
      if(nbwidth + dwt_ext > rbwidth) { // not enough for dwt extension
        nbwidth = rbwidth;              // adjust the width of dwt block
        b_ext &= 0x2;                   // b'x0, right extension = 0;
      }

      nbwidth_ext = nbwidth;
      if(b_ext & 0x2)   nbwidth_ext += dwt_ext;   // left extension
      if(b_ext & 0x1)   nbwidth_ext += dwt_ext;   // right extension

      off_out   = 0;                    // out_buf offset
      off_inp_h = 0;                    // inp_h_buf offset
      off_inp_l = 0;                    // inp_l_buf offset
      off_out_gr = 0;                   // out_gr_buf offset
      first_dma = 1;                    // first DMA
      rrows     = height;               // remaining rows

      // nrows: number of rows for one DMA, have to be able to divided by 4(53) or 8(97)
      // L2_SRAM = sizeof(int) * (2*nrows + 2*3*(nrows+1) + 4) * width  : mct
      // L2_SRAM = sizeof(int) * (2*nrows + 2*(nrows+1) + 4) * width    : otherwise
      // L2_SRAM = sizeof(int) * (8*nrows + 10) * width                 : mct
      // L2_SRAM = sizeof(int) * (4*nrows + 6) * width                  : otherwise
      nrows = ((scratch_buffer->dwt_proc_size - (6 + mct * 4) * nbwidth_ext * sizeof(int)) /
              ((mct + 1) * 4 * nbwidth_ext * sizeof(int))) & ~3;

      // cache alignment : out, out_g, out_r, prev_vert_h/vert_h/vert_l
      // chche alignment : inp_ll, inp_hl, inp_lh, inp_hh, inp
      // siz_vert             : nbwidth_ext
      // siz_out, siz_out_g/b : nbwidth
      // siz_inp              : nbwidth_ext
      for(; nrows; nrows -= 4) {
        nbhigh_h = (nbwidth_ext + !even_h) >> 1;  // size of horzental high in one dwt block
        nblow_h = (nbwidth_ext + even_h) >> 1;    // size of horzental low in one dwt block
        nbhigh_v = (nrows + !even_v) >> 1;        // size of vertical high in one dwt block
        nblow_v = (nrows + even_v) >> 1;          // size of vertical low in one dwt block

        siz_ll = (nblow_v * nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_hl = (nblow_v * nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_lh = (nbhigh_v * nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_hh = (nbhigh_v * nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_vert = (4 * nbwidth_ext * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_out = ((nrows + 1) * nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN;
        siz_inp = siz_ll + siz_hl + siz_lh + siz_hh;

        if( (siz_vert + ((siz_inp + (1 + (mct << 1)) * siz_out) << 1)) <=
            scratch_buffer->dwt_proc_size) {
          break;
        }
      }
      if(s_opt->conf_dwt97)     nrows &= ~7;
      if(nrows > rrows)         nrows = rrows;

      // setup on chip memory partition, ping-pong style
      prev_vert_h = (int *)scratch_buffer->dwt_proc_buffer;
      vert_h = prev_vert_h + nbwidth_ext;
      vert_l = vert_h + nbwidth_ext;
      prev_horz_l = vert_l + nbwidth_ext;
      inp[0] = (int *)((char *)prev_vert_h + siz_vert);
      inp[1] = (int *)((char *)inp[0] + siz_inp);
      out[0] = (int *)((char *)inp[1] + siz_inp);
      out[1] = (int *)((char *)out[0] + siz_out);
      if(mct) {
        out_g[0] = (int *)((char *)out  [1] + siz_out);
        out_g[1] = (int *)((char *)out_g[0] + siz_out);
        out_r[0] = (int *)((char *)out_g[1] + siz_out);
        out_r[1] = (int *)((char *)out_r[0] + siz_out);
      }

      inp_ll[0] = (int *)((char *)inp[0]);
      inp_hl[0] = (int *)((char *)inp_ll[0] + siz_ll);
      inp_lh[0] = (int *)((char *)inp_hl[0] + siz_hl);
      inp_hh[0] = (int *)((char *)inp_lh[0] + siz_lh);

      inp_ll[1] = (int *)((char *)inp[1]);
      inp_hl[1] = (int *)((char *)inp_ll[1] + siz_ll);
      inp_lh[1] = (int *)((char *)inp_hl[1] + siz_hl);
      inp_hh[1] = (int *)((char *)inp_lh[1] + siz_lh);
      inp_h_buf = inp_l_buf + height_low * width;

      // tmp number of rows for one DMA
      // 53: odd: (0-12)-(34-56)..., even: (01-23)-(45-67)...
      // 97: odd: (0-1234)-(5678-9abc)..., even: (0123-4567)-(89ab-cdef)...
      irows = nrows - ((!even_v && rrows > nrows) ? ((s_opt->conf_dwt97) ? 3 : 1) : 0);

      // double buffer DMA
      bmi_DAT_wait(id_ll);                  // done copying LL to new buffer
      if(mct) {
        grrows = irows - (rrows > irows);
        for(n = 0; n < grrows; ++n) {
          bmi_CACHE_wbInvL2_my(inp_g_buf + off_out_gr + n * width,
                            (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        id_inp_g[idx] = bmi_DAT_copy2d( DAT_2D1D, inp_g_buf + off_out_gr, out_g[idx],
                                        nbwidth * sizeof(int), grrows,
                                        width * sizeof(int));
        for(n = 0; n < grrows; ++n) {
          bmi_CACHE_wbInvL2_my(inp_r_buf + off_out_gr + n * width,
                            (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        id_inp_r[idx] = bmi_DAT_copy2d( DAT_2D1D, inp_r_buf + off_out_gr, out_r[idx],
                                        nbwidth * sizeof(int), grrows,
                                        width * sizeof(int));
        off_out_gr += grrows * width;
      }
      for(n = 0; n < (irows + even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + n * width,
                          (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_ll[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_l_buf + off_inp_l, inp_ll[idx],
                                      nblow_h * sizeof(int), ((irows + even_v) >> 1),
                                      width * sizeof(int));
      for(n = 0; n < (irows + even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + width_low + n * width,
                          (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_hl[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_l_buf + off_inp_l + width_low, inp_hl[idx],
                                      nbhigh_h * sizeof(int), ((irows + even_v) >> 1),
                                      width * sizeof(int));
      for(n = 0; n < (irows + !even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + n * width,
                          (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_lh[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_h_buf + off_inp_h, inp_lh[idx],
                                      nblow_h * sizeof(int), ((irows + !even_v) >> 1),
                                      width * sizeof(int));
      for(n = 0; n < (irows + !even_v) >> 1; ++n) {
        bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + width_low + n * width,
                          (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                          CACHE_WAIT);
      }
      id_inp_hh[idx] = bmi_DAT_copy2d(DAT_2D1D, inp_h_buf + off_inp_h + width_low, inp_hh[idx],
                                      nbhigh_h * sizeof(int), ((irows + !even_v) >> 1),
                                      width * sizeof(int));
      off_inp_l += (irows >> 1) * width;
      off_inp_h += ((irows + 1) >> 1) * width;

      rrows -= irows;
      orows = irows - !!rrows;          // orows == grrows
      if(nrows > rrows)         nrows = rrows;

      // brings arrays into the cache
//      touch(vert_l,      nbwidth_ext * sizeof(int));
//      touch(vert_h,      nbwidth_ext * sizeof(int));
//      touch(prev_vert_h, nbwidth_ext * sizeof(int));

      do {
  /////////////////////// mct/dwt core
        if(nrows) {
          if(mct) {
            grrows = nrows + (rrows == nrows);
            for(n = 0; n < grrows; ++n) {
              bmi_CACHE_wbInvL2_my(inp_g_buf + off_out_gr + n * width,
                                (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                                CACHE_WAIT);
            }
           id_inp_g[!idx] = bmi_DAT_copy2d(DAT_2D1D, inp_g_buf + off_out_gr, out_g[!idx],
                                           nbwidth * sizeof(int), grrows,
                                           width * sizeof(int));
            for(n = 0; n < grrows; ++n) {
              bmi_CACHE_wbInvL2_my(inp_r_buf + off_out_gr + n * width,
                                (nbwidth * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                                CACHE_WAIT);
            }
           id_inp_r[!idx] = bmi_DAT_copy2d(DAT_2D1D, inp_r_buf + off_out_gr, out_r[!idx],
                                           nbwidth * sizeof(int), grrows,
                                           width * sizeof(int));
          off_out_gr += grrows * width;
          }
          for(n = 0; n < (nrows + 1) >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + n * width,
                              (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_ll[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_l_buf + off_inp_l, inp_ll[!idx],
                                            nblow_h * sizeof(int), ((nrows + 1) >> 1),
                                            width * sizeof(int));
          for(n = 0; n < (nrows + 1) >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_l_buf + off_inp_l + width_low + n * width,
                              (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_hl[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_l_buf + off_inp_l + width_low, inp_hl[!idx],
                                            nbhigh_h * sizeof(int), ((nrows + 1) >> 1),
                                            width * sizeof(int));
          for(n = 0; n < nrows >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + n * width,
                              (nblow_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_lh[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_h_buf + off_inp_h, inp_lh[!idx],
                                            nblow_h * sizeof(int), (nrows >> 1),
                                            width * sizeof(int));
          for(n = 0; n < nrows >> 1; ++n) {
            bmi_CACHE_wbInvL2_my(inp_h_buf + off_inp_h + width_low + n * width,
                              (nbhigh_h * sizeof(int) + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_inp_hh[!idx] = bmi_DAT_copy2d( DAT_2D1D, inp_h_buf + off_inp_h + width_low, inp_hh[!idx],
                                            nbhigh_h * sizeof(int), (nrows >> 1),
                                            width * sizeof(int));
          off_inp_l += ((nrows + 1) >> 1) * width;
          off_inp_h += (nrows >> 1) * width;
        }

        bmi_DAT_wait(id_out[idx]);
        bmi_DAT_wait(id_inp_ll[idx]);
        bmi_DAT_wait(id_inp_hl[idx]);
        bmi_DAT_wait(id_inp_hh[idx]);
        bmi_DAT_wait(id_inp_lh[idx]);
        if(mct) {
          bmi_DAT_wait(id_inp_g[idx]);
          bmi_DAT_wait(id_inp_r[idx]);
          bmi_DAT_wait(id_out_g[idx]);
          bmi_DAT_wait(id_out_r[idx]);
        }

        // dwt
        ihrows = 0;                     // number of high processed in one DMA
        ilrows = 0;                     // number of low  processed in one DMA
        if(!first_dma) {                                              // begin
          cnt = 2;
          pouth = out[idx];
        } else {
          cnt = 3;

          if(even_v) {
            pin0 = vert_l;
            prev_poutl = poutl = out[idx];
            dec_dwti53_horz( vert_l,
                          inp_ll[idx] + ilrows * nblow_h,
                          inp_hl[idx] + ilrows * nbhigh_h,
                          even_h, nbwidth, dwt_ext, b_ext);
            ++ilrows;

            if(irows != 1) {
              dec_dwti53_horz( prev_vert_h,
                            inp_lh[idx] + ihrows * nblow_h,
                            inp_hh[idx] + ihrows * nbhigh_h,
                            even_h, nbwidth, dwt_ext, b_ext);
              ++ihrows;

              for(pin1 = prev_vert_h, n = nbwidth; n; --n, ++poutl, ++pin0, ++pin1) {
                *poutl = *pin0 - ((*pin1 + 1) >> 1);
              }
            } else {                      // one rows only
              for(n = nbwidth; n; --n, ++poutl, ++pin0) {
                *poutl = *pin0;
              }
            }

            ++cnt;
            pouth = poutl;
          } else {
            pouth = out[idx];
            prev_poutl = pouth + nbwidth;
            dec_dwti53_horz( prev_vert_h,
                          inp_lh[idx] + ihrows * nblow_h,
                          inp_hh[idx] + ihrows * nbhigh_h,
                          even_h, nbwidth, dwt_ext, b_ext);
            ++ihrows;
          }
        }

        for(; cnt <= irows; cnt += 2) {                               // middle
          dec_dwti53_horz( vert_h,
                        inp_lh[idx] + ihrows * nblow_h,
                        inp_hh[idx] + ihrows * nbhigh_h,
                        even_h, nbwidth, dwt_ext, b_ext);
          ++ihrows;
          dec_dwti53_horz( vert_l,
                        inp_ll[idx] + ilrows * nblow_h,
                        inp_hl[idx] + ilrows * nbhigh_h,
                        even_h, nbwidth, dwt_ext, b_ext);
          ++ilrows;

          for(pin0 = prev_vert_h, pin1 = vert_l, pin2 = vert_h,
              poutl = pouth + nbwidth, n = nbwidth;
              n; --n, ++pin0, ++pin1, ++pin2, ++poutl, ++pouth, ++prev_poutl) {
            *poutl = *pin1 - ((*pin0 + *pin2 + 2) >> 2);
            *pouth = *pin0 + ((*prev_poutl + *poutl + 1) >> 1);
          }

          prev_poutl = vert_h;
          vert_h = prev_vert_h;
          prev_vert_h = prev_poutl;

          prev_poutl = pouth;
          pouth = poutl;
        }

        if(!nrows) {                                                  // end
          if(irows == 1 && first_dma) {
            if(!even_v) {
              for(pin0 = prev_vert_h, n = nbwidth; n; --n, ++pouth, ++pin0) {
                *pouth = *pin0 >> 1;
              }
            }       // !even_v is done in "begin" section
          } else {
            if((irows & 1) ^ (!even_v && first_dma)) {
              dec_dwti53_horz( vert_l,
                            inp_ll[idx] + ilrows * nblow_h,
                            inp_hl[idx] + ilrows * nbhigh_h,
                            even_h, nbwidth, dwt_ext, b_ext);
              ++ilrows;
              for(pin0 = prev_vert_h, pin1 = vert_l, poutl = pouth + nbwidth, n = nbwidth;
                  n; --n, ++pin0, ++pin1, ++poutl, ++pouth, ++prev_poutl) {
                *poutl = *pin1 - ((*pin0 + 1) >> 1);
                *pouth = *pin0 + ((*prev_poutl + *poutl + 1) >> 1);
              }
            } else {
              for(pin0 = prev_vert_h, n = nbwidth; n; --n, ++pin0, ++pouth, ++prev_poutl) {
                *pouth = *pin0 + ((*prev_poutl + *prev_poutl + 1) >> 1);
              }
            }
          }
        }

        if(reso_num == thumb_num) {                                   // dcshift/mct
          // copy prev_poutl to a tmp buffer
          memcpy(prev_horz_l, prev_poutl, nbwidth * sizeof(int));
          prev_poutl = prev_horz_l;

          // dcshift/mct
          dec_dc_shift_i53ct( out_r[idx], out_g[idx], out[idx],
                              comp_num, s_opt->conf_mct, dc_offset,
                              nbwidth, orows, sample_bytes,
                              dec_param->bitdepth, s_opt->guard_bit);
        }

  /////////////////////// mct/dwt core

        if(mct) {
          for(n = 0; n < orows; ++n) {
            bmi_CACHE_wbInvL2_my(out_g_buf + off_out + n * nbwidth * sample_bytes,
                              (nbwidth * sample_bytes + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_out_g[idx] = bmi_DAT_copy2d( DAT_1D2D, out_g[idx], out_g_buf + off_out,
                                          nbwidth * sample_bytes, orows,
                                          pitch * sample_bytes);
          for(n = 0; n < orows; ++n) {
            bmi_CACHE_wbInvL2_my(out_r_buf + off_out + n * pitch * sample_bytes,
                              (nbwidth * sample_bytes + DSP_ALIGN) & ~DSP_ALIGN,
                              CACHE_WAIT);
          }
          id_out_r[idx] = bmi_DAT_copy2d( DAT_1D2D, out_r[idx], out_r_buf + off_out,
                                          nbwidth * sample_bytes, orows,
                                          pitch * sample_bytes);
        }
        for(n = 0; n < orows; ++n) {
          bmi_CACHE_wbInvL2_my(out_buf + off_out + n * pitch * sample_bytes,
                            (nbwidth * sample_bytes + DSP_ALIGN) & ~DSP_ALIGN,
                            CACHE_WAIT);
        }
        id_out[idx] = bmi_DAT_copy2d( DAT_1D2D, out[idx], out_buf + off_out,
                                      nbwidth * sample_bytes, orows,
                                      pitch * sample_bytes);
        off_out += orows * pitch * sample_bytes;

        idx = !idx;
        first_dma = 0;
        irows = nrows;
        rrows -= nrows;
        orows = nrows + !rrows;         // orows == grrows
        if(nrows > rrows)         nrows = rrows;
      } while(irows);                   // one dwt block done

      // going to update memory partition, wait till all buf done
      bmi_DAT_wait(id_ll);
      bmi_DAT_wait(id_out[!idx]);
      bmi_DAT_wait(id_inp_ll[idx]);
      bmi_DAT_wait(id_inp_hl[idx]);
      bmi_DAT_wait(id_inp_hh[idx]);
      bmi_DAT_wait(id_inp_lh[idx]);
      if(mct) {
        bmi_DAT_wait(id_inp_g[idx]);
        bmi_DAT_wait(id_inp_r[idx]);
        bmi_DAT_wait(id_out_g[!idx]);
        bmi_DAT_wait(id_out_r[!idx]);
      }

      // update block based dwt
      inp_l_buf += ((nbwidth + even_h) >> 1);
      out_buf   += nbwidth * sample_bytes;
      if(!(b_ext & 0x2)) {
        inp_l_buf   -= (dwt_ext >> 1);
      }

      if(mct) {
        inp_g_buf += nbwidth;
        inp_r_buf += nbwidth;
        out_g_buf += nbwidth * sample_bytes;
        out_r_buf += nbwidth * sample_bytes;
      }

      rbwidth -= nbwidth;
      b_ext |= 0x2;                     // b'1x, left extension = 1

    } while(rbwidth);                   // block based dwt
  }
/*
  // done processing one component
  bmi_DAT_wait(id_ll);                      // done copying LL to new buffer

  for(n = 0; n < 2; ++n) {
    bmi_DAT_wait(id_inp[n]);
    bmi_DAT_wait(id_inp_g[n]);
    bmi_DAT_wait(id_inp_b[n]);
    bmi_DAT_wait(id_out_g[n]);
    bmi_DAT_wait(id_out_b[n]);
    bmi_DAT_wait(id_out_ll[n]);
    bmi_DAT_wait(id_out_hl[n]);
    bmi_DAT_wait(id_out_lh[n]);
    bmi_DAT_wait(id_out_hh[n]);
  }
*/

  return 0;
}


#endif // !SOFTWARE_MULTI_THREAD