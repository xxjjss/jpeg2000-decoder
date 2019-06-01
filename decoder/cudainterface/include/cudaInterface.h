#pragma once

#include "codecconcrete.h"
#include "cudainterfacedatatype.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <vector>

#include "codec.h"

#if !_MAC_OS && !_LINUX
#include <malloc.h>
#endif

#include "singleton.h"
#include "testclock.h"


using namespace BMI;



namespace BMI
{

	const	int		DYNAMIC_ASYNC_ID_NUM	=	20;

	typedef	enum	CudaThreadJobType_e
	{
		INIT_CUDA,
		INIT_GPU_BUF,
		CUDA_BUF_COPY,
		CUDA_BUF_COPY_WITH_DIFF_PITCH,
		CUDA_BUF_SYNC,
		CUDA_STREAM_SYNC,
		CREATE_ASYNC_ID,
		DESTORY_STREAM_ID,
		FREE_CUDA_BUF,
		DEQ_97_SUBBAND,
		DEQ_97_COMPONENT,
		DWT_RESOLUTION,
		DWT_COMPONENT,
		MCT_53_TILE,
		ICT_97_TILE,
		MERGE_TILE,
		CHECK_OUT_ONE_COMPONENT,
		GET_ONE_COMPONENT,
// 		CUDA_COPY_OUT,
		SYNC_ALL,
		QUIT_CUDA,
		RELEASE_SEMAPHORE,
#if GPU_T1_TESTING
		GPU_DECODE_CODEBLOCK,
#endif
		UNKNOWN_CUDA_THREAD_JOB
	}CudaThreadJobType;

	typedef	struct	AsyncStream_s
	{
		StreamId	mAsyncId;
		bool		mIsIdle;
	}AsyncStream, * P_AsyncStream;

	struct CudaInitJob 
	{
		GpuInfo * mGpuInfo;
		int		* mErrorCode;
	};

	struct CudaInitBufJob
	{
		CudaBuf	* mCudaBuf; 
		CudaBufType mType;
		unsigned int mSize;
		unsigned int mWidth; 
		unsigned int mHeight;
	};


	struct Cuda2DCopyJob
	{
		CudaBuf * mSourceBuf;
		CudaBuf * mDestBuf;
		int mSrcX_off;
		int mSrcY_off;
		int mDestX_off;
		int mDestY_off;
		int mWidth;
		int mHeight;
		CudaCopyDirection mDirection;
		bool mDestNoPitch;
		bool mSrcNoPitch;
		StreamId mAsync_id;
	};

	struct Cuda2DCopyDiffPitchJob
	{
		CudaBuf * mSourceBuf;
		CudaBuf * mDestBuf;
		int mSrcX_off;
		int mSrcY_off;
		int mDestX_off;
		int mDestY_off;
		int mSrcPitch;
		int mDestPitch;
		int mWidth;
		int mHeight;
		CudaCopyDirection mDirection;
		bool mDestNoPitch;
		bool mSrcNoPitch;
		StreamId mAsync_id;
	};

// 	struct	CudaBufSync
// 	{
// 		CudaBuf * mBuf;
// 		StreamId mAsync_id;
// 	};

	struct	DwtComp_ResJob
	{
		CudaBuf	* mOriginalBuf;
		CudaBuf	* mTempBuf;
		TileInfo_c * mTile;
		CompInfo_c * mWorkingComp;
		StreamId	mAsyncId;
		EncodeMathod	mMethod;
		short			mWordShift;
		union
		{
			int				mTopLevel;
			int				mResolutionLevel;
		};
	};

	struct CompDcShiftJob
	{			
		StreamId	mAsyncId;
		EncodeMathod mMethod;
		TileInfo_c * mTileInfo;	
		CudaBuf	   * mCompBuf;
		CudaBuf	   * mCompBufTemp;
		short		mBitDepth;
		short		mPizelWordLength;
		int			mWidth;
		int			mHeight;
		int			mSrcX_off;
		int			mSrcY_off;
		int			mDstX_off;
		int			mDstY_off;
	};


