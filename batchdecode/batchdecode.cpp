
#include "codec_base.h"
#include "platform.h"
#include "testclock.h"
#include "debug.h"

#if _MAC_OS
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
#endif

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
} ;	

void convertToPrintable(char * dest, int destlen, char * src)
{
	int l = 0;
	while (*src != '\0' && l < destlen-1)
	{
		if (*src == '\\')
		{
			*dest++ = '\\';
			++l;
		}
		*dest++ = *src++;
		++l;
	}
	*dest = '\0';
}

int readInFile(const char *fileName, void * &buf, int & bufSize)
{
	int ret = 0;
	FILE * fp;
	FOPEN(fp, fileName, "rb");
	if (fp)
	{
		PRINT(("File %s opened.\n", fileName));
		fseek(fp, 0, SEEK_END);
		int filesize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (filesize >= bufSize)
		{
			SAFE_FREE(buf);
			buf = malloc(filesize);
			bufSize = filesize;
		}
		FREAD_S(buf, bufSize, 1, bufSize, fp);
		fclose(fp);
		ret = filesize;
	}
	else
	{
		PRINT(("File %s open failed\n", fileName));
	}

	return ret;
}


bool SaveBmp(const char * fileName, BMI::CodeStreamProperties &properties, const void * bmpBuf, int bufSize)
{

	bmpFileHeader fileHeader;
	bool ret = false;

	fileHeader.fileSize = 	bufSize + 54;
	fileHeader.infoHeader.biSize = sizeof(BITMAPINFOHEADER);	
	fileHeader.infoHeader.biWidth = properties.mOutputSize.x;
	fileHeader.infoHeader.biHeight = properties.mOutputSize.y;
	fileHeader.infoHeader.biPlanes = 1;
	fileHeader.infoHeader.biBitCount = 32;

#if _MAC_OS
	#define BI_RGB	0
#endif

	fileHeader.infoHeader.biCompression = BI_RGB;
	fileHeader.infoHeader.biSizeImage = bufSize;
	fileHeader.infoHeader.biXPelsPerMeter = fileHeader.infoHeader.biYPelsPerMeter = 100;
	fileHeader.infoHeader.biClrUsed = 0xffffff;
	fileHeader.infoHeader.biClrImportant = 0;

	FILE * fs ;
	FOPEN(fs, fileName, "wb");
	if (fs)
	{
		fwrite(&fileHeader, 1, 2, fs);
		fwrite(&(fileHeader.fileSize), 1, 12, fs );
		fwrite(&(fileHeader.infoHeader), 1, 40, fs);
		const unsigned char * fbuf = reinterpret_cast<const unsigned char *>(bmpBuf);

		for (int l = properties.mOutputSize.y - 1; l >= 0; --l)	// v-flip
		{

			fwrite((fbuf + ((l * properties.mOutputSize.x)<<2)), 1, (properties.mOutputSize.x<<2), fs);
		}
		fclose(fs);
		ret = true;
	}

	return ret;
}


