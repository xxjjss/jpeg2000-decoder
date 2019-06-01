
#pragma once

#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif


	typedef		int		int_32;
	typedef		unsigned int	uint_32;
	typedef		short	int_16;
	typedef		unsigned short	uint_16;

	typedef enum	EncodeMathod_e
	{
		Ckernels_W5X3,
		Ckernels_W9X7,
		UNKNOWN_MATHOD
	}EncodeMathod;

	typedef enum EncodeFormat_e
	{
		RAW,
		RGB,
		YUV444,
		YUV422,
		YUV420,
		BAYERGRB,
		COMPONENT4,
		UNKNOWN_FORMAT
	}EncodeFormat;



	// Need to modify the same enum in GPU kernel
	typedef enum	OutputFormat_e
	{
		IMAGE_FOTMAT_32BITS_ARGB		= 0,	// default format, bit depth = 8 bits 4 channels
		IMAGE_FOTMAT_48BITS_RGB,	//	bit depth = 16 bits 3 channels, DCI default output
		IMAGE_FOTMAT_24BITS_RGB,	//	bit depth = 8 bits 3 channels
		IMAGE_FOTMAT_32BITS_4COMPONENTS,
		IMAGE_FOTMAT_64BITS_4COMPONENTS,
		IMAGE_FOTMAT_48BITS_3COMPONENTS,
		IMAGE_FOTMAT_24BITS_3COMPONENTS,
		IMAGE_FOTMAT_NONE,			// todo : when we support aformat, bring that ahead of this
		IMAGE_FOTMAT_64BITS_ARGB,	// bit depth = 16 bits 4 channels
		IMAGE_FOTMAT_36BITS_RGB,	//	bit depth = 12 bits 3 channels
		IMAGE_FOTMAT_48BITS_ARGB	// bit depth = 16 bits 4 channels
	}OutputFormat;

	typedef enum	InputFormat_e
	{
		INFORMAT_RGB_START, 
		IMAGE_INFOTMAT_32BITS_ARGB		= INFORMAT_RGB_START,	// default format, bit depth = 8 bits 4 channels
		IMAGE_INFOTMAT_24BITS_RGB,	//	bit depth = 8 bits 3 channels
		IMAGE_INFOTMAT_48BITS_RGB,	//	bit depth = 16 bits 3 channels, DCI default output
		INFORMAT_RGB_END = IMAGE_INFOTMAT_48BITS_RGB, 
		IMAGE_INFOTMAT_SINGLE_COMPONENT_START,
		IMAGE_INFOTMAT_8BITS_256COLOR = IMAGE_INFOTMAT_SINGLE_COMPONENT_START,	//	bit depth = 8 bits , treat as 1 channel
		IMAGE_INFOTMAT_4BITS_16COLOR,	//	bit depth = 4 bits , treat as 1 channel
		IMAGE_INFOTMAT_SINGLE_COMPONENT_END,
		IMAGE_INFOTMAT_RAW_DATA_START,

		IMAGE_INFOTMAT_RAW_DATA_END,
		IMAGE_INFOTMAT_COMPRESSED,	

		IMAGE_INFOTMAT_UNKNOWN,			// todo : when we support aformat, bring that ahead of this
	}InputFormat;

	struct Size_s 
	{
		int	x;
		int y;
	};
	typedef	struct Size_s		Size, Pos, TwoD, *P_Size, *P_Pos, *P_TwoD;

	struct ShortSize_s 
	{
		short	x;
		short	y;
	};
	typedef	struct ShortSize_s		sSize, sPos, s2d, *P_sSize, *P_sPos, *P_s2d;

#ifdef __cplusplus
}
#endif