	struct	MCT_ICT_job
	{
		TileInfo_c * mTileInfo;
		StreamId	mAsyncId;
		short		mBitDepth;
		short		mWordShift;
		int			mDwtLevel;
		int			mDecodeLevel;
		OutputFormat	mOutputFormat;
	};

	struct	CheckOutComponet_job
	{
		TileInfo_c * mTileInfo;
		StreamId	mAsyncId;
		int			mComponentId;
		EncodeMathod mMethod;
		bool		mHasMCT;
		short		mBitDepth;
		short		mWordShift;
		int			mDwtLevel;
		int			mDecodeLevel;

	};

	struct Deq97Job
	
	{
		CudaBuf			* mDeviceBuf;
		TileInfo_c		* mWorkingTile;
// 		int				mTileOff_x;
// 		int				mTileOff_y;
		int				mComponentId;
		int				mTopDeqLevel;
		SubbandInfo_c	* mWorkingSubband;
		short			mWordShift;
		StreamId mAsync_id;
	};


	struct TileMergeJob
	{
		TileInfo_c	* mTileInfo;
		int			mComponentNum;
		StreamId	mAsyncId;
		short		mBitDepth;
		short		mWordShift;
		int			mCompBufPitchBytes;
		EncodeMathod mMethod;
		int			mDwtLevel;
		int			mDecodeLevel;
		OutputFormat mOutputFormat;
	};

	struct CudaThreadJob 
	{
		CudaThreadJob():
		mIsBlockJob(false)
		{}

		ThreadInfo	* mCallingThreadInfo;
		bool		mIsBlockJob;
		SEMAPHORE_HANDLE		mReleaseMe;
		CudaThreadJobType type;
		union
		{
			CudaInitJob	mCudaInit;
			CudaInitBufJob mCudaInitBuf;
			Cuda2DCopyJob	mCuda2DCopy;
			Cuda2DCopyDiffPitchJob	mCuda2DCopyDiffPitch;
			CudaBuf		* mCudaBufSync;
			StreamId	*	mP_AsyncId;
			CudaBuf		*	mDeleteBuf;
			int			mStreamSyncIndex;
			DwtComp_ResJob	mDwtComp_ResJob;
			CompDcShiftJob  mCompDcShiftJob;
			MCT_ICT_job	mMctIctJob;
			CheckOutComponet_job	mCheckOutComponentJob;
			Deq97Job	mDeq97Job;
			TileMergeJob mTileMergeJob;
#if T1_SIMULATION_TEST
			T1SimulateJob mT1SimulateJob;
#endif
		};
	};

	typedef	enum	CudaThreadJobQueue_e
	{
		ASYNC_JOB_QUEUE,
		SYNC_JOB_QUEUE,
		UNKNOWN_JOB_QUEUE
	}CudaThreadJobQueue;

	typedef	enum	CudaJobStatus_e
	{
		CUDA_JOB_UNINITED,
		CUDA_JOB_IDLE,
		CUDA_JOB_WORKING,
		CUDA_JOB_READY,
		CUDA_JOB_UNKNOWN
	}CudaJobStatus;

	typedef	enum	GPU_AVAILABLITY
	{
		GPU_UNKNOWN = INVALID,
		GPU_AVAILABLE,
		GPU_UNAVILABLE
	}GpuAvailbility;


	struct	SubbandInfo_s : public SubbandInfo_c
	{
		SubbandInfo_s()
		{	
			mId = INVALID;
			mLinesInserted = 0;
		}
	~SubbandInfo_s(); 

	};
	typedef struct	SubbandInfo_s SubbandInfo, * P_SubbandInfo;


	struct	CompInfo_s : public CompInfo_c
	{
		CompInfo_s()
		{
			mSubbandInfo = NULL;
			mNumOfSubband = 0;
			mSubbandFinishedBitmap = 0;
			mResolutionFinishedId = INVALID;
			mSubbandFinishedCounter = 0;
			mAsyncOperationStreamIndex = INVALID;
			mCompBuf = NULL;
			mDeShifted = 0;
			mCompBufMoved = 0;
		}

	~CompInfo_s();

	};
	typedef	struct	CompInfo_s	CompInfo, * P_CompInfo;

