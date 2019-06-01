#ifndef GPU_SETTING_H
#define GPU_SETTING_H

//////////////////////////////////////////////////////////////////////
//
// IDWT defines
//
//////////////////////////////////////////////////////////////////////
#define GPU_W9X7_FLOAT	1		// defined to 1 then use float in 97 DWT like CPU version, otherwise use fixed point like Jasper

#define IDWT_TILE_WIDTH		16		// 0x10 : must be powers of 2
#define IDWT_TILE_EXT		8		// (IDWT_TILE_WIDTH >> 1)
#define IDWT_LO_BASE		8		//  IDWT_TILE_EXT
#define IDWT_HI_BASE		40		// (IDWT_TILE_EXT + IDWT_TILE_WIDTH + (IDWT_TILE_EXT << 1))

#define IDWT_ALPHA		-1.58613434205992f
#define IDWT_BETA		-0.05298011857296f
#define IDWT_GAMMA		 0.88291107553093f
#define IDWT_DELTA		 0.44350685204397f
#define IDWT_LO_GAIN	 1.23017410558578f
#define IDWT_HI_GAIN	 1.62578613134411f		// (2.0 / IDWT_LO_GAIN)
#define IDWT_RENORM		 8192.0f			// 1 << 13
#define IDWT_NORM		 0.0001220703125f	// (1.0 / IDWT_RENORM)

#define		SET_BOUNDARY(min_v, v, max_v)		((v) < (min_v) ? (min_v) : ((v) > (max_v) ? (max_v) : (v)))
#define		LIFT(v, inDeep, outDeep)		(((inDeep) >= (outDeep)) ? ((v) >> ((inDeep)-(outDeep))) : ((v) << ((outDeep) - (inDeep))))

#define		BmiDoubleToFix(x)				(int)((x) * (double)(0x2000))		//	0x2000 = 1 << 13
#define		BmiFixMul(x, y)					(int)(((BMI_INT_64)(x) * (BMI_INT_64)(y)) >> 13)


#define	MAX_THREADS		512

#endif