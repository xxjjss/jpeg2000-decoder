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
#include "dec_t1.h"
#if !defined(USE_MARCO_T1)
static inline void update_ctx(unsigned short *cb_ctx, int ctx_width, short sign, char causal);
static inline void dec_p1(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                          int ctx_width, int mbp, char causal, unsigned char *lut_ctx);
static inline void dec_p2(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                          int ctx_width, int mbp);
static inline void dec_p3(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                          int ctx_width, int mbp, char causal, unsigned char *lut_ctx);
static inline void dec_p1_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                              int ctx_width, int mbp, char causal);
static inline void dec_p2_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                              int ctx_width, int mbp);

/**************************************************************************************/
/* static                                update_ctx                                   */
/**************************************************************************************/
static inline void update_ctx(unsigned short *cb_ctx, int ctx_width, short sign, char causal)
{
  if(!causal) {
    cb_ctx[-ctx_width - 1] |= 0x1;
    cb_ctx[-ctx_width    ] |= 0x2;
    cb_ctx[-ctx_width + 1] |= 0x4;
  }
  cb_ctx[           - 1] |= 0x8;
  cb_ctx[             0] |= MASK_SIG | (MASK_SIGN * sign);
  cb_ctx[           + 1] |= 0x10;
  cb_ctx[ ctx_width - 1] |= 0x20;
  cb_ctx[ ctx_width    ] |= 0x40;
  cb_ctx[ ctx_width + 1] |= 0x80;
}

/**************************************************************************************/
/* static                                dec_p1                                       */
/**************************************************************************************/
static inline void dec_p1(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                          int ctx_width, int mbp, char causal, unsigned char *lut_ctx)
{
  unsigned char idx, ctx;
  unsigned short sign, bit;
  unsigned short ctx_flag = *cb_ctx;

  if(!(ctx_flag & MASK_SIG) && (ctx_flag & MASK_CTX)) {
    ctx = lut_ctx[(unsigned char)ctx_flag];
    mqd_dec(mqd, (&bit), ctx);

    if(bit) {
      idx = ((cb_ctx[0]          & 0x5A     )     ) |
            ((cb_ctx[-ctx_width] & MASK_SIGN) >> 1) |
            ((cb_ctx[        -1] & MASK_SIGN) >> 3) |
            ((cb_ctx[        +1] & MASK_SIGN) >> 6) |
            ((cb_ctx[ ctx_width] & MASK_SIGN) >> 8);
      ctx = lut_ctx_sign[idx];
      mqd_dec(mqd, (&sign), ctx);
      sign ^= lut_ctx_xor[idx];
      update_ctx(cb_ctx, ctx_width, sign, causal);
      *cb_data |= mbp;
    }
    *cb_ctx |= MASK_VISIT;
  }
}

/**************************************************************************************/
/* static                                dec_p1_raw                                   */
/**************************************************************************************/
static inline void dec_p1_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                              int ctx_width, int mbp, char causal)
{
  unsigned short sign, bit;
  unsigned short ctx_flag = *cb_ctx;

  if(!(ctx_flag & MASK_SIG) && (ctx_flag & MASK_CTX)) {
    mqd_dec_raw(mqd, (&bit));

    if(bit) {
      mqd_dec_raw(mqd, (&sign));
      update_ctx(cb_ctx, ctx_width, sign, causal);
      *cb_data |= mbp;
    }
    *cb_ctx |= MASK_VISIT;
  }
}

/**************************************************************************************/
/* static                                dec_p2                                       */
/**************************************************************************************/
static inline void dec_p2(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                          int ctx_width, int mbp)
{
  unsigned char ctx;
  unsigned short bit;
  unsigned short ctx_flag = *cb_ctx;

  if((ctx_flag & (MASK_SIG | MASK_VISIT)) == MASK_SIG) {
    if(ctx_flag & MASK_P2)          ctx = 16;
    else if(ctx_flag & MASK_CTX)    ctx = 15;
    else                            ctx = 14;
    mqd_dec(mqd, (&bit), ctx);
    *cb_ctx |= MASK_P2 | MASK_VISIT;
    *cb_data ^= ((mbp * !bit) << 1) | mbp;
  }
}

/**************************************************************************************/
/* static                                dec_p2_raw                                   */
/**************************************************************************************/
static inline void dec_p2_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                              int ctx_width, int mbp)
{
  unsigned short bit;
  unsigned short ctx_flag = *cb_ctx;

  if((ctx_flag & (MASK_SIG | MASK_VISIT)) == MASK_SIG) {
    mqd_dec_raw(mqd, (&bit));
    *cb_ctx |= MASK_P2 | MASK_VISIT;
    *cb_data ^= ((mbp * !bit) << 1) | mbp;
  }
}

