#include "cudaInterface.h"
#include "gpulib.h"
#include "debug.h"
#include "gpu_setting.h"

#define		_SAVE_BMP_FILE				0

#define		LAST_DWT_DECODE_BY_RESOLUTION	0

const	int	GPU_JOB_NUMBER		=	2;
const	int	MIN_STRIPE_BUF	=	15;
const	int	THREAD_STRIPE_BUF_NMBER	=	30;
const	int BMI_CUDA_ERROR		=	-1;
const	int	INVALID_STREAM		=	0;
const	int	MIN_ASYNC_JOBS	=	1500;
// const	int	THREAD_ASYNC_JOBS	=	20;
const	int	MIN_SYNC_JOBS	=	1000;
// const	int	THREAD_SYNC_JOBS	=	10;



using namespace BMI;

namespace	CudaManageFunc
{


	inline	void SafeFreeCudaBuf(CudaBuf	& cudaBuf)
	{
		if (cudaBuf.mStatus != BUF_STATUS_UNINITIALIZED && cudaBuf.mBuf)
		{
			FreeGpuBuf(&cudaBuf);
		}
		cudaBuf.mBuf = NULL;
		cudaBuf.mSize = 0;
		cudaBuf.mStatus = BUF_STATUS_UNINITIALIZED;
		cudaBuf.mAsyncId = 0;
	}

	inline	void SafeFreeCudaBuf(P_CudaBuf	cudaBuf)
	{
		SafeFreeCudaBuf(*cudaBuf);
	}

	inline	void InitGpuBuf(CudaBuf	& cudaBuf, CudaBufType type, unsigned int size, unsigned int width, unsigned int height)
	{
		bool reInit = false;
		if (cudaBuf.mStatus == BUF_STATUS_UNINITIALIZED || cudaBuf.mType != type)
		{
			reInit = true;
		}
		else if ( size == static_cast<unsigned int>(INVALID))
		{
			if (type != GPU_ALLIGN_BUF && type != HOST_ALLLIGN_BUF)
			{
				size = width * height;
				if (size > cudaBuf.mSize)
				{
					reInit = true;
				}
			}
			else if (width > cudaBuf.mPitch || height > cudaBuf.mHeight)	
				// type == XXX_ALLIGN_BUF;
			{
				reInit = true;
			}
		}
		else if (size > cudaBuf.mSize)
		{
			reInit = true;
		}

		cudaBuf.mWidth = width;
		cudaBuf.mHeight = height;
		cudaBuf.mType = type;

		if (reInit)
		{
			SafeFreeCudaBuf(cudaBuf);
			if ((int)size == INVALID)
			{
				BMI_ASSERT((int)width != INVALID && (int)height != INVALID);
				cudaBuf.mSize = width * height;
			}
			else
			{
				BMI_ASSERT((int)width == INVALID || (int)height == INVALID || (width * height) == size);
				cudaBuf.mSize = size;
				if ((int)width == INVALID || (int)height == INVALID)
				{
					cudaBuf.mWidth = size;
					cudaBuf.mHeight = 1;
				}

			}
			MallocGpuBuf(&cudaBuf);
// 			cudaBuf.mSize = cudaBuf.mPitch * cudaBuf.mHeight;
		}

		cudaBuf.mStatus = BUF_STATUS_IDLE;
	}

	inline	void InitGpuBuf(CudaBuf	* cudaBuf, CudaBufType type, unsigned int size, unsigned int width, unsigned int height)
	{
		InitGpuBuf(* cudaBuf,  type, size, width, height);
	}

}	// CudaManageFunc


//////////////////////////////////////////////////////////////////////////
// SubbandInfo
//////////////////////////////////////////////////////////////////////////
SubbandInfo_s::~SubbandInfo_s()
{}

//////////////////////////////////////////////////////////////////////////
// CompInfo
//////////////////////////////////////////////////////////////////////////
CompInfo_s::~CompInfo_s()
{
	if (mSubbandInfo != NULL)
	{
		SubbandInfo * tmp = reinterpret_cast<SubbandInfo *>(mSubbandInfo);
		SAFE_DELETE_ARRAY(tmp);

	}

}

//////////////////////////////////////////////////////////////////////////
//	TileInfo
//////////////////////////////////////////////////////////////////////////
TileInfo_s::~TileInfo_s()
{
	if (mCompInfo != NULL)
	{
		CompInfo * tmp = reinterpret_cast<CompInfo *>(mCompInfo);
		SAFE_DELETE_ARRAY(tmp);
	}	
}

///////////////////////////////////////////////////////////////////////////
//		PictureInfo
//////////////////////////////////////////////////////////////////////////
PictureInfo_s::~PictureInfo_s()
{
	if (mTileinfo)
	{
		SAFE_DELETE_ARRAY(mTileinfo);
		SAFE_DELETE_ARRAY(mTileinfoThumb);
	}
	if (mParent )
	{
		reinterpret_cast<P_CudaJob>(mParent)->mPicture = NULL;
		reinterpret_cast<P_CudaJob>(mParent)->mJobStatus = CUDA_JOB_IDLE;
	}
}

//////////////////////////////////////////////////////////////////////////
//		CudaJob_s
//////////////////////////////////////////////////////////////////////////
CudaJob_s::CudaJob_s() : 
mId(INVALID),
mJobStatus(CUDA_JOB_UNINITED),
mOutputIndex(INVALID),
mPicture(NULL)
{
// 	mGpuOrigBuf = new 
// 	mGpuOrigBuf->mStatus = BUF_STATUS_UNINITIALIZED;
// 	mGpuTempBuf->mStatus = BUF_STATUS_UNINITIALIZED;
}

