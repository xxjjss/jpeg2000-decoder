
//#include "codec_base.h"
#pragma  once

#if _WINDOWS
#include "..\..\common\include\codec_datatype.h"
#include "..\..\codec\include\codec_macro.h"
#else
#include "codec_datatype.h"
#include "codec_macro.h"
#endif

#include <cuda_runtime.h>
typedef	cudaStream_t	StreamId;


typedef	enum	CudaBufType_e
{
	HOST_BUF,
	HOST_ALLLIGN_BUF,
	GPU_BUF,
	GPU_ALLIGN_BUF,
	UNKNOWN_BUF_TYPE
}CudaBufType;

typedef	enum	CudaBufStatus_e
{
	BUF_STATUS_UNINITIALIZED,
	BUF_STATUS_IDLE,
	BUF_STATUS_DECODING,
	BUF_STATUS_UPLOADING,
	BUF_STATUS_SYNCING
}CudaBufStatus;

typedef enum	CudaCopyDirection_e
{
	CUDA_COPY_HOST_TO_DEVICE,
	CUDA_COPY_DEVICE_TO_HOST,
	CUDA_COPY_HOST_TO_HOST,
	CUDA_COPY_DEVICE_TO_DEVICE,
	INVALID_COPY_DIRECTION
}CudaCopyDirection;

typedef	struct  SpcValue_s
{
	unsigned char	 mRes_Subband;	// 8 bites : xxxxabcd : xxxx is the resolution level  abcd indicate the position of this subband: ll : 0001 LH : 0010 HL : 0100 HH : 1000
	unsigned char	mComponentId;
	unsigned short	mTileId;
}SpcValue;

typedef	union BufContainInfo_s
{
	unsigned int mContainInfo;	// 0xttttccRS
	SpcValue	 mSpcValue;
}BufContainInfo;

typedef		struct	CudaBuf_s
{
	CudaBufType		mType;
	unsigned int	mSize;
	unsigned int	mPitch;
	unsigned int	mWidth;
	unsigned int	mHeight;
	CudaBufStatus	mStatus;
	StreamId		mAsyncId;		

	int				mOwnerThreadIndex;
	BufContainInfo	mBufContain;

	void *	mBuf;

}CudaBuf, *P_CudaBuf;


typedef	struct bmi_gpu_info_s {
	// active?
	int active;
	// device number
	int device;
	// device properties
	char name[256];
	int major;
	int minor;
	unsigned int	memory;
	int multiProcessorCount;
	int	clockRate;
}GpuInfo;


typedef	struct	SubbandInfo_s_c
{
	int		mId;
	s2d		mOff;
	s2d		mSize;
	union
	{
		int		miAbsStep;
		float	mfAbsStepNorm;
	};
	int		mQquanShift;

	int		mLinesInserted;
}SubbandInfo_c;


typedef	struct	CompInfo_s_c
{

	SubbandInfo_c * mSubbandInfo;
// 	union 
// 	{
		int		mNumOfSubband;
// 		int		mNextDwtLevel;	
// 	};
// 	union
// 	{
		int		mSubbandFinishedBitmap;
		int		mResolutionFinishedId;
		int		mSubbandFinishedCounter;

		int	mAsyncOperationStreamIndex;
// 	};

	union
	{
		CudaBuf * mCompBuf;
		void	* mDwtBuf;
	};
	
	uint_8 mDeShifted;
	uint_8 mCompBufMoved;

}CompInfo_c;

typedef	struct	TileInfo_s_c
{
	s2d		mOff;
	s2d		mSize;

	CompInfo_c *	mCompInfo;
	int		mComFinishedCounter;
	int		mLastCompId;
	union
	{
		CudaBuf * mResultBuf;
		void	* mCpuResultBuf;
	};

	uint_8	mXParity[MAX_DWT_LEVEL];
	uint_8	mYParity[MAX_DWT_LEVEL];

}TileInfo_c;


