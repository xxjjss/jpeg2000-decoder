#include "codecconcrete.h"
#include "cudaInterface.h"
#include "bmi_cpu_dwt.h"
#include "debug.h"
#include "testclock.h"

#if _MAC_OS
#include <sys/types.h>
#include <sys/sysctl.h>
#elif _LINUX
#include <sys/types.h>
//#include <sys/param.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif
#define SYNC_BETWEEN_COMPS	0

#define		T1_DECODE_POOL_SIZE		0xa00000		// 10* 1024 * 1024

const	int	DECODE_BUF_BLOCK_NUM	= 3;
const int clockId = 10;


namespace	BMI
{


	//////////////////////////////////////////////////////////////////////////
	//		Static member Init
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// constructor/destructor
	//////////////////////////////////////////////////////////////////////////
	DecoderConcrete::DecoderConcrete():
	mT1DecodeBuffer(NULL),
	mInput(NULL),
	mResult(NULL),
	mNextInputSlot(0),
	mNextResultSlot(0),
	mChildThread(NULL)
	{

		CREATE_SEMAPHORE(mDecodeThreadCallback, 0, 1);


		MUTEX_INIT(mStatusMutex,NULL);
		mT1DecodeBufSize[0] = mT1DecodeBufSize[0] = 0;
		mDecodePara.mempool[0] = mDecodePara.mempool[1] = NULL;
		mDecodePara.pool_size[0] = mDecodePara.pool_size[1] = 0;


	}

	DecoderConcrete::~DecoderConcrete()
	{

#if _MAC_OS || _LINUX
		if ( mChildThread)
		{
			DecodeJob	  newJob;
			newJob.mJobType = JOB_EXIT_THREAD;
			for (int i = 0; i < mEnginConfig.mThreadNum; ++i)
			{
				mJobQueue.Access((void *)&newJob, ATTACH_A_JOB);
			}
		}

#else
		if ( mChildThread)
		{
			for (int i = 0; i < mEnginConfig.mThreadNum; ++i)
			{
				END_THREAD(mChildThread[i].mHandle);
			}
		}
#endif
			
		DESTROY_SEMAPHORE(mDecodeThreadCallback);

		SAFE_DELETE_ARRAY(mInput);
		SAFE_DELETE_ARRAY(mResult);
		
		SAFE_FREE(mT1DecodeBuffer);


		if (mEnginConfig.mUsingGpu)
		{
 			CudaManager::Destory();
		}
		else
		{
			CpuDwtDecoder::Destory();
		}
		
		if (mEnginConfig.mThreadNum > 1)
		{
			DESTROY_MUTEX(mStatusMutex);

			if (!mEnginConfig.mUsingGpu) // CPU DWT
			{
				DESTROY_SEMAPHORE(mDwtSyncSemaphore);
			}

		}
		SAFE_DELETE_ARRAY(mChildThread);	
	}

	//////////////////////////////////////////////////////////////////////////
	// public member function
	//////////////////////////////////////////////////////////////////////////
	int DecoderConcrete::Init(const P_EnginConfig enginConfig /*= NULL*/)
	{
		static bool isInited = false;
		CREAT_CLOCK(100);
		if (!isInited)
		{

			if (enginConfig != NULL)
			{
				mEnginConfig = *enginConfig;
			}

			mEnginConfig.mInputQueueLen =  (mEnginConfig.mInputQueueLen <= 0) ? 1 : mEnginConfig.mInputQueueLen;
			mEnginConfig.mOutputQueueLen =  (mEnginConfig.mOutputQueueLen <= 0) ? 1 : mEnginConfig.mOutputQueueLen;

			mInput = new J2K_InputStream[mEnginConfig.mInputQueueLen];
			mResult = new DecodeResult[mEnginConfig.mOutputQueueLen];
	
// 			mJobQueue.init(MAX_JOBS_IN_QUEUE);	

#if _WINDOWS		
			SYSTEM_INFO	sysInfo;
			GetSystemInfo(&sysInfo);
			mEnginConfig.mCpuCoresNum = sysInfo.dwNumberOfProcessors;
#elif _LINUX
			//size_t result_size = sizeof(mEnginConfig.mCpuCoresNum);
			//if (sysctlbyname("hw.ncpu",&mEnginConfig.mCpuCoresNum,&result_size,NULL,0) != 0)
			//{
				mEnginConfig.mCpuCoresNum = 1;
			//}
#else
			size_t result_size = sizeof(mEnginConfig.mCpuCoresNum);
			if (sysctlbyname("hw.ncpu",&mEnginConfig.mCpuCoresNum,&result_size,NULL,0) != 0)
			{
				mEnginConfig.mCpuCoresNum = 1;
			}
#endif		
			if (mEnginConfig.mThreadNum <= 0)
			{
				// Set threadNums to the CPU cores Num
				mEnginConfig.mThreadNum = mEnginConfig.mCpuCoresNum;
			}


			mChildThread = new ThreadInfo[mEnginConfig.mThreadNum];
			mJobQueue.Init(0,0);
			for (int i = 0; i < mEnginConfig.mThreadNum; ++i)
			{
				mChildThread[i].mThreadIndex = i;
#if _MAC_OS
				int ret = 
					pthread_create(
					&mChildThread[i].mHandle,
					NULL,
					Thread_Decode,
					&mChildThread[i]);

				BMI_ASSERT(ret == 0);
				mChildThread[i].mThreadId = i+1;
#elif _LINUX
				int ret = 
					pthread_create(
					&mChildThread[i].mHandle,
					NULL,
					Thread_Decode,
					&mChildThread[i]);

				BMI_ASSERT(ret == 0);
				mChildThread[i].mThreadId = i+1;

#else
				mChildThread[i].mHandle = CreateThread(
					NULL,
					0,
					Thread_Decode,
					&mChildThread[i],			
					0,
					&mChildThread[i].mThreadId);
				BMI_ASSERT(mChildThread[i].mHandle);
#endif
			}


			if (mEnginConfig.mUsingGpu)
			{
				if (!CudaManager::IsCreated())
				{
					CudaManager::Create();
				}

				if (CudaManager::GetInstance()->Init(mEnginConfig) != GPU_AVAILABLE)
				{
					CudaManager::Destory();
					mEnginConfig.mUsingGpu = false;
					if (enginConfig)
					{
						enginConfig->mUsingGpu = false;
					}
				}			
			}

			if (!mEnginConfig.mUsingGpu)
			{
				mEnginConfig.mUsingGpu = false;
				CpuDwtDecoder::Create();
				CpuDwtDecoder::GetInstance()->Init(mEnginConfig);
			}

			if (mEnginConfig.mThreadNum > 1) // multi threads
			{
				CREATE_SEMAPHORE(mDwtSyncSemaphore, 0, mEnginConfig.mThreadNum*2);
			}
		}
		return	mEnginConfig.mThreadNum;
	}

	CodeStreamHandler DecoderConcrete::Decode(const void * codeStreamBuffer, int bufSize, double & time)
	{
// 		RESETCLOCK(99);

		int ret = Decode(codeStreamBuffer, bufSize);

		time = GETCLOCKTIME(0);
		RESETCLOCK(0);
//		PRINT(("hit %d cost %f\n",GETCLOCKHIT(99), GETCLOCKTIME(99)));
		return ret;
	}


	Status DecoderConcrete::PreDecode(const void * codeStreamBuffer, int bufSize,  CodeStreamProperties & properties)
	{
		BMI_ASSERT(mEnginConfig.mInputQueueLen > 0);
		BMI_ASSERT(codeStreamBuffer);


// 		int slot = mEnginConfig.mInputQueueLen;		// the last slot for predecode job
// 
// 		mInput[slot].mBufSize = bufSize;
// 		mInput[slot].mCodeStream = codeStreamBuffer;
// 		mInput[slot].mConfig = mCurrentJobConfig;
// 		mInput[slot].mHandler = INVALID;
// 		mInput[slot].mStatus = READY;

		Status ret = READY;

		if (CHECK_EXPIRED_DATE)
		{
			ret = ENGINE_EXPIRED;
		}
		else
		{

			mDecodePara.in_size = bufSize;
			mDecodePara.input = const_cast<void *>(codeStreamBuffer);
			mDecodePara.configuration = 0;
#if SOFTWARE_MULTI_THREAD
			if (BMI_dec_info(&mDecodePara, mEnginConfig.mThreadNum))		// this function return 0 when success
#else
			if (BMI_dec_info(&mDecodePara))		// this function return 0 when success
#endif
			{
				ret = DECODE_ERROR;
			}
			else
			{
				properties.mSize.x			= mDecodePara.width;
				properties.mSize.y			= mDecodePara.height;
				properties.mOutputSize		= properties.mSize;
				properties.mOutputSize		= properties.mSize;

				properties.mCodeBlockSize.y = mDecodePara.cb_height;
				properties.mCodeBlockSize.x = mDecodePara.cb_width;
				properties.mDwtLevel		= mDecodePara.dwt_level;
				properties.mTileSize.x		= mDecodePara.tile_width;
				properties.mTileSize.y		= mDecodePara.tile_height;
				properties.mTileNum.x		= (mDecodePara.width + mDecodePara.tile_width - 1) / mDecodePara.tile_width;
				properties.mTileNum.y		= (mDecodePara.height + mDecodePara.tile_height - 1) / mDecodePara.tile_height;
				properties.mLayer			= mDecodePara.layer;
				properties.mRoiShift		= mDecodePara.roi_shift;
				properties.mBitDepth		= mDecodePara.bitdepth;
				properties.mFormat			= static_cast<EncodeFormat>(mDecodePara.format);
				properties.mProgression		= mDecodePara.progression;
				properties.mThumbOutputSize = properties.mSize;
				properties.mComponentNum	= mDecodePara.num_components;

				for (int i = properties.mDwtLevel; i > 0; --i)
				{
					properties.mThumbOutputSize.x = (properties.mThumbOutputSize.x + 1)>>1;
					properties.mThumbOutputSize.y = (properties.mThumbOutputSize.y + 1)>>1;
				}
					// method??

			}

		}

		return ret;
	}