CudaJob_s::~CudaJob_s()
{
	SAFE_DELETE(mPicture);
	if (mJobStatus != CUDA_JOB_UNINITED)
	{
		DecodeJob newJob;
		newJob.mJobType = JOB_CUDA_THREAD;
		CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
		newCudaJob->type = FREE_CUDA_BUF;
		newCudaJob->mIsBlockJob = false;

		for (int i = 0; i < MAX_COMPONENT; ++i)
		{

			newCudaJob->mDeleteBuf = mGpuOrigBuf[i];
			CudaManager::GetInstance()->ThreadSafeAccessCudaThreadJob(NULL, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

			newCudaJob->mDeleteBuf = mGpuOrigBufThumb[i];
			CudaManager::GetInstance()->ThreadSafeAccessCudaThreadJob(NULL, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

			newCudaJob->mDeleteBuf = mGpuTempBuf[i];
			if (i == MAX_COMPONENT - 1)
			{
				newCudaJob->mIsBlockJob = true;		// Set the last one as a block job
				newCudaJob->mReleaseMe = CudaManager::GetInstance()->mCudaBlockSemaphore;
			}		
			CudaManager::GetInstance()->ThreadSafeAccessCudaThreadJob(NULL, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

		}
		WAIT_SEMAPHORE(CudaManager::GetInstance()->mCudaBlockSemaphore);

		for (int i = 0; i < MAX_COMPONENT; ++i)
		{
			SAFE_DELETE(mGpuOrigBuf[i]);
			SAFE_DELETE(mGpuOrigBufThumb[i]);
			SAFE_DELETE(mGpuTempBuf[i])
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//		CudaManager
//////////////////////////////////////////////////////////////////////////
CudaManager::~CudaManager()
{
	if (mInit )
	{

		DecodeJob newJob;
		newJob.mJobType = JOB_CUDA_THREAD;
		CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
		newCudaJob->type = FREE_CUDA_BUF;
		newCudaJob->mIsBlockJob = false;

		newCudaJob->mDeleteBuf = mComponentResultBuf;
		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

		for (int i = 0; i < mEngineConfig->mOutputQueueLen; ++i)
		{
			newCudaJob->mDeleteBuf = &mGpuResultBuf[i];
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

			newCudaJob->mDeleteBuf = &mGpuResultBufThumb[i];
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

			newCudaJob->mDeleteBuf = &mHostResultBufThumb[i];
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

			if (i == mEngineConfig->mOutputQueueLen - 1)
			{
				newCudaJob->mIsBlockJob = true;		// Set the last one as a block job
				newCudaJob->mReleaseMe = mCudaBlockSemaphore;
			}		
			newCudaJob->mDeleteBuf = &mHostResultBuf[i];
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
		}
		WAIT_SEMAPHORE(mCudaBlockSemaphore);

		SAFE_DELETE(mComponentResultBuf);
		SAFE_DELETE_ARRAY(mGpuResultBuf);
		SAFE_DELETE_ARRAY(mGpuResultBufThumb);
		SAFE_DELETE_ARRAY(mHostResultBuf);
		SAFE_DELETE_ARRAY(mHostResultBufThumb);

		newCudaJob->mIsBlockJob = false;

		for (int i = 0; i < mStripeBufNum ; ++i)
		{

			if (mStripeBuf[i].mAsyncId >= 0)
			{
				newCudaJob->type = DESTORY_STREAM_ID;
				newCudaJob->mP_AsyncId = &mStripeBuf[i].mAsyncId;
				ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
			}

			if (i == mStripeBufNum - 1)
			{
				newCudaJob->mIsBlockJob = true;		// Set the last one as a block job
				newCudaJob->mReleaseMe = mCudaBlockSemaphore;
			}	
			newCudaJob->type = FREE_CUDA_BUF;
			newCudaJob->mDeleteBuf = &mStripeBuf[i];
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
		}

		WAIT_SEMAPHORE(mCudaBlockSemaphore);


		SAFE_DELETE_ARRAY(mStripeBuf);
		SAFE_DELETE_ARRAY(mStripeBufNextIndex);
		SAFE_DELETE_ARRAY(mCudajob);

		// remove GPU here
		GpuRemove(&(mGpuInfo));

		END_THREAD(mCudaThreadInfo.mHandle);

		for (int i = 0; i < mEngineConfig->mThreadNum; ++i)
		{
			DESTROY_SEMAPHORE(mCudaBufSemaphore[i]);
			DESTROY_MUTEX(mCudaBufMutex[i]);
		}
		SAFE_DELETE_ARRAY(mCudaBufSemaphore);
		SAFE_DELETE_ARRAY(mCudaBufMutex);

		DESTROY_SEMAPHORE(mCudaThreadSemaphore);		// for access cuda thread
		DESTROY_SEMAPHORE(mCudaBlockSemaphore);		// for cuda-block job
		DESTROY_SEMAPHORE(mCudaThreadEndingSemaphore);
		DESTROY_MUTEX(mSyncJobMutex);				// for accessing sync jobs
		DESTROY_MUTEX(mAsyncJobMutex);				// for sccessing async jobs
		DESTROY_MUTEX(mCudaJobCounterMutex);

		int useless;
		AccessIdleStream(useless, 0, true);	// destroy static mutex

	}


}

int		CudaManager::Init(EnginConfig & engineConfig)
{

	int gpu_status = mGpuAvailability == GPU_AVAILABLE ? 0 : INVALID;

	if (!mInit && mGpuAvailability != GPU_UNAVILABLE)
	{

#if _MAC_OS || _LINUX
		mCudaManagerThreadInfo.mHandle = pthread_self();
		mCudaManagerThreadInfo.mThreadId = 0;
#else
		mCudaManagerThreadInfo.mHandle = GetCurrentThread();
		mCudaManagerThreadInfo.mThreadId = GetCurrentThreadId();
#endif
		mAsyncJobQueue.init(MIN_ASYNC_JOBS + engineConfig.mThreadNum * THREAD_STRIPE_BUF_NMBER, sizeof(DecodeJob));
		mSyncJobQueue.init(MIN_SYNC_JOBS + engineConfig.mThreadNum * THREAD_STRIPE_BUF_NMBER, sizeof(DecodeJob));

		mCudaBufSemaphore = new SEMAPHORE_HANDLE[engineConfig.mThreadNum];
		mCudaBufMutex = new MUTEX_HANDLE[engineConfig.mThreadNum];
#if _MAC_OS || _LINUX
		for (int i = 0; i < engineConfig.mThreadNum; ++i)
		{
			MUTEX_INIT(mCudaBufMutex[i], NULL);
		}
#endif

		CREATE_MUTEX(mAsyncJobMutex);
		CREATE_MUTEX(mSyncJobMutex);
		CREATE_MUTEX(mCudaJobCounterMutex);

		CREATE_SEMAPHORE(mCudaThreadSemaphore, 0, 1000);
		CREATE_SEMAPHORE(mCudaBlockSemaphore, 0, 10);
		CREATE_SEMAPHORE(mCudaThreadEndingSemaphore,0,1);

		int err;
		DecodeJob newJob;
		newJob.mJobType = JOB_CUDA_THREAD;
		CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
		newCudaJob->type = INIT_CUDA;
		newCudaJob->mCudaInit.mErrorCode = &err;
		newCudaJob->mCudaInit.mGpuInfo = &(mGpuInfo);
		newCudaJob->mIsBlockJob = true;
		newCudaJob->mReleaseMe = mCudaBlockSemaphore;
		newCudaJob->mCallingThreadInfo = &mCudaManagerThreadInfo;

		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo,ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
#if _MAC_OS || _LINUX
			int ret = 
				pthread_create(&mCudaThreadInfo.mHandle ,NULL,Thread_Cuda,&mCudaThreadInfo);
			BMI_ASSERT(ret == 0);
#else
			mCudaThreadInfo.mHandle = CreateThread(
				NULL,
				0,
				Thread_Cuda,
				&mCudaThreadInfo,			// thread handler
				0,
				&mCudaThreadInfo.mThreadId);
#endif


		WAIT_SEMAPHORE(mCudaBlockSemaphore);

		if (err == BMI_CUDA_ERROR )
		{
#if _MAC_OS || _LINUX
			newCudaJob->type = QUIT_CUDA;
			newCudaJob->mIsBlockJob = true;
			newCudaJob->mReleaseMe = mCudaBlockSemaphore;
			newCudaJob->mCallingThreadInfo = &mCudaManagerThreadInfo;

			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo,ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
			WAIT_SEMAPHORE(mCudaBlockSemaphore);
#else
			END_THREAD(mCudaThreadInfo.mHandle);
#endif
			mGpuAvailability = GPU_UNAVILABLE;
			DESTROY_SEMAPHORE(mCudaThreadSemaphore);
			DESTROY_SEMAPHORE(mCudaBlockSemaphore);
			DESTROY_SEMAPHORE(mCudaThreadEndingSemaphore);
			DESTROY_MUTEX(mSyncJobMutex);				// for accessing sync jobs
			DESTROY_MUTEX(mAsyncJobMutex);				// for sccessing async jobs
			DESTROY_MUTEX(mCudaJobCounterMutex);

			gpu_status = INVALID;
		}
		else
		{
			newCudaJob->mIsBlockJob = false;
			newCudaJob->type =	CREATE_ASYNC_ID;
			for (int i = 0; i < DYNAMIC_ASYNC_ID_NUM; ++i)
			{
				newCudaJob->mP_AsyncId = & mAsyncStream[i].mAsyncId;

				if (i == DYNAMIC_ASYNC_ID_NUM - 1)	// set last one as block
				{
					newCudaJob->mIsBlockJob = true;
					newCudaJob->mReleaseMe = mCudaBlockSemaphore;
				}

				ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo,ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
				mAsyncStream[i].mIsIdle = true;
			}
			WAIT_SEMAPHORE(mCudaBlockSemaphore);



			mEngineConfig = &engineConfig;

// 			mJobNum = GPU_JOB_NUMBER;
			mNextJobIndex = 0;

			mGpuResultBuf = new CudaBuf[mEngineConfig->mOutputQueueLen];
			mGpuResultBufThumb = new CudaBuf[mEngineConfig->mOutputQueueLen];
			mHostResultBuf = new CudaBuf[mEngineConfig->mOutputQueueLen];
			mHostResultBufThumb = new CudaBuf[mEngineConfig->mOutputQueueLen];
			for (int i = 0; i < mEngineConfig->mOutputQueueLen; ++i)
			{
				mGpuResultBuf[i].mStatus = BUF_STATUS_UNINITIALIZED;
				mGpuResultBufThumb[i].mStatus = BUF_STATUS_UNINITIALIZED;
				mHostResultBuf[i].mStatus = BUF_STATUS_UNINITIALIZED;
				mHostResultBufThumb[i].mStatus = BUF_STATUS_UNINITIALIZED;
			}
			mComponentResultBuf = new CudaBuf;
			mComponentResultBuf->mStatus = BUF_STATUS_UNINITIALIZED;
			mComponentResultBuf->mBuf = NULL;

			mStripeBufNextIndex = 0;
			// 		mStripeBufAsyncId  = new int[mThreadNum * THREAD_STRIPE_BUF_NMBER];
			mStripeBufNum = mEngineConfig->mThreadNum * THREAD_STRIPE_BUF_NMBER; 
			mStripeBufNum = mStripeBufNum < MIN_STRIPE_BUF ? MIN_STRIPE_BUF : mStripeBufNum;
			mEachThreadBufferNum =  THREAD_STRIPE_BUF_NMBER;
			mStripeBuf = new CudaBuf[mStripeBufNum];
			mStripeBufNextIndex = new int[mEngineConfig->mThreadNum];


			for (int i = 0; i < mEngineConfig->mThreadNum; ++i)
			{
				mStripeBufNextIndex[i] = 0;
				CREATE_SEMAPHORE(mCudaBufSemaphore[i], THREAD_STRIPE_BUF_NMBER, THREAD_STRIPE_BUF_NMBER<<1);
				CREATE_MUTEX(mCudaBufMutex[i]);
				for (int j = 0; j < THREAD_STRIPE_BUF_NMBER; ++j)
				{
					mStripeBuf[i*THREAD_STRIPE_BUF_NMBER + j].mStatus = BUF_STATUS_UNINITIALIZED;
					mStripeBuf[i*THREAD_STRIPE_BUF_NMBER + j].mBuf = NULL;
					mStripeBuf[i*THREAD_STRIPE_BUF_NMBER + j].mOwnerThreadIndex = i;
// 					mStripeBuf[i*THREAD_STRIPE_BUF_NMBER + j].mAsyncId = INVALID;
				}
			}
			mStripeBufElementSize = 0;

			mCudajob = new CudaJob[mEngineConfig->mOutputQueueLen];

			for (int i = 0; i < mEngineConfig->mOutputQueueLen; ++i)
			{
				mCudajob[i].mId = i;
			}

			mCurJobIndex = 0;

			mInit = true;
			mGpuAvailability = GPU_AVAILABLE;
			gpu_status = 0;
		}

	}
	return gpu_status;
}


void	CudaManager::InitPicture(J2K_InputStream &inStream, int outputIndex, const stru_opt * s_opt, int opt_size )
{
	BMI_ASSERT(mInit);

	int jobIndex = mNextJobIndex;

	// find next available job slot
	while (mCudajob[jobIndex].mJobStatus != CUDA_JOB_UNINITED && mCudajob[jobIndex].mJobStatus != CUDA_JOB_IDLE )
	{
		jobIndex++;
		if (jobIndex == mEngineConfig->mOutputQueueLen)
		{
			jobIndex = 0;
		}	
		if (jobIndex == mNextJobIndex)
		{
			jobIndex = -1;
			break;
		}
	}
	if (jobIndex == -1)
	{
		jobIndex = mNextJobIndex;	
		BMI_ASSERT(0);// todo : have to wait until this job finished, sync....
	}

	P_CudaJob curJob = &(mCudajob[jobIndex]);

	if (curJob->mJobStatus == CUDA_JOB_UNINITED)
	{
		// init member
		for (int i = 0; i < MAX_COMPONENT; ++i)
		{
			curJob->mGpuOrigBuf[i] = new CudaBuf;
			curJob->mGpuOrigBuf[i]->mStatus = BUF_STATUS_UNINITIALIZED;
			curJob->mGpuOrigBuf[i]->mBuf = NULL;
			curJob->mGpuOrigBufThumb[i] = new CudaBuf;
			curJob->mGpuOrigBufThumb[i]->mStatus = BUF_STATUS_UNINITIALIZED;
			curJob->mGpuOrigBufThumb[i]->mBuf = NULL;
			curJob->mGpuTempBuf[i] = new CudaBuf;
			curJob->mGpuTempBuf[i]->mStatus = BUF_STATUS_UNINITIALIZED;
			curJob->mGpuTempBuf[i]->mBuf = NULL;
		}
		curJob->mPicture = new PictureInfo;
		curJob->mJobStatus = CUDA_JOB_IDLE;
	}

	curJob->mSizeShift = (inStream.mConfig.mWordLength == USHORT_16_BITS) ? 1 : 2;

	DecodeJob newJob;
	newJob.mJobType = JOB_CUDA_THREAD;
	CudaThreadJob * newCudaThreadJob;

 	newCudaThreadJob = reinterpret_cast<CudaThreadJob * >(newJob.mJobPara);
	newCudaThreadJob->mIsBlockJob = false;
	newCudaThreadJob->mCallingThreadInfo =  &mCudaManagerThreadInfo;
	newCudaThreadJob->type = INIT_GPU_BUF;

	int LastCompId = 0;
	// Init gou buf ***UUU The thumbnil buffers are inited only after the subband sizes are calculated
	for (int i = 0, cal = 0; i < inStream.mProperties.mComponentNum; ++i)
	{
		if ((inStream.mConfig.mIgnoreComponents >> i) & 0x01)
		{
			continue; // current components ignored
		}
		++cal;
		LastCompId = i;

		newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	curJob->mGpuOrigBuf[i];
		newCudaThreadJob->mCudaInitBuf.mType = GPU_ALLIGN_BUF;
		newCudaThreadJob->mCudaInitBuf.mSize = INVALID;
		newCudaThreadJob->mCudaInitBuf.mWidth = inStream.mProperties.mSize.x<<curJob->mSizeShift;
		newCudaThreadJob->mCudaInitBuf.mHeight = inStream.mProperties.mSize.y;

		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );

		newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	curJob->mGpuTempBuf[i];
		newCudaThreadJob->mCudaInitBuf.mType = GPU_ALLIGN_BUF;
		newCudaThreadJob->mCudaInitBuf.mSize = INVALID;
		newCudaThreadJob->mCudaInitBuf.mWidth = inStream.mProperties.mSize.y<<curJob->mSizeShift;
		newCudaThreadJob->mCudaInitBuf.mHeight = inStream.mProperties.mSize.x;

		if (cal == inStream.mProperties.mCalculatedComponentNum)
		{
			newCudaThreadJob->mIsBlockJob = true;
			newCudaThreadJob->mReleaseMe = mCudaBlockSemaphore;
		}		
		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );
	}

	WAIT_SEMAPHORE(mCudaBlockSemaphore);

	P_PictureInfo cur_picture = curJob->mPicture;
	inStream.mCudaPictureInfo = reinterpret_cast<void * >(cur_picture);

	int tileNum = inStream.mProperties.mTileNum.x * inStream.mProperties.mTileNum.y;
	if (cur_picture->mProperties.mTileNum.x * cur_picture->mProperties.mTileNum.y != inStream.mProperties.mTileNum.x  * inStream.mProperties.mTileNum.y)
	{
		SAFE_DELETE_ARRAY(cur_picture->mTileinfo);
		SAFE_DELETE_ARRAY(cur_picture->mTileinfoThumb);
		cur_picture->mProperties.mComponentNum = 0;	// all components deleted
		cur_picture->mProperties.mCalculatedComponentNum = 0;	// all components deleted
		cur_picture->mTileinfo = new TileInfo[tileNum];
		cur_picture->mTileinfoThumb = new TileInfo[tileNum]; //***UUU However, set the size and off params later when subband[0] sizes are known

//		const unsigned char * opt_memory = reinterpret_cast<const unsigned char *>(s_opt);
		for (int i = 0; i < tileNum; ++i)
		{
//			const stru_opt * tile_opt =  reinterpret_cast<const stru_opt *>(opt_memory);

			cur_picture->mTileinfo[i].mOff.x = (i % inStream.mProperties.mTileNum.x) * inStream.mProperties.mTileSize.x;
			cur_picture->mTileinfo[i].mOff.y = (i / inStream.mProperties.mTileNum.x) * inStream.mProperties.mTileSize.y;
			cur_picture->mTileinfo[i].mSize.x = (cur_picture->mTileinfo[i].mOff.x + inStream.mProperties.mTileSize.x > inStream.mProperties.mSize.x) 
												? (inStream.mProperties.mSize.x - cur_picture->mTileinfo[i].mOff.x)
												: inStream.mProperties.mTileSize.x;
			cur_picture->mTileinfo[i].mSize.y = (cur_picture->mTileinfo[i].mOff.y + inStream.mProperties.mTileSize.y > inStream.mProperties.mSize.y) 
												? (inStream.mProperties.mSize.y - cur_picture->mTileinfo[i].mOff.y)
												: inStream.mProperties.mTileSize.y;

			for (int d = inStream.mProperties.mDwtLevel - 1, xOff = cur_picture->mTileinfo[i].mOff.x, yOff = cur_picture->mTileinfo[i].mOff.y;
				d >=0; --d)
			{

				cur_picture->mTileinfo[i].mXParity[d] = xOff & 1;
				cur_picture->mTileinfo[i].mYParity[d] = yOff & 1;
				xOff =  BMI_CEILDIV2(xOff);
				yOff =  BMI_CEILDIV2(yOff);
			}
		}
	}

//	if (cur_picture->mProperties.mComponentNum != inStream.mProperties.mComponentNum || 
//		cur_picture->mProperties.mCalculatedComponentNum != inStream.mProperties.mCalculatedComponentNum ||
//		(cur_picture->mProperties.mCalculatedComponentNum == inStream.mProperties.mCalculatedComponentNum && inStream.mConfig.mIgnoreComponents))
	{
		for (int i = 0; i < tileNum; ++i)
		{
			P_TileInfo curTile = &cur_picture->mTileinfo[i];
			P_TileInfo curTileThumb = &cur_picture->mTileinfoThumb[i];
			CompInfo * tmp = (CompInfo *)curTile->mCompInfo;
			SAFE_DELETE_ARRAY(tmp);
			tmp = (CompInfo *)curTileThumb->mCompInfo;
			SAFE_DELETE_ARRAY(tmp);
			cur_picture->mProperties.mDwtLevel = 0;	//	all subband deleted
			curTile->mCompInfo = new CompInfo[inStream.mProperties.mComponentNum];
			curTileThumb->mCompInfo = new CompInfo[inStream.mProperties.mComponentNum];

			for (int j = 0; j < inStream.mProperties.mComponentNum; ++j)
			{
				curTileThumb->mCompInfo[j].mCompBuf = curJob->mGpuOrigBufThumb[j];
			}
		}
	}

	if (cur_picture->mProperties.mDwtLevel != inStream.mProperties.mDwtLevel)
	{
		const unsigned char * opt_memory = reinterpret_cast<const unsigned char *>(s_opt);
		int tileThumbOffX = 0, tileThumbOffY = 0;
		for (int i = 0; i < tileNum; ++i)
		{
			P_TileInfo curTileThumb = &cur_picture->mTileinfoThumb[i];
			const stru_opt * tile_opt =  reinterpret_cast<const stru_opt *>(opt_memory);
			for (int j = 0; j < inStream.mProperties.mComponentNum; ++j)
			{
				if (((inStream.mConfig.mIgnoreComponents >> j) & 0x01) && j != 0)
					// i == 0 : some where we use the infomation of comp0 in the codes, so we nned to set the compInfo 0
				{
					continue; // current component ignored
				}

				const stru_tile_component * comp = &tile_opt->component[j];
				P_CompInfo curComp = reinterpret_cast<P_CompInfo>(&cur_picture->mTileinfo[i].mCompInfo[j]);
				P_CompInfo curCompThumb = reinterpret_cast<P_CompInfo>(&cur_picture->mTileinfoThumb[i].mCompInfo[j]);
				curComp->mNumOfSubband = inStream.mProperties.mDwtLevel * 3 + 1;
				curCompThumb->mNumOfSubband = 1; //***UUU only the lowest level
				SubbandInfo * subbPtr = (SubbandInfo *)curComp->mSubbandInfo;
				SAFE_DELETE_ARRAY(subbPtr);
				curComp->mSubbandInfo = new  SubbandInfo[curComp->mNumOfSubband];
				subbPtr = (SubbandInfo *)curCompThumb->mSubbandInfo;
				SAFE_DELETE_ARRAY(subbPtr);
				curCompThumb->mSubbandInfo = new  SubbandInfo[1];
				const unsigned int * qqsteps = tile_opt->qcc_quant_step[j];
					//&qcc_qsteps[j * (1 + 3 * MAX_DWT_LEVEL)];

// 				int resWidth = cur_picture->mTileinfo[i].mSize.x;
// 				int resHeight = cur_picture->mTileinfo[i].mSize.y;
				for (int k = 0; k <= inStream.mProperties.mDwtLevel - 1; ++k)
				{
					for (int s = (k == 0 ? 0 : 1); s < 4; ++s)
					{
						int id = k*3 + s;
						stru_subband * band = &comp->band[id];
						curComp->mSubbandInfo[id].mId = id;
						stru_code_block * cb = band->codeblock;
						curComp->mSubbandInfo[id].mSize.x = cb->width;
						curComp->mSubbandInfo[id].mSize.y = cb->height;
						for (int bw = 1; bw < band->ncb_bw; ++bw)
						{	
							curComp->mSubbandInfo[id].mSize.x += cb[bw].width;
						}
						for (int bh = 1; bh < band->ncb_bh; ++bh)
						{
							curComp->mSubbandInfo[id].mSize.y += cb[band->ncb_bw*bh].height;
						}

						curComp->mSubbandInfo[id].mOff.x = 
						curComp->mSubbandInfo[id].mOff.y = 0;

						if (s == 3)	// 1 & 3
						{
							curComp->mSubbandInfo[id].mOff.x = 
							curComp->mSubbandInfo[id-2].mOff.x = curComp->mSubbandInfo[id - 1].mSize.x;

							curComp->mSubbandInfo[id].mOff.y = 
							curComp->mSubbandInfo[id-1].mOff.y = curComp->mSubbandInfo[id - 2].mSize.y;
						}

// 						curComp->mSubbandInfo[id].mOff.x = (s & 0x01) ? (resWidth + 1)>>1 : 0;
// 						curComp->mSubbandInfo[id].mOff.y = (s >1 ) ? (resHeight + 1)>>1 : 0;
// 						
// 						curComp->mSubbandInfo[id].mSize.x = (s & 0x01) ? (resWidth)>>1 : (resWidth + 1)>>1;
// 						curComp->mSubbandInfo[id].mSize.y = (s > 1) ? (resHeight)>>1 : (resHeight + 1)>>1;
// 						DEBUG_PAUSE(curComp->mSubbandInfo[id].mSize.x != band->width || curComp->mSubbandInfo[id].mSize.y != band->height);
// 

						if (inStream.mProperties.mMethod == Ckernels_W9X7)
						{

							int absStepSize = ((qqsteps[id] & 0x7ff)<< 2) | 0x2000;	// 0x2000 = 0x01 << 13
							int n = inStream.mProperties.mBitDepth  - (qqsteps[id] >> 11);
							absStepSize = (n > 0) ? (absStepSize << n) : (absStepSize >> (-n));

							curComp->mSubbandInfo[id].mfAbsStepNorm = static_cast<float>(absStepSize) * IDWT_NORM;

// 							curComp->mSubbandInfo[id].miAbsStep = absStepSize;
// 							curComp->mSubbandInfo[id].mQquanShift = n + 19;		// 19 = 32 - 13; integer part for fix

						}
					}
// 					resWidth = (resWidth + 1)>>1;
// 					resHeight = (resHeight + 1)>>1;
				}
				curCompThumb->mSubbandInfo[0].mId = 0;
				curCompThumb->mSubbandInfo[0].mSize.x = curComp->mSubbandInfo[0].mSize.x;
				curCompThumb->mSubbandInfo[0].mSize.y = curComp->mSubbandInfo[0].mSize.y;
				curCompThumb->mSubbandInfo[0].mOff.x = curCompThumb->mSubbandInfo[0].mOff.y = 0;						
			}

			curTileThumb->mOff.x = tileThumbOffX;
			curTileThumb->mOff.y = tileThumbOffY;

			curTileThumb->mSize.x = curTileThumb->mCompInfo[0].mSubbandInfo[0].mSize.x;
			curTileThumb->mSize.y = curTileThumb->mCompInfo[0].mSubbandInfo[0].mSize.y;

			tileThumbOffX += curTileThumb->mSize.x;
			if( (i % inStream.mProperties.mTileNum.x) == inStream.mProperties.mTileNum.x - 1)
			{
				tileThumbOffX = 0;
				tileThumbOffY += curTileThumb->mSize.y;
			}
			
			opt_memory += opt_size;	// next tile opt memory space
		}
	}	

	int thumbnailWidth = cur_picture->mTileinfoThumb[tileNum - 1].mOff.x + cur_picture->mTileinfoThumb[tileNum - 1].mSize.x;
	int thumbnailHeight = cur_picture->mTileinfoThumb[tileNum - 1].mOff.y + cur_picture->mTileinfoThumb[tileNum - 1].mSize.y;

	inStream.mProperties.mThumbOutputSize.x = thumbnailWidth;
	inStream.mProperties.mThumbOutputSize.y = thumbnailHeight;


	newJob.mJobType = JOB_CUDA_THREAD; //Currently this is not needed since set above, but need to be future safe

 	newCudaThreadJob = reinterpret_cast<CudaThreadJob * >(newJob.mJobPara);
	newCudaThreadJob->mIsBlockJob = false;
	newCudaThreadJob->mCallingThreadInfo =  &mCudaManagerThreadInfo;
	newCudaThreadJob->type = INIT_GPU_BUF;

	//***UUU init the thumbnil buffers 
	for (int i = 0, cal = 0; i < inStream.mProperties.mComponentNum; ++i)
	{
		if ((inStream.mConfig.mIgnoreComponents >> i) & 0x01)
		{
			continue;
		}
		++cal;
		newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	curJob->mGpuOrigBufThumb[i];
		newCudaThreadJob->mCudaInitBuf.mType = GPU_ALLIGN_BUF;
		newCudaThreadJob->mCudaInitBuf.mSize = INVALID;
		newCudaThreadJob->mCudaInitBuf.mWidth = thumbnailWidth<<curJob->mSizeShift;
		newCudaThreadJob->mCudaInitBuf.mHeight = thumbnailHeight;

		if (cal == inStream.mProperties.mCalculatedComponentNum)
		{
			newCudaThreadJob->mIsBlockJob = true;
			newCudaThreadJob->mReleaseMe = mCudaBlockSemaphore;
		}		
		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );
	}
	WAIT_SEMAPHORE(mCudaBlockSemaphore);

	cur_picture->mLastTileId = tileNum - 1;
	for (int i = 0; i < tileNum; ++i) // Reset mDeShifted 
	{
		P_TileInfo curTile = &cur_picture->mTileinfo[i];
		curTile->mLastCompId = LastCompId;

		P_TileInfo curTileThumb = &cur_picture->mTileinfoThumb[i];
		for (int j = 0; j < inStream.mProperties.mComponentNum; ++j)
		{
			curTile->mCompInfo[j].mDeShifted = 0;
			curTile->mCompInfo[j].mCompBufMoved = 0;
			curTileThumb->mCompInfo[j].mDeShifted = 0;
		}
	}	


	cur_picture->mProperties = inStream.mProperties;
	cur_picture->mTopDwtLevel = cur_picture->mProperties.mDwtLevel;
	if (inStream.mConfig.mDwtDecodeTopLevel != INVALID)
	{
		cur_picture->mTopDwtLevel = inStream.mConfig.mDwtDecodeTopLevel;
	}
	cur_picture->mProperties.mOutputFormat = inStream.mConfig.mOutputFormat;
	int pixelSize = sizeof(int_32);

	switch (cur_picture->mProperties.mOutputFormat)
	{
	case	IMAGE_FOTMAT_32BITS_ARGB:
	case	IMAGE_FOTMAT_32BITS_4COMPONENTS:
		pixelSize = sizeof(ARGB_32BITS);
		break;

	case	IMAGE_FOTMAT_48BITS_RGB:
	case	IMAGE_FOTMAT_48BITS_ARGB:
	case	IMAGE_FOTMAT_48BITS_3COMPONENTS:
		pixelSize = sizeof(RGB_48BITS);
		break;

	case	IMAGE_FOTMAT_24BITS_RGB:
	case	IMAGE_FOTMAT_24BITS_3COMPONENTS:
		pixelSize = sizeof(RGB_24BITS);
		break;

	case	IMAGE_FOTMAT_64BITS_ARGB:
	case	IMAGE_FOTMAT_64BITS_4COMPONENTS:
		pixelSize = sizeof(ARGB_64BITS);
		break;

	case	IMAGE_FOTMAT_NONE:
		pixelSize = 1;	
		break;

	default:
		BMI_ASSERT(0);
		break;
	}


	newCudaThreadJob->mIsBlockJob = false;
	newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	&mGpuResultBuf[outputIndex];
	newCudaThreadJob->mCudaInitBuf.mType = GPU_ALLIGN_BUF;
	newCudaThreadJob->mCudaInitBuf.mSize = INVALID;
	newCudaThreadJob->mCudaInitBuf.mHeight = inStream.mProperties.mSize.y;
	newCudaThreadJob->mCudaInitBuf.mWidth = inStream.mProperties.mSize.x * pixelSize;

	ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );

	newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	&mHostResultBuf[outputIndex];
	newCudaThreadJob->mCudaInitBuf.mType = HOST_BUF;

	ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );
	
	newCudaThreadJob->mCudaInitBuf.mCudaBuf = &mGpuResultBufThumb[outputIndex];
	newCudaThreadJob->mCudaInitBuf.mType = GPU_ALLIGN_BUF; //To be move proof, otherwise already set above
	newCudaThreadJob->mCudaInitBuf.mSize = INVALID;
	newCudaThreadJob->mCudaInitBuf.mWidth = thumbnailWidth  * pixelSize;
	newCudaThreadJob->mCudaInitBuf.mHeight = thumbnailHeight;

	ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );

	newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	&mHostResultBufThumb[outputIndex];
	newCudaThreadJob->mCudaInitBuf.mType = HOST_BUF;
	newCudaThreadJob->mIsBlockJob = true;		// need to wait cuda thread return
	newCudaThreadJob->mReleaseMe = mCudaBlockSemaphore;	

	ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );	

	WAIT_SEMAPHORE(mCudaBlockSemaphore);

	if (mStripeBufElementSize < (((inStream.mProperties.mTileSize.x+1)>>1)<<curJob->mSizeShift) * inStream.mProperties.mCodeBlockSize.y)
	{
		newCudaThreadJob->mIsBlockJob = false;
		for (int i = 0; i < mStripeBufNum; ++i)
		{
			newCudaThreadJob->type = INIT_GPU_BUF;
			newCudaThreadJob->mCudaInitBuf.mCudaBuf = 	&mStripeBuf[i];
			newCudaThreadJob->mCudaInitBuf.mType = HOST_BUF;
			newCudaThreadJob->mCudaInitBuf.mWidth = ((inStream.mProperties.mTileSize.x+1)>>1)<<curJob->mSizeShift;
			newCudaThreadJob->mCudaInitBuf.mHeight = inStream.mProperties.mCodeBlockSize.y;

			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );

			newCudaThreadJob->type = CREATE_ASYNC_ID;
			newCudaThreadJob->mP_AsyncId = &mStripeBuf[i].mAsyncId;

			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );

		}
		mStripeBufElementSize = (((inStream.mProperties.mTileSize.x+1)>>1)<<curJob->mSizeShift) * inStream.mProperties.mCodeBlockSize.y;

		newCudaThreadJob->type = RELEASE_SEMAPHORE;
		newCudaThreadJob->mIsBlockJob = true;
		newCudaThreadJob->mReleaseMe = mCudaBlockSemaphore;
		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
		WAIT_SEMAPHORE(mCudaBlockSemaphore);
	}

	curJob->mJobStatus = CUDA_JOB_WORKING;
	curJob->mOutputIndex = outputIndex;
	//

	mNextJobIndex = ++jobIndex;
	if (mNextJobIndex == mEngineConfig->mOutputQueueLen)
	{
		mNextJobIndex = 0;
	}
}

