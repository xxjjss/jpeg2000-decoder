
#pragma once

#include "platform.h"
#include "codec_datatype.h"

#define		SAFE_DELETE_ARRAY(x)		if(x) {delete[] (x); x = NULL;}
#define		SAFE_DELETE(x)			if(x) {delete(x); x = NULL;}
#define		SAFE_FREE(x)			if(x) {free(x); x = NULL;}



typedef	enum enum_dec_progression {
	UNKNOWN_PROGRESSION = -1,
	LRCP,
	RLCP,
	RPCL,
	PCRL,
	CPRL
}Jp2_Progression;


namespace BMI
{

	const int INVALID	= -1;

	typedef	enum Status_e
	{
		INVALID_STATUS = INVALID,
		READY = 0,
		PARSING,
		PARSE_ERROR,
		PARSE_ERROR_INPUTFILE_ERROR,
		PARSE_ERROR_INVALID_INPUT,
		PARSE_ERROR_UNKNOWN_FORMAT,
		PARSE_ERROR_UNSUPPORTED_FORMAT,
		DECODE_IGNORED,
		DECODING,
		ENCODING = DECODING,
		PRE_DECODE_ERROR,
		PRE_ENCODE_ERROR = PRE_DECODE_ERROR,
		DECODE_ERROR,
		ENCODE_ERROR = DECODE_ERROR,
		RELEASED,
		ENGINE_EXPIRED,
		UNKNOWN
	}Status, *P_Status;


	typedef	struct ARGB_32BITS_s
	{
		union
		{
			uint_32	mPixel;
			struct  
			{
				union 
				{
					uint_8	mBlue;
					uint_8 mComp3;
				};
				union
				{
					uint_8	mGreen;
					uint_8 mComp2;
				};
				union
				{
					uint_8	mRed;
					uint_8 mComp1;
				};
				union
				{
					uint_8	mAlpha;
					uint_8 mComp0;
				};
			};
		};
	} ARGB_32BITS, FOUR_COMPONENT_32BITS;


	typedef	struct RGB_24BITS_s
	{
		union 
		{
			uint_8	mBlue;
			uint_8 mComp2;
		};
		union
		{
			uint_8	mGreen;
			uint_8 mComp1;
		};
		union
		{
			uint_8	mRed;
			uint_8 mComp0;
		};
	} RGB_24BITS, THREE_COMPONENTS_24BITS;

	typedef	struct RGB_48BITS_s
	{
		union
		{
			uint_16	mBlue;
			uint_16	mComp2;
		};
		union
		{
			uint_16	mGreen;
			uint_16	mComp1;
		};
		union
		{
			uint_16	mRed;
			uint_16	mComp0;
		};	
	}RGB_48BITS, THREE_COMPONENTS_48BITS;

	typedef	struct ARGB_64BITS_s
	{
		union
		{
			uint_16	mBlue;
			uint_16	mComp3;
		};
		union
		{
			uint_16	mGreen;
			uint_16	mComp2;
		};
		union
		{
			uint_16	mRed;
			uint_16	mComp1;
		};
		union
		{
			uint_16	mAlpha;
			uint_16	mComp0;
		};
	} ARGB_64BITS, FOUR_COMPONENTS_64BITS;


	typedef	int	CodeStreamHandler;

	typedef	struct CodeStreamProperties_s
	{
		sSize	mSize;
		sSize	mOutputSize;
		sSize	mThumbOutputSize;
		OutputFormat	mOutputFormat;
		sSize	mCodeBlockSize;
		sSize	mTileSize;
		s2d		mTileNum;
		short	mComponentNum;
		short	mCalculatedComponentNum;
		short	mDwtLevel;
		short	mLayer;
		short	mBitDepth;
		short	mRoiShift;
		EncodeFormat	mFormat;
		EncodeMathod	mMethod;
		Jp2_Progression	mProgression;
		int		mHeaderLength;

	} CodeStreamProperties, *P_CodeStreamProperties;

	typedef	struct BMPPictureProperties_s
	{
		sSize	mSize;
		short	mComponentNum;
		short	mBitDepth;
		short	mPixelLengthInBits;
	} BMPPictureProperties, *P_BMPPictureProperties;


	typedef enum WordLength_e
	{
		USHORT_16_BITS = 2,
		UINT_32_BITS = 4
	} WordLength;