/**************************************************************************************/
/* static                                dec_p3                                       */
/**************************************************************************************/
static inline void dec_p3(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                          int ctx_width, int mbp, char causal, unsigned char *lut_ctx)
{
  unsigned char idx, ctx;
  unsigned short sign, bit;
  unsigned short ctx_flag = *cb_ctx;

  if(!(ctx_flag & MASK_VISIT)) {
    ctx = lut_ctx[(unsigned char)ctx_flag];
    mqd_dec(mqd, (&bit), ctx);

    if(bit) {
      idx = ((cb_ctx[0]          & 0x5A     )     ) |
            ((cb_ctx[-ctx_width] & MASK_SIGN) >> 1) |
            ((cb_ctx[        -1] & MASK_SIGN) >> 3) |
            ((cb_ctx[        +1] & MASK_SIGN) >> 6) |
            ((cb_ctx[ ctx_width] & MASK_SIGN) >> 8);
      ctx = lut_ctx_sign[idx];
      mqd_dec(mqd, (&sign), ctx);
      sign ^= lut_ctx_xor[idx];
      update_ctx(cb_ctx, ctx_width, sign, causal);
      *cb_data |= mbp;
    }
  }
}
#else
/**************************************************************************************/
/* static                                update_ctx                                   */
/**************************************************************************************/
#define update_ctx(cb_ctx, ctx_width, sign, causal) \
{ \
  if(!causal) { \
    cb_ctx[-ctx_width - 1] |= 0x1; \
    cb_ctx[-ctx_width    ] |= 0x2; \
    cb_ctx[-ctx_width + 1] |= 0x4; \
  } \
  cb_ctx[           - 1] |= 0x8; \
  cb_ctx[             0] |= MASK_SIG | (MASK_SIGN * sign); \
  cb_ctx[           + 1] |= 0x10; \
  cb_ctx[ ctx_width - 1] |= 0x20; \
  cb_ctx[ ctx_width    ] |= 0x40; \
  cb_ctx[ ctx_width + 1] |= 0x80; \
}

/**************************************************************************************/
/* static                                dec_p1                                       */
/**************************************************************************************/
#define dec_p1( mqd, cb_data, cb_ctx, ctx_width, mbp, causal, lut_ctx) \
{ \
  unsigned char idx, ctx; \
  unsigned short sign, bit; \
  unsigned short ctx_flag = *cb_ctx; \
 \
  if(!(ctx_flag & MASK_SIG) && (ctx_flag & MASK_CTX)) { \
    ctx = lut_ctx[(unsigned char)ctx_flag]; \
    mqd_dec(mqd, (&bit), ctx); \
 \
    if(bit) { \
      idx = ((cb_ctx[0]          & 0x5A     )     ) | \
            ((cb_ctx[-ctx_width] & MASK_SIGN) >> 1) | \
            ((cb_ctx[        -1] & MASK_SIGN) >> 3) | \
            ((cb_ctx[        +1] & MASK_SIGN) >> 6) | \
            ((cb_ctx[ ctx_width] & MASK_SIGN) >> 8); \
      ctx = lut_ctx_sign[idx]; \
      mqd_dec(mqd, (&sign), ctx); \
      sign ^= lut_ctx_xor[idx]; \
      update_ctx(cb_ctx, ctx_width, sign, causal); \
      *cb_data |= mbp; \
    } \
    *cb_ctx |= MASK_VISIT; \
  } \
}

/**************************************************************************************/
/* static                                dec_p1_raw                                   */
/**************************************************************************************/
#define dec_p1_raw(mqd, cb_data, cb_ctx, ctx_width, mbp, causal) \
{ \
  unsigned short sign, bit; \
  unsigned short ctx_flag = *cb_ctx; \
 \
  if(!(ctx_flag & MASK_SIG) && (ctx_flag & MASK_CTX)) { \
    mqd_dec_raw(mqd, (&bit)); \
 \
    if(bit) { \
      mqd_dec_raw(mqd, (&sign)); \
      update_ctx(cb_ctx, ctx_width, sign, causal); \
      *cb_data |= mbp; \
    } \
    *cb_ctx |= MASK_VISIT; \
  } \
}

/**************************************************************************************/
/* static                                dec_p2                                       */
/**************************************************************************************/
#define dec_p2( mqd, cb_data, cb_ctx, ctx_width, mbp) \
{ \
  unsigned char ctx; \
  unsigned short bit; \
  unsigned short ctx_flag = *cb_ctx; \
 \
  if((ctx_flag & (MASK_SIG | MASK_VISIT)) == MASK_SIG) { \
    if(ctx_flag & MASK_P2)          ctx = 16; \
    else if(ctx_flag & MASK_CTX)    ctx = 15; \
    else                            ctx = 14; \
    mqd_dec(mqd, (&bit), ctx); \
    *cb_ctx |= MASK_P2 | MASK_VISIT; \
    *cb_data ^= ((mbp * !bit) << 1) | mbp; \
  } \
}

/**************************************************************************************/
/* static                                dec_p2_raw                                   */
/**************************************************************************************/
#define dec_p2_raw(mqd, cb_data, cb_ctx, ctx_width, mbp) \
{ \
  unsigned short bit; \
  unsigned short ctx_flag = *cb_ctx; \
 \
  if((ctx_flag & (MASK_SIG | MASK_VISIT)) == MASK_SIG) { \
    mqd_dec_raw(mqd, (&bit)); \
    *cb_ctx |= MASK_P2 | MASK_VISIT; \
    *cb_data ^= ((mbp * !bit) << 1) | mbp; \
  } \
}

/**************************************************************************************/
/* static                                dec_p3                                       */
/**************************************************************************************/
#define dec_p3( mqd, cb_data, cb_ctx, ctx_width, mbp, causal, lut_ctx) \
{ \
  unsigned char idx, ctx; \
  unsigned short sign, bit; \
  unsigned short ctx_flag = *cb_ctx; \
 \
  if(!(ctx_flag & MASK_VISIT)) { \
    ctx = lut_ctx[(unsigned char)ctx_flag]; \
    mqd_dec(mqd, (&bit), ctx); \
 \
    if(bit) { \
      idx = ((cb_ctx[0]          & 0x5A     )     ) | \
            ((cb_ctx[-ctx_width] & MASK_SIGN) >> 1) | \
            ((cb_ctx[        -1] & MASK_SIGN) >> 3) | \
            ((cb_ctx[        +1] & MASK_SIGN) >> 6) | \
            ((cb_ctx[ ctx_width] & MASK_SIGN) >> 8); \
      ctx = lut_ctx_sign[idx]; \
      mqd_dec(mqd, (&sign), ctx); \
      sign ^= lut_ctx_xor[idx]; \
      update_ctx(cb_ctx, ctx_width, sign, causal); \
      *cb_data |= mbp; \
    } \
  } \
}
#endif

/**************************************************************************************/
/*                                       dec_t1_p1                                    */
/**************************************************************************************/
int dec_t1_p1(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
              int cb_width, int cb_height, int mbp, char causal, unsigned char *lut_ctx)
{
  int width, height;
  int ctx_width = cb_width + 2;

  mbp = (1 << mbp) + (1 << (mbp - 1));
  for(height = cb_height; height > 0; height -= 4) {
    for(width = cb_width; width; --width, ++cb_ctx, ++cb_data) {
/*
      // column skip
      if( height >= 4 &&
          ((cb_ctx[0            ] & MASK_SIG) || !(cb_ctx[0            ] & MASK_CTX)) &&
          ((cb_ctx[ctx_width    ] & MASK_SIG) || !(cb_ctx[ctx_width    ] & MASK_CTX)) &&
          ((cb_ctx[ctx_width * 2] & MASK_SIG) || !(cb_ctx[ctx_width * 2] & MASK_CTX)) &&
          ((cb_ctx[ctx_width * 3] & MASK_SIG) || !(cb_ctx[ctx_width * 3] & MASK_CTX))) {
        continue;
      }
*/
      dec_p1( mqd, cb_data, cb_ctx,
              ctx_width, mbp, causal, lut_ctx);

      if(height == 1)   continue;
      dec_p1( mqd, (cb_data + cb_width), (cb_ctx + ctx_width),
              ctx_width, mbp, 0, lut_ctx);

      if(height == 2)   continue;
      dec_p1( mqd, (cb_data + cb_width * 2), (cb_ctx + ctx_width * 2),
              ctx_width, mbp, 0, lut_ctx);

      if(height == 3)   continue;
      dec_p1( mqd, (cb_data + cb_width * 3), (cb_ctx + ctx_width * 3),
              ctx_width, mbp, 0, lut_ctx);
    }
    cb_ctx += (ctx_width << 2) - cb_width;
    cb_data += (cb_width << 2) - cb_width;
  }

  return 0;
}