void	CudaManager::SyncUnsyncBuffer(ThreadInfo * callingThread /*= NULL*/, unsigned int spcValue/* = 0*/, unsigned int spcMask /*= 0*/)
{
// 					DecodeJob newJob;
// 					newJob.mJobType = JOB_CUDA_THREAD;
// 					CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
// 					newCudaJob->mIsBlockJob = false;
// 					newCudaJob->mCallingThreadInfo = callingThread;
// 					newCudaJob->type = SYNC_ALL;
// 
// 					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);


	if (callingThread == NULL)	// sync all buf
	{
		spcValue &= spcMask;
		for (int index = 0; index < mStripeBufNum; ++index)
		{
			int threadIndex = index / mEachThreadBufferNum;

			if ((mStripeBuf[index].mBufContain.mContainInfo & spcMask) == spcValue )
			{
				LOCK_MUTEX(mCudaBufMutex[threadIndex]);
				if (mStripeBuf[index].mStatus == BUF_STATUS_UPLOADING)
				{
					mStripeBuf[index].mStatus = BUF_STATUS_SYNCING;
					RELEASE_MUTEX(mCudaBufMutex[threadIndex]);

					DecodeJob newJob;
					newJob.mJobType = JOB_CUDA_THREAD;
					CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
					newCudaJob->mIsBlockJob = false;
					newCudaJob->mCallingThreadInfo = callingThread;
					newCudaJob->type = CUDA_BUF_SYNC;
					newCudaJob->mCudaBufSync = &mStripeBuf[index];

					DEBUG_PRINT((">-----> SyncUnsyncBuffer:Sync buf with id %d \n", mStripeBuf[index].mAsyncId ));
					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
				}
				else
				{
					RELEASE_MUTEX(mCudaBufMutex[threadIndex]);
				}
			}
		}
	}
	else
	{
		int threadIndex = callingThread->mThreadIndex;
		for (int index = threadIndex * mEachThreadBufferNum; index < (threadIndex + 1) * mEachThreadBufferNum; ++index)
		{
			if (spcValue && (mStripeBuf[index].mBufContain.mContainInfo & spcMask) == spcValue)
			{
				LOCK_MUTEX(mCudaBufMutex[threadIndex]);
				if (mStripeBuf[index].mStatus == BUF_STATUS_UPLOADING)
				{
					mStripeBuf[index].mStatus = BUF_STATUS_SYNCING;
					RELEASE_MUTEX(mCudaBufMutex[threadIndex]);

					DecodeJob newJob;
					newJob.mJobType = JOB_CUDA_THREAD;
					CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
					newCudaJob->mIsBlockJob = false;
					newCudaJob->mCallingThreadInfo = callingThread;
					newCudaJob->type = CUDA_BUF_SYNC;
					newCudaJob->mCudaBufSync = &mStripeBuf[index];

					DEBUG_PRINT((">-----> SyncUnsyncBuffer:Sync buf with id %d \n", mStripeBuf[index].mAsyncId ));
					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
				}
				else
				{
					RELEASE_MUTEX(mCudaBufMutex[threadIndex]);
				}
			}
		}
	}
}

CudaBuf	* CudaManager::GetStripeBuffer(PictureIndex & pictureIndex, ThreadInfo * callingThread)
{

	
	CudaBuf * ret = NULL;
	int threadIndex = callingThread->mThreadIndex;

	BMI_ASSERT(mInit);
	{
		WAIT_SEMAPHORE(mCudaBufSemaphore[threadIndex]);
		int index = mStripeBufNextIndex[threadIndex] + threadIndex * mEachThreadBufferNum;

		LOCK_MUTEX(mCudaBufMutex[threadIndex]);
		while(mStripeBuf[index].mStatus != BUF_STATUS_IDLE)
		{
			++index;
			if (++mStripeBufNextIndex[threadIndex] == mEachThreadBufferNum)
			{
				mStripeBufNextIndex[threadIndex] = 0;
				index = threadIndex * mEachThreadBufferNum;
			}
		}
		mStripeBuf[index].mStatus = BUF_STATUS_DECODING;
		RELEASE_MUTEX(mCudaBufMutex[threadIndex]);

		//check_back
		int syncBufIndex =  index - mEachThreadBufferNum / 2;
		if (syncBufIndex < threadIndex * mEachThreadBufferNum)
		{
			syncBufIndex += mEachThreadBufferNum;
		}

		LOCK_MUTEX(mCudaBufMutex[threadIndex]);
		if (mStripeBuf[syncBufIndex].mStatus == BUF_STATUS_UPLOADING)
		{
			mStripeBuf[syncBufIndex].mStatus = BUF_STATUS_SYNCING;
			RELEASE_MUTEX(mCudaBufMutex[threadIndex]);

			DecodeJob newJob;
			newJob.mJobType = JOB_CUDA_THREAD;
			newJob.mPictureIndex = pictureIndex;
			CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
			newCudaJob->mIsBlockJob = false;
			newCudaJob->mCallingThreadInfo = callingThread;
			newCudaJob->type = CUDA_BUF_SYNC;
			newCudaJob->mCudaBufSync = &mStripeBuf[syncBufIndex];

			DEBUG_PRINT((">-----> GetStripeBuffer:Sync buf with id %d \n", mStripeBuf[syncBufIndex].mAsyncId ));
			ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

		}
		else
		{
			RELEASE_MUTEX(mCudaBufMutex[threadIndex]);
		}
#if CHECK_BUSY_BUF
		if (++step >= 10)
		{
			PRINT(("All buf busy in thread %d, try %d steps to find a free buffer\n", GetCurrentThreadId(), step)); // please consider increase the stripe buf number
		}
#endif
		ret = &mStripeBuf[index];
	}
	return ret;
}

#if GPU_T1_TESTING
void	CudaManager::DecodeCodeBlock(bool bypass ,int_32 * context_words, uint_8 * pass_buf,	short numBps, stru_code_block *cur_cb, 	unsigned char *lut_ctx)
{
// upload necessary data and insert cuda job GPU_DECODE_CODEBLOCK
// call BMI_gpu_decode_codeblock
	return;
}

#endif