	typedef struct EnginConfig_s
	{
		BMI_J2K_CODEC_EXPORTS EnginConfig_s():
		mInputQueueLen(1),
		mOutputQueueLen(1),
		mThreadNum(0),
		mCpuCoresNum(1),
		mSyncMode(true),
		mUsingGpu(true),
		mGpuT1(false)
		{
		}

		int		mInputQueueLen;
		int		mOutputQueueLen;
		int		mThreadNum;
		int		mCpuCoresNum;
		bool	mSyncMode;
		bool	mUsingGpu;
		bool	mGpuT1;

	}EnginConfig, *P_EnginConfig;

	/****************************************************************************
	Name : CodecJobConfig
	Type : structure with constructor 
	Member:
	mWordLength	: The word length used in calculation, in 97, we use float, so it must be 32 bits, in 53, we could select use interger
					or short. now only support Integer
	mIgnoreComponents : A bit map for the components to be ignored in coding or decoding, for example:
					Ignore component 0 set this value to 01 (0x01)
					Ignore component 0 and 2 set this value to 101 (0x05)
	****************************************************************************/
	typedef struct CodecJobConfig_s
	{
		BMI_J2K_CODEC_EXPORTS CodecJobConfig_s():
		mWordLength(UINT_32_BITS),
		mIgnoreComponents(0)
		{
		}

		WordLength	mWordLength;
		unsigned int mIgnoreComponents;
	}CodecJobConfig, *P_CodecJobConfig;

	typedef struct DecodeJobConfig_s : public CodecJobConfig_s
	{
		BMI_J2K_CODEC_EXPORTS DecodeJobConfig_s():
		mCheckOutComponent(INVALID),
		mDwtDecodeTopLevel(-1),
		mOutputFormat(IMAGE_FOTMAT_NONE)
		{
		}

		int			mCheckOutComponent;
		int			mDwtDecodeTopLevel;
		OutputFormat	mOutputFormat;

	}DecodeJobConfig, *P_DecodeJobConfig;

	typedef struct EncodeJobConfig_s
	{
		BMI_J2K_CODEC_EXPORTS EncodeJobConfig_s():
		mEncodeMethod(Ckernels_W9X7),
		mDwtLevel(5)
		{
			// by default ,we set the encode methos to w9x7 and the codeblock size to 32*32, single tile, 5 level DWT to match the DCI spec
			mCodeBlockSize.x = mCodeBlockSize.y = 32;		
			mTileNum.x = mTileNum.y = 1;
		}
		
		EncodeMathod mEncodeMethod;
		sSize mCodeBlockSize;
		sSize mTileNum;
		int	mDwtLevel;
	}EncodeJobConfig, *P_EncodeJobConfig;

	typedef	struct	InputProperties_s
	{
		InputProperties_s():
		mInputFormat(IMAGE_INFOTMAT_UNKNOWN),
		mWithHeader(false),
		mBitStreamOffset(0),
		mCompressed(false)
		{
		}

		InputFormat	mInputFormat;			// bits stream format

		bool			mWithHeader;		// true/false
		int				mBitStreamOffset;	// the bits strem data offset, for skipping the header

		bool	mCompressed;
		sSize	mSize;				// picture's size, in pixel
		short	mComponentNum;		// number of channel of the input data
		short	mBitDepth;			// bitdepth of each channel
		short	mPixelLengthInBits;	// bits of each pixel
		

	}InputProperties	;

	typedef struct InputStream_s
	{
		InputStream_s(): 
		mHandler(INVALID),
		mInputBuf(NULL),
		mBufSize(0),
		mStatus(INVALID_STATUS),
		mResultIndex(INVALID),
		mUncompressedBuf(NULL),
		mUncompressedBufLength(0)

	{
		// 				memset(reinterpret_cast<void *>(&mCudaPictureInfo), 0, sizeof(PictureInfo));
	}

	CodeStreamHandler		mHandler;
	const void *			mInputBuf;
	int						mBufSize;
	Status					mStatus;
	int						mResultIndex;
	InputProperties			mProperties;
	EncodeJobConfig			mConfig;

	unsigned char *			mUncompressedBuf;
	int						mUncompressedBufLength;


	}InputStream, *P_InputStream;

	class Decoder
	{
		public:

