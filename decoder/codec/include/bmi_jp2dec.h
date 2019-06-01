/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#ifndef BMI_JP2DEC_H
#define BMI_JP2DEC_H

#ifndef WIN32
#define BMI_LIB_API
#elif DEC_LIB_EXPORTS
#define BMI_LIB_API /*__declspec(dllexport)*/
#else
#define BMI_LIB_API /*__declspec(dllimport)*/
#endif

#include "codec.h"
#include "codec_base.h"
#include "codec_macro.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// use register variable to enhance performance
// only valid when we use fast macro, set USE_FAST_MACRO in platform.h
//	when we are not using macro, it's useless

#ifdef __cplusplus
extern "C" {
#endif


enum enum_dec_configuration {
  CONF_BYPASS   = 0x00000001,
  CONF_RESET    = 0x00000002,
  CONF_TERMALL  = 0x00000004,
  CONF_CAUSAL   = 0x00000008,

  CONF_PTERM    = 0x00000010,
  CONF_SEGSYM   = 0x00000020,
  CONF_SOP      = 0x00000040,
  CONF_EPH      = 0x00000080,

  CONF_DWT97    = 0x00000100,
  CONF_MCT      = 0x00000200,
  CONF_QUANT    = 0x00000400,

  CONF_JP2FILE  = 0x01000000,
  CONF_VIDEO    = 0x02000000,

  CONF_VHDLTB   = 0x10000000,
  CONF_OPTIM    = 0xC0000000
};

typedef struct tag_stru_dec_param {

  void *mempool[2];
  int pool_size[2];

  void *input;
  void *output[MAX_COMPONENT];
  unsigned int in_size;

  int width;
  int height;
  int bitdepth;
  int format;
  int num_components;

  int cb_width;
  int cb_height;
  int prec_width;
  int prec_height;
  int tile_width;
  int tile_height;
  int tile_num;
  int size_opt;


  int layer;
  int dwt_level;
  int roi_shift;
  Jp2_Progression progression;

  int configuration;

  int guard_bit;
  unsigned int quant_step[1 + 3 * MAX_DWT_LEVEL];

} stru_dec_param;

// decoding

#if SOFTWARE_MULTI_THREAD
BMI_LIB_API int BMI_dec_info(stru_dec_param *dec_param, int thread_num);
BMI_LIB_API int BMI_dec_init(stru_dec_param *dec_param, int use_j2k_header, int thread_num);

#else	// SOFTWARE_MULTI_THREAD
BMI_LIB_API int BMI_dec_info(stru_dec_param *dec_param);
BMI_LIB_API int BMI_dec_init(stru_dec_param *dec_param, int use_j2k_header);

#endif	// SOFTWARE_MULTI_THREAD

BMI_LIB_API int BMI_dec_isr(stru_dec_param *dec_param, int fpga_irq);

BMI_LIB_API int BMI_dec(stru_dec_param *dec_param, void *output[MAX_COMPONENT], int use_j2k_header);

// BMI_LIB_API void BMI_get_context_table(const unsigned char ** lllh, const unsigned char ** hl, const unsigned char ** hh);

BMI_LIB_API BMI::Status BMI_dec_thumbnail(stru_dec_param *dec_param, void *output[MAX_COMPONENT], int resolution, int use_j2k_header);

BMI_LIB_API int BMI_dec_exit(stru_dec_param *dec_param);

#ifdef __cplusplus
}
#endif

#endif //BMI_JP2DEC_H