void	CudaManager::UploadStripeBuf(CudaBuf * buf, const DecodeJobConfig & config, PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int linesAvailable, int yOffsetInBand, ThreadInfo * callingThread)
{
	BMI_ASSERT (mInit);
	CudaJob * curJob = &mCudajob[mCurJobIndex];

	bool nextStep = false;

	BMI_ASSERT(curJob->mJobStatus != CUDA_JOB_UNINITED);


	// offset = tile offset plus the subband offset plus the current stripe offset
	int x_off = curJob->mPicture->mTileinfo[tileId].mOff.x + curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mOff.x;
	int y_off = curJob->mPicture->mTileinfo[tileId].mOff.y + curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mOff.y + yOffsetInBand;

// 	DEBUG_PAUSE(tileId && subbandId == 0);
	DEBUG_PRINT(("Upload buf %d lines for tile %d component %d subband %d (%d/%d)with id %d to dest (%d : %d)\n",linesAvailable, tileId, compId, subbandId, 
		curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mLinesInserted, curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mSize.y,
		buf->mAsyncId,  (x_off<<curJob->mSizeShift), y_off));

	buf->mStatus = BUF_STATUS_UPLOADING;

	DEBUG_PRINT(("thread %d insert Start async copy %d\n", callingThread->mThreadId, buf->mAsyncId));

	DecodeJob newJob;
	newJob.mJobType = JOB_CUDA_THREAD;
	newJob.mPictureIndex = pictureIndex;
	CudaThreadJob * newCudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);
	newCudaJob->mIsBlockJob = false;
	newCudaJob->mCallingThreadInfo = callingThread;


	newCudaJob->type = CUDA_BUF_COPY;
	newCudaJob->mCuda2DCopy.mSourceBuf = buf;
	newCudaJob->mCuda2DCopy.mDestBuf = curJob->mGpuOrigBuf[compId];
	newCudaJob->mCuda2DCopy.mDestX_off = (x_off<<curJob->mSizeShift);
	newCudaJob->mCuda2DCopy.mDestY_off = y_off;
	newCudaJob->mCuda2DCopy.mSrcX_off = newCudaJob->mCuda2DCopy.mSrcY_off = 0;
	newCudaJob->mCuda2DCopy.mWidth = (curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mSize.x)<<curJob->mSizeShift;
	newCudaJob->mCuda2DCopy.mHeight = linesAvailable;
	newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_HOST_TO_DEVICE;
	newCudaJob->mCuda2DCopy.mAsync_id = buf->mAsyncId;
	newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;

	ThreadSafeAccessCudaThreadJob(callingThread, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
	
// 	newCudaJob->type = CUDA_BUF_SYNC;
// 	newCudaJob->mCudaBufSync = buf;
// 
// 	ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);


	LOCK_MUTEX(mCudaJobCounterMutex);
	curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mLinesInserted += linesAvailable;
	if (curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mSize.y <= curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mLinesInserted)
	{
		nextStep = true;	// subband finished
		curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mLinesInserted = 0;
	}
	RELEASE_MUTEX(mCudaJobCounterMutex);

	if (nextStep)
	{
		nextStep = false;
		// current subband Finished
// 		static int componentAsyncOperationStreamIndex[MAX_COMPONENT] = {INVALID, INVALID, INVALID, INVALID};
		BufContainInfo	tcs;
		tcs.mSpcValue.mTileId = tileId;
		tcs.mSpcValue.mComponentId = compId;
		DEBUG_PRINT((">>>>> Tile %d Component %d subband %d [%d:%d]decode finished\n", tileId, compId, subbandId, curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedCounter+1,curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mNumOfSubband ));

		// get async id
		if (curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex == INVALID)
		{
			LOCK_MUTEX(mCudaJobCounterMutex);
			AccessIdleStream(curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex);
			RELEASE_MUTEX(mCudaJobCounterMutex);
		}	

		if (LAST_DWT_DECODE_BY_RESOLUTION && tileId == curJob->mPicture->mLastTileId && compId == curJob->mPicture->mTileinfo[tileId].mLastCompId && curJob->mPicture->mTopDwtLevel > 1)	// last component for the last tile
		{
			int resolutionId = (subbandId == 0 ? 0 : (subbandId - 1) / 3);
			LOCK_MUTEX(mCudaJobCounterMutex);
			curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedBitmap |= (1 << subbandId);
			if (resolutionId == curJob->mPicture->mTopDwtLevel - 1 && resolutionId > curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mResolutionFinishedId)
			{
				int resolutionMask = ((1 << (resolutionId * 3 + 1)) - 1);
				nextStep = ((curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedBitmap & resolutionMask) == resolutionMask);
			}
			RELEASE_MUTEX(mCudaJobCounterMutex);

			if (nextStep)
			{
				DEBUG_PRINT(("oooooo Resolution %d finished\n", resolutionId - 1));
				nextStep = false;
				// current resolution finished
				LOCK_MUTEX(mCudaJobCounterMutex);
				int startResolution = curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mResolutionFinishedId + 1;
				int resolutionMask = ((1 << ((resolutionId + 1) * 3 + 1)) - 1);
				nextStep = ((curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedBitmap & resolutionMask) == resolutionMask);
				if (!nextStep)
				{
					--resolutionId;
				}
				curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mResolutionFinishedId = resolutionId;

				RELEASE_MUTEX(mCudaJobCounterMutex);

				for (int r = startResolution; r <= resolutionId; ++r)
				{
					// sync buffer for this resolution
					STARTCLOCK(10 + callingThread->mThreadIndex);
					tcs.mSpcValue.mRes_Subband = (unsigned char)(r << 4);
					SyncUnsyncBuffer(NULL, tcs.mContainInfo, 0xfffffff0);
					STOPCLOCK(10 + callingThread->mThreadIndex);

					if (r == 0)
					{
						newCudaJob->type = CUDA_BUF_COPY;
						newCudaJob->mIsBlockJob = false;
						newCudaJob->mCuda2DCopy.mAsync_id = mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex].mAsyncId;
						newCudaJob->mCuda2DCopy.mDestBuf	= curJob->mGpuOrigBufThumb[compId];
						newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mGpuOrigBuf[compId];
						newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_DEVICE;
						newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;


						newCudaJob->mCuda2DCopy.mHeight =	curJob->mPicture->mTileinfoThumb[tileId].mSize.y;
						newCudaJob->mCuda2DCopy.mWidth	=	curJob->mPicture->mTileinfoThumb[tileId].mSize.x<<curJob->mSizeShift;
						newCudaJob->mCuda2DCopy.mSrcX_off = curJob->mPicture->mTileinfo[tileId].mOff.x<<curJob->mSizeShift;
						newCudaJob->mCuda2DCopy.mSrcY_off =	curJob->mPicture->mTileinfo[tileId].mOff.y;
						newCudaJob->mCuda2DCopy.mDestX_off = curJob->mPicture->mTileinfoThumb[tileId].mOff.x<<curJob->mSizeShift;
						newCudaJob->mCuda2DCopy.mDestY_off = curJob->mPicture->mTileinfoThumb[tileId].mOff.y;
						ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);			

					}

					// do DWT for this cresolution
					newCudaJob->type = DWT_RESOLUTION;
					newCudaJob->mDwtComp_ResJob.mTile	 = &curJob->mPicture->mTileinfo[tileId];
					newCudaJob->mDwtComp_ResJob.mWorkingComp = &curJob->mPicture->mTileinfo[tileId].mCompInfo[compId];
					newCudaJob->mDwtComp_ResJob.mOriginalBuf = curJob->mGpuOrigBuf[compId];
					newCudaJob->mDwtComp_ResJob.mTempBuf	 = curJob->mGpuTempBuf[compId];
					newCudaJob->mDwtComp_ResJob.mResolutionLevel = r;
					newCudaJob->mIsBlockJob = false;
					// 			newCudaJob->mDwtComp_ResJob.mAsyncId = mAsyncStream[tileAsyncJobStreamIndex].mAsyncId;
					newCudaJob->mDwtComp_ResJob.mAsyncId = mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex].mAsyncId;
					newCudaJob->mDwtComp_ResJob.mMethod = curJob->mPicture->mProperties.mMethod;
					newCudaJob->mDwtComp_ResJob.mWordShift = curJob->mSizeShift;
					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

					DEBUG_PRINT(("-----> Add DWT job for tile %d comp %d resolution %d id %d\n",tileId, compId, r, newCudaJob->mDwtComp_ResJob.mAsyncId));
				}

				nextStep = (resolutionId == curJob->mPicture->mTopDwtLevel - 1);

			}

		}
		else
		{
			LOCK_MUTEX(mCudaJobCounterMutex);
			++curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedCounter;
			if (curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedCounter == curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mNumOfSubband)
			{
				nextStep = true;
			}
			RELEASE_MUTEX(mCudaJobCounterMutex);
		}

		if (nextStep)
		{
			nextStep = false;
			static int lastTileAsyncJobStreamIndex =	INVALID;
			// this component finished
			// call cudaDwt for this component here

			DEBUG_PRINT((">>>>> Tile %d Component %d [%d:%d]decode finished\n", tileId, compId,curJob->mPicture->mTileinfo[tileId].mComFinishedCounter+1, curJob->mPicture->mProperties.mComponentNum ));

			if (!LAST_DWT_DECODE_BY_RESOLUTION || tileId != curJob->mPicture->mLastTileId || compId != curJob->mPicture->mTileinfo[tileId].mLastCompId || curJob->mPicture->mTopDwtLevel <= 1)	// it's not the last component
			{
				STARTCLOCK(10 + callingThread->mThreadIndex);
				BufContainInfo	tcs;
				tcs.mSpcValue.mTileId = tileId;
				tcs.mSpcValue.mComponentId = compId;
				SyncUnsyncBuffer(NULL, tcs.mContainInfo, 0xffffff00);
				STOPCLOCK(10 + callingThread->mThreadIndex);

#if !GPU_W9X7_FLOAT
				if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)	// is i97?
				{

					newCudaJob->type = DEQ_97_COMPONENT;
					newCudaJob->mIsBlockJob = false;
					newCudaJob->mDeq97Job.mDeviceBuf = curJob->mGpuOrigBuf[compId];
					newCudaJob->mDeq97Job.mWorkingTile = &curJob->mPicture->mTileinfo[tileId];
					newCudaJob->mDeq97Job.mTopDeqLevel = config.mDwtDecodeTopLevel;
					newCudaJob->mDeq97Job.mComponentId = compId;
					newCudaJob->mDeq97Job.mWordShift = curJob->mSizeShift;
					newCudaJob->mDeq97Job.mAsync_id = mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex].mAsyncId;

					DEBUG_PRINT(("-----> Add DEQ job for comp %d with id %d\n", compId, mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex].mAsyncId));

					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

				}
#endif
			newCudaJob->type = CUDA_BUF_COPY;
			newCudaJob->mIsBlockJob = false;
				newCudaJob->mCuda2DCopy.mAsync_id = mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex].mAsyncId;
				newCudaJob->mCuda2DCopy.mDestBuf	= curJob->mGpuOrigBufThumb[compId];
				newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mGpuOrigBuf[compId];
				newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_DEVICE;
				newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;


				newCudaJob->mCuda2DCopy.mHeight =	curJob->mPicture->mTileinfoThumb[tileId].mSize.y;
				newCudaJob->mCuda2DCopy.mWidth	=	curJob->mPicture->mTileinfoThumb[tileId].mSize.x<<curJob->mSizeShift;
				newCudaJob->mCuda2DCopy.mSrcX_off = curJob->mPicture->mTileinfo[tileId].mOff.x<<curJob->mSizeShift;
				newCudaJob->mCuda2DCopy.mSrcY_off =	curJob->mPicture->mTileinfo[tileId].mOff.y;
				newCudaJob->mCuda2DCopy.mDestX_off = curJob->mPicture->mTileinfoThumb[tileId].mOff.x<<curJob->mSizeShift;
				newCudaJob->mCuda2DCopy.mDestY_off = curJob->mPicture->mTileinfoThumb[tileId].mOff.y;
				ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);			


				newCudaJob->type = DWT_COMPONENT;
				newCudaJob->mDwtComp_ResJob.mTile	 = &curJob->mPicture->mTileinfo[tileId];
				newCudaJob->mDwtComp_ResJob.mWorkingComp = &curJob->mPicture->mTileinfo[tileId].mCompInfo[compId];
				newCudaJob->mDwtComp_ResJob.mOriginalBuf = curJob->mGpuOrigBuf[compId];
				newCudaJob->mDwtComp_ResJob.mTempBuf	 = curJob->mGpuTempBuf[compId];
				newCudaJob->mDwtComp_ResJob.mTopLevel = curJob->mPicture->mTopDwtLevel;
				newCudaJob->mIsBlockJob = false;
				// 			newCudaJob->mDwtComp_ResJob.mAsyncId = mAsyncStream[tileAsyncJobStreamIndex].mAsyncId;
				newCudaJob->mDwtComp_ResJob.mAsyncId = mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex].mAsyncId;
				newCudaJob->mDwtComp_ResJob.mMethod = curJob->mPicture->mProperties.mMethod;
				newCudaJob->mDwtComp_ResJob.mWordShift = curJob->mSizeShift;
				ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

				DEBUG_PRINT(("-----> Add DWT job for tile %d comp %d id %d\n",tileId, compId, newCudaJob->mDwtComp_ResJob.mAsyncId));
			}




			LOCK_MUTEX(mCudaJobCounterMutex);
			curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedCounter = 0;
// 			curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mAsyncOperationStreamIndex = INVALID;
			curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mResolutionFinishedId = INVALID;
			curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandFinishedBitmap = 0;
			if (++curJob->mPicture->mTileinfo[tileId].mComFinishedCounter == curJob->mPicture->mProperties.mComponentNum)
			{
				nextStep = true;
				curJob->mPicture->mTileinfo[tileId].mComFinishedCounter = 0;
			}
			RELEASE_MUTEX(mCudaJobCounterMutex);

