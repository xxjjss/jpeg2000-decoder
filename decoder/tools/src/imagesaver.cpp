#include "imagesaver.h"
#include <stdio.h>
#include "tiff.h"
#include "platform.h"
#include "debug.h"
#include "memory.h"

namespace BMI
{

#if _MAC_OS ||  _LINUX
	typedef struct tagBITMAPINFOHEADER{
		unsigned int      biSize;
		int       biWidth;
		int       biHeight;
		unsigned short       biPlanes;
		unsigned short       biBitCount;
		unsigned int      biCompression;
		unsigned int      biSizeImage;
		int       biXPelsPerMeter;
		int       biYPelsPerMeter;
		unsigned int      biClrUsed;
		unsigned int      biClrImportant;
	} BITMAPINFOHEADER;

#define BI_RGB	0

#endif

	bool	ImageParamsCorrect(const CodeStreamProperties  & imageProperties, ImageSavingFileType fileType, int bufLength)
	{

#pragma	BMI_PRAGMA_TODO_MSG("check the parameter and data are good to save")
		return true;

	}
	
	void BMPImageSaver(FILE * &fimg, const CodeStreamProperties  & imageProperties, const void * buf, int bufLength)
	{

		
		BMI_ASSERT((imageProperties.mOutputFormat == IMAGE_FOTMAT_24BITS_RGB) || (imageProperties.mOutputFormat == IMAGE_FOTMAT_48BITS_RGB) 
			||  (imageProperties.mOutputFormat == IMAGE_FOTMAT_24BITS_3COMPONENTS) ||  (imageProperties.mOutputFormat == IMAGE_FOTMAT_48BITS_3COMPONENTS));
		BMI_ASSERT(fimg);

		struct bmpFileHeader
		{
			bmpFileHeader():
			reserved(0),
			imageOffset(54)
			{
				signature[0] = 'B';
				signature[1] = 'M';
			}

			char	signature[2];
			int		fileSize;
			int		reserved;
			int		imageOffset;
			BITMAPINFOHEADER	infoHeader;
		}  fileHeader;	

		memset(&fileHeader.infoHeader, 0, sizeof(BITMAPINFOHEADER));
		int lineLengthInBytes; 
		switch(imageProperties.mOutputFormat)
		{
		case IMAGE_FOTMAT_32BITS_ARGB:
		case IMAGE_FOTMAT_32BITS_4COMPONENTS:
			fileHeader.infoHeader.biBitCount = 32;
			lineLengthInBytes = imageProperties.mOutputSize.x * 4;
			break;
		case IMAGE_FOTMAT_24BITS_RGB:
		case IMAGE_FOTMAT_24BITS_3COMPONENTS:
			fileHeader.infoHeader.biBitCount = 24;
			lineLengthInBytes = imageProperties.mOutputSize.x * 3;
			break;
		case IMAGE_FOTMAT_36BITS_RGB:
		case IMAGE_FOTMAT_48BITS_ARGB:
		case IMAGE_FOTMAT_48BITS_RGB:
			// incompatibles format for BMP
			return;

		default:
			BMI_ASSERT(0);
			break;
		}

		fileHeader.fileSize = 	bufLength + 54;
		fileHeader.infoHeader.biSize = sizeof(BITMAPINFOHEADER);	
		fileHeader.infoHeader.biWidth = imageProperties.mOutputSize.x;
		fileHeader.infoHeader.biHeight = imageProperties.mOutputSize.y;
		fileHeader.infoHeader.biPlanes = 1;
		fileHeader.infoHeader.biCompression = BI_RGB;

		fwrite(&fileHeader, 1, 2, fimg);
		fwrite(&(fileHeader.fileSize), 1, 12, fimg );
		fwrite(&(fileHeader.infoHeader), 1, 40, fimg);
		const unsigned char * fbuf = reinterpret_cast<const unsigned char *>(buf);
		for (int l = imageProperties.mOutputSize.y - 1; l >= 0; --l)	// v-flip
		{

			fwrite(fbuf + l * lineLengthInBytes, 1, lineLengthInBytes, fimg);
		}

	}


	void TIFFImageSaver(FILE * fimg, const CodeStreamProperties  & imageProperties, const void * buf,int bufLength)
	{
		ImageParams imageParams;
		int numBytesPerComponent = 1;
		if(imageProperties.mOutputFormat == IMAGE_FOTMAT_48BITS_RGB || imageProperties.mOutputFormat == IMAGE_FOTMAT_48BITS_3COMPONENTS)
		{
			numBytesPerComponent = 2;
		}

		imageParams.mWidth = (short)imageProperties.mOutputSize.x;
		imageParams.mHeight = (short)imageProperties.mOutputSize.y;
		imageParams.mSamplesPerPixel = 3;
		imageParams.mBitsPerSample[0] = imageParams.mBitsPerSample[1] = imageParams.mBitsPerSample[2] = 8 * numBytesPerComponent;

		IFD ifd;
		ifd.initialize(imageParams);

//		int pixel_data_byte_cnt = imageParams.mHeight * imageParams.mWidth * numBytesPerComponent * imageParams.mSamplesPerPixel;
		ifd.writeHeader(fimg);

		const unsigned char * fbuf = reinterpret_cast<const unsigned char *>(buf);

		for(int i = 0; i < imageParams.mHeight * imageParams.mWidth ; i++)
		{
			fwrite((fbuf + 2*numBytesPerComponent),numBytesPerComponent,1,fimg);
			fwrite((fbuf + numBytesPerComponent),numBytesPerComponent,1,fimg);
			fwrite(fbuf ,numBytesPerComponent,1,fimg);
			fbuf += numBytesPerComponent * 3;

		}

		//fwrite(resultBuf,1,pixel_data_byte_cnt,fs);								
		ifd.writeEntries(fimg);
		ifd.writeOutSideValues(fimg); //***UUU writeHeader assumes outside value to be after the directory
	}

	ImageSavingResult	SaveImage(const CodeStreamProperties  & imageProperties, const char * fileName, ImageSavingFileType fileType, const void * buf, int bufLength)
	{


		if (fileType <= IMAGEFILE_NONE || fileType >= IMAGEFILE_UNKNOWN)
		{
			return IMAGESAVE_FILE_TYPE_UNKNOWN;
		}
		if (!buf || !bufLength )
		{
			return IMAGESAVE_PARAMETERS_ERROR;
		}
		if (!ImageParamsCorrect(imageProperties, fileType, bufLength))
		{
			return IMAGESAVE_PARAMETERS_ERROR;
		}

		FILE * fimg;
		FOPEN(fimg, fileName, "wb");
		if (!fimg)
		{
			return IMAGESAVE_FILE_OPEN_FAILED;
		}

		// now good to processing filw saving

		switch (fileType)
		{
		case IMAGEFILE_BMP:
			BMPImageSaver(fimg, imageProperties,  buf, bufLength);
			break;

		case IMAGEFILE_TIFF:
			TIFFImageSaver(fimg, imageProperties, buf, bufLength);
			break;

		default:
			BMI_ASSERT(0);

		}

		fclose(fimg);
		return IMAGESAVE_SUCCEED;
	}
}