	CodeStreamHandler DecoderConcrete::Decode(const void * codeStreamBuffer, int bufSize)
	{
		BMI_ASSERT(mEnginConfig.mInputQueueLen > 0);
		BMI_ASSERT(codeStreamBuffer);


		int slot = mNextInputSlot;
		static	int  pictureCounter = 0;
			
		++pictureCounter;

		while (1)
		{
			if (mInput[slot].mStatus == PARSING || mInput[slot].mStatus == DECODING)	// this buffer is in used
			{
				++slot;
			}
			else
			{
				break;
			}

			if (slot == mEnginConfig.mInputQueueLen) 
			{
				slot = 0;
			}
			if (slot == mNextInputSlot)	// find a round
			{
				slot = INVALID;
				break;
			}
		}

		if (slot != INVALID)
		{
			mInput[slot].mBufSize = bufSize;
			mInput[slot].mCodeStream = codeStreamBuffer;
			mInput[slot].mConfig = mCurrentJobConfig;
			mInput[slot].mHandler = pictureCounter;
			mInput[slot].mStatus = PARSING;
			if (CHECK_EXPIRED_DATE)
			{
				mInput[slot].mStatus = ENGINE_EXPIRED;
				return pictureCounter;
			}

			if (ParseCodeStream(slot))
			{
				mInput[slot].mStatus = DECODING;
				STARTCLOCK(0);
				if ((mInput[slot].mStatus = PreDecode(slot)) == PARSING)
				{

					mInput[slot].mProperties.mCalculatedComponentNum = mInput[slot].mProperties.mComponentNum;
					if (mInput[slot].mConfig.mIgnoreComponents)
					{
						for (int igC =0; igC < mInput[slot].mProperties.mComponentNum; ++igC)
						{
							if ((mInput[slot].mConfig.mIgnoreComponents >> igC) & 0x01)
							{
								--mInput[slot].mProperties.mCalculatedComponentNum;
							}
						}
					}

					if (mInput[slot].mProperties.mCalculatedComponentNum)
					{
						
						stru_pipe * s_pipe = reinterpret_cast<stru_pipe *>(mDecodePara.mempool[0]);
						if (s_pipe->s_jp2file.s_jp2stream.s_cod->dwt97)
						{
							mInput[slot].mProperties.mMethod = Ckernels_W9X7;
						}
						else
						{
							mInput[slot].mProperties.mMethod = Ckernels_W5X3;
						}

										

						int outputSlot = DecoderConcrete::GetInstance()->GetNextAvailableResultSlot();
						BMI_ASSERT(outputSlot != INVALID);
						mInput[slot].mResultIndex = outputSlot;
						mResult[outputSlot].mHandler = mInput[slot].mHandler;
						mResult[outputSlot].mConfig = mInput[slot].mConfig;					
						mResult[outputSlot].mBufSize = INVALID;
						if (mEnginConfig.mUsingGpu)
						{
							CudaManager::GetInstance()->InitPicture(mInput[slot], slot,  s_pipe->s_opt, mDecodePara.size_opt); //[s_pipe->idx_opt_t1].qcc_quant_step[0][0]);
						}
						else
						{
							CpuDwtDecoder::GetInstance()->InitPicture(mInput[slot], slot,  s_pipe->s_opt, mDecodePara.size_opt);
						}

						mInput[slot].mProperties.mOutputFormat = mCurrentJobConfig.mOutputFormat;
						mResult[outputSlot].mProperties = mInput[slot].mProperties; //***UUU to make the changes made in InitPicture()to be reflected in output props (thumb size)

						MultiThread_decode( slot);

					}
					else
					{
						// all components ignored
						mInput[slot].mStatus = DECODE_IGNORED;
					}

				}
				STOPCLOCK(0);
			}
			else
			{
				mInput[slot].mStatus = PARSE_ERROR;
			}

			mNextInputSlot = slot + 1;
			if (mNextInputSlot == mEnginConfig.mInputQueueLen)
			{
				mNextInputSlot = 0;
			}
		}

		BMI_ASSERT(mJobQueue.Access(NULL, GET_JOB_COUNT) == 0);

		return pictureCounter;

	}

	Status	DecoderConcrete::CheckStatus(CodeStreamHandler handler) const
	{
		int index = GetIndex(handler);
		Status ret = INVALID_STATUS;
		if (index != INVALID)
		{
			ret = mInput[index].mStatus;
		}
		if (ret == READY)
		{
			if (mResult[mInput[index].mResultIndex].mHandler != handler)
			{
				ret = RELEASED;		// output data has been covered by another new code stream
			}
		}
		return ret;
	}

	bool	DecoderConcrete::CheckProperties(CodeStreamHandler handler, CodeStreamProperties & properties) const
	{
		if (CheckStatus(handler) == READY)	// properties avaliable
		{
			int index = GetIndex(handler);
			properties = mInput[index].mProperties;
			return true;
		}
		else	
		{
			return false;
		}
	}

	const void *	DecoderConcrete::GetResult(CodeStreamHandler handler, int & bufSize)
	{

		bufSize = 0;
		const void * decodedBuf = NULL;

		for (int i = 0; i < mEnginConfig.mOutputQueueLen; ++i)
		{
			if (mResult[i].mHandler == handler)
			{
				if (mResult[i].mBufSize == static_cast<unsigned int>(INVALID) )		// need to re-get the picture
				{
					if (mEnginConfig.mUsingGpu)
					{
						CudaBuf * pic = CudaManager::GetInstance()->GetResultBuf(i, mResult[i]);
						mResult[i].mBuf = (pic ? pic->mBuf : NULL);
					}
					else
					{
						mResult[i].mBuf = CpuDwtDecoder::GetInstance()->GetResultBuf(i, mResult[i].mBufSize);
					}
				}			

				decodedBuf = mResult[i].mBuf;
				bufSize = mResult[i].mBufSize;
				mResult[i].mLocked = true;
				break;
			}
		}


		return decodedBuf;
	}

	const void *	DecoderConcrete::GetThumbnail(CodeStreamHandler handler, int & bufSize)
	{

		bufSize = 0;
		const void * decodedBuf = NULL;

		for (int i = 0; i < mEnginConfig.mOutputQueueLen; ++i)
		{
			if (mResult[i].mHandler == handler)
			{
				if (mEnginConfig.mUsingGpu)
				{
					CudaBuf * pic = CudaManager::GetInstance()->GetThumbnail(i, bufSize);
					decodedBuf = pic->mBuf;;					
				}
				else
				{
					decodedBuf = CpuDwtDecoder::GetInstance()->GetThumbnail(i, bufSize);
				}
				break;

			}
		}
		return decodedBuf;
	}

	const float  *	DecoderConcrete::GetDecodedComponentFloat( CodeStreamHandler handler, int &bufSize, int componentId, int tileId)
	{
		bufSize = 0;
		const float * decodedBuf = NULL;

		for (int i = 0; i < mEnginConfig.mOutputQueueLen; ++i)
		{
			if (mResult[i].mHandler == handler)
			{
				if(mResult[i].mProperties.mMethod == Ckernels_W9X7)
				{
					if (mEnginConfig.mUsingGpu)
					{
 						CudaBuf * pic = CudaManager::GetInstance()->GetOneComponent(i, bufSize, tileId, componentId, mCurrentJobConfig.mDwtDecodeTopLevel);
 						decodedBuf = pic ? (float *)pic->mBuf : NULL;
					}
					else
					{
						decodedBuf = CpuDwtDecoder::GetInstance()->GetComponentFloat(handler, (unsigned int &)bufSize, i, componentId, tileId);
					}
				}
				break;
			}
		}
		return (decodedBuf);	
	}

	const int  *	DecoderConcrete::GetDecodedComponentInt( CodeStreamHandler handler, int &bufSize, int componentId, int tileId)
	{
		bufSize = 0;
		const int * decodedBuf = NULL;

		for (int i = 0; i < mEnginConfig.mOutputQueueLen; ++i)
		{
			if (mResult[i].mHandler == handler)
			{
				if(mResult[i].mProperties.mMethod == Ckernels_W5X3)
				{
					if (mEnginConfig.mUsingGpu)
					{
 						CudaBuf * pic = CudaManager::GetInstance()->GetOneComponentInt(i, bufSize, tileId, componentId, mCurrentJobConfig.mDwtDecodeTopLevel);
 						decodedBuf = pic ? (int *)pic->mBuf : NULL;
					}
					else
					{
						decodedBuf = CpuDwtDecoder::GetInstance()->GetComponentInt(handler, (unsigned int &)bufSize, i, componentId, tileId);
					}
				}

				break;
			}
		}
		return decodedBuf;
	}