	struct	TileInfo_s : public TileInfo_c
	{
		TileInfo_s()
		{
			mCompInfo = NULL;
			mComFinishedCounter = 0;
			memset(mXParity, 0, MAX_DWT_LEVEL);
			memset(mYParity, 0, MAX_DWT_LEVEL);
		}

	~TileInfo_s();

	};
	typedef	struct	 TileInfo_s TileInfo, * P_TileInfo;

	typedef	struct	PictureInfo_s
	{
		PictureInfo_s():
		mTileinfo(NULL),
		mTileinfoThumb(NULL),
		mTileFinishedCounter(0),
		mParent(NULL)
		{
			memset(&mProperties, 0, sizeof(CodeStreamProperties));
		}

		~PictureInfo_s();

		CodeStreamProperties	mProperties;
		P_TileInfo				mTileinfo;
		P_TileInfo				mTileinfoThumb;
		int					mTileFinishedCounter;
		void *				mParent;

		int					mLastTileId;
		int					mTopDwtLevel;
	}PictureInfo, * P_PictureInfo;

	typedef	struct CudaJob_s
	{
		CudaJob_s();
		~CudaJob_s();

		int				mId;
		CudaJobStatus	mJobStatus;

		int				mOutputIndex;
		P_CudaBuf		mGpuOrigBuf[MAX_COMPONENT];
		P_CudaBuf		mGpuOrigBufThumb[MAX_COMPONENT];
		P_CudaBuf		mGpuTempBuf[MAX_COMPONENT];
		P_PictureInfo	mPicture;
		int				mSizeShift;
	} CudaJob, *P_CudaJob;


	class CudaManager : public Singleton<CudaManager>
	{
	public:
		CudaManager(): 
		mInit(false),
		mGpuAvailability(GPU_UNKNOWN),
		mEngineConfig(NULL),
// 		mJobNum(0),
		mNextJobIndex(0),
		mGpuResultBuf(NULL),
		mHostResultBuf(NULL),
		mComponentResultBuf(NULL),
		mGpuResultBufThumb(NULL),
		mHostResultBufThumb(NULL),
		mStripeBuf(NULL),
		mStripeBufElementSize(0),
		mStripeBufNextIndex(0),
		mStripeBufNum(0),
		mCudajob(NULL),
		mCurJobIndex(0),
		mCudaBufMutex(NULL)
	#if !_MAC_OS && !_LINUX
			,
	    mCudaThreadSemaphore(NULL),
		mCudaBlockSemaphore(NULL),
		mCudaBufSemaphore(NULL),
		mCudaThreadEndingSemaphore(NULL)
		#endif
		{ 

			MUTEX_INIT(mSyncJobMutex,NULL);
			MUTEX_INIT(mAsyncJobMutex,NULL);
			MUTEX_INIT(mCudaJobCounterMutex,NULL);
		}

		virtual	~CudaManager();

		//////////////////////////////////////////////////////////////////////////
		//
		//	In class CudaJob's destructor, we need to call Cudathread to release their buffer
		//	Therefor set CudaJob as friend class
		//
		//////////////////////////////////////////////////////////////////////////
		friend struct CudaJob_s;

	/***********************************************************************************************************
	Function name:	Init
	Description:	Initializes the system for GPU based decoding including initializing the GPU if cuda supported one availalble
					creatung streamsd, creating semaphores etc. 
	IN:
		engineConfig: Engines configuration

	OUT: N/A
	Return: 
			returns 0 if a cuda supported gpu is availalbe otherwise -1
	************************************************************************************************************/
		int		Init(EnginConfig & engineConfig);

	/***********************************************************************************************************
	Function name:	InitPicture
	Description:	Initializes the pictures structures, parameters buffers etc. from the information from the codestream being decoded 
	IN:
		inStream: input j2k stream from which the information are taken
		outputIndex: output index used for this frame (multi frame decoding support)
		s_opt: has the images information recieved from T2 decoding such as tile, component, subband info

	OUT: Thumbnail size is updated in inStream
	Return: N/A
	************************************************************************************************************/

		void	InitPicture(J2K_InputStream &inStream, int outputIndex, const stru_opt * s_opt, int opt_size);