int main(int argc, char * argv[])
{

	BMI::Decoder * decoder = BMI::Decoder::Get();
	int threadNum = 0;

	if (argc < 3 || argc == 4 || argc > 7)
	{
		PRINT(("Usage :\n"));
		PRINT(("batchdecoder input_filename_template output_filename_template [file_No_start] [file_No_end] [DWT_level] [Component]\n"));
		PRINT(("input_filename_template : Input jpeg2000 format file(s)\n"));
		PRINT(("\tIf you want to decode more than one file, you can input the file name \n"));
		PRINT(("\ttemplate here. For example you have 100 files named input001.jp2 to \n"));
		PRINT(("\tinput100.jp2,you can set the file name template here as 'input%%03d.jp2'\n"));
		PRINT(("\tAnd set the file_No_start parameter as 1 and file_No_end as 100\n"));
		PRINT(("output_filename_template: Same as input_filename_template\n"));
		PRINT(("file_No_start : Optional, The start file No apply into the template, without\n"));
		PRINT(("\tthis parameter, we only convert one file.\n"));
		PRINT(("file_No_end   : Optional, if you set file_No_start, you must set this too\n"));
		PRINT(("DWT_level     : Optional, decode DWT level, default -1. If input number\n\tequal or greater than DWT level or less than 0, ignore\n"));
		PRINT(("Component     : Optional, check out component, Will generate grey picture\n\tuse the component. default -1 will use the first 3 components\n"));
		exit(0);
	}

	char inFileFmt[1024];
	convertToPrintable(inFileFmt, 1024,argv[1]);
	char outFileFmt[1024];
	convertToPrintable(outFileFmt, 1024,argv[2]);

	int startSn = BMI::INVALID, stopSn = BMI::INVALID;

	if (argc > 4)
	{
		startSn = atoi(argv[3]);
		stopSn = atoi(argv[4]);
	}

	BMI::EnginConfig enginConfig;
	threadNum = decoder->Init(&enginConfig);		

	BMI::DecodeJobConfig config;
	decoder->GetJobConfig(config);
	config.mCheckOutComponent = -1;
	config.mDwtDecodeTopLevel = -1;
	config.mOutputFormat = IMAGE_FOTMAT_32BITS_ARGB;
	if (argc >= 6)
	{
		config.mDwtDecodeTopLevel = atoi(argv[5]);
	}
	if (argc >= 7)
	{
		config.mCheckOutComponent = atoi(argv[6]);
	}
	decoder->SetJobConfig(config);

	char filename[1024];
	void * buf = NULL;
	int bufsize = 0;

	if (startSn == BMI::INVALID)
	{
		int filesize = readInFile(inFileFmt, buf, bufsize);
		if (filesize)
		{
			double t;
			int handler	= decoder->Decode(buf, filesize, t);
			BMI::Status status = decoder->CheckStatus(handler);
			if (status != BMI::READY)
			{
				PRINT(("Status error with code : %d !!\n", status));
			}
			else
			{
				PRINT(("\tDecode SUCCEED!! Cost %f msec\n",  t));

				BMI::CodeStreamProperties properties;
				decoder->CheckProperties(handler, properties);

				const void * result = NULL;
				int picSize;
				result =  decoder->GetResult(handler, picSize);


				if(SaveBmp(outFileFmt, properties, result, bufsize))
				{
					PRINT(("\tSave bmp file as %s\n", filename));
				}
				else
				{
					PRINT(("\tFile save error!!!\n"));
				}

				decoder->Release(handler);
			}
		}
	}
	else
	{
		BMI_ASSERT(startSn >= 0 && stopSn >= startSn);
		double t, totalT = 0.0f, minT = 999.9f, maxT = 0.0f;
		int decodedFile = 0;

		for (int i = startSn; i <= stopSn; ++i)
		{
			SPRINTF_S((filename, 1024, inFileFmt, i));
			int filesize = readInFile(filename, buf, bufsize);
			if (filesize)
			{
				int handler	= decoder->Decode(buf, filesize, t);
				BMI::Status status = decoder->CheckStatus(handler);
				if (status != BMI::READY)
				{
					PRINT(("Status error with code : %d !!\n", status));
				}
				else
				{
					PRINT(("\tDecode SUCCEED!! Cost %f msec\n",  t));
					++decodedFile;

					totalT += t;
					minT = minT<t ? minT : t;
					maxT = maxT>t ? maxT : t;

					BMI::CodeStreamProperties properties;
					decoder->CheckProperties(handler, properties);

					const void * result = NULL;
					int picSize;
					result =  decoder->GetResult(handler, picSize);
				
					SPRINTF_S((filename, 1024, outFileFmt, i));

					if(SaveBmp(filename, properties, result, bufsize))
					{
						PRINT(("\tSave bmp file as %s\n", filename));
					}
					else
					{
						PRINT(("\tFile save error!!!\n"));
					}

					decoder->Release(handler);
				}
			}
		}
		PRINT(("\nToatlly decode %d files\n\tDecode time min: %f  ave: %f  max: %f\n", decodedFile, minT, totalT/decodedFile, maxT));
	}

	SAFE_FREE(buf);
	return 0;
}