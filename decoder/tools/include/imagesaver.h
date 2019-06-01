#pragma  once

#include "codec_datatype.h"
#include "codec_base.h"

namespace BMI
{

	enum	ImageSavingFileType
	{
		IMAGEFILE_NONE = 0,
		IMAGEFILE_BMP,
		IMAGEFILE_TIFF,
		IMAGEFILE_UNKNOWN
	};

	enum	ImageSavingResult
	{
		IMAGESAVE_SUCCEED = 0,
		IMAGESAVE_FILE_OPEN_FAILED,
		IMAGESAVE_FILE_TYPE_UNKNOWN,
		IMAGESAVE_PARAMETERS_ERROR

	};


	BMI_J2K_CODEC_EXPORTS	ImageSavingResult	SaveImage(const CodeStreamProperties  & imageProperties, const char * fileName, ImageSavingFileType fileType, const void * rawBuf, int bufLength);

}