	void	DecoderConcrete::Release(CodeStreamHandler handler)
	{
		for (int i = 0; i < mEnginConfig.mOutputQueueLen; ++i)
		{
			if (mResult[i].mHandler == handler)
			{
				mResult[i].mLocked = false;
				if (!mEnginConfig.mUsingGpu)	// using CPU DWT
				{
					CpuDwtDecoder::GetInstance()->ReleasePicture(i);
				}
				else
				{
 					CudaManager::GetInstance()->ReleaseCudaJob(i);
				}
				break;
			}
		}
	}


	void	DecoderConcrete::SetJobConfig(const DecodeJobConfig & config)
	{
		// todo : should we need to do anychecking before we changhe the setting
		mCurrentJobConfig = config;
	}

	void	DecoderConcrete::GetJobConfig(DecodeJobConfig & config, CodeStreamHandler handler /* = INVALID */) const
	{
		if (handler == INVALID|| GetIndex(handler) == INVALID)
		{
			config = mCurrentJobConfig;
		}
		else
		{
			config = mInput[GetIndex(handler)].mConfig;		// return config for certain picture
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// private member function
	//////////////////////////////////////////////////////////////////////////
	int DecoderConcrete::GetIndex(CodeStreamHandler handler) const
	{
		int index = INVALID;
		if (handler != INVALID)
		{
			for (int i = 0; i < mEnginConfig.mInputQueueLen; ++i)
			{
				if (mInput[i].mHandler == handler)
				{
					index = i;
					break;
				}
			}	
		}

		return index;
	}


	int	DecoderConcrete::GetNextAvailableResultSlot()
	{
		static int nextResultSlot = 0;
		int step = 0;
		int ret = INVALID;

		while (mResult[nextResultSlot].mLocked != false)
		{
			++step;
			if (step > mEnginConfig.mOutputQueueLen)
			{
				// all slot locked
				break;
			}
			++nextResultSlot;
			if (nextResultSlot == mEnginConfig.mOutputQueueLen)	
			{
				nextResultSlot = 0;
			}
		}
		if (step > mEnginConfig.mOutputQueueLen)
		{
			ret = INVALID;
		}
		else
		{
			ret = nextResultSlot;
			++nextResultSlot;
			if (nextResultSlot == mEnginConfig.mOutputQueueLen)	
			{
				nextResultSlot = 0;
			}		
		}
		return ret;

	}

	bool	DecoderConcrete::ParseCodeStream(int inputIndex)
	{
		mDecodePara.in_size = mInput[inputIndex].mBufSize;
		mDecodePara.input = const_cast<void *>(mInput[inputIndex].mCodeStream);
		mDecodePara.configuration = 0;
#if SOFTWARE_MULTI_THREAD
		if (BMI_dec_info(&mDecodePara, mEnginConfig.mThreadNum))		// this function return 0 when success
#else
		if (BMI_dec_info(&mDecodePara))		// this function return 0 when success
#endif
		{
			return false;
		}

		mInput[inputIndex].mProperties.mSize.x			= mDecodePara.width;
		mInput[inputIndex].mProperties.mSize.y			= mDecodePara.height;
		mInput[inputIndex].mProperties.mOutputSize		= mInput[inputIndex].mProperties.mSize;

		mInput[inputIndex].mProperties.mCodeBlockSize.y = mDecodePara.cb_height;
		mInput[inputIndex].mProperties.mCodeBlockSize.x = mDecodePara.cb_width;
		mInput[inputIndex].mProperties.mDwtLevel		= mDecodePara.dwt_level;
		mInput[inputIndex].mProperties.mTileSize.x		= mDecodePara.tile_width;
		mInput[inputIndex].mProperties.mTileSize.y		= mDecodePara.tile_height;
		mInput[inputIndex].mProperties.mTileNum.x		= (mDecodePara.width + mDecodePara.tile_width - 1) / mDecodePara.tile_width;
		mInput[inputIndex].mProperties.mTileNum.y		= (mDecodePara.height + mDecodePara.tile_height - 1) / mDecodePara.tile_height;
		mInput[inputIndex].mProperties.mLayer			= mDecodePara.layer;
		mInput[inputIndex].mProperties.mRoiShift		= mDecodePara.roi_shift;
		mInput[inputIndex].mProperties.mBitDepth		= mDecodePara.bitdepth;
		mInput[inputIndex].mProperties.mFormat			= static_cast<EncodeFormat>(mDecodePara.format);
		mInput[inputIndex].mProperties.mProgression		= mDecodePara.progression;

		if (mInput[inputIndex].mConfig.mDwtDecodeTopLevel != INVALID)
		{
			if (mInput[inputIndex].mConfig.mDwtDecodeTopLevel < mInput[inputIndex].mProperties.mDwtLevel)
			{
				mInput[inputIndex].mProperties.mOutputSize.x = (++mInput[inputIndex].mProperties.mOutputSize.x)>>(mInput[inputIndex].mProperties.mDwtLevel - mInput[inputIndex].mConfig.mDwtDecodeTopLevel);
				mInput[inputIndex].mProperties.mOutputSize.y = (++mInput[inputIndex].mProperties.mOutputSize.y)>>(mInput[inputIndex].mProperties.mDwtLevel - mInput[inputIndex].mConfig.mDwtDecodeTopLevel);
			}
			else
			{
				mInput[inputIndex].mConfig.mDwtDecodeTopLevel = mInput[inputIndex].mProperties.mDwtLevel;
			}
		}

		if (mT1DecodeBuffer == NULL || (mT1DecodeBufSize[0] + mT1DecodeBufSize[1]) < (mDecodePara.pool_size[0] + mDecodePara.pool_size[1]) )
		{
			SAFE_FREE(mT1DecodeBuffer);
			mT1DecodeBuffer = malloc(mDecodePara.pool_size[0] + mDecodePara.pool_size[1]);
			mT1DecodeBufSize[0] = mDecodePara.pool_size[0];
			mT1DecodeBufSize[1] = mDecodePara.pool_size[1];
		}
		mDecodePara.mempool[0] = mT1DecodeBuffer;
		mDecodePara.mempool[1] = reinterpret_cast<void *>(reinterpret_cast<uint_8 *>(mT1DecodeBuffer) + mDecodePara.pool_size[0]);

		return true;
	}

	Status	DecoderConcrete::PreDecode(int inputIndex)
	{
		// initialize memory
		Status ret = PARSING;
#if SOFTWARE_MULTI_THREAD
		ret = (BMI_dec_init(&mDecodePara, 1, mEnginConfig.mThreadNum) ? PARSE_ERROR :  PARSING);
#else
		ret = (BMI_dec_init(&mDecodePara, 1) ? false :  true);
#endif
		if (ret == PARSING)
		{
			ret = BMI_dec_thumbnail(&mDecodePara, NULL,  mDecodePara.dwt_level , 1);

			if (ret ==  PARSING)
			{
				stru_pipe * s_pipe = reinterpret_cast<stru_pipe *>(mDecodePara.mempool[0]);
				mInput[inputIndex].mProperties.mComponentNum	= static_cast<short>(s_pipe->s_opt[0].num_component);
			}
		}
		// Init Cuda
		// here we need to set the output buf in DecodePara

		return ret;
	}

	void	DecoderConcrete::MultiThread_decode( int inputIndex)
	{

		CreateJobQueue(inputIndex);

		if (mEnginConfig.mSyncMode)
		{
			WAIT_SEMAPHORE(mDecodeThreadCallback);
		}
		return;
	}


	void	DecoderConcrete::CreateJobQueue(int inputIndex)
	{


		stru_pipe * s_pipe = reinterpret_cast<stru_pipe *>(mDecodePara.mempool[0]);
		unsigned char * opt_memory = reinterpret_cast<unsigned char *>(s_pipe->s_opt);
		stru_dec_param *dec_param = &s_pipe->s_opt->tile_dec_param;
// 		stru_opt *s_opt = s_pipe->s_opt; // = &s_pipe->s_opt[s_pipe->idx_opt_t1];

		int minJobNum = mInput[inputIndex].mProperties.mTileNum.x * mInput[inputIndex].mProperties.mTileNum.y * mInput[inputIndex].mProperties.mComponentNum * ceil_ratio((mInput[inputIndex].mProperties.mTileSize.y >> mInput[inputIndex].mProperties.mDwtLevel), mInput[inputIndex].mProperties.mCodeBlockSize.y) * (3 << mInput[inputIndex].mProperties.mDwtLevel);
		minJobNum <<= 1;	// enlarge 
		if (!mEnginConfig.mSyncMode)
		{
			minJobNum *= mEnginConfig.mInputQueueLen;
		}
		minJobNum *= 2;
		mJobQueue.Init(minJobNum, sizeof(DecodeJob));

		int tot_entry;
		char cb_lut[256];

//		bool	started = false;

		const uint_8	* lut_ctx_lllh;
		const uint_8	* lut_ctx_hl;
		const uint_8	* lut_ctx_hh;
		DecodeJob	  newJob;

		dec_get_context_table(&lut_ctx_lllh, &lut_ctx_hl, &lut_ctx_hh);

		stru_subband *cur_band;

		unsigned char *lut_ctx;

		int i, j, m;

		int levMap[5] = {13, 10, 7, 4, 1};		

		int decodeDwtlevel = dec_param->dwt_level;
		if (mInput[inputIndex].mConfig.mDwtDecodeTopLevel != INVALID)
		{
			decodeDwtlevel = decodeDwtlevel > mInput[inputIndex].mConfig.mDwtDecodeTopLevel ? mInput[inputIndex].mConfig.mDwtDecodeTopLevel : decodeDwtlevel;
		}
	
		P_PictureInfo curPicture = reinterpret_cast<P_PictureInfo>(mInput[inputIndex].mCudaPictureInfo);

		int b_loop = 3* decodeDwtlevel;
		if (!mEnginConfig.mUsingGpu && mEnginConfig.mThreadNum > 1 && decodeDwtlevel)	// multithread and run DWT on CPU
		{
			b_loop -= 3;
		}

		for (int tileId = 0; tileId < dec_param->tile_num; ++ tileId)
		{
			stru_opt *s_opt = reinterpret_cast<stru_opt *>(opt_memory);
			stru_tile_component *cur_comp = s_opt->component; 
			dec_param = &(s_opt->tile_dec_param);

			for(int comp_num = 0; comp_num < mInput[inputIndex].mProperties.mComponentNum; ++comp_num, ++cur_comp)
			{
				if ((mInput[inputIndex].mConfig.mIgnoreComponents >> comp_num) & 0x01)
				{
					continue;	// this component ignored
				}
				int cbheight[MAX_DWT_LEVEL] = {0};

				// Compute table for component
				cbheight[0] = curPicture->mTileinfo[s_opt->tile_idx].mSize.y;	// dec_param->height;
				for (i = 1; i < dec_param->dwt_level; ++i)
					cbheight[i] = (cbheight[i-1] + 1) >> 1;

				i=0;
				tot_entry = 0;
				while(cbheight[dec_param->dwt_level-1] > 0) 
				{
					if     (!(i & 1))   j = 0;
					else if(!(i & 2))   j = 1;
					else if(!(i & 4))   j = 2;
					else if(!(i & 8))   j = 3;
					else                j = 4;

					i++;

					for(m = 0; m <= j; ++m)
					{
						if(cbheight[m] > 0) 
						{
							cbheight[m] -= 32;
							cb_lut[tot_entry++] = levMap[m];

							if(m == (dec_param->dwt_level - 1)) 
							{
								cb_lut[tot_entry++] = 0;
								break;
							}
						}
					}
				}

				for (int b_num = 0, cnt = 0, y_off_in_band = 0; b_num <= b_loop;)
				{

// 						int b_num = idx + band_num;
// 						if(idx)   b_num -= 3 * (5 - dec_param->dwt_level);

					cur_band = cur_comp->band + b_num;

					if(!cur_band->ncb_bw || !cur_band->ncb_bh) 
					{
						++ b_num; 
						continue;
					}
					if(cnt == cur_band->ncb_bh)      
					{
						++ b_num; 
						cnt = 0; 
						y_off_in_band = 0; 
						continue;
					}

					// context table selection
					lut_ctx = (unsigned char *)lut_ctx_lllh;
					if(cur_band->subband == BAND_HL)  lut_ctx = (unsigned char *)lut_ctx_hl;
					if(cur_band->subband == BAND_HH)  lut_ctx = (unsigned char *)lut_ctx_hh;


					DecodeT1Job	* newJobPara = reinterpret_cast<DecodeT1Job *> (newJob.mJobPara);
					BMI_ASSERT(sizeof(DecodeT1Job) <= MAX_JOB_PARAM_LENGTH);

					newJob.mJobType = JOB_DECODE_T1;
					newJob.mPictureIndex.mHandler = mInput[inputIndex].mHandler;
					newJob.mPictureIndex.mInputSlot = inputIndex;

					newJobPara->mTileId = tileId;	
					newJobPara->mComponentId = comp_num;
					newJobPara->mSubband_id = cur_band->subband;
					newJobPara->mCodeBlock = cur_band->codeblock + cnt * cur_band->ncb_bw;
					newJobPara->mCodeBlockNumInAStripe = cur_band->ncb_bw;
					newJobPara->mCurrentBandId = b_num;
					newJobPara->mYOffsetInBand = y_off_in_band;
					newJobPara->mCodeBlockStripeId = cnt;
					y_off_in_band += newJobPara->mCodeBlock->height;

					newJobPara->mContextTable = lut_ctx;

					mJobQueue.Access(((void *)&newJob), ATTACH_A_JOB);
// 					DEBUG_PRINT(("+++++Insert Job JOB_DECODE_T1\n"));


					++cnt;
				}

			}

			if ( (!mEnginConfig.mUsingGpu) && mEnginConfig.mThreadNum > 1 /*&&  (dec_param->tile_num == 1)*/)
			{
				stru_tile_component *cur_comp = s_opt->component; 
				dec_param = &(s_opt->tile_dec_param);

				for(int comp_num = 0; comp_num < mInput[inputIndex].mProperties.mComponentNum; ++comp_num, ++cur_comp)
				{
					if ((mInput[inputIndex].mConfig.mIgnoreComponents >> comp_num) & 0x01)
					{
						continue;	// this component ignored
					}

					int cbheight[MAX_DWT_LEVEL] = {0};

					// Compute table for component
					cbheight[0] = curPicture->mTileinfo[s_opt->tile_idx].mSize.y;	// dec_param->height;
					for (i = 1; i < dec_param->dwt_level; ++i)
						cbheight[i] = (cbheight[i-1] + 1) >> 1;

					i=0;
					tot_entry = 0;
					while(cbheight[dec_param->dwt_level-1] > 0) 
					{
						if     (!(i & 1))   j = 0;
						else if(!(i & 2))   j = 1;
						else if(!(i & 4))   j = 2;
						else if(!(i & 8))   j = 3;
						else                j = 4;

						i++;

						for(m = 0; m <= j; ++m)
						{
							if(cbheight[m] > 0) 
							{
								cbheight[m] -= 32;
								cb_lut[tot_entry++] = levMap[m];

								if(m == (dec_param->dwt_level - 1)) 
								{
									cb_lut[tot_entry++] = 0;
									break;
								}
							}
						}
					}

					for (int b_num = b_loop + 1, cnt = 0, y_off_in_band = 0; b_num <= b_loop + 3;)
					{

						// 						int b_num = idx + band_num;
						// 						if(idx)   b_num -= 3 * (5 - dec_param->dwt_level);

						cur_band = cur_comp->band + b_num;

						if(!cur_band->ncb_bw || !cur_band->ncb_bh) 
						{
							++ b_num; 
							continue;
						}
						if(cnt == cur_band->ncb_bh)      
						{
							++ b_num; 
							cnt = 0; 
							y_off_in_band = 0; 
							continue;
						}

						// context table selection
						lut_ctx = (unsigned char *)lut_ctx_lllh;
						if(cur_band->subband == BAND_HL)  lut_ctx = (unsigned char *)lut_ctx_hl;
						if(cur_band->subband == BAND_HH)  lut_ctx = (unsigned char *)lut_ctx_hh;


						DecodeT1Job	* newJobPara = reinterpret_cast<DecodeT1Job *> (newJob.mJobPara);
						BMI_ASSERT(sizeof(DecodeT1Job) <= MAX_JOB_PARAM_LENGTH);

						newJob.mJobType = JOB_DECODE_T1;
						newJob.mPictureIndex.mHandler = mInput[inputIndex].mHandler;
						newJob.mPictureIndex.mInputSlot = inputIndex;

						newJobPara->mTileId = tileId;	
						newJobPara->mComponentId = comp_num;
						newJobPara->mSubband_id = cur_band->subband;
						newJobPara->mCodeBlock = cur_band->codeblock + cnt * cur_band->ncb_bw;

						newJobPara->mCodeBlockNumInAStripe = cur_band->ncb_bw;
						newJobPara->mCurrentBandId = b_num;
						newJobPara->mYOffsetInBand = y_off_in_band;
						newJobPara->mCodeBlockStripeId = cnt;
						y_off_in_band += newJobPara->mCodeBlock->height;

						newJobPara->mContextTable = lut_ctx;


						mJobQueue.Access((void *)&newJob, ATTACH_A_JOB);
						// 						DEBUG_PRINT(("+++++Insert Job JOB_DECODE_T1\n"));

						++cnt;
					}

				}	

			}

 			if (!mEnginConfig.mUsingGpu && mEnginConfig.mThreadNum > 1)	// multithread and run DWT on CPU
			{
				newJob.mJobType = JOB_DECODE_DWT;
				DecodeDWTJob * dwtJob = reinterpret_cast<DecodeDWTJob *>(newJob.mJobPara);
				dwtJob->mSubType = SUBTYPE_SYNC_THREADS;
				dwtJob->mLastSyncJob = false;

				for (int i = 0 ; i < mEnginConfig.mThreadNum; ++i)
				{
					// insert sync flag before last VDWT
					mJobQueue.Access((void *)&newJob, ATTACH_A_JOB);

				}
				GetUndoDWTJobsAndDispatchToChildThread(tileId);

			}

			opt_memory += dec_param->size_opt;
		}		


		if(s_pipe->pipe_use) 
		{
			s_pipe->idx_opt_t1 = ++s_pipe->idx_opt_t1 % 3;
			s_pipe->idx_persis_t1 = !s_pipe->idx_persis_t1;
		}

		if (mEnginConfig.mThreadNum > 1)	// multithreads
		{
			newJob.mJobType = JOB_DECODE_DWT;
			DecodeDWTJob * dwtJob = reinterpret_cast<DecodeDWTJob *>(newJob.mJobPara);
			dwtJob->mSubType = SUBTYPE_SYNC_THREADS;
			dwtJob->mLastSyncJob = true;

			for (int i = 0 ; i < mEnginConfig.mThreadNum; ++i)
			{
				// insert sync flag
				mJobQueue.Access((void *)&newJob, ATTACH_A_JOB);
//				DEBUG_PRINT(("+++++Insert Job SUBTYPE_SYNC_THREADS\n"));

			}

		}
		else	// single thread
		{
			newJob.mJobType = JOB_ALL_DONE_FLAG;
			newJob.mPictureIndex.mHandler = mInput[inputIndex].mHandler;
			newJob.mPictureIndex.mInputSlot = inputIndex;
			int * slot = reinterpret_cast<int *>(newJob.mJobPara);
			slot[0] = inputIndex;
			mJobQueue.Access((void *)&newJob, ATTACH_A_JOB);
// 			DEBUG_PRINT(("+++++Insert Job JOB_ALL_DONE_FLAG\n"));

		}
		return;
	}


	void  removeJobForCurrentFrame(int index, void * job)
	{
		DecodeJob * curJob = reinterpret_cast<DecodeJob *>(job);
		if (curJob->mPictureIndex.mInputSlot == index)
		{
			curJob->mJobType = JOB_SKIPED;
		}
	}

	Status	DecoderConcrete::ThreadSafeChangeStatus(int inputIndex, Status newStatus)
	{

		if (mEnginConfig.mThreadNum > 1)
		{
			LOCK_MUTEX(mStatusMutex);
		}
		Status ret;

		if (newStatus == DECODE_ERROR && mInput[inputIndex].mStatus != DECODE_ERROR)
		{
			// first error
			mInput[inputIndex].mStatus = DECODE_ERROR;
			if (mEnginConfig.mThreadNum > 1)
			{
				RELEASE_MUTEX(mStatusMutex);
			}
			ret = DECODE_ERROR;
  			mJobQueue.Access(NULL, TRAVELSAL_DO_FUNC,inputIndex, true, removeJobForCurrentFrame);
			INCREASE_SEMAPHORE(mDecodeThreadCallback);
		}
		else if (newStatus == READY && mInput[inputIndex].mStatus != DECODE_ERROR)
		{
			// done flag
			mInput[inputIndex].mStatus = READY;
			if (mEnginConfig.mThreadNum > 1)
			{
				RELEASE_MUTEX(mStatusMutex);
			}
			ret = READY;
			INCREASE_SEMAPHORE(mDecodeThreadCallback);
		}
		else
		{
			ret =  mInput[inputIndex].mStatus;
			if (mEnginConfig.mThreadNum > 1)
			{
				RELEASE_MUTEX(mStatusMutex);
			}
		}

		return ret;

	}


	THREAD_ENTRY WINAPI DecoderConcrete::Thread_Decode(PVOID pParam)
	{

		BMI_ASSERT(DecoderConcrete::IsCreated());
		P_ThreadInfo threadInfo = reinterpret_cast<P_ThreadInfo>(pParam);
		DecodeJob curJob;

		Status retStatus;

		while (1)
		{

			DecoderConcrete::GetInstance()->mJobQueue.Access((void *)&curJob, GET_A_JOB);

			switch (curJob.mJobType)
			{
#if _MAC_OS || _LINUX
				case JOB_EXIT_THREAD:
					return 0;
					break;
#endif

				case JOB_SKIPED:
					retStatus = DECODING;
					break;

				case JOB_ALL_DONE_FLAG:		// last job, the picture decode has all done
					DecoderConcrete::GetInstance()->mJustFinishedPictureHandler = curJob.mPictureIndex.mHandler;
					retStatus = READY;
					break;

				case JOB_FETCH_RESULT:	// fetch the result data and fill into output_buf
					BMI_ASSERT(0); // todo
					break;

				case JOB_DECODE_T1:
					// convert void * mJobPara to t1 decoding parameters
					// call functions to decode T1
					{
						DecodeT1Job * t1Job = reinterpret_cast<DecodeT1Job *>(curJob.mJobPara);

						unsigned char * stripe_buf;
						int  buf_pitch_in_bytes;
						CudaBuf * buf = NULL;

						if (DecoderConcrete::GetInstance()->mEnginConfig.mUsingGpu)
						{
							STARTCLOCK(10 + threadInfo->mThreadIndex);
							buf = CudaManager::GetInstance()->GetStripeBuffer(curJob.mPictureIndex, threadInfo);
							STOPCLOCK(10 + threadInfo->mThreadIndex);

#if GPU_T1_TESTING
							{
								stru_code_block * cur_cb = t1Job->mCodeBlock;
								stru_pipe * s_pipe = reinterpret_cast<stru_pipe *>(DecoderConcrete::GetInstance()->mDecodePara.mempool[0]);
								stru_opt *s_opt = s_pipe->s_opt;
								unsigned char * opt_memory = (unsigned char *)s_pipe->s_opt;
								opt_memory += t1Job->mTileId * s_opt->tile_dec_param.size_opt;

								s_opt = (stru_opt *)opt_memory;

								stru_tile_component *cur_comp = &s_opt->component[t1Job->mComponentId];
								stru_subband * cur_band = cur_comp->band + t1Job->mCurrentBandId;
								stru_dec_param *dec_param = &s_opt->tile_dec_param;
								stru_scratch_buffer * scratch_buffer = &s_pipe->scratch_buffer;


								int shift_bit_plan;
								int ret = 0;

								shift_bit_plan = 31 - (s_opt->qcc_guard_bit[t1Job->mComponentId] - 1 +
									s_opt->qcc_quant_step[t1Job->mComponentId][t1Job->mCurrentBandId]);

								for(int j = 0; j < t1Job->mCodeBlockNumInAStripe; ++j, ++cur_cb)
								{

									/*IN*/int_32 *cb_ctx = scratch_buffer->cb_ctx + (scratch_buffer->size_ctx_buf * threadInfo->mThreadIndex) / sizeof(int_32);
									/*IN*/uint_8 * pass_buf = scratch_buffer->pass_buffer + scratch_buffer->size_pass_buf * threadInfo->mThreadIndex;
									/*IN*/unsigned char * lut_ctx = const_cast<unsigned char *>(t1Job->mContextTable);

									CudaManager::GetInstance()->DecodeCodeBlock((s_opt->conf_bypass != 0), cb_ctx, pass_buf, cur_band->numbps, cur_cb, lut_ctx);

									// Call cuda function to decode code block in GPU
									// Need to upload all data necessary data to GPU, save result data somewhere in GPU
									// For testing, we can download the result data and compare the result we got in function dec_cb_stripe
								}
							}	
#endif
							stripe_buf = reinterpret_cast<unsigned char *>(buf->mBuf);
							buf_pitch_in_bytes = buf->mPitch;
							buf->mBufContain.mSpcValue.mTileId = static_cast<int_16>(t1Job->mTileId);
							buf->mBufContain.mSpcValue.mComponentId = static_cast<int_8>(t1Job->mComponentId);
							int subbandP = t1Job->mCurrentBandId % 3;
							buf->mBufContain.mSpcValue.mRes_Subband = (unsigned char)(t1Job->mCurrentBandId ? ((((t1Job->mCurrentBandId - 1) / 3) << 4) | (subbandP ? (1 << subbandP) : 0x08)): 0x01);

						}
						else
						{
							stripe_buf = CpuDwtDecoder::GetInstance()->GetStripeBuffer(curJob.mPictureIndex, t1Job->mTileId,t1Job->mComponentId,t1Job->mCurrentBandId,t1Job->mYOffsetInBand, buf_pitch_in_bytes);
						}

// 						PictureIndex & pictureIndex, ThreadInfo * callingThread
// 						DEBUG_PRINT(("thread [%d] get T1 job %d cb %d lines",threadInfo->mThreadIndex, t1Job->mCodeBlockNumInAStripe, t1Job->mCodeBlock->height));
						STARTCLOCK(threadInfo->mThreadIndex + 1);
						int ret = dec_cb_stripe(	reinterpret_cast<stru_pipe *>(DecoderConcrete::GetInstance()->mDecodePara.mempool[0]), 
							t1Job->mTileId,
							t1Job->mComponentId,
							t1Job->mCurrentBandId,
							t1Job->mCodeBlock,
							t1Job->mCodeBlockNumInAStripe,
							const_cast<unsigned char *>(t1Job->mContextTable),
							threadInfo->mThreadIndex,
							stripe_buf,
							buf_pitch_in_bytes
							);
						STOPCLOCK(threadInfo->mThreadIndex + 1);


						if (ret != -1)	// T1 Deocde Success
						{
							if (DecoderConcrete::GetInstance()->mEnginConfig.mUsingGpu)
							{
								STARTCLOCK(20 + threadInfo->mThreadIndex);
									CudaManager::GetInstance()->UploadStripeBuf(buf, 
									DecoderConcrete::GetInstance()->GetJobConfig(curJob.mPictureIndex.mInputSlot), 
									curJob.mPictureIndex, 
									t1Job->mTileId, 
									t1Job->mComponentId, 
									t1Job->mCurrentBandId, 
									t1Job->mCodeBlock->height, 
									t1Job->mYOffsetInBand, 
									threadInfo);
								STOPCLOCK(20 + threadInfo->mThreadIndex);
							}
							else
							{
								STARTCLOCK(10 + threadInfo->mThreadIndex);
// 								void	UploadStripeBuf(const DecodeJobConfig & config, const PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int linesInserted, int y_offset_in_band,int codeBlockStripeId, int threadId);

								CpuDwtDecoder::GetInstance()->UploadStripeBuf(
									stripe_buf, 
									DecoderConcrete::GetInstance()->GetJobConfig(curJob.mPictureIndex.mInputSlot), 
									curJob.mPictureIndex, 
									t1Job->mTileId, 
									t1Job->mComponentId, 
									t1Job->mCurrentBandId, 
									t1Job->mCodeBlock->height,
									t1Job->mYOffsetInBand,
									t1Job->mCodeBlockStripeId,
									threadInfo->mThreadIndex);
								STOPCLOCK(10 + threadInfo->mThreadIndex);

							}

							retStatus = DECODING;
						}
						else
						{
							retStatus = DECODE_ERROR;
						}
					}
					break;

				case JOB_DECODE_DWT:
					{
						P_DecodeDWTJob job =  reinterpret_cast<P_DecodeDWTJob>(curJob.mJobPara);


						if (job->mSubType == SUBTYPE_SYNC_THREADS)
						{
							if (threadInfo->mThreadIndex != 0)
							{
								INCREASE_SEMAPHORE(DecoderConcrete::GetInstance()->mDwtSyncSemaphore);
								WAIT_SEMAPHORE(threadInfo->mThreadSemaphore);
								retStatus = DECODING;
							}
							else
							{
								STARTCLOCK(40);
								// main thread collect the Semaphore to sync the child thread jobs
								int n = DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum - 1;
// 								DEBUG_PRINT(( "Main thread [%d] got sync job,Wait for DWT semaphore %d left: \n  ",threadInfo->mThreadIndex , n));
								while (n--)
								{
									WAIT_SEMAPHORE(DecoderConcrete::GetInstance()->mDwtSyncSemaphore);
// 									DEBUG_PRINT(( " == %d == left \n  " , n));
								}

// 								PRINT_SCREEN(( "\n"));

								// main thread run to here the DWT jobs all done
								STOPCLOCK(40);
								// restart all others decode threads
								for (n = 1; n < DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum; ++n)
								{
									INCREASE_SEMAPHORE(DecoderConcrete::GetInstance()->mChildThread[n].mThreadSemaphore);
								}
								if (job->mLastSyncJob)
								{
									retStatus = READY;
								}
							}
						}
						else
						{								
							BMI_ASSERT(!DecoderConcrete::GetInstance()->mEnginConfig.mUsingGpu);

							// creat job by the threadId
							switch (job->mSubType)
							{

							case SUBTYPE_HDWT_53:
							case SUBTYPE_HDWT_97:
								{
									STARTCLOCK(20 + threadInfo->mThreadIndex);


									if (DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum == 1)
									{
										BMI_ASSERT(0);	// shouldn't run into here
										// single thread for this jobs
										if (job->mSubType == SUBTYPE_HDWT_53)
										{
											if (job->mWordLength == 4)
											{
												CpuDwtDecoder::GetInstance()->HDWT_R53_INT(
													reinterpret_cast<int_32 *>(job->mResult),
													reinterpret_cast<int_32 *>(job->mHDWT.mBand[0]),
													reinterpret_cast<int_32 *>(job->mHDWT.mBand[1]), 
													job->mHDWT.mLow_w, job->mHDWT.mHigh_w, job->mHDWT.mLow_h, 
													job->mHDWT.mXParity,
													job->mHDWT.mIsLTBandOrig);
												CpuDwtDecoder::GetInstance()->HDWT_R53_INT( 
													reinterpret_cast<int_32 *>(job->mResult + job->mResultPitch * job->mHDWT.mLow_h * job->mWordLength),
													reinterpret_cast<int_32 *>(job->mHDWT.mBand[2]), 
													reinterpret_cast<int_32 *>(job->mHDWT.mBand[3]), 
													job->mHDWT.mLow_w, job->mHDWT.mHigh_w, job->mHDWT.mHigh_h, 
													job->mHDWT.mXParity,
													true);
											}
											else
											{
												CpuDwtDecoder::GetInstance()->HDWT_R53_SHORT( 
													reinterpret_cast<int_16 *>(job->mResult),
													reinterpret_cast<int_16 *>(job->mHDWT.mBand[0]),
													reinterpret_cast<int_16 *>(job->mHDWT.mBand[1]), 
													job->mHDWT.mLow_w, job->mHDWT.mHigh_w, job->mHDWT.mLow_h, 
													job->mHDWT.mXParity,
													job->mHDWT.mIsLTBandOrig);
												CpuDwtDecoder::GetInstance()->HDWT_R53_SHORT( 
													reinterpret_cast<int_16 *>(job->mResult + job->mResultPitch * job->mHDWT.mLow_h * job->mWordLength),
													reinterpret_cast<int_16 *>(job->mHDWT.mBand[2]), 
													reinterpret_cast<int_16 *>(job->mHDWT.mBand[3]), 
													job->mHDWT.mLow_w, job->mHDWT.mHigh_w, job->mHDWT.mHigh_h, 
													job->mHDWT.mXParity,
													true);
											}
										}
										else
										{	
#if ENABLE_ASSEMBLY_OPTIMIZATION
											CpuDwtDecoder::GetInstance()->HDWT_R97_FLOAT_ASM(
												threadInfo->mThreadIndex,
#else
											CpuDwtDecoder::GetInstance()->HDWT_R97(
#endif
												reinterpret_cast<float *>(job->mResult),
												reinterpret_cast<float *>(job->mHDWT.mBand[0]), 
												reinterpret_cast<float *>(job->mHDWT.mBand[1]), 
												job->mHDWT.mLow_w, job->mHDWT.mHigh_w, job->mHDWT.mLow_h, 
												(job->mHDWT.mIsLTBandOrig ? job->mHDWT.mfAbsStepNorm[0] : 1.0f),
												job->mHDWT.mfAbsStepNorm[1],
												job->mHDWT.mXParity,
												job->mHDWT.mIsLTBandOrig);

#if ENABLE_ASSEMBLY_OPTIMIZATION
											CpuDwtDecoder::GetInstance()->HDWT_R97_FLOAT_ASM(
												threadInfo->mThreadIndex,
#else
											CpuDwtDecoder::GetInstance()->HDWT_R97(
#endif
												reinterpret_cast<float *>(job->mResult + job->mResultPitch * job->mHDWT.mLow_h * job->mWordLength),
												reinterpret_cast<float *>(job->mHDWT.mBand[2]),
												reinterpret_cast<float *>(job->mHDWT.mBand[3]), 
												job->mHDWT.mLow_w, job->mHDWT.mHigh_w, job->mHDWT.mHigh_h, 
												job->mHDWT.mfAbsStepNorm[2],
												job->mHDWT.mfAbsStepNorm[3],
												job->mHDWT.mXParity,
												true);
										}
									}
									else
									{
										int linesOffset, linesDecode;
										int resultLinesOffset;
										int halfThreadNum = DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum >> 1;
										bool isLowBandForVDWT = true;

										if (job->mSubJobId < halfThreadNum)
										{
											// in LL and LH bands
											linesDecode = (job->mHDWT.mLow_h + halfThreadNum - 1) / halfThreadNum;
											linesOffset = linesDecode * job->mSubJobId;
											if (linesOffset >= job->mHDWT.mLow_h)
											{
												linesDecode = 0;
											}
											else
											{
												resultLinesOffset = linesOffset;
												if (linesOffset + linesDecode > job->mHDWT.mLow_h)
												{
													linesDecode = job->mHDWT.mLow_h - linesDecode;
												}
												job->mHDWT.mBand[0] += linesOffset * job->mHDWT.mLow_w * job->mWordLength;
												job->mHDWT.mBand[1] += linesOffset * job->mHDWT.mHigh_w * job->mWordLength;
												if (job->mSubType == SUBTYPE_HDWT_97)
												{
#if ENABLE_ASSEMBLY_OPTIMIZATION
													CpuDwtDecoder::GetInstance()->HDWT_R97_FLOAT_ASM(
														threadInfo->mThreadIndex,
#else
													CpuDwtDecoder::GetInstance()->HDWT_R97(
#endif
														reinterpret_cast<float *>(job->mResult + job->mResultPitch * resultLinesOffset * job->mWordLength),
														reinterpret_cast<float *>(job->mHDWT.mBand[0]),
														reinterpret_cast<float *>(job->mHDWT.mBand[1]), 
														job->mHDWT.mLow_w, job->mHDWT.mHigh_w, linesDecode, 
														job->mHDWT.mfAbsStepNorm[0],
														job->mHDWT.mfAbsStepNorm[1],
														job->mHDWT.mXParity,
														(job->mHDWT.mIsLTBandOrig));
												}
												else	// W5X3
												{
													if (job->mWordLength == 4)
													{
														CpuDwtDecoder::GetInstance()->HDWT_R53_INT(
															reinterpret_cast<int_32 *>(job->mResult + job->mResultPitch * resultLinesOffset * job->mWordLength),
															reinterpret_cast<int_32 *>(job->mHDWT.mBand[0]),
															reinterpret_cast<int_32 *>(job->mHDWT.mBand[1]), 
															job->mHDWT.mLow_w, job->mHDWT.mHigh_w, linesDecode, 
															job->mHDWT.mXParity,
															job->mHDWT.mIsLTBandOrig);
													}
													else
													{
														CpuDwtDecoder::GetInstance()->HDWT_R53_SHORT(
															reinterpret_cast<int_16 *>(job->mResult + job->mResultPitch * resultLinesOffset * job->mWordLength),
															reinterpret_cast<int_16 *>(job->mHDWT.mBand[0]),
															reinterpret_cast<int_16 *>(job->mHDWT.mBand[1]), 
															job->mHDWT.mLow_w, job->mHDWT.mHigh_w, linesDecode, 
															job->mHDWT.mXParity,
															job->mHDWT.mIsLTBandOrig);

													}												}

											}
										}
										else
										{
											isLowBandForVDWT = false;
											job->mHDWT.mIsLTBandOrig = false;
											// in HL and HH bands
											job->mSubJobId -= halfThreadNum;
											halfThreadNum = DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum - halfThreadNum;

											linesDecode = (job->mHDWT.mHigh_h + halfThreadNum - 1) / halfThreadNum;
											linesOffset = linesDecode * job->mSubJobId;
											if (linesOffset >= job->mHDWT.mHigh_h)
											{
												linesDecode = 0;
											}
											else
											{
												resultLinesOffset = linesOffset + job->mHDWT.mLow_h;
												if (linesOffset + linesDecode > job->mHDWT.mHigh_h)
												{
													linesDecode = job->mHDWT.mHigh_h - linesDecode;
												}
												job->mHDWT.mBand[2] += linesOffset * job->mHDWT.mLow_w * job->mWordLength;
												job->mHDWT.mBand[3] += linesOffset * job->mHDWT.mHigh_w * job->mWordLength;
												if (job->mSubType == SUBTYPE_HDWT_97)
												{
#if ENABLE_ASSEMBLY_OPTIMIZATION
													CpuDwtDecoder::GetInstance()->HDWT_R97_FLOAT_ASM(
														threadInfo->mThreadIndex,
#else
													CpuDwtDecoder::GetInstance()->HDWT_R97(
#endif
														reinterpret_cast<float *>(job->mResult + job->mResultPitch * resultLinesOffset * job->mWordLength),
														reinterpret_cast<float *>(job->mHDWT.mBand[2]),
														reinterpret_cast<float *>(job->mHDWT.mBand[3]), 
														job->mHDWT.mLow_w, job->mHDWT.mHigh_w, linesDecode, 
														job->mHDWT.mfAbsStepNorm[2],
														job->mHDWT.mfAbsStepNorm[3],
														job->mHDWT.mXParity,
														true);
												}
												else	// W5X3
												{
													if (job->mWordLength == 4)
													{
														CpuDwtDecoder::GetInstance()->HDWT_R53_INT(
															reinterpret_cast<int_32 *>(job->mResult + job->mResultPitch * resultLinesOffset * job->mWordLength),
															reinterpret_cast<int_32 *>(job->mHDWT.mBand[2]),
															reinterpret_cast<int_32 *>(job->mHDWT.mBand[3]), 
															job->mHDWT.mLow_w, job->mHDWT.mHigh_w, linesDecode, 
															job->mHDWT.mXParity,
															true);
													}
													else
													{
														CpuDwtDecoder::GetInstance()->HDWT_R53_SHORT(
															reinterpret_cast<int_16 *>(job->mResult + job->mResultPitch * resultLinesOffset * job->mWordLength),
															reinterpret_cast<int_16 *>(job->mHDWT.mBand[0]),
															reinterpret_cast<int_16 *>(job->mHDWT.mBand[1]), 
															job->mHDWT.mLow_w, job->mHDWT.mHigh_w, linesDecode, 
															job->mHDWT.mXParity,
															true);

													}
												}

											}

										}
									}
	

								}
								STOPCLOCK(20 + threadInfo->mThreadIndex);

								retStatus = DECODING;
								break;

							case SUBTYPE_VDWT_53:
							case SUBTYPE_VDWT_97:
								{

									int Offset, decodeLine;
									decodeLine = (job->mVDWT.mLow_h + DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum - 1) / DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum;

									if (decodeLine < 3)
									{
										decodeLine = 3;
									}

									Offset = decodeLine * job->mSubJobId;

									if (Offset + decodeLine > job->mVDWT.mLow_h)
									{
										decodeLine = job->mVDWT.mLow_h - Offset;
										if (decodeLine < 3 && Offset != 0)	// only less than 6 line in this resolution
										{
											decodeLine = 0;	// should be done in precious level
										}
									}
									else if (Offset + decodeLine + 3 > job->mVDWT.mLow_h)
									{
										decodeLine = job->mVDWT.mLow_h - Offset;
									}
// 									PRINT_SCREEN(("Thread %d get V-DWT job, decode lines %4d - %4d  \n\t => Result %8x \tHDWTResult %8x\n", threadInfo->mThreadIndex, linesOffset, linesDecode+linesOffset-1, job->mResult, job->mVDWT.mHDWTResult));

									if (decodeLine )
									{
										STARTCLOCK(20 + threadInfo->mThreadIndex);

										if (SUBTYPE_VDWT_53 == job->mSubType)
										{
											if (job->mWordLength == 4)
											{
												CpuDwtDecoder::GetInstance()->VDWT_R53<int_32>(
													reinterpret_cast<int_32 *>(job->mResult),
													reinterpret_cast<int_32 *>(job->mVDWT.mHDWTResult_up),
													job->mVDWT.mLow_h,
													job->mVDWT.mHigh_h, 
													job->mVDWT.mWidth, 
													Offset, decodeLine,
													job->mVDWT.mYParity,
													threadInfo->mThreadIndex);
											}
											else
											{
												CpuDwtDecoder::GetInstance()->VDWT_R53<int_16>(
													reinterpret_cast<int_16 *>(job->mResult),
													reinterpret_cast<int_16 *>(job->mVDWT.mHDWTResult_up),
													job->mVDWT.mLow_h,
													job->mVDWT.mHigh_h, 
													job->mVDWT.mWidth, 
													Offset, decodeLine,
													job->mVDWT.mYParity,
													threadInfo->mThreadIndex);
											}
										}
										else
										{
											// SUBTYPE_VDWT_97
											CpuDwtDecoder::GetInstance()->VDWT_R97(
												job->mResult, 
												job->mVDWT.mHDWTResult_up, 
												job->mVDWT.mHDWTResult_down, 
												job->mVDWT.mLow_h, 
												job->mVDWT.mHigh_h, 
												job->mVDWT.mWidth, 
												Offset, 
												decodeLine,
												job->mVDWT.mYParity,
												threadInfo->mThreadIndex);
										}
										STOPCLOCK(20 + threadInfo->mThreadIndex);
									}

								}
								retStatus = DECODING;
								break;



							case SUBTYPE_53_MCT:
							case SUBTYPE_53_MERGE:
							case SUBTYPE_97_ICT:
							case SUBTYPE_97_MERGE:
								{
									int linesOffset, linesDecode;

									linesDecode = (job->mMCT.mHeight + DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum - 1) / DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum ;
									linesOffset = linesDecode * job->mSubJobId;
// 									DEBUG_PAUSE(threadInfo->mThreadIndex == DecoderConcrete::GetInstance()->mEnginConfig.mThreadNum - 1);
									if (linesDecode + linesOffset > job->mMCT.mHeight)
									{
										linesDecode = job->mMCT.mHeight - linesOffset;
									}
									if (linesDecode > 0)
									{
										int mctPitch = ALIGN_TO_SSE_DWORDS(job->mMCT.mWidth);

										switch (job->mSubType)
										{
										case SUBTYPE_97_ICT:

											// 											PRINT_SCREEN(("Thread %d get MCT job, decode lines  %4d - %4d\n ", threadInfo->mThreadIndex, linesOffset, linesOffset + linesDecode - 1));

											STARTCLOCK(30 + threadInfo->mThreadIndex);

											CpuDwtDecoder::GetInstance()->MCT_R97( job->mResult + job->mResultPitch * linesOffset * job->mMCT.outPizelSizeinBytes, 
												job->mMCT.mComp[0] + mctPitch * linesOffset * job->mWordLength,
												job->mMCT.mComp[1] + mctPitch * linesOffset * job->mWordLength, 
												job->mMCT.mComp[2] + mctPitch * linesOffset * job->mWordLength,
												job->mResultPitch,
												job->mMCT.mWidth, 
												linesDecode,
												job->mMCT.mBitDepth, 
												job->mMCT.mOutFormat,
												job->mMCT.mCheckoutComp, job->mWordLength);
											STOPCLOCK(30 + threadInfo->mThreadIndex);
											break;
										case SUBTYPE_97_MERGE:
											STARTCLOCK(30 + threadInfo->mThreadIndex);

											if (job->mWordLength == 4)
											{
#if INTEL_ASSEMBLY_32 || INTEL_ASSEMBLY_64
												CpuDwtDecoder::GetInstance()->MergeConponentsFloat_WIN_SSE(
#else 
												CpuDwtDecoder::GetInstance()->MergeComponents<float>( 
#endif
													(job->mResult + job->mResultPitch * linesOffset *job->mMCT.outPizelSizeinBytes), 
													job->mMCT.mComp[0] ? reinterpret_cast<float *>(job->mMCT.mComp[0] + mctPitch * linesOffset * job->mWordLength) : NULL,
													job->mMCT.mComp[1] ? reinterpret_cast<float *>(job->mMCT.mComp[1] + mctPitch * linesOffset * job->mWordLength) : NULL, 
													job->mMCT.mComp[2] ? reinterpret_cast<float *>(job->mMCT.mComp[2] + mctPitch * linesOffset * job->mWordLength) : NULL,
													job->mMCT.mComp[3] ? reinterpret_cast<float *>(job->mMCT.mComp[3] + mctPitch * linesOffset * job->mWordLength) : NULL,
													job->mResultPitch,
													job->mMCT.mWidth, 
													linesDecode,
													job->mMCT.mBitDepth, 
													job->mMCT.mOutFormat,
													job->mMCT.mCheckoutComp);
											}
											else
											{
												BMI_ASSERT(0);
											}
											STOPCLOCK(30 + threadInfo->mThreadIndex);
											break;

										case SUBTYPE_53_MCT:
											STARTCLOCK(30 + threadInfo->mThreadIndex);
											if (job->mWordLength == 4)	// int
											{
#if INTEL_ASSEMBLY_32
												CpuDwtDecoder::GetInstance()->RMCT_R53_INT32_ASM(
#else
												CpuDwtDecoder::GetInstance()->RRct_53<int_32>(
#endif
													reinterpret_cast<void *>(job->mResult + job->mResultPitch * linesOffset * job->mMCT.outPizelSizeinBytes), 
													reinterpret_cast<int_32 *>(job->mMCT.mComp[0] + mctPitch * linesOffset * job->mWordLength),
													reinterpret_cast<int_32 *>(job->mMCT.mComp[1] + mctPitch * linesOffset * job->mWordLength), 
													reinterpret_cast<int_32 *>(job->mMCT.mComp[2] + mctPitch * linesOffset * job->mWordLength),
													job->mResultPitch,
													job->mMCT.mWidth, 
													linesDecode,
													job->mMCT.mBitDepth, 
													job->mMCT.mOutFormat,
													job->mMCT.mCheckoutComp);						
											}
											else
											{
												BMI_ASSERT(job->mWordLength == 2);
												CpuDwtDecoder::GetInstance()->RRct_53<int_16>(
													reinterpret_cast<int *>(job->mResult + job->mResultPitch * linesOffset * job->mMCT.outPizelSizeinBytes), 
													reinterpret_cast<int_16 *>(job->mMCT.mComp[0] + mctPitch * linesOffset * job->mWordLength),
													reinterpret_cast<int_16 *>(job->mMCT.mComp[1] + mctPitch * linesOffset * job->mWordLength), 
													reinterpret_cast<int_16 *>(job->mMCT.mComp[2] + mctPitch * linesOffset * job->mWordLength),
													job->mResultPitch,
													job->mMCT.mWidth, 
													linesDecode,
													job->mMCT.mBitDepth, 
													job->mMCT.mOutFormat,
													job->mMCT.mCheckoutComp);	
											}

											STOPCLOCK(30 + threadInfo->mThreadIndex);
											break;
										case SUBTYPE_53_MERGE:
											STARTCLOCK(30 + threadInfo->mThreadIndex);

											if (job->mWordLength == 4)
											{
#if INTEL_ASSEMBLY_32 || INTEL_ASSEMBLY_64
												CpuDwtDecoder::GetInstance()->MergeConponentsInt32_WIN_SSE(
#else
												CpuDwtDecoder::GetInstance()->MergeComponents<int_32>(
#endif
													(job->mResult + job->mResultPitch * linesOffset * job->mMCT.outPizelSizeinBytes), 
													job->mMCT.mComp[0] ? reinterpret_cast<int_32 *>(job->mMCT.mComp[0] + mctPitch * linesOffset * job->mWordLength) : NULL,
													job->mMCT.mComp[1] ? reinterpret_cast<int_32 *>(job->mMCT.mComp[1] + mctPitch * linesOffset * job->mWordLength) : NULL, 
													job->mMCT.mComp[2] ? reinterpret_cast<int_32 *>(job->mMCT.mComp[2] + mctPitch * linesOffset * job->mWordLength) : NULL,
													job->mMCT.mComp[3] ? reinterpret_cast<int_32 *>(job->mMCT.mComp[3] + mctPitch * linesOffset * job->mWordLength) : NULL,
													job->mResultPitch,
													job->mMCT.mWidth, 
													linesDecode,
													job->mMCT.mBitDepth, 
													job->mMCT.mOutFormat,
													job->mMCT.mCheckoutComp);
											}
											else
											{
												BMI_ASSERT(0);
#pragma BMI_PRAGMA_TODO_MSG("to support 53 in short int")
											}
											STOPCLOCK(30 + threadInfo->mThreadIndex);
											break;	
										default:

											BMI_ASSERT(0);

										}

									}

								}
// 								CpuDwtDecoder::GetInstance()->MCT_R97(reinterpret_cast<int *>(job->mResult), job->mMCT.mComp[0], job->mMCT.mComp[1], job->mMCT.mComp[2], job->mResultPitch, job->mMctOffset, job->mMctWidth, job->mMctHeight, job->mBitDepth, job->mCheckOutComp, job->mWordLength);
								retStatus = DECODING;
								break;

							default:
								BMI_ASSERT(0);
								break;

							}


						}

					}
					break;

				default:
					BMI_ASSERT(0);
					break;
	
				}

			if (retStatus == READY)
			{
				if (DecoderConcrete::GetInstance()->mEnginConfig.mUsingGpu)
				{
					STARTCLOCK(29);
					CudaManager::GetInstance()->ThreadSafeWaitForCudaThreadFinish(threadInfo);
					STOPCLOCK(29);
				}
				DecoderConcrete::GetInstance()->ThreadSafeChangeStatus(curJob.mPictureIndex.mInputSlot, READY);
// 				RELEASE_SEMAPHORE(DecoderConcrete::GetInstance()->mDecodeThreadCallback);
			}
			else if (retStatus == DECODE_ERROR)
			{
				DecoderConcrete::GetInstance()->ThreadSafeChangeStatus(curJob.mPictureIndex.mInputSlot, DECODE_ERROR);
			}
		}	// while(1) get next job
	}

	bool DecoderConcrete::GetUndoDWTJobsAndDispatchToChildThread(int tileId /*= INVALID*/)
	{

		BMI_ASSERT(mEnginConfig.mThreadNum > 1 && (!mEnginConfig.mUsingGpu));

		bool ret = false;

		DecodeJob insertJob;
		insertJob.mJobType = JOB_DECODE_DWT;
		DecodeDWTJob * newJob = reinterpret_cast<DecodeDWTJob *>(insertJob.mJobPara);

		P_DecodeDWTJob undoDwtJob;

//		int inserted = 0;
		int jobNum = CpuDwtDecoder::GetInstance()->DwtUnfinished();
	
		while (jobNum) 
		{
			undoDwtJob = CpuDwtDecoder::GetInstance()->GetUnfinishedDwtJobs(tileId, insertJob.mPictureIndex.mHandler, insertJob.mPictureIndex.mInputSlot);
			if (!undoDwtJob)
			{
				break;
			}
			ret = true;
			--jobNum;
			*newJob = *undoDwtJob;

			if (undoDwtJob->mSubType == SUBTYPE_SYNC_THREADS)
			{	

				newJob->mSubType = SUBTYPE_SYNC_THREADS;
				newJob->mLastSyncJob = (jobNum == 0);
				for (int i = 0 ; i < mEnginConfig.mThreadNum; ++i)
				{
					// insert sync flag
					mJobQueue.Access((void *)&insertJob, ATTACH_A_JOB);
// 					DEBUG_PRINT(("+++++Insert Job SUBTYPE_SYNC_THREADS\n"));

				}
			}
			else
			{
				for (int i = 0; i < mEnginConfig.mThreadNum;	++i)
				{
					newJob->mSubJobId = i;
					mJobQueue.Access((void *)&insertJob, ATTACH_A_JOB);
				}
			}
		}

		return ret;
	}

}