// #if WRITE_TO_FILE 
// 			{
// 				static bool printOut[4] = {false}; 
// 				if (tileId == (curJob->mPicture->mProperties.mTileNum.x *curJob->mPicture->mProperties.mTileNum.y)-1 ) // last tile
// 				{
// 					if (!printOut[compId])
// 					{
// 						printOut[compId] = true;
// 						newCudaJob->mIsBlockJob = true;
// 						newCudaJob->mReleaseMe = mCudaBlockSemaphore;
// 
// 						newCudaJob->type = SYNC_ALL;
// 						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);	
// 						// Add job as last one in sync queue and sync all jobs, make sure all data copy finished
// 
// 						WAIT_SEMAPHORE(mCudaBlockSemaphore);
// 
// 						//	CUDA_COPY_OUT
// 						newCudaJob->type = CUDA_BUF_COPY;
// 						// 				newCudaJob->mIsBlockJob = true;
// 						// 				newCudaJob->mReleaseMe = mCudaBlockSemaphore;
// 						newCudaJob->mCuda2DCopy.mDestBuf = &mHostResultBuf[curJob->mOutputIndex];
// 						newCudaJob->mCuda2DCopy.mSourceBuf = curJob->mGpuOrigBuf[compId];
// 						newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_HOST;
// 						newCudaJob->mCuda2DCopy.mHeight = curJob->mPicture->mProperties.mSize.y;
// 						newCudaJob->mCuda2DCopy.mWidth =  curJob->mPicture->mProperties.mSize.x<<curJob->mSizeShift;
// 						newCudaJob->mCuda2DCopy.mSrcX_off = newCudaJob->mCuda2DCopy.mDestX_off 
// 							= 0;
// 						newCudaJob->mCuda2DCopy.mSrcY_off = newCudaJob->mCuda2DCopy.mDestY_off
// 							= 0;
// 						newCudaJob->mCuda2DCopy.mAsync_id = 0;
// 						newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;
// 
// 						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 
// 						WAIT_SEMAPHORE(mCudaBlockSemaphore);
// 						char filename[255];
// 
// 						sprintf_s(filename, 255,"d:\\test\\comp_%d_DWT.txt", compId );
// 						// 				sprintf_s(filename, 255,"d:\\test\\comp_%d_up.txt", compId );
// 						print_buf_to_file<unsigned int, unsigned int>(reinterpret_cast<unsigned int *>(mHostResultBuf[curJob->mOutputIndex].mBuf),
// 							mHostResultBuf[curJob->mOutputIndex].mSize, filename, mHostResultBuf[curJob->mOutputIndex].mPitch>>curJob->mSizeShift, true, true);
// 
// 					}
// 				}
// 			}
// #endif

			if (nextStep)
			{
				nextStep = false;
				// this tile finished
				DEBUG_PRINT((">>>>> Tile %d finished\n", tileId));

				// all components finished, need to sync DWT before we can do MCT/ICT
//				LOCK_MUTEX(mCudaJobCounterMutex);
				for (int i = 0; i < curJob->mPicture->mProperties.mComponentNum; ++i)
				{
					if (curJob->mPicture->mTileinfo[tileId].mCompInfo[i].mAsyncOperationStreamIndex != INVALID)
					{
						DEBUG_PRINT(("-----> Sync component %d  with id %d\n", i,mAsyncStream[curJob->mPicture->mTileinfo[tileId].mCompInfo[i].mAsyncOperationStreamIndex].mAsyncId));
						// now it's time to sync the last component dwt
						newCudaJob->type = CUDA_STREAM_SYNC;
						newCudaJob->mIsBlockJob = false;
						newCudaJob->mStreamSyncIndex = curJob->mPicture->mTileinfo[tileId].mCompInfo[i].mAsyncOperationStreamIndex;
						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
						// sync the component DWT
// 						AccessIdleStream(curJob->mPicture->mTileinfo[tileId].mCompInfo[i].mAsyncOperationStreamIndex, false)
						curJob->mPicture->mTileinfo[tileId].mCompInfo[i].mAsyncOperationStreamIndex = INVALID;
					}
				}
//				RELEASE_MUTEX(mCudaJobCounterMutex);

#if _DEBUG && WRITE_TO_FILE
// 				static bool dwtprintOut = true;
// 				if (dwtprintOut)
// 				{
// 					dwtprintOut = false;
// 					for (int c = 0; c < 3; ++c)
// 					{
// 						//	CUDA_COPY_OUT
// 						newCudaJob->type = CUDA_BUF_COPY;
// 						newCudaJob->mIsBlockJob = true;
// 						newCudaJob->mReleaseMe = mCudaBlockSemaphore;
// 						newCudaJob->mCuda2DCopy.mDestBuf = &mHostResultBuf[curJob->mOutputIndex];
// 						newCudaJob->mCuda2DCopy.mSourceBuf = curJob->mGpuOrigBuf[c];
// 						newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_HOST;
// 						newCudaJob->mCuda2DCopy.mHeight = curJob->mPicture->mProperties.mSize.y;
// 						newCudaJob->mCuda2DCopy.mWidth =  curJob->mPicture->mProperties.mSize.x<<curJob->mSizeShift;
// 						newCudaJob->mCuda2DCopy.mX_off = 0;
// 						newCudaJob->mCuda2DCopy.mY_off = 0;
// 						newCudaJob->mCuda2DCopy.mAsync_id = 0;
// 
// 						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 
// 						WAIT_SEMAPHORE(mCudaBlockSemaphore);
// 						char filename[255];
// 						sprintf_s(filename, 255,"d:\\test\\comp_%d_DWT.txt", c );
// 
// 						print_buf_to_file<unsigned int, unsigned int>(reinterpret_cast<unsigned int *>(mHostResultBuf[curJob->mOutputIndex].mBuf),
// 							mHostResultBuf[curJob->mOutputIndex].mSize,filename , mHostResultBuf[curJob->mOutputIndex].mPitch>>curJob->mSizeShift, true, true);
// 
// 					}
// 				}

#endif
				int checkOutComponent = config.mCheckOutComponent;
				if ( checkOutComponent >= curJob->mPicture->mProperties.mComponentNum)
				{
					checkOutComponent = INVALID;
				}

				if (lastTileAsyncJobStreamIndex == INVALID)
				{
					AccessIdleStream(lastTileAsyncJobStreamIndex);
				}

				if (checkOutComponent < 0 || checkOutComponent >= curJob->mPicture->mProperties.mComponentNum || ((config.mIgnoreComponents >> checkOutComponent) && 0x01))
				{
					checkOutComponent = INVALID;
				}
 
				if  (curJob->mPicture->mProperties.mCalculatedComponentNum >= 3 && checkOutComponent == INVALID)	
				{
	
					int componentNum = 0;

					for (int i = 0; i < curJob->mPicture->mProperties.mComponentNum; ++i)
					{
						if ((config.mIgnoreComponents >> componentNum) & 0x01)
						{
							continue;
						}
						curJob->mPicture->mTileinfo[tileId].mCompInfo[componentNum].mCompBuf = curJob->mGpuOrigBuf[i];
						curJob->mPicture->mTileinfoThumb[tileId].mCompInfo[componentNum].mCompBuf = curJob->mGpuOrigBufThumb[i];
						componentNum++;
					}
					curJob->mPicture->mTileinfo[tileId].mResultBuf = &mGpuResultBuf[curJob->mOutputIndex];
					curJob->mPicture->mTileinfoThumb[tileId].mResultBuf = &mGpuResultBufThumb[curJob->mOutputIndex];

					switch (curJob->mPicture->mProperties.mOutputFormat)
					{
					case IMAGE_FOTMAT_32BITS_ARGB:
					case IMAGE_FOTMAT_48BITS_RGB:
					case IMAGE_FOTMAT_24BITS_RGB:
					case IMAGE_FOTMAT_64BITS_ARGB:
					case IMAGE_FOTMAT_36BITS_RGB:
					case IMAGE_FOTMAT_48BITS_ARGB:
						{

							if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)	// is i97?
							{
								newCudaJob->type = ICT_97_TILE;
							}
							else
							{
								newCudaJob->type = MCT_53_TILE;
							}					
							newCudaJob->mMctIctJob.mTileInfo = &curJob->mPicture->mTileinfo[tileId];
							newCudaJob->mMctIctJob.mAsyncId = mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId;
							newCudaJob->mMctIctJob.mBitDepth = curJob->mPicture->mProperties.mBitDepth;
							newCudaJob->mMctIctJob.mWordShift = curJob->mSizeShift;
							newCudaJob->mMctIctJob.mDwtLevel = curJob->mPicture->mProperties.mDwtLevel;
							newCudaJob->mMctIctJob.mDecodeLevel = curJob->mPicture->mTopDwtLevel;
							newCudaJob->mMctIctJob.mOutputFormat = config.mOutputFormat;

							DEBUG_PRINT(("-----> ICT//MCT job id : %d\n", newCudaJob->mMctIctJob.mAsyncId));
							ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

							for (int i = 0, c = 0; i < curJob->mPicture->mProperties.mComponentNum; ++i)
							{
								if ((config.mIgnoreComponents >> c) & 0x01)
								{
									continue;
								}
								curJob->mPicture->mTileinfoThumb[tileId].mCompInfo[c++].mCompBuf = curJob->mGpuOrigBufThumb[i];
							}
							curJob->mPicture->mTileinfoThumb[tileId].mResultBuf = &mGpuResultBufThumb[curJob->mOutputIndex];							
							newCudaJob->mMctIctJob.mTileInfo = &curJob->mPicture->mTileinfoThumb[tileId];
							newCudaJob->mMctIctJob.mDwtLevel = 0;
							newCudaJob->mMctIctJob.mDecodeLevel = -1;

							DEBUG_PRINT(("-----> ICT//MCT job for thumb id : %d\n", newCudaJob->mMctIctJob.mAsyncId));
							ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
						}
						break;

					case IMAGE_FOTMAT_32BITS_4COMPONENTS:
					case IMAGE_FOTMAT_48BITS_3COMPONENTS:
					case IMAGE_FOTMAT_24BITS_3COMPONENTS:
					case IMAGE_FOTMAT_64BITS_4COMPONENTS:
						{
							DEBUG_PRINT(("-----> merge all component into picture job id : %d\n", newCudaJob->mMctIctJob.mAsyncId));
							newCudaJob->type = MERGE_TILE;
							newCudaJob->mTileMergeJob.mComponentNum = componentNum;
							newCudaJob->mTileMergeJob.mTileInfo = &curJob->mPicture->mTileinfo[tileId];
							newCudaJob->mTileMergeJob.mAsyncId = mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId;
							newCudaJob->mTileMergeJob.mBitDepth = curJob->mPicture->mProperties.mBitDepth;
							newCudaJob->mTileMergeJob.mDwtLevel = curJob->mPicture->mProperties.mDwtLevel;
							newCudaJob->mTileMergeJob.mDecodeLevel = curJob->mPicture->mTopDwtLevel;
							newCudaJob->mTileMergeJob.mWordShift = curJob->mSizeShift;
							newCudaJob->mTileMergeJob.mOutputFormat = config.mOutputFormat;
							newCudaJob->mTileMergeJob.mMethod = curJob->mPicture->mProperties.mMethod;
							newCudaJob->mTileMergeJob.mCompBufPitchBytes = curJob->mPicture->mTileinfo[tileId].mCompInfo[0].mCompBuf->mPitch;
// 							for (int c = 0; c < MAX_COMPONENT; ++c)
// 							{
// 								newCudaJob->mTileMergeJob.mColorComponent[c] = c;
// // 								curJob->mPicture->mTileinfo[tileId].mCompInfo[c].mCompBuf 
// // 									= curJob->mGpuOrigBuf[newCudaJob->mTileMergeJob.mColorComponent[c]];
// 								if (c < curJob->mPicture->mProperties.mComponentNum)
// 								{
// 									curJob->mPicture->mTileinfo[tileId].mCompInfo[c].mDeShifted = 1;
// 								}
// 							}
// 							newCudaJob->mTileMergeJob.mMethod = curJob->mPicture->mProperties.mMethod;
// 							newCudaJob->mTileMergeJob.mDecodeLevel = config.mDwtDecodeTopLevel;
							DEBUG_PRINT(("-----> merge all component into picture job id : %d\n", newCudaJob->mTileMergeJob.mAsyncId));

							ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

							newCudaJob->mTileMergeJob.mTileInfo = &curJob->mPicture->mTileinfoThumb[tileId];
							newCudaJob->mTileMergeJob.mCompBufPitchBytes = curJob->mPicture->mTileinfoThumb[tileId].mCompInfo[0].mCompBuf->mPitch;

							ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
						}

						// 					}
						// 
						// 					else if (checkOutComponent != INVALID)
						// 					{
						// 						newCudaJob->type = CHECK_OUT_ONE_COMPONENT;
						// 
						// 						newCudaJob->mCheckOutComponentJob.mTileInfo = &curJob->mPicture->mTileinfo[tileId];
						// 						curJob->mPicture->mTileinfo[tileId].mResultBuf = &mGpuResultBuf[curJob->mOutputIndex];
						// 						newCudaJob->mCheckOutComponentJob.mAsyncId = mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId;
						// 						newCudaJob->mCheckOutComponentJob.mWordShift = curJob->mSizeShift;
						// 						newCudaJob->mCheckOutComponentJob.mBitDepth = curJob->mPicture->mProperties.mBitDepth;
						// 						newCudaJob->mCheckOutComponentJob.mComponentId = checkOutComponent;	
						// 						newCudaJob->mCheckOutComponentJob.mDecodeLevel = config.mDwtDecodeTopLevel;
						// 
						// 						curJob->mPicture->mTileinfo[tileId].mCompInfo[checkOutComponent].mCompBuf = curJob->mGpuOrigBuf[checkOutComponent];
						// 
						// 						newCudaJob->mCheckOutComponentJob.mMethod = curJob->mPicture->mProperties.mMethod;
						// 						newCudaJob->mCheckOutComponentJob.mHasMCT = (curJob->mPicture->mProperties.mComponentNum == 3);	// todo : should check mct/ict
						// 
						// 						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
						// 
						// 						newCudaJob->mCheckOutComponentJob.mTileInfo = &curJob->mPicture->mTileinfoThumb[tileId];
						// 						curJob->mPicture->mTileinfoThumb[tileId].mResultBuf = &mGpuResultBufThumb[curJob->mOutputIndex];
						// 						newCudaJob->mCheckOutComponentJob.mDecodeLevel = -1;
						// 
						// 						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
						// 					}
						break;

					case IMAGE_FOTMAT_NONE:
						// don't need to do MCT
						return;

						BMI_ASSERT(0);
						break;

					}


		
				}
				else 
				{
					if (checkOutComponent == INVALID)
					{
						// use the first decoded componet as result
						int c = 0;
						while ((config.mIgnoreComponents >> (c++))& 0x01);
						checkOutComponent = c - 1;
					}
									
					newCudaJob->type = CHECK_OUT_ONE_COMPONENT;

					newCudaJob->mIsBlockJob = false;
					newCudaJob->mCheckOutComponentJob.mTileInfo = &curJob->mPicture->mTileinfo[tileId];
					curJob->mPicture->mTileinfo[tileId].mResultBuf = &mGpuResultBuf[curJob->mOutputIndex];
					newCudaJob->mCheckOutComponentJob.mAsyncId = mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId;
					newCudaJob->mCheckOutComponentJob.mWordShift = curJob->mSizeShift;
					newCudaJob->mCheckOutComponentJob.mBitDepth = curJob->mPicture->mProperties.mBitDepth;
					newCudaJob->mCheckOutComponentJob.mComponentId = 0;	
					newCudaJob->mCheckOutComponentJob.mDwtLevel = curJob->mPicture->mProperties.mDwtLevel;
					newCudaJob->mCheckOutComponentJob.mDecodeLevel = curJob->mPicture->mTopDwtLevel;

					curJob->mPicture->mTileinfo[tileId].mCompInfo[0].mCompBuf = curJob->mGpuOrigBuf[0];

					newCudaJob->mCheckOutComponentJob.mMethod = curJob->mPicture->mProperties.mMethod;
					newCudaJob->mCheckOutComponentJob.mHasMCT = false;	//***UUU No MCT
					
					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

					newCudaJob->mCheckOutComponentJob.mTileInfo = &curJob->mPicture->mTileinfoThumb[tileId];
					curJob->mPicture->mTileinfoThumb[tileId].mResultBuf = &mGpuResultBufThumb[curJob->mOutputIndex];
					newCudaJob->mCheckOutComponentJob.mDecodeLevel =  -1;
					curJob->mPicture->mTileinfoThumb[tileId].mCompInfo[0].mCompBuf = curJob->mGpuOrigBufThumb[0];

					ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
				}


				LOCK_MUTEX(mCudaJobCounterMutex);
				curJob->mPicture->mTileinfo[tileId].mComFinishedCounter = 0;
				if (++curJob->mPicture->mTileFinishedCounter == curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y)
				{
					nextStep = true;
				}
				RELEASE_MUTEX(mCudaJobCounterMutex);

				if (nextStep)
				{
					// all tiles finished, whole picture finished!!

					DEBUG_PRINT((">>>>> Whole picture finished\n"));

					// sync unfinished tile jobs


// 					if (curJob->mPicture->mProperties.mComponentNum != 3 && config.mCheckOutComponent == INVALID)	// todo : 0 user don't want to check out a certain component
// 					{
// 						if (lastTileAsyncJobStreamIndex != INVALID)
// 						{
// 							DEBUG_PRINT(("-----> Sync job %d \n",mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId));
// 
// 							// now it's time to sync the last component dwt
// 							newCudaJob->type = CUDA_STREAM_SYNC;
// 							newCudaJob->mIsBlockJob = false;
// 							newCudaJob->mStreamSyncIndex = lastTileAsyncJobStreamIndex;
// 							ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 							// sync the component DWT
// 							lastTileAsyncJobStreamIndex = INVALID;
// 						}
// 
// 						if (lastTileAsyncJobStreamIndex == INVALID)
// 						{
// 							AccessIdleStream(lastTileAsyncJobStreamIndex);
// 						}
// 
// 					// copy the first component as output
// 						newCudaJob->type = CUDA_BUF_COPY;
// 						newCudaJob->mIsBlockJob = false;
// 						newCudaJob->mCuda2DCopy.mAsync_id = lastTileAsyncJobStreamIndex;
// 						newCudaJob->mCuda2DCopy.mDestBuf	= &mGpuResultBuf[curJob->mOutputIndex];
// 						newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mGpuOrigBuf[1];
// 						newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_DEVICE;
// 						newCudaJob->mCuda2DCopy.mHeight = curJob->mPicture->mProperties.mSize.y;
// 						newCudaJob->mCuda2DCopy.mWidth =  curJob->mPicture->mProperties.mSize.x << curJob->mSizeShift;
// 						newCudaJob->mCuda2DCopy.mSrcX_off = newCudaJob->mCuda2DCopy.mSrcY_off = 0;
// 						newCudaJob->mCuda2DCopy.mDestX_off = newCudaJob->mCuda2DCopy.mDestY_off = 0;
// 						newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;
// 
// 						ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 					}

// 					if (config.mDwtDecodeTopLevel != INVALID && config.mDwtDecodeTopLevel != curJob->mPicture->mProperties.mDwtLevel 
// 						&& curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y > 1)
// 					{
// 						if (lastTileAsyncJobStreamIndex != INVALID)
// 						{
// 							DEBUG_PRINT(("-----> Sync job %d \n",mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId));
// 
// 							// now it's time to sync tile's job
// 							newCudaJob->type = CUDA_STREAM_SYNC;
// 							newCudaJob->mIsBlockJob = false;
// 							newCudaJob->mStreamSyncIndex = lastTileAsyncJobStreamIndex;
// 							ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 							// sync the component DWT
// 							lastTileAsyncJobStreamIndex = INVALID;
// 						}
// 
// 						AccessIdleStream(lastTileAsyncJobStreamIndex);
// 
// 						BMI_ASSERT(config.mDwtDecodeTopLevel < curJob->mPicture->mProperties.mDwtLevel);
// 
// 						int downLeveL = curJob->mPicture->mProperties.mDwtLevel - config.mDwtDecodeTopLevel;
// 
// 						newCudaJob->type = CUDA_BUF_COPY;
// 						newCudaJob->mIsBlockJob = false;
// 						newCudaJob->mCuda2DCopy.mAsync_id = mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId;
// 						newCudaJob->mCuda2DCopy.mDestBuf	= &mGpuResultBuf[curJob->mOutputIndex];
// 						newCudaJob->mCuda2DCopy.mSourceBuf	= &mGpuResultBuf[curJob->mOutputIndex];
// 						newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_DEVICE;
// 						newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;
// 
// 						for (int tid = 1; tid < curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y; ++tid)	// doesn't need to move the first part
// 						{
// 							newCudaJob->mCuda2DCopy.mHeight =	(curJob->mPicture->mTileinfo->mSize.y + 1) >> downLeveL;
// 							newCudaJob->mCuda2DCopy.mWidth	=	((curJob->mPicture->mTileinfo->mSize.x + 1) >> downLeveL)<<curJob->mSizeShift;
// 							newCudaJob->mCuda2DCopy.mSrcX_off = curJob->mPicture->mTileinfo[tid].mOff.x<<curJob->mSizeShift;
// 							newCudaJob->mCuda2DCopy.mSrcY_off =	curJob->mPicture->mTileinfo[tid].mOff.y;
// 							newCudaJob->mCuda2DCopy.mDestX_off = ((curJob->mPicture->mTileinfo[tid].mOff.x + 1) >> downLeveL)<<curJob->mSizeShift;
// 							newCudaJob->mCuda2DCopy.mDestY_off =(curJob->mPicture->mTileinfo[tid].mOff.y + 1) >> downLeveL;
// 							ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 
// 						}
// 
// 					}

					// sync last tile's job if needed
					if (lastTileAsyncJobStreamIndex != INVALID)
					{
						DEBUG_PRINT(("-----> Sync job %d \n",mAsyncStream[lastTileAsyncJobStreamIndex].mAsyncId));

						newCudaJob->type = CUDA_STREAM_SYNC;
						newCudaJob->mIsBlockJob = false;
						newCudaJob->mStreamSyncIndex = lastTileAsyncJobStreamIndex;
						ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
						lastTileAsyncJobStreamIndex = INVALID;
					}



					curJob->mJobStatus = CUDA_JOB_READY;
					curJob->mPicture->mTileFinishedCounter = 0;

					++mCurJobIndex;	// switch to next job
					if (mCurJobIndex == mEngineConfig->mOutputQueueLen)
					{
						mCurJobIndex = 0;
					}
				}
			}
		}

// 		if (tileAsyncJobStreamIndex != INVALID)
// 		{
// 			newCudaJob->type = CUDA_STREAM_SYNC;
// 			newCudaJob->mIsBlockJob = false;
// 			newCudaJob->mStreamSyncIndex = tileAsyncJobStreamIndex;
// 			ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
// 		}
	}
	return;
}