/**************************************************************************************/
/*                                       dec_t1_p1_raw                                */
/**************************************************************************************/
int dec_t1_p1_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                  int cb_width, int cb_height, int mbp, char causal)
{
  int width, height;
  int ctx_width = cb_width + 2;

  mbp = (1 << mbp) + (1 << (mbp - 1));
  for(height = cb_height; height > 0; height -= 4) {
    for(width = cb_width; width; --width, ++cb_ctx, ++cb_data) {
/*
      // column skip
      if( height >= 4 &&
          ((cb_ctx[0            ] & MASK_SIG) || !(cb_ctx[0            ] & MASK_CTX)) &&
          ((cb_ctx[ctx_width    ] & MASK_SIG) || !(cb_ctx[ctx_width    ] & MASK_CTX)) &&
          ((cb_ctx[ctx_width * 2] & MASK_SIG) || !(cb_ctx[ctx_width * 2] & MASK_CTX)) &&
          ((cb_ctx[ctx_width * 3] & MASK_SIG) || !(cb_ctx[ctx_width * 3] & MASK_CTX))) {
        continue;
      }
*/
      dec_p1_raw( mqd, cb_data, cb_ctx,
                  ctx_width, mbp, causal);

      if(height == 1)   continue;
      dec_p1_raw( mqd, (cb_data + cb_width), (cb_ctx + ctx_width),
                  ctx_width, mbp, 0);

      if(height == 2)   continue;
      dec_p1_raw( mqd, (cb_data + cb_width * 2), (cb_ctx + ctx_width * 2),
                  ctx_width, mbp, 0);

      if(height == 3)   continue;
      dec_p1_raw( mqd, (cb_data + cb_width * 3), (cb_ctx + ctx_width * 3),
                  ctx_width, mbp, 0);
    }
    cb_ctx += (ctx_width << 2) - cb_width;
    cb_data += (cb_width << 2) - cb_width;
  }

  return 0;
}

/**************************************************************************************/
/*                                       dec_t1_p2                                    */
/**************************************************************************************/
int dec_t1_p2(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
              int cb_width, int cb_height, int mbp)
{
  int width, height;
  int ctx_width = cb_width + 2;

  mbp = 1 << (mbp - 1);
  for(height = cb_height; height > 0; height -= 4) {
    for(width = cb_width; width; --width, ++cb_ctx, ++cb_data) {
/*
      // column skip
      if( height >= 4 &&
          ((!(cb_ctx[0            ] & MASK_SIG)) || (cb_ctx[0            ] & MASK_VISIT)) &&
          ((!(cb_ctx[ctx_width    ] & MASK_SIG)) || (cb_ctx[ctx_width    ] & MASK_VISIT)) &&
          ((!(cb_ctx[ctx_width * 2] & MASK_SIG)) || (cb_ctx[ctx_width * 2] & MASK_VISIT)) &&
          ((!(cb_ctx[ctx_width * 3] & MASK_SIG)) || (cb_ctx[ctx_width * 3] & MASK_VISIT))) {
        continue;
      }
*/
      dec_p2( mqd, cb_data, cb_ctx,
              ctx_width, mbp);

      if(height == 1)   continue;
      dec_p2( mqd, (cb_data + cb_width), (cb_ctx + ctx_width),
              ctx_width, mbp);

      if(height == 2)   continue;
      dec_p2( mqd, (cb_data + cb_width * 2), (cb_ctx + ctx_width * 2),
              ctx_width, mbp);

      if(height == 3)   continue;
      dec_p2( mqd, (cb_data + cb_width * 3), (cb_ctx + ctx_width * 3),
              ctx_width, mbp);
    }
    cb_ctx += (ctx_width << 2) - cb_width;
    cb_data += (cb_width << 2) - cb_width;
  }

  return 0;
}

/**************************************************************************************/
/*                                       dec_t1_p2_raw                                */
/**************************************************************************************/
int dec_t1_p2_raw(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
                  int cb_width, int cb_height, int mbp)
{
  int width, height;
  int ctx_width = cb_width + 2;

  mbp = 1 << (mbp - 1);
  for(height = cb_height; height > 0; height -= 4) {
    for(width = cb_width; width; --width, ++cb_ctx, ++cb_data) {
/*
      // column skip
      if( height >= 4 &&
          ((!(cb_ctx[0            ] & MASK_SIG)) || (cb_ctx[0            ] & MASK_VISIT)) &&
          ((!(cb_ctx[ctx_width    ] & MASK_SIG)) || (cb_ctx[ctx_width    ] & MASK_VISIT)) &&
          ((!(cb_ctx[ctx_width * 2] & MASK_SIG)) || (cb_ctx[ctx_width * 2] & MASK_VISIT)) &&
          ((!(cb_ctx[ctx_width * 3] & MASK_SIG)) || (cb_ctx[ctx_width * 3] & MASK_VISIT))) {
        continue;
      }
*/
      dec_p2_raw( mqd, cb_data, cb_ctx,
                  ctx_width, mbp);

      if(height == 1)   continue;
      dec_p2_raw( mqd, (cb_data + cb_width), (cb_ctx + ctx_width),
                  ctx_width, mbp);

      if(height == 2)   continue;
      dec_p2_raw( mqd, (cb_data + cb_width * 2), (cb_ctx + ctx_width * 2),
                  ctx_width, mbp);

      if(height == 3)   continue;
      dec_p2_raw( mqd, (cb_data + cb_width * 3), (cb_ctx + ctx_width * 3),
                  ctx_width, mbp);
    }
    cb_ctx += (ctx_width << 2) - cb_width;
    cb_data += (cb_width << 2) - cb_width;
  }

  return 0;
}

