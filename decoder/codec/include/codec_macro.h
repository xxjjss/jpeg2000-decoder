/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#ifndef CODEC_MACRO_H
#define CODEC_MACRO_H


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_COMPONENT         4         // max number of components
#define MAX_DWT_LEVEL         6         // max number of decomposition level

#define MAX_TILE			  256       // max number of Tile in an image
#define MAX_PACKAGE			  256       // max number of Package in a Tile


#define BMI_CEILDIV2(x) (((x) + 1) >> 1)

#ifdef __cplusplus
}
#endif

#endif