CudaBuf * CudaManager::GetResultBuf(int outputIndex, DecoderConcrete::DecodeResult & output)
{

	P_CudaBuf ret = NULL;
	DecodeJob	newJob;
	newJob.mJobType = JOB_CUDA_THREAD;
	CudaThreadJob	* newCudaJob = reinterpret_cast<CudaThreadJob	*>(newJob.mJobPara);

	int bytesperPixels; 
	switch (output.mProperties.mOutputFormat)
	{
	case IMAGE_FOTMAT_32BITS_ARGB:
	case IMAGE_FOTMAT_32BITS_4COMPONENTS:
		bytesperPixels = 4;
		break;

	case IMAGE_FOTMAT_48BITS_RGB:
	case IMAGE_FOTMAT_48BITS_ARGB:
	case IMAGE_FOTMAT_48BITS_3COMPONENTS:
		bytesperPixels = 6;
		break;

	case IMAGE_FOTMAT_24BITS_RGB:
	case IMAGE_FOTMAT_24BITS_3COMPONENTS:
		bytesperPixels = 3;
		break;

	case IMAGE_FOTMAT_64BITS_ARGB:
	case IMAGE_FOTMAT_64BITS_4COMPONENTS:
		bytesperPixels = 8;
		break;

	case IMAGE_FOTMAT_36BITS_RGB:
		BMI_ASSERT(0);
		break;

	case IMAGE_FOTMAT_NONE:
		bytesperPixels = 0;
		break;

	}

	if (bytesperPixels)
	{
		newCudaJob->type = CUDA_BUF_COPY;
		newCudaJob->mIsBlockJob = true;
		newCudaJob->mReleaseMe = mCudaBlockSemaphore;
		newCudaJob->mCuda2DCopy.mAsync_id = 0;		// Sync job;

		newCudaJob->mCuda2DCopy.mDestBuf	= &mHostResultBuf[outputIndex];
		newCudaJob->mCuda2DCopy.mSourceBuf	= &mGpuResultBuf[outputIndex];
		newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_HOST;
		newCudaJob->mCuda2DCopy.mHeight = output.mProperties.mOutputSize.y;
		newCudaJob->mCuda2DCopy.mWidth =  output.mProperties.mOutputSize.x * bytesperPixels;
		newCudaJob->mCuda2DCopy.mSrcX_off = newCudaJob->mCuda2DCopy.mDestX_off = 0;
		newCudaJob->mCuda2DCopy.mSrcY_off = newCudaJob->mCuda2DCopy.mDestY_off = 0;
		newCudaJob->mCuda2DCopy.mDestNoPitch = true;
		newCudaJob->mCuda2DCopy.mSrcNoPitch = false;

		ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

		output.mBufSize = newCudaJob->mCuda2DCopy.mHeight * newCudaJob->mCuda2DCopy.mWidth;
		WAIT_SEMAPHORE(mCudaBlockSemaphore);
		ret = &mHostResultBuf[outputIndex];

	}

	return ret;
}

CudaBuf * CudaManager::GetThumbnail(int outputIndex, int & sizeofBuf)
{

	P_CudaBuf ret = NULL;
	DecodeJob	newJob;
	newJob.mJobType = JOB_CUDA_THREAD;
	CudaThreadJob	* newCudaJob = reinterpret_cast<CudaThreadJob	*>(newJob.mJobPara);

	int pixelSize;
	switch (mCudajob[outputIndex].mPicture->mProperties.mOutputFormat)
	{
	case	IMAGE_FOTMAT_32BITS_ARGB:
		pixelSize = sizeof(ARGB_32BITS);
		break;

	case	IMAGE_FOTMAT_48BITS_RGB:
		pixelSize = sizeof(RGB_48BITS);
		break;

	case	IMAGE_FOTMAT_24BITS_RGB:
		pixelSize = sizeof(RGB_24BITS);
		break;

	case	IMAGE_FOTMAT_64BITS_ARGB:
		pixelSize = sizeof(ARGB_64BITS);
		break;

	case	IMAGE_FOTMAT_36BITS_RGB:
	case	IMAGE_FOTMAT_48BITS_ARGB:
	default:
#pragma	BMI_PRAGMA_TODO_MSG("for supporting more format")
		break;
	}

	newCudaJob->type = CUDA_BUF_COPY;
	newCudaJob->mIsBlockJob = true;
	newCudaJob->mReleaseMe = mCudaBlockSemaphore;
	newCudaJob->mCuda2DCopy.mAsync_id = 0;		// Sync job;
	
	newCudaJob->mCuda2DCopy.mDestBuf	= &mHostResultBufThumb[outputIndex];
	newCudaJob->mCuda2DCopy.mSourceBuf	= &mGpuResultBufThumb[outputIndex] ;
	newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_HOST;
	newCudaJob->mCuda2DCopy.mHeight = mCudajob[outputIndex].mPicture->mProperties.mThumbOutputSize.y/*mGpuResultBufThumb[outputIndex].mHeight*/;
	newCudaJob->mCuda2DCopy.mWidth =  mCudajob[outputIndex].mPicture->mProperties.mThumbOutputSize.x * pixelSize/*mGpuResultBufThumb[outputIndex].mWidth*/;
	newCudaJob->mCuda2DCopy.mSrcX_off = newCudaJob->mCuda2DCopy.mDestX_off = 0;
	newCudaJob->mCuda2DCopy.mSrcY_off = newCudaJob->mCuda2DCopy.mDestY_off = 0;
	newCudaJob->mCuda2DCopy.mDestNoPitch = true;
	newCudaJob->mCuda2DCopy.mSrcNoPitch = false;

	ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);

	WAIT_SEMAPHORE(mCudaBlockSemaphore);
	ret = &mHostResultBufThumb[outputIndex];

	return ret;
}


CudaBuf * CudaManager::GetOneComponentInt(int outputIndex, int & sizeofBuf, int tileId, int componentId, int decodeTopLevel)
{

	sizeofBuf = 0;
	P_CudaBuf ret = NULL;
	P_CudaJob curJob = &mCudajob[outputIndex];

	int tileNum = curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y;

	if (tileId <  tileNum && 
		tileId >= INVALID && 
		outputIndex < mEngineConfig->mOutputQueueLen && 
		componentId < curJob->mPicture->mProperties.mComponentNum)
	{

		DecodeJob	newJob;
		newJob.mJobType = JOB_CUDA_THREAD;
		CudaThreadJob	* newCudaJob = reinterpret_cast<CudaThreadJob	*>(newJob.mJobPara);

		if (mComponentResultBuf->mStatus == BUF_STATUS_UNINITIALIZED || mComponentResultBuf->mBuf == NULL || mComponentResultBuf->mSize < (unsigned int)(curJob->mPicture->mProperties.mOutputSize.x * curJob->mPicture->mProperties.mOutputSize.y * 4))
		{
			if (mComponentResultBuf->mBuf && mComponentResultBuf->mStatus != BUF_STATUS_UNINITIALIZED )
			{
				newCudaJob->type = FREE_CUDA_BUF;
				newCudaJob->mDeleteBuf = mComponentResultBuf;
				ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
			}

			newCudaJob->type = INIT_GPU_BUF;
			newCudaJob->mCudaInitBuf.mCudaBuf = mComponentResultBuf;
			newCudaJob->mCudaInitBuf.mType = HOST_BUF;
			newCudaJob->mCudaInitBuf.mSize = curJob->mPicture->mProperties.mOutputSize.x * curJob->mPicture->mProperties.mOutputSize.y * 4;
			newCudaJob->mCudaInitBuf.mWidth = curJob->mPicture->mProperties.mOutputSize.x * 4;
			newCudaJob->mCudaInitBuf.mHeight = curJob->mPicture->mProperties.mOutputSize.y;
			newCudaJob->mIsBlockJob = true;		// need to wait cuda thread return
			newCudaJob->mReleaseMe = mCudaBlockSemaphore;	
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );	
			WAIT_SEMAPHORE(mCudaBlockSemaphore);
		}

		BMI_ASSERT(curJob->mSizeShift == 2);	// only support this size
		if (!curJob->mPicture->mTileinfo[0].mCompInfo[componentId].mDeShifted)
		{
			// alway transfer the whole picture
			newCudaJob->type = GET_ONE_COMPONENT;
			newCudaJob->mCompDcShiftJob.mAsyncId = 0;
			newCudaJob->mCompDcShiftJob.mBitDepth = curJob->mPicture->mProperties.mBitDepth;
			newCudaJob->mCompDcShiftJob.mCompBuf = curJob->mPicture->mTileinfo[0].mCompInfo[componentId].mCompBuf; //***UUU why not use curJob->mGpuOrigBuf
			newCudaJob->mCompDcShiftJob.mCompBufTemp = curJob->mGpuTempBuf[componentId];
			newCudaJob->mCompDcShiftJob.mMethod = curJob->mPicture->mProperties.mMethod;
			newCudaJob->mCompDcShiftJob.mPizelWordLength = (1 << curJob->mSizeShift);
			newCudaJob->mIsBlockJob = false; 			

			for (int tid = 0; tid < curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y; ++tid)	
			{
				if (decodeTopLevel != INVALID && decodeTopLevel != curJob->mPicture->mProperties.mDwtLevel)
				{
					newCudaJob->mCompDcShiftJob.mHeight = curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.y + 
						curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 -1].mSize.y;
					newCudaJob->mCompDcShiftJob.mWidth = (curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.x + 
						curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 1].mSize.x);				

					newCudaJob->mCompDcShiftJob.mDstX_off = curJob->mPicture->mTileinfo[tid].mOff.x;
					newCudaJob->mCompDcShiftJob.mDstY_off = curJob->mPicture->mTileinfo[tid].mOff.y;

					for (int dwtLevel = curJob->mPicture->mProperties.mDwtLevel;  dwtLevel > decodeTopLevel; --dwtLevel)
					{
						newCudaJob->mCompDcShiftJob.mDstX_off = (newCudaJob->mCompDcShiftJob.mDstX_off + 1) / 2;
						newCudaJob->mCompDcShiftJob.mDstY_off = (newCudaJob->mCompDcShiftJob.mDstY_off + 1) / 2;

					}
					//newCudaJob->mCompDcShiftJob.mDstX_off <<= curJob->mSizeShift;

					if(curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mCompBufMoved)
					{
						newCudaJob->mCompDcShiftJob.mSrcX_off = newCudaJob->mCompDcShiftJob.mDstX_off;
						newCudaJob->mCompDcShiftJob.mSrcY_off = newCudaJob->mCompDcShiftJob.mDstY_off;
					}
					else
					{
						newCudaJob->mCompDcShiftJob.mSrcX_off = curJob->mPicture->mTileinfo[tid].mOff.x;
						newCudaJob->mCompDcShiftJob.mSrcY_off =	curJob->mPicture->mTileinfo[tid].mOff.y;
					}
				}
				else
				{
					newCudaJob->mCompDcShiftJob.mHeight = curJob->mPicture->mTileinfo[tid].mSize.y;
					newCudaJob->mCompDcShiftJob.mWidth = curJob->mPicture->mTileinfo[tid].mSize.x; //***UU should this be in bytes ?
					newCudaJob->mCompDcShiftJob.mSrcX_off = curJob->mPicture->mTileinfo[tid].mOff.x;
					newCudaJob->mCompDcShiftJob.mSrcY_off =	curJob->mPicture->mTileinfo[tid].mOff.y;
					newCudaJob->mCompDcShiftJob.mDstX_off = curJob->mPicture->mTileinfo[tid].mOff.x;
					newCudaJob->mCompDcShiftJob.mDstY_off = curJob->mPicture->mTileinfo[tid].mOff.y;
				}

				if(tid == curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y - 1)
				{
					newCudaJob->mIsBlockJob = true; 
					newCudaJob->mReleaseMe = mCudaBlockSemaphore; 
				}
				ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
			}			
			WAIT_SEMAPHORE(mCudaBlockSemaphore);
			for (int tid = 0; tid < curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y; ++tid)
			{
				curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mDeShifted = 1;
			}

		}

		if (tileId == INVALID)
		{

			newCudaJob->type = CUDA_BUF_COPY_WITH_DIFF_PITCH;
			newCudaJob->mIsBlockJob = true; 
			newCudaJob->mReleaseMe = mCudaBlockSemaphore; 

			/*tempBufParams.width = curJob->mGpuTempBuf[componentId]->mWidth;
			tempBufParams.height = curJob->mGpuTempBuf[componentId]->mHeight;
			tempBufParams.pitch = curJob->mGpuTempBuf[componentId]->mPitch;
			curJob->mGpuTempBuf[componentId]->mWidth = curJob->mGpuOrigBuf[componentId]->mWidth;
			curJob->mGpuTempBuf[componentId]->mHeight = curJob->mGpuOrigBuf[componentId]->mHeight;
			curJob->mGpuTempBuf[componentId]->mPitch = curJob->mGpuOrigBuf[componentId]->mPitch; */

			newCudaJob->mCuda2DCopyDiffPitch.mDestPitch = newCudaJob->mCuda2DCopyDiffPitch.mSrcPitch
														= curJob->mGpuOrigBuf[componentId]->mPitch;
			newCudaJob->mCuda2DCopyDiffPitch.mSourceBuf	= curJob->mGpuTempBuf[componentId];
			newCudaJob->mCuda2DCopyDiffPitch.mDestBuf = mComponentResultBuf;
			newCudaJob->mCuda2DCopyDiffPitch.mHeight = curJob->mPicture->mProperties.mOutputSize.y;
			newCudaJob->mCuda2DCopyDiffPitch.mWidth =  curJob->mPicture->mProperties.mOutputSize.x * 4;
			sizeofBuf = newCudaJob->mCuda2DCopyDiffPitch.mHeight * newCudaJob->mCuda2DCopyDiffPitch.mWidth;
			newCudaJob->mCuda2DCopyDiffPitch.mSrcX_off = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mDestX_off = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mSrcY_off =  0;
			newCudaJob->mCuda2DCopyDiffPitch.mDestY_off = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mSrcNoPitch = false;
			newCudaJob->mCuda2DCopyDiffPitch.mDestNoPitch = true;
			newCudaJob->mCuda2DCopyDiffPitch.mAsync_id = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mDirection = CUDA_COPY_DEVICE_TO_HOST;
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);						
		}
		else
		{
			newCudaJob->type = CUDA_BUF_COPY_WITH_DIFF_PITCH;
			newCudaJob->mIsBlockJob = true; 
			newCudaJob->mReleaseMe = mCudaBlockSemaphore; 

			/*tempBufParams.width = curJob->mGpuTempBuf[componentId]->mWidth;
			tempBufParams.height = curJob->mGpuTempBuf[componentId]->mHeight;
			tempBufParams.pitch = curJob->mGpuTempBuf[componentId]->mPitch; 
			curJob->mGpuTempBuf[componentId]->mWidth = curJob->mGpuOrigBuf[componentId]->mWidth;
			curJob->mGpuTempBuf[componentId]->mHeight = curJob->mGpuOrigBuf[componentId]->mHeight;
			curJob->mGpuTempBuf[componentId]->mPitch = curJob->mGpuOrigBuf[componentId]->mPitch; */

			newCudaJob->mCuda2DCopyDiffPitch.mDestPitch = newCudaJob->mCuda2DCopyDiffPitch.mSrcPitch
														= curJob->mGpuOrigBuf[componentId]->mPitch;
			newCudaJob->mCuda2DCopyDiffPitch.mSourceBuf	= curJob->mGpuTempBuf[componentId];
			newCudaJob->mCuda2DCopyDiffPitch.mDestBuf = mComponentResultBuf;

			if (decodeTopLevel != INVALID && decodeTopLevel != curJob->mPicture->mProperties.mDwtLevel)
			{
				newCudaJob->mCuda2DCopyDiffPitch.mHeight = curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.y + 
					curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 -1].mSize.y;
				newCudaJob->mCuda2DCopyDiffPitch.mWidth = (curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.x + 
					curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 1].mSize.x) << curJob->mSizeShift;				

				newCudaJob->mCuda2DCopyDiffPitch.mSrcX_off = curJob->mPicture->mTileinfo[tileId].mOff.x;
				newCudaJob->mCuda2DCopyDiffPitch.mSrcY_off = curJob->mPicture->mTileinfo[tileId].mOff.y;

				for (int dwtLevel = curJob->mPicture->mProperties.mDwtLevel;  dwtLevel > decodeTopLevel; --dwtLevel)
				{
					newCudaJob->mCuda2DCopyDiffPitch.mSrcX_off = (newCudaJob->mCuda2DCopyDiffPitch.mSrcX_off + 1) / 2;
					newCudaJob->mCuda2DCopyDiffPitch.mSrcY_off = (newCudaJob->mCuda2DCopyDiffPitch.mSrcY_off + 1) / 2;
				}
			}
			else
			{
				newCudaJob->mCuda2DCopyDiffPitch.mHeight = curJob->mPicture->mTileinfo[tileId].mSize.y ;
				newCudaJob->mCuda2DCopyDiffPitch.mWidth = curJob->mPicture->mTileinfo[tileId].mSize.x ;
				newCudaJob->mCuda2DCopyDiffPitch.mSrcX_off = curJob->mPicture->mTileinfo[tileId].mOff.x;
				newCudaJob->mCuda2DCopyDiffPitch.mSrcY_off = curJob->mPicture->mTileinfo[tileId].mOff.y;
			}

			newCudaJob->mCuda2DCopyDiffPitch.mSrcX_off <<= curJob->mSizeShift;

			sizeofBuf = newCudaJob->mCuda2DCopyDiffPitch.mHeight * newCudaJob->mCuda2DCopyDiffPitch.mWidth;
			newCudaJob->mCuda2DCopyDiffPitch.mDestX_off = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mDestY_off = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mSrcNoPitch = false;
			newCudaJob->mCuda2DCopyDiffPitch.mDestNoPitch = true;
			newCudaJob->mCuda2DCopyDiffPitch.mAsync_id = 0;
			newCudaJob->mCuda2DCopyDiffPitch.mDirection = CUDA_COPY_DEVICE_TO_HOST;
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);				

		}
		WAIT_SEMAPHORE(mCudaBlockSemaphore);
		ret =  mComponentResultBuf;
	}

	return ret;
}