	/***********************************************************************************************************
	Function name:	ReleaseCudaJob
	Description:	Releases the cudaJob specified by the jobIndex 
	IN:
		jobIndex: Index of the job to be released
		
	OUT: N/A
	Return: N/A
	************************************************************************************************************/

		void	ReleaseCudaJob(int jobIndex) 
		{
			if (mCudajob[jobIndex].mJobStatus != CUDA_JOB_UNINITED)
			{
				mCudajob[jobIndex].mJobStatus = CUDA_JOB_IDLE; 
			}
		}
	/***********************************************************************************************************
	Function name:	GetStripeBuffer
	Description:	Gets a stripe buffer from the pool of buffers allocated for the particular thread. This buffer is used for 
					storing the T1 decoding of a code block stripe
	IN:
		pictureIndex: Index of the picture
		callingThread: Calling threads information

	OUT: N/A
	Return: 
			Returns the CudaBuffer containing the stripe buffer given by the function
	************************************************************************************************************/

		CudaBuf	* GetStripeBuffer(PictureIndex & pictureIndex, ThreadInfo * callingThread);

	/***********************************************************************************************************
	Function name:	SyncUnsyncBuffer
	Description:	Synchornizes the buffers that are waiting cuda buffer copies. Buffers are specified by the callingThread
					spcMask and the spcValue

	IN:		
		callingThread: Calling threads information
		spcValue: The integer value that specifies the subband tile and component
		spcMask: Mask used to extract the required infromation from subband tile and component information of spcValue

	OUT: N/A
	Return: N/A
	************************************************************************************************************/

		void	SyncUnsyncBuffer(ThreadInfo * callingThread = NULL, unsigned int spcValue = 0, unsigned int spcMask = 0);


	/***********************************************************************************************************
	Function name:	UploadStripeBuf
	Description:	This is the main function that fills the cuda job queues with jobs for buffers copies to / from gpu, dequantization, 
					IDWT, ICT/MCT etc. The function is called from the decoder with a decoded T1 stripe buffer as the input.

	IN:		
		data: The stripe buffer containing T1 decoded information
		pictureIndex: index of the pictur ebeing decoded
		tileId, compId, subbandId: Information about the stripe where it belongs (I.e. which tile, component and subabnd)
		yOffsetInBand: y direction offset of the stripe within the band
		callingThread: Calling threads information

	OUT: N/A
	Return: N/A
	************************************************************************************************************/

		void	UploadStripeBuf(CudaBuf * data, const DecodeJobConfig & config, PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int linesAvailable, int yOffsetInBand, ThreadInfo * callingThread);

	/***********************************************************************************************************
	Function name:	GetResultBuf
	Description:	Gets the decoded image's buffers

	IN:		
		outputIndex: output index of the frame	
		isThumb: whether it is thumbnail of the whole decoded image, true = it is thumbnail

	OUT:
		output: structure containing the information about the result
	Return: Returns the cuda bauufer containing the results
	************************************************************************************************************/

		CudaBuf * GetResultBuf(int outputIndex, DecoderConcrete::DecodeResult & output);

	/***********************************************************************************************************
	Function name:	GetOneComponent
	Description:	Gets a  tile component before reverse color transform

	IN:		
		outputIndex: output index of the frame		
		tileId, componentId: specifies which tile and which component it is. tileId = -1 means all the tiles
		decodeTopLevel: topDWT level decoded

	OUT:
		sizeofBuf: size of the data in the buffer returned
	Return: Returns the cuda bauufer containing the requested component
	************************************************************************************************************/
		CudaBuf * GetOneComponentInt(int outputIndex, int & sizeofBuf, int tileId, int componentId, int decodeTopLevel);

