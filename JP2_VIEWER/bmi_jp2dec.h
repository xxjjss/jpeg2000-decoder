/*****************************************************************************/
// File: kdu_utils.h [scope = CORESYS/COMMON]
// Version: Kakadu, V5.2.4
// Author: David Taubman
// Last Revised: 1 December, 2006
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: Mr Wang
// License number: 90027
/******************************************************************************
Description:
   Provides some handy in-line functions.
******************************************************************************/

#ifndef BMI_JP2DEC_H
#define BMI_JP2DEC_H

#define MAX_COMPONENT 4

enum enum_dec_format {
  RAW,
  RGB,
  YUV444,
  YUV422,
  YUV420,
  BAYERGRB
};

enum enum_dec_progression {
  LRCP,
  RLCP,
  RPCL,
  PCRL,
  CPRL
};

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

  CONF_JP2FILE  = 0x01000000,
  CONF_VIDEO    = 0x02000000,
  CONF_OPTIM    = 0xC0000000
};

typedef struct tag_stru_dec_param {

  void *mempool[2];
  int pool_size[2];

  void *input;
  void *output[MAX_COMPONENT];
  int in_size;

  short width;
  short height;
  short bitdepth;
  short format;

  short cb_width;
  short cb_height;
  short tile_width;
  short tile_height;

  short layer;
  short dwt_level;
  short roi_shift;
  short progression;

  unsigned int configuration;

} stru_dec_param;

// 
// struct		expand_buf
// {
// 	expand_buf():
// buf(NULL),
// buf_size(0)
// {}
// 
// void * buf;
// unsigned int buf_size;
// int width;
// int height;
// };
// 
// typedef expand_buf	ExpendBufInfo;



#endif // BMI_JP2DEC_H