CudaBuf * CudaManager::GetOneComponent(int outputIndex, int & sizeofBuf, int tileId, int componentId, int decodeTopLevel)
{

	sizeofBuf = 0;
	P_CudaBuf ret = NULL;
	P_CudaJob curJob = &mCudajob[outputIndex];

	int tileNum = curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y;

	if (tileId <  tileNum && 
		tileId >= INVALID && 
		outputIndex < mEngineConfig->mOutputQueueLen && 
		componentId < curJob->mPicture->mProperties.mComponentNum)
	{
		DecodeJob	newJob;
		newJob.mJobType = JOB_CUDA_THREAD;
		CudaThreadJob	* newCudaJob = reinterpret_cast<CudaThreadJob	*>(newJob.mJobPara);

		if (mComponentResultBuf->mStatus == BUF_STATUS_UNINITIALIZED || mComponentResultBuf->mBuf == NULL || mComponentResultBuf->mSize < (unsigned int)(curJob->mPicture->mProperties.mOutputSize.x * curJob->mPicture->mProperties.mOutputSize.y * 4))
		{
			if (mComponentResultBuf->mBuf && mComponentResultBuf->mStatus != BUF_STATUS_UNINITIALIZED )
			{
				newCudaJob->type = FREE_CUDA_BUF;
				newCudaJob->mDeleteBuf = mComponentResultBuf;
				ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
			}

			newCudaJob->type = INIT_GPU_BUF;
			newCudaJob->mCudaInitBuf.mCudaBuf = mComponentResultBuf;
			newCudaJob->mCudaInitBuf.mType = HOST_BUF;
			newCudaJob->mCudaInitBuf.mSize = curJob->mPicture->mProperties.mOutputSize.x * curJob->mPicture->mProperties.mOutputSize.y * 4;
			newCudaJob->mCudaInitBuf.mWidth = curJob->mPicture->mProperties.mOutputSize.x * 4;
			newCudaJob->mCudaInitBuf.mHeight = curJob->mPicture->mProperties.mOutputSize.y;
			newCudaJob->mIsBlockJob = true;		// need to wait cuda thread return
			newCudaJob->mReleaseMe = mCudaBlockSemaphore;	
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB );	
			WAIT_SEMAPHORE(mCudaBlockSemaphore);
		}

		if (decodeTopLevel != INVALID && decodeTopLevel != curJob->mPicture->mProperties.mDwtLevel 
			&& curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y > 1 && tileId != 0)
		{

			if(!curJob->mPicture->mTileinfo[1].mCompInfo[componentId].mCompBufMoved)
			{
				BMI_ASSERT(decodeTopLevel < curJob->mPicture->mProperties.mDwtLevel);

				newCudaJob->type = CUDA_BUF_COPY;
				newCudaJob->mIsBlockJob = false;
				newCudaJob->mCuda2DCopy.mAsync_id = 0;			
				newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_DEVICE;
				newCudaJob->mCuda2DCopy.mDestNoPitch = newCudaJob->mCuda2DCopy.mSrcNoPitch = false;

				/*if (tileId != INVALID)
				{
				newCudaJob->mCuda2DCopy.mDestBuf	= curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mCompBuf;
				newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mCompBuf;
				}
				else
				{ */

				//We have to do this for all the tiles whether we take the component from that tile or not
				// This is to avoid writing on and thereby corrupting  the data of a tile that is not taken yet.
				for (int tid = 1; tid < curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y; ++tid)	
				{

					newCudaJob->mCuda2DCopy.mDestBuf	= curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mCompBuf;
					newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mCompBuf;
					newCudaJob->mCuda2DCopy.mHeight = curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.y + 
						curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 -1].mSize.y;
					newCudaJob->mCuda2DCopy.mWidth = (curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.x + 
						curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 1].mSize.x) << curJob->mSizeShift;


					newCudaJob->mCuda2DCopy.mSrcX_off = curJob->mPicture->mTileinfo[tid].mOff.x<<curJob->mSizeShift;
					newCudaJob->mCuda2DCopy.mSrcY_off =	curJob->mPicture->mTileinfo[tid].mOff.y;

					newCudaJob->mCuda2DCopy.mDestX_off = curJob->mPicture->mTileinfo[tid].mOff.x;
					newCudaJob->mCuda2DCopy.mDestY_off = curJob->mPicture->mTileinfo[tid].mOff.y;

					for (int dwtLevel = curJob->mPicture->mProperties.mDwtLevel;  dwtLevel > decodeTopLevel; --dwtLevel)
					{
						newCudaJob->mCuda2DCopy.mDestX_off = (newCudaJob->mCuda2DCopy.mDestX_off + 1) / 2;
						newCudaJob->mCuda2DCopy.mDestY_off = (newCudaJob->mCuda2DCopy.mDestY_off + 1) / 2;

					}

					newCudaJob->mCuda2DCopy.mDestX_off <<= curJob->mSizeShift;	

					if(tid == curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y - 1)
					{
						newCudaJob->mReleaseMe = mCudaBlockSemaphore; 
						newCudaJob->mIsBlockJob = true;
					}
					ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);
					if(tid == curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y - 1)
					{
						WAIT_SEMAPHORE(mCudaBlockSemaphore);						
					}
					curJob->mPicture->mTileinfo[tid].mCompInfo[componentId].mCompBufMoved = 1;
				}
			}

		}
		//}

		BMI_ASSERT(curJob->mSizeShift == 2);	// only support this size		

		if (tileId == INVALID)
		{

			newCudaJob->type = CUDA_BUF_COPY;
			newCudaJob->mIsBlockJob = true; 
			newCudaJob->mReleaseMe = mCudaBlockSemaphore; 
			newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mGpuOrigBuf[componentId];
			newCudaJob->mCuda2DCopy.mDestBuf = mComponentResultBuf;
			newCudaJob->mCuda2DCopy.mHeight = curJob->mPicture->mProperties.mOutputSize.y;
			newCudaJob->mCuda2DCopy.mWidth =  curJob->mPicture->mProperties.mOutputSize.x * 4;
			sizeofBuf = newCudaJob->mCuda2DCopy.mHeight * newCudaJob->mCuda2DCopy.mWidth;
			newCudaJob->mCuda2DCopy.mSrcX_off = 0;
			newCudaJob->mCuda2DCopy.mDestX_off = 0;
			newCudaJob->mCuda2DCopy.mSrcY_off =  0;
			newCudaJob->mCuda2DCopy.mDestY_off = 0;
			newCudaJob->mCuda2DCopy.mDestNoPitch = true;
			newCudaJob->mCuda2DCopy.mSrcNoPitch = false;
			newCudaJob->mCuda2DCopy.mAsync_id = 0;
			newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_HOST;
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);						
		}
		else
		{

			newCudaJob->type = CUDA_BUF_COPY;
			newCudaJob->mIsBlockJob = true; 
			newCudaJob->mReleaseMe = mCudaBlockSemaphore; 
			newCudaJob->mCuda2DCopy.mSourceBuf	= curJob->mGpuOrigBuf[componentId];
			newCudaJob->mCuda2DCopy.mDestBuf = mComponentResultBuf;

			if (decodeTopLevel != INVALID && decodeTopLevel != curJob->mPicture->mProperties.mDwtLevel)		
			{

				newCudaJob->mCuda2DCopy.mSrcX_off = curJob->mPicture->mTileinfo[tileId].mOff.x;
				newCudaJob->mCuda2DCopy.mSrcY_off =	curJob->mPicture->mTileinfo[tileId].mOff.y;

				for (int dwtLevel = curJob->mPicture->mProperties.mDwtLevel;  dwtLevel > decodeTopLevel; --dwtLevel)
				{
					newCudaJob->mCuda2DCopy.mSrcX_off = (newCudaJob->mCuda2DCopy.mSrcX_off + 1) / 2;
					newCudaJob->mCuda2DCopy.mSrcY_off = (newCudaJob->mCuda2DCopy.mSrcY_off + 1) / 2;

				}
			}

			

			newCudaJob->mCuda2DCopy.mSrcX_off <<= curJob->mSizeShift;

			newCudaJob->mCuda2DCopy.mHeight = curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.y + 
														curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 -1].mSize.y;
			//curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mCompBuf->mHeight; //***UUU change this for down sizing
			newCudaJob->mCuda2DCopy.mWidth =  (curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 - 2].mSize.x + 
														curJob->mPicture->mTileinfo[tileId].mCompInfo[componentId].mSubbandInfo[decodeTopLevel * 3 -1].mSize.x) << curJob->mSizeShift;
			sizeofBuf = newCudaJob->mCuda2DCopy.mHeight * newCudaJob->mCuda2DCopy.mWidth;
			newCudaJob->mCuda2DCopy.mDestX_off = 0;
			newCudaJob->mCuda2DCopy.mDestY_off = 0;
			newCudaJob->mCuda2DCopy.mDestNoPitch = true;
			newCudaJob->mCuda2DCopy.mSrcNoPitch = false;
			newCudaJob->mCuda2DCopy.mAsync_id = 0;
			newCudaJob->mCuda2DCopy.mDirection = CUDA_COPY_DEVICE_TO_HOST;
			ThreadSafeAccessCudaThreadJob(&mCudaManagerThreadInfo, ASYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);				

		}
		WAIT_SEMAPHORE(mCudaBlockSemaphore);
		ret =  mComponentResultBuf;
	}

	return ret;
}