		/***********************************************************************************************************
	Function name:	GetOneComponentFloat
	Description:	Gets a  tile component before reverse color transform in the original float format without shifting and clipping

	IN:		
		outputIndex: output index of the frame		
		tileId, componentId: specifies which tile and which component it is. tileId = -1 means all the tiles
		decodeTopLevel: topDWT level decoded

	OUT:
		sizeofBuf: size of the data in the buffer returned
	Return: Returns the cuda bauufer containing the requested component
	************************************************************************************************************/
		CudaBuf * GetOneComponent(int outputIndex, int & sizeofBuf, int tileId, int componentId, int decodeTopLevel);

		
   /***********************************************************************************************************
	Function name:	GetThumbnail
	Description:	Gets the decoded image's thumbnail buffers

	IN:		
		outputIndex: output index of the frame			

	OUT:
		 sizeofBuf: size of the data in the buffer returned
	Return: Returns the cuda bauufer containing the results
	************************************************************************************************************/
		
	CudaBuf * GetThumbnail(int outputIndex, int & sizeofBuf);

	/***********************************************************************************************************
	Function name:	ThreadSafeAccessCudaThreadJob
	Description:	Access the cuda job quese without conflicts among multiple threads accessing them

	IN:		
		callingThread: Calling threads information	
		jobQueue: Specifies which job queue is accessed (ex: sync queue or async job queue)
		job: Particular job with which the job queue is accessed (for example a job for idwt)
		mode: Type of access (for example attach a job, get a job remove a job etc )	
		
	OUT: N/A		
	Return: Returns whether the access was successful (true = successful)
	************************************************************************************************************/

		bool	ThreadSafeAccessCudaThreadJob(const ThreadInfo * callingThread,CudaThreadJobQueue jobQueue, DecodeJob &job, JobQueueAccessMode mode = GET_A_JOB, int * para = NULL);
		void	ThreadSafeWaitForCudaThreadFinish(ThreadInfo * callingThread);

#if GPU_T1_TESTING
		void	DecodeCodeBlock(bool bypass , int_32 * context_words, uint_8 * pass_buf,	short numBps, stru_code_block *cur_cb, 	unsigned char *lut_ctx);

#endif
		enum	SyncType{
			SyncAll,
			OneTile,
			OneComponent,
			OneSubband,
			OneJob,
			SyncInvalid
		} ;

	private:

		static THREAD_ENTRY WINAPI Thread_Cuda(PVOID pParam);

		void 	AccessIdleStream(int &releaseindex, bool isGet = true, bool destroyMutex = false);

		bool	mInit;
		GpuAvailbility	mGpuAvailability;


		ThreadInfo	mCudaManagerThreadInfo;
// 		int		mThreadNum;
// 		int		mOutputPipeLength;
		EnginConfig * mEngineConfig;
// 		int		mJobNum;
		int		mNextJobIndex;

		GpuInfo	mGpuInfo;

		P_CudaBuf	 mGpuResultBuf;
		P_CudaBuf	 mHostResultBuf;
		P_CudaBuf	 mComponentResultBuf;
		P_CudaBuf	 mGpuResultBufThumb;
		P_CudaBuf	 mHostResultBufThumb;

		P_CudaBuf	 mStripeBuf;
		int			 mStripeBufElementSize;
		int			 mEachThreadBufferNum;
		int			 * mStripeBufNextIndex;
		int			 mStripeBufNum;
		// 	int			*mStripeBufAsyncId;

		P_CudaJob	 mCudajob;
		int			 mCurJobIndex;

		ThreadInfo	mCudaThreadInfo;

		JobQueue	mSyncJobQueue;
		JobQueue	mAsyncJobQueue;

		AsyncStream	mAsyncStream[DYNAMIC_ASYNC_ID_NUM];

		MUTEX_HANDLE		mSyncJobMutex;				// for accessing sync jobs
		MUTEX_HANDLE		mAsyncJobMutex;				// for sccessing async jobs
		SEMAPHORE_HANDLE	mCudaThreadSemaphore;		// for access cuda thread
		SEMAPHORE_HANDLE	mCudaBlockSemaphore;		// for cuda-block job
 		SEMAPHORE_HANDLE	* mCudaBufSemaphore;			// for access cuda buf
		MUTEX_HANDLE		* mCudaBufMutex;			// for access cuda buf
		MUTEX_HANDLE		mCudaJobCounterMutex;		// used in upload stripe buf to control the counter in job
		SEMAPHORE_HANDLE	mCudaThreadEndingSemaphore;


	};

}	// namespace BMI