		BMI_J2K_CODEC_EXPORTS	virtual CodeStreamHandler Decode(const void * codeStreamBuffer, int bufSize) = 0;
		BMI_J2K_CODEC_EXPORTS	virtual CodeStreamHandler Decode(const void * codeStreamBuffer, int bufSize, double & time) = 0;
		BMI_J2K_CODEC_EXPORTS	virtual Status PreDecode(const void * codeStreamBuffer, int bufSize,  CodeStreamProperties & properties) = 0;

		BMI_J2K_CODEC_EXPORTS static	Decoder *	Get();
		BMI_J2K_CODEC_EXPORTS static	void		Terminate();

		BMI_J2K_CODEC_EXPORTS virtual	void GetVersion(int & mainVersion, int & subvVrsion, int &branchVersion, int &year, int & month, int & date) = 0;
		
		BMI_J2K_CODEC_EXPORTS virtual int	Init(const P_EnginConfig enginConfig = NULL) = 0;
		BMI_J2K_CODEC_EXPORTS virtual const EnginConfig * GetEngineConfig() const = 0;

		BMI_J2K_CODEC_EXPORTS virtual void	SetJobConfig(const DecodeJobConfig & config) = 0;
		BMI_J2K_CODEC_EXPORTS virtual void	GetJobConfig(DecodeJobConfig & config, CodeStreamHandler handler = INVALID) const = 0;

		BMI_J2K_CODEC_EXPORTS	virtual Status	CheckStatus(CodeStreamHandler handler) const = 0;
		BMI_J2K_CODEC_EXPORTS	virtual bool	CheckProperties(CodeStreamHandler handler, CodeStreamProperties & properties) const = 0;
		BMI_J2K_CODEC_EXPORTS	virtual	const void *	GetResult(CodeStreamHandler handler, int &bufSize) = 0;
		BMI_J2K_CODEC_EXPORTS	virtual	const void *	GetThumbnail(CodeStreamHandler handler, int &bufSize) = 0;
		BMI_J2K_CODEC_EXPORTS	virtual	const int *	GetDecodedComponentInt(CodeStreamHandler handler, int &bufSize, int componentId, int tileId = INVALID) = 0;
		BMI_J2K_CODEC_EXPORTS	virtual	const float *	GetDecodedComponentFloat(CodeStreamHandler handler, int &bufSize, int componentId, int tileId = INVALID) = 0;
		BMI_J2K_CODEC_EXPORTS	virtual void	Release(CodeStreamHandler handler) = 0;

		BMI_J2K_CODEC_EXPORTS virtual ~Decoder() {}

	};

	class Encoder
	{
	public:

		BMI_J2K_CODEC_EXPORTS virtual CodeStreamHandler Encode(const void * codeStreamBuffer, int bufSize, InputProperties	* inputInfor = NULL) = 0;
		BMI_J2K_CODEC_EXPORTS virtual CodeStreamHandler Encode(const void * codeStreamBuffer, int bufSize, double & time, InputProperties	* inputInfor = NULL) = 0;

		BMI_J2K_CODEC_EXPORTS virtual Status	CheckStatus(CodeStreamHandler handler) const = 0;
		BMI_J2K_CODEC_EXPORTS virtual bool	CheckProperties(CodeStreamHandler handler, CodeStreamProperties & properties) const = 0;
		BMI_J2K_CODEC_EXPORTS virtual const void *	GetResult(CodeStreamHandler handler, int &bufSize) = 0;
		BMI_J2K_CODEC_EXPORTS virtual void	Release(CodeStreamHandler handler) = 0;

		BMI_J2K_CODEC_EXPORTS virtual void	SetJobConfig(const EncodeJobConfig & config) = 0;
		BMI_J2K_CODEC_EXPORTS virtual void	GetJobConfig(EncodeJobConfig & config, CodeStreamHandler handler = INVALID) const = 0;
		BMI_J2K_CODEC_EXPORTS virtual int	Init(const P_EnginConfig enginConfig = NULL) = 0;


		BMI_J2K_CODEC_EXPORTS static	Encoder *	Get();
		BMI_J2K_CODEC_EXPORTS static	void		Terminate();

		BMI_J2K_CODEC_EXPORTS virtual	void GetVersion(int & mainVersion, int & subvVrsion, int &branchVersion, int &year, int & month, int & date) = 0;

		BMI_J2K_CODEC_EXPORTS virtual ~Encoder() {}

	};

}