THREAD_ENTRY WINAPI CudaManager::Thread_Cuda(PVOID pParam)
{

	P_ThreadInfo threadInfo = reinterpret_cast<P_ThreadInfo>(pParam);
	DecodeJob curJob;
	while(1)
	{

		WAIT_SEMAPHORE(CudaManager::GetInstance()->mCudaThreadSemaphore);

		STARTCLOCK(19);

		STARTCLOCK(35);	// waiting for mutex
		CudaThreadJobQueue curQueue;

		if (CudaManager::GetInstance()->ThreadSafeAccessCudaThreadJob(threadInfo, ASYNC_JOB_QUEUE, curJob))	// Check async job queue first
		{
			curQueue = ASYNC_JOB_QUEUE;
		}
		else if (CudaManager::GetInstance()->ThreadSafeAccessCudaThreadJob(threadInfo, SYNC_JOB_QUEUE, curJob))
		{
			curQueue = SYNC_JOB_QUEUE;
		}
		else	// no job? 
		{
			BMI_ASSERT_MSG(0, "Semaphore error!!\n");		// Semaphore error!!
			curQueue = UNKNOWN_JOB_QUEUE;
		}
		STOPCLOCK(35);
		// do the job we got:
		if (curQueue != UNKNOWN_JOB_QUEUE)
		{
			CudaThreadJob * doingJob = reinterpret_cast<CudaThreadJob *>(curJob.mJobPara);
			switch(doingJob->type)
			{
			case INIT_CUDA:
				STARTCLOCK(34);
				*doingJob->mCudaInit.mErrorCode = GpuInit(doingJob->mCudaInit.mGpuInfo);
				STOPCLOCK(34);
				PRINT(("%s \nwith %d processor %4.2fM memory running on %1.2f GHz\n", doingJob->mCudaInit.mGpuInfo->name, doingJob->mCudaInit.mGpuInfo->multiProcessorCount, doingJob->mCudaInit.mGpuInfo->memory/(1024.0 * 1024.0), doingJob->mCudaInit.mGpuInfo->clockRate/ (1024.0 * 1024.0) ));
				break;

			case INIT_GPU_BUF:
				STARTCLOCK(34);


				CudaManageFunc::InitGpuBuf(	doingJob->mCudaInitBuf.mCudaBuf, 
					doingJob->mCudaInitBuf.mType,
					doingJob->mCudaInitBuf.mSize,
					doingJob->mCudaInitBuf.mWidth,
					doingJob->mCudaInitBuf.mHeight);
				// 			doingJob->mCudaInitBuf.mCudaBuf->mStatus = BUF_STATUS_IDLE;
				STOPCLOCK(34);

				break;
			case	CUDA_BUF_COPY:
				{
					STARTCLOCK(30);

					StreamId	* P_asyncId = NULL;
					if (curQueue == ASYNC_JOB_QUEUE)
					{
						P_asyncId = &(doingJob->mCuda2DCopy.mAsync_id);	// todo
					}
					//  				DEBUG_PRINT(("=====> Cuda working on async copy %d\n", asyncId));
					// 				DEBUG_CONDITION_PRINT(doingJob->mCuda2DCopy.mDirection == CUDA_COPY_HOST_TO_DEVICE,
					// 					("  =====> Copy from (Host)%08x to (Gpu)%08x, loc:[%d,%d] size[%d:%d]\n",
					// 					(int)doingJob->mCuda2DCopy.mSourceBuf->mBuf,
					// 					(int)doingJob->mCuda2DCopy.mDestBuf->mBuf,
					// 					doingJob->mCuda2DCopy.mDestX_off,
					// 					doingJob->mCuda2DCopy.mDestY_off, 
					// 					doingJob->mCuda2DCopy.mWidth, 
					// 					doingJob->mCuda2DCopy.mHeight));
					// 				DEBUG_PAUSE(doingJob->mCuda2DCopy.mDestX_off == 3840 && doingJob->mCuda2DCopy.mDestY_off == 0);
					Gpu2DCopy(	doingJob->mCuda2DCopy.mSourceBuf, 
						doingJob->mCuda2DCopy.mDestBuf, 
						doingJob->mCuda2DCopy.mSrcX_off, 
						doingJob->mCuda2DCopy.mSrcY_off, 
						doingJob->mCuda2DCopy.mDestX_off,
						doingJob->mCuda2DCopy.mDestY_off,
						doingJob->mCuda2DCopy.mWidth, 
						doingJob->mCuda2DCopy.mHeight,
						P_asyncId,
						doingJob->mCuda2DCopy.mDirection,
						(doingJob->mCuda2DCopy.mSrcNoPitch ? 1 : 0),
						(doingJob->mCuda2DCopy.mDestNoPitch ? 1 : 0));
				}
				STOPCLOCK(30);
				break;

			case	CUDA_BUF_COPY_WITH_DIFF_PITCH:
				{
					STARTCLOCK(30);

					StreamId	* P_asyncId = NULL;
					if (curQueue == ASYNC_JOB_QUEUE)
					{
						P_asyncId = &(doingJob->mCuda2DCopyDiffPitch.mAsync_id);	// todo
					}

					Gpu2DCopyWithDiffPitch(	doingJob->mCuda2DCopyDiffPitch.mSourceBuf, 
						doingJob->mCuda2DCopyDiffPitch.mDestBuf, 
						doingJob->mCuda2DCopyDiffPitch.mSrcX_off, 
						doingJob->mCuda2DCopyDiffPitch.mSrcY_off, 
						doingJob->mCuda2DCopyDiffPitch.mDestX_off,
						doingJob->mCuda2DCopyDiffPitch.mDestY_off,
						doingJob->mCuda2DCopyDiffPitch.mSrcPitch,
						doingJob->mCuda2DCopyDiffPitch.mDestPitch,
						doingJob->mCuda2DCopyDiffPitch.mWidth, 
						doingJob->mCuda2DCopyDiffPitch.mHeight,
						P_asyncId,
						doingJob->mCuda2DCopyDiffPitch.mDirection,
						(doingJob->mCuda2DCopyDiffPitch.mSrcNoPitch ? 1 : 0),
						(doingJob->mCuda2DCopyDiffPitch.mDestNoPitch ? 1 : 0));
				}
				STOPCLOCK(30);
				break;

			case CUDA_BUF_SYNC:
				STARTCLOCK(31);
				BMI_ASSERT(doingJob->mCudaBufSync->mStatus == BUF_STATUS_SYNCING);
				GpuSync(&(doingJob->mCudaBufSync->mAsyncId));
				DEBUG_PRINT(("=====> Sync and Set buffer with async id %d to IDLE\n", doingJob->mCudaBufSync->mAsyncId));
				LOCK_MUTEX(CudaManager::GetInstance()->mCudaBufMutex[doingJob->mCudaBufSync->mOwnerThreadIndex]);
				doingJob->mCudaBufSync->mStatus = BUF_STATUS_IDLE;
				RELEASE_MUTEX(CudaManager::GetInstance()->mCudaBufMutex[doingJob->mCudaBufSync->mOwnerThreadIndex]);
				INCREASE_SEMAPHORE(CudaManager::GetInstance()->mCudaBufSemaphore[doingJob->mCudaBufSync->mOwnerThreadIndex]);
				STOPCLOCK(31);
				break;

			case CREATE_ASYNC_ID:
				STARTCLOCK(34);
				*doingJob->mP_AsyncId = CreateANewStream();
				STOPCLOCK(34);
				break;

			case CUDA_STREAM_SYNC:

				STARTCLOCK(33);
				GpuSync(&(CudaManager::GetInstance()->mAsyncStream[doingJob->mStreamSyncIndex].mAsyncId));
				CudaManager::GetInstance()->AccessIdleStream(doingJob->mStreamSyncIndex, false);
				DEBUG_PRINT(("=====> Syncing stream %d\n",CudaManager::GetInstance()->mAsyncStream[doingJob->mStreamSyncIndex].mAsyncId));
				STOPCLOCK(33);
				break;

			case FREE_CUDA_BUF:
				STARTCLOCK(34);
				if (doingJob->mDeleteBuf->mStatus != BUF_STATUS_UNINITIALIZED && doingJob->mDeleteBuf->mBuf)
				{
					FreeGpuBuf(doingJob->mDeleteBuf);
				}
				STOPCLOCK(34);
				break;

			case DESTORY_STREAM_ID:
				STARTCLOCK(34);
				DestroyStream(doingJob->mP_AsyncId);
				*doingJob->mP_AsyncId = 0;
				STOPCLOCK(34);
				break;

			case RELEASE_SEMAPHORE:
				// do nothing, the mIsBlock and mRelease Shoule be set in this case
				break;

#if !GPU_W9X7_FLOAT	
			case	DEQ_97_SUBBAND:
				STARTCLOCK(32);
				BMI_i97_dequantize_subband(	doingJob->mDeq97Job.mDeviceBuf,
					doingJob->mDeq97Job.mWorkingTile->mOff.x,
					doingJob->mDeq97Job.mWorkingTile->mOff.y,
					doingJob->mDeq97Job.mWorkingSubband,
					doingJob->mDeq97Job.mWordShift,
					doingJob->mDeq97Job.mAsync_id);
				STOPCLOCK(32);
				DEBUG_PRINT(("=====> Deq for subband %d asycn id %d\n", doingJob->mDeq97Job.mWorkingSubband->mId, doingJob->mDeq97Job.mAsync_id));
				break;

			case	DEQ_97_COMPONENT:
				STARTCLOCK(32);
				BMI_i97_dequantize_component(	doingJob->mDeq97Job.mDeviceBuf,
					doingJob->mDeq97Job.mWorkingTile,
					doingJob->mDeq97Job.mTopDeqLevel,
					doingJob->mDeq97Job.mComponentId,
					doingJob->mDeq97Job.mWordShift,
					doingJob->mDeq97Job.mAsync_id);
				STOPCLOCK(32);
				DEBUG_PRINT(("=====> Deq for Comp %d asycn id %d\n", doingJob->mDeq97Job.mComponentId, doingJob->mDeq97Job.mAsync_id));
#else
			case	DEQ_97_SUBBAND:
			case	DEQ_97_COMPONENT:
				BMI_ASSERT(0);
#endif
				break;

			case	DWT_RESOLUTION:
				{
					STARTCLOCK(32);
					// 			DEBUG_PAUSE(doingJob->mDwtComp_ResJob.mTileOff_x > 0);
					BMI_resolution_idwt(	doingJob->mDwtComp_ResJob.mTile,
						doingJob->mDwtComp_ResJob.mWorkingComp, 
						doingJob->mDwtComp_ResJob.mOriginalBuf,
						doingJob->mDwtComp_ResJob.mTempBuf,
						doingJob->mDwtComp_ResJob.mAsyncId,
						doingJob->mDwtComp_ResJob.mWordShift,
						doingJob->mDwtComp_ResJob.mMethod,
						doingJob->mDwtComp_ResJob.mResolutionLevel);

					STOPCLOCK(32);
					DEBUG_PRINT(("=====> DWT resolution %d with asycn id %d\n", doingJob->mDwtComp_ResJob.mResolutionLevel, doingJob->mDwtComp_ResJob.mAsyncId));
				}			break;

			case	DWT_COMPONENT:
				{
					STARTCLOCK(32);
					// 			DEBUG_PAUSE(doingJob->mDwtComp_ResJob.mTileOff_x > 0);
					BMI_comp_idwt(	doingJob->mDwtComp_ResJob.mTile,
						doingJob->mDwtComp_ResJob.mWorkingComp, 
						doingJob->mDwtComp_ResJob.mOriginalBuf,
						doingJob->mDwtComp_ResJob.mTempBuf,
						doingJob->mDwtComp_ResJob.mAsyncId,
						doingJob->mDwtComp_ResJob.mWordShift,
						doingJob->mDwtComp_ResJob.mMethod,
						doingJob->mDwtComp_ResJob.mTopLevel);

					STOPCLOCK(32);
					DEBUG_PRINT(("=====> DWT with asycn id %d\n", doingJob->mDwtComp_ResJob.mAsyncId));
				}

				break;

			case	MCT_53_TILE:
				{
					STARTCLOCK(32);
					BMI_tile_MCT(	doingJob->mMctIctJob.mTileInfo, 
						doingJob->mMctIctJob.mBitDepth, 
						doingJob->mMctIctJob.mWordShift, 
						doingJob->mMctIctJob.mDwtLevel,
						doingJob->mMctIctJob.mDecodeLevel,
						doingJob->mMctIctJob.mOutputFormat, 
						doingJob->mMctIctJob.mAsyncId);
					STOPCLOCK(32);
				}
				break;

			case	ICT_97_TILE:
				{
					STARTCLOCK(32);
					BMI_tile_ICT(	doingJob->mMctIctJob.mTileInfo, 
						doingJob->mMctIctJob.mBitDepth, 
						doingJob->mMctIctJob.mWordShift, 
						doingJob->mMctIctJob.mDwtLevel,
						doingJob->mMctIctJob.mDecodeLevel,
						doingJob->mMctIctJob.mOutputFormat,
						doingJob->mMctIctJob.mAsyncId);
					STOPCLOCK(32);
					DEBUG_PRINT(("=====> ICT with  asycn id %d\n", doingJob->mMctIctJob.mAsyncId));
				}
				break;

			case MERGE_TILE:
				{
					STARTCLOCK(32);
					BMI_tile_Merge(	doingJob->mTileMergeJob.mTileInfo, 
						doingJob->mTileMergeJob.mComponentNum,
						doingJob->mTileMergeJob.mDwtLevel,
						doingJob->mTileMergeJob.mDecodeLevel,
						doingJob->mTileMergeJob.mBitDepth, 
						doingJob->mTileMergeJob.mWordShift, 
						doingJob->mTileMergeJob.mCompBufPitchBytes,
						doingJob->mTileMergeJob.mOutputFormat, 
						doingJob->mTileMergeJob.mMethod,
						doingJob->mTileMergeJob.mAsyncId);

					STOPCLOCK(32);
					DEBUG_PRINT(("=====> Merge tile with  asycn id %d\n", doingJob->mMctIctJob.mAsyncId));
				}
				break;

			case CHECK_OUT_ONE_COMPONENT:
				STARTCLOCK(32);

				BMI_tile_One_Component(doingJob->mCheckOutComponentJob.mTileInfo,
					doingJob->mCheckOutComponentJob.mComponentId,
					doingJob->mCheckOutComponentJob.mMethod,
					(doingJob->mCheckOutComponentJob.mHasMCT ? 1 : 0),
					doingJob->mCheckOutComponentJob.mBitDepth,
					doingJob->mCheckOutComponentJob.mWordShift,
					doingJob->mCheckOutComponentJob.mDwtLevel,
					doingJob->mCheckOutComponentJob.mDecodeLevel,
					doingJob->mCheckOutComponentJob.mAsyncId);
				STOPCLOCK(32);
				DEBUG_PRINT(("=====> Check out component %d in tile with  asycn id %d\n",  doingJob->mCheckOutComponentJob.mComponentId,doingJob->mMctIctJob.mAsyncId));
				break;

			case GET_ONE_COMPONENT:
				STARTCLOCK(32); // ***UUU change this later
				BMI_Comp_DC_SHIFT(doingJob->mCompDcShiftJob.mCompBuf,
					doingJob->mCompDcShiftJob.mCompBufTemp,
					doingJob->mCompDcShiftJob.mMethod,
					doingJob->mCompDcShiftJob.mSrcX_off,
					doingJob->mCompDcShiftJob.mSrcY_off,
					doingJob->mCompDcShiftJob.mDstX_off,
					doingJob->mCompDcShiftJob.mDstY_off,
					doingJob->mCompDcShiftJob.mWidth,
					doingJob->mCompDcShiftJob.mHeight,
					doingJob->mCompDcShiftJob.mBitDepth,
					doingJob->mCompDcShiftJob.mPizelWordLength,
					doingJob->mCompDcShiftJob.mAsyncId);

				STOPCLOCK(32);
				break;


				// 		case	CUDA_COPY_OUT:
			case	SYNC_ALL:
				STARTCLOCK(31);
				// 			for (int OwnerThreadIndex = 0; OwnerThreadIndex < CudaManager::GetInstance()->mThreadNum; ++OwnerThreadIndex)
				// 			{
				// 				LOCK_MUTEX(CudaManager::GetInstance()->mCudaBufMutex[OwnerThreadIndex]);
				// 			}
				GpuSync(NULL);
				// 			for (int bufIndex = 0, OwnerThreadIndex = 0; OwnerThreadIndex < CudaManager::GetInstance()->mThreadNum; ++OwnerThreadIndex)
				// 			{
				// // 				LOCK_MUTEX(CudaManager::GetInstance()->mCudaBufMutex[OwnerThreadIndex]);
				// 				for (int bufN = 0; bufN < CudaManager::GetInstance()->mEachThreadBufferNum; ++bufN, ++ bufIndex)	// Set all uploading buf to idle
				// 				{
				// 					if (CudaManager::GetInstance()->mStripeBuf[bufIndex].mStatus == BUF_STATUS_UPLOADING)
				// 					{
				// 						CudaManager::GetInstance()->mStripeBuf[bufIndex].mStatus = BUF_STATUS_IDLE;
				// 						INCREASE_SEMAPHORE(CudaManager::GetInstance()->mCudaBufSemaphore[OwnerThreadIndex]);
				// 					}
				// 				}
				// // 				RELEASE_MUTEX(CudaManager::GetInstance()->mCudaBufMutex[OwnerThreadIndex]);
				// 
				// 			}
				// 			for (int OwnerThreadIndex = 0; OwnerThreadIndex < CudaManager::GetInstance()->mThreadNum; ++OwnerThreadIndex)
				// 			{
				// 				RELEASE_MUTEX(CudaManager::GetInstance()->mCudaBufMutex[OwnerThreadIndex]);
				// 			}
				STOPCLOCK(31);
				DEBUG_PRINT(("=====> Sync all unfinished jobs\n"));
				break;

			case	QUIT_CUDA:
				if (doingJob->mIsBlockJob)	// the calling thread is waiting
				{
					INCREASE_SEMAPHORE(doingJob->mReleaseMe);
				}
				return 0;
				break;

			default:
				BMI_ASSERT(0);
				break;
			}

			if (doingJob->mIsBlockJob)	// the calling thread is waiting
			{
				INCREASE_SEMAPHORE(doingJob->mReleaseMe);
			}

		}

		STOPCLOCK(19);
	}


	return 0;
}


void		CudaManager::AccessIdleStream(int &releaseindex , bool isGet/*= true*/, bool destroyMutex /*= false*/)
{
	static		int		nextIdleStreamIndex = 0;
	static bool mutexInited = false;
	static		MUTEX_HANDLE	streamMutex;


	if (destroyMutex == true)
	{
		if (mutexInited)
		{
			DESTROY_MUTEX(streamMutex);
			mutexInited = false;
		}
		return;
	}

	if (!mutexInited)
	{
		CREATE_MUTEX(streamMutex);
		mutexInited = true;
	}

	LOCK_MUTEX(streamMutex);
	if (isGet)
	{
		if (releaseindex == INVALID)		// get
		{
			static int step = 0;
			while (!mAsyncStream[nextIdleStreamIndex].mIsIdle)
			{
				BMI_ASSERT((++step) < DYNAMIC_ASYNC_ID_NUM);
				++nextIdleStreamIndex;
				if (nextIdleStreamIndex == DYNAMIC_ASYNC_ID_NUM)
				{
					nextIdleStreamIndex = 0;
				}
			}
			step = 0;

// 			DEBUG_PRINT(("Take stream %d\n", mAsyncStream[nextIdleStreamIndex].mAsyncId));

			mAsyncStream[nextIdleStreamIndex].mIsIdle = false;
			releaseindex = nextIdleStreamIndex;
			++nextIdleStreamIndex;
			if (nextIdleStreamIndex == DYNAMIC_ASYNC_ID_NUM)
			{
				nextIdleStreamIndex = 0;
			}
		}
	}
	else		// release
	{
		BMI_ASSERT(!mAsyncStream[releaseindex].mIsIdle);
// 		DEBUG_PRINT(("Release stream %d\n", mAsyncStream[releaseindex].mAsyncId));
		mAsyncStream[releaseindex].mIsIdle = true;
	}

	RELEASE_MUTEX(streamMutex);

}


bool	CudaManager::ThreadSafeAccessCudaThreadJob(const ThreadInfo * callingThread,CudaThreadJobQueue jobQueue, DecodeJob &job, JobQueueAccessMode mode /*= GET_A_JOB*/, int * para/* = NULL*/)
{

	MUTEX_HANDLE * mutex = (jobQueue == ASYNC_JOB_QUEUE) ? &mAsyncJobMutex : &mSyncJobMutex;
	JobQueue * curJobQueue  = (jobQueue == ASYNC_JOB_QUEUE) ? &mAsyncJobQueue : &mSyncJobQueue;

// 	DecodeJob * curJob;
// 	curJob->mJobType = 
	bool ret = true;

// 	if (mode == ATTACH_A_JOB)
// 	{
// 		LOCK_MUTEX(mutex);
// 	}
// 

	switch (mode)
	{
	case GET_A_JOB:
		if (curJobQueue->size())
		{
			job = *reinterpret_cast<DecodeJob *>(curJobQueue->begin());


// 			LOCK_MUTEX(*mutex);
			curJobQueue->pop_front();	
// 			RELEASE_MUTEX(*mutex);
// 
// 			{
// 				printf("%d queue Get_A_JOB head : tail : %d %d\n", (jobQueue == ASYNC_JOB_QUEUE ? 0 : 1), curJobQueue->get_head_index(), curJobQueue->get_tail_index());
// 				CudaThreadJob * doingJob = reinterpret_cast<CudaThreadJob *>(job.mJobPara);
// 				BMI_ASSERT(doingJob->type >= 0 && doingJob->type  < UNKNOWN_CUDA_THREAD_JOB);
// 			}	
		}
		else
		{
			ret = false;
		}
		break;
// 	case INSERT_A_JOB:
// 		BMI_ASSERT(0);	// not allow insert on the front
// 		curJobQueue->push_front(job);
// 		INCREASE_SEMAPHORE(mCudaThreadSemaphore);
// 		break;

	case ATTACH_A_JOB:
		{
			LOCK_MUTEX(*mutex);
			bool r =
				curJobQueue->push_back(reinterpret_cast<void *>(&job), job.mPictureIndex.mHandler);
			RELEASE_MUTEX(*mutex);
			BMI_ASSERT(r);
		}
		INCREASE_SEMAPHORE(mCudaThreadSemaphore);
		// 		DEBUG_PRINT(("%d lines, jobs %d + %d\n", __LINE__, mAsyncJobQueue.size() , mSyncJobQueue.size()));
		break;

	case GET_JOB_COUNT:
		BMI_ASSERT(para);
		*para = curJobQueue->size();
		break;

	default:
		BMI_ASSERT(0);		// todo
		break;

	}
	return ret;

}

void	CudaManager::ThreadSafeWaitForCudaThreadFinish(ThreadInfo  *callingThread)
{

	DecodeJob newJob;
	newJob.mJobType = JOB_CUDA_THREAD;
	CudaThreadJob * cudaJob = reinterpret_cast<CudaThreadJob *>(newJob.mJobPara);

// 	PRINT(("wait remaining cuda %d jobs finished\n", mSyncJobQueue.size() + mAsyncJobQueue.size()));
	cudaJob->type = RELEASE_SEMAPHORE;
	cudaJob->mIsBlockJob = true;
	cudaJob->mReleaseMe = mCudaThreadEndingSemaphore;

	ThreadSafeAccessCudaThreadJob(callingThread, SYNC_JOB_QUEUE, newJob, ATTACH_A_JOB);	// Add job as last one in sync queue
	
	WAIT_SEMAPHORE(mCudaThreadEndingSemaphore);

// 	PRINT(("Max job length od async / sync job queue is %d / %d\n", mAsyncJobQueue.mMaxSize, mSyncJobQueue.mMaxSize));

}