/**************************************************************************************/
/*                                       dec_t1_p3                                    */
/**************************************************************************************/
int dec_t1_p3(stru_mqd *mqd, unsigned int *cb_data, unsigned short *cb_ctx,
              int cb_width, int cb_height, int mbp, char causal, unsigned char *lut_ctx)
{
  int width, height;
  int ctx_width = cb_width + 2;

  short nb;
  unsigned short sign;
  unsigned char idx, ctx;

  unsigned int *pdata;
  unsigned short *pctx, *pctx2 = cb_ctx;

  mbp = (1 << mbp) + (1 << (mbp - 1));
  for(height = cb_height; height > 0; height -= 4) {
    for(width = cb_width; width; --width, ++cb_ctx, ++cb_data) {
/*
      // column skip
      if( height >= 4 &&
          (cb_ctx[0            ] & MASK_VISIT) &&
          (cb_ctx[ctx_width    ] & MASK_VISIT) &&
          (cb_ctx[ctx_width * 2] & MASK_VISIT) &&
          (cb_ctx[ctx_width * 3] & MASK_VISIT)) {
        continue;
      }
*/
      if( height >= 4 &&
          !(cb_ctx[0            ] & (MASK_VISIT | MASK_CTX)) &&
          !(cb_ctx[ctx_width    ] & (MASK_VISIT           )) &&
          !(cb_ctx[ctx_width * 2] & (MASK_VISIT           )) &&
          !(cb_ctx[ctx_width * 3] & (MASK_VISIT | MASK_CTX))) 
	  {

        // zero coding
        mqd_dec(mqd, (&sign), CTX_RUN);
        if(!sign)   continue;

        mqd_dec(mqd, (&sign), CTX_UNIFORM);
        mqd_dec(mqd, ((unsigned short *)(&nb)),  CTX_UNIFORM);
        nb |= sign << 1;

        pctx = cb_ctx + ctx_width * nb;
        pdata = cb_data + cb_width * nb;

        idx = ((pctx[0]          & 0x5A     )     ) |
              ((pctx[-ctx_width] & MASK_SIGN) >> 1) |
              ((pctx[        -1] & MASK_SIGN) >> 3) |
              ((pctx[        +1] & MASK_SIGN) >> 6) |
              ((pctx[ ctx_width] & MASK_SIGN) >> 8);
        ctx = lut_ctx_sign[idx];
        mqd_dec(mqd, (&sign), ctx);
        sign ^= lut_ctx_xor[idx];
        update_ctx(pctx, ctx_width, sign, ((!nb) & causal));
        *pdata |= mbp;

        switch(nb) 
		{
        case 0  : goto row_1;
        case 1  : goto row_2;
        case 2  : goto row_3;
        default : continue;
        }
      }
	  else 
	  {
        dec_p3( mqd, cb_data, cb_ctx,
                ctx_width, mbp, causal, lut_ctx);
      }

row_1:
      if(height == 1)   continue;
      dec_p3( mqd, (cb_data + cb_width), (cb_ctx + ctx_width),
              ctx_width, mbp, 0, lut_ctx);

row_2:
      if(height == 2)   continue;
      dec_p3( mqd, (cb_data + cb_width * 2), (cb_ctx + ctx_width * 2),
              ctx_width, mbp, 0, lut_ctx);

row_3:
      if(height == 3)   continue;
      dec_p3( mqd, (cb_data + cb_width * 3), (cb_ctx + ctx_width * 3),
              ctx_width, mbp, 0, lut_ctx);
    }
    cb_ctx += (ctx_width << 2) - cb_width;
    cb_data += (cb_width << 2) - cb_width;
  }

  // reset MASK_VISIT
  for(nb = cb_height * ctx_width - 2; nb; --nb)   *pctx2++ &= ~MASK_VISIT;

  return 0;
}
