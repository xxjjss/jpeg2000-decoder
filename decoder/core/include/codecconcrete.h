#pragma once

#include "platform.h"
#include "singleton.h"

#include "codec_base.h"

#include "bmi_jp2dec.h"
#include "dec_stru.h"
#include "cudainterfacedatatype.h"
#include "jobqueue.h"

namespace	BMI
{	

	enum	JobType
	{
		JOB_SKIPED,
		JOB_DECODE_T1,
		JOB_DECODE_DWT,
		JOB_DECODE_MCT,
		JOB_FETCH_RESULT,
		JOB_ALL_DONE_FLAG,
		JOB_CUDA_THREAD,
#if _MAC_OS || _LINUX
		JOB_EXIT_THREAD,
#endif
		UNKNOWN_JOB
	};


	enum ThreadStatus
	{
		STATUS_DECODE_DONE,
		STATUS_DECODE_SYNCING,
		STATUS_NO_JOBS,
		STATUS_DECODE_ERROR,
		STATUS_DECODE_GOOD,
		STATUS_WORKING,
		STATUS_SUSPENDED,
		STATUS_UNKNOWN
	};

	typedef struct ThreadInfo_s
	{
		ThreadInfo_s():
		mHandle(NULL),
		mThreadId(-1),
		mThreadIndex(INVALID)
		{
			CREATE_SEMAPHORE(mThreadSemaphore,0, 1 );
		}

		~ThreadInfo_s()
		{
			DESTROY_SEMAPHORE(mThreadSemaphore);
		}

		THREAD_HANDLE		mHandle;
#if _WIN32_ || _WIN64_
		unsigned long		mThreadId;
#else
		unsigned int		mThreadId;
#endif
		int					mThreadIndex;
// 		ThreadStatus	mStatus;
		SEMAPHORE_HANDLE mThreadSemaphore;
	}ThreadInfo, *P_ThreadInfo;

#if _WIN64_
	const unsigned int MAX_JOB_PARAM_LENGTH	= 150;
#else
	const unsigned int MAX_JOB_PARAM_LENGTH	= 100;
#endif
	typedef	struct PictureIndex_s
	{
		int mHandler;
		int	mInputSlot;
	}PictureIndex, * P_PictureIndex;

	typedef	struct CodecJob_s
	{
		JobType mJobType;
		PictureIndex		mPictureIndex;
		uint_8	mJobPara[MAX_JOB_PARAM_LENGTH];
	}DecodeJob, *P_DecodeJob, EncodeJob, *P_EncodeJob; 




	typedef struct J2K_InputStream_s
	{
		J2K_InputStream_s(): 
		mHandler(INVALID),
		mCodeStream(NULL),
		mBufSize(0),
		mStatus(INVALID_STATUS),
		mResultIndex(INVALID)
		{
			// 				memset(reinterpret_cast<void *>(&mCudaPictureInfo), 0, sizeof(PictureInfo));
		}

		CodeStreamHandler		mHandler;
		const void *			mCodeStream;
		int						mBufSize;
		Status					mStatus;
		int						mResultIndex;
		CodeStreamProperties	mProperties;
		DecodeJobConfig			mConfig;

		union
		{
			void *				mDwtPictureInfo;
			void *				mCudaPictureInfo;
		};

	}J2K_InputStream, *P_J2K_InputStream;


	enum	DwtMctSubType
	{
		SUBTYPE_HDWT_97,
		SUBTYPE_HDWT_53,
		SUBTYPE_VDWT_97,
		SUBTYPE_VDWT_53,
		SUBTYPE_97_ICT,
		SUBTYPE_53_MCT,
		SUBTYPE_97_MERGE,
		SUBTYPE_53_MERGE,
		SUBTYPE_SYNC_THREADS,
		SUBTYPE_UNKNOWN
	};

	class DecoderConcrete : public Decoder, public Singleton<DecoderConcrete>
	{
	public:
		DecoderConcrete();
		virtual	~DecoderConcrete();

		//////////////////////////////////////////////////////////////////////////
		//		Function derived from Decoder
		//////////////////////////////////////////////////////////////////////////
		virtual	int Init(const P_EnginConfig enginConfig = NULL);

		virtual	CodeStreamHandler Decode(const void * codeStreamBuffer, int bufSize);
		
		virtual CodeStreamHandler Decode(const void * codeStreamBuffer, int bufSize, double & time);
		virtual Status PreDecode(const void * codeStreamBuffer, int bufSize,  CodeStreamProperties & properties);


		virtual	Status	CheckStatus(CodeStreamHandler handler) const;
		virtual	bool	CheckProperties(CodeStreamHandler handler, CodeStreamProperties & properties) const;
		virtual	const void *	GetResult(CodeStreamHandler handler, int & bufSize);
		virtual	const void *	GetThumbnail(CodeStreamHandler handler, int & bufSize);

		/***************************************************************************************
		Function name : GetDecodedComponentInt
		Description:	User can call this function get the decoded data for a certain components
		IN:
		handler :	decode job handler
		componentId : the component ID want to fetch
		tileId	:	the tile ID want to fetch, for single tile picture, this parameter will be ignore, for multi tiles picture, 
					if tile id set to INVALID, will return the component data for whole picture
		OUT:
		bufSize : The size count in byte for the result buffer
		RETURN:		
			The buffer filled with decoded component data (integer), each pixel will occupy 32 bits. if W9X7, will return NULL
		***************************************************************************************/
		virtual	const int  *	GetDecodedComponentInt( CodeStreamHandler handler, int &bufSize, int componentId, int tileId = INVALID);

		/***************************************************************************************
		Function name : GetDecodedComponentFloat
		Description:	User can call this function get the decoded data for a certain components
		IN:
		handler :	decode job handler
		componentId : the component ID want to fetch
		tileId	:	the tile ID want to fetch, for single tile picture, this parameter will be ignore, for multi tiles picture, 
					if tile id set to INVALID, will return the component data for whole picture
		OUT:
		bufSize : The size count in byte for the result buffer
		RETURN:		
			The buffer filled with decoded component data (float ), each pixel will occupy 32 bits. if W5X3, will return NULL
		***************************************************************************************/
		virtual	const float  *	GetDecodedComponentFloat( CodeStreamHandler handler, int &bufSize, int componentId, int tileId = INVALID);

		virtual void	Release(CodeStreamHandler handler);

		virtual void	SetJobConfig(const DecodeJobConfig & config);
		virtual	void	GetJobConfig(DecodeJobConfig & config, CodeStreamHandler handler = INVALID) const;
		virtual const	EnginConfig * GetEngineConfig() const {return &mEnginConfig; }

// 		ThreadStatus	ThreadSafeChangeStatus(P_ThreadInfo threadInfo, ThreadStatus newStatus, bool newFrame = false);


		void GetVersion(int & mainVersion, int & subvVrsion, int &branchVersion, int &year, int &month, int &date) 
		{ 
			branchVersion = ENGINE_VERSION % 100;
			subvVrsion = (ENGINE_VERSION / 100 ) % 100;
			mainVersion = (ENGINE_VERSION / 10000 ) % 100;

			date = RELEASE_DATE % 100;
			month = (RELEASE_DATE / 100 ) % 100;
			year = (RELEASE_DATE / 10000 ) % 100 + 2000;

		}

		typedef	struct DecodeResult_s
		{
			DecodeResult_s():
			mBuf(NULL),
			mBufSize(INVALID),
			mBufComponent(INVALID),
			mLocked(false)
			{}

			const void				* mBuf;
			unsigned int			mBufSize;
			int						mBufComponent;
			CodeStreamHandler		mHandler;
			bool					mLocked;
			CodeStreamProperties	mProperties;
			DecodeJobConfig			mConfig;
		}DecodeResult, *P_DecodeResult;

		
		typedef struct  DecodeHDWTInput_s
		{
			unsigned char * mBand[4];
			int mLow_w;
			int mHigh_w;
			int mLow_h;
			int mHigh_h;

			float mfAbsStepNorm[4];
			bool  mIsLTBandOrig;
			uint_8 mXParity;
		}DecodeHDWTInput, * P_DecodeHDWTInput;

		typedef struct  DecodeVDWTInput_s
		{
			unsigned char * mHDWTResult_up;
			unsigned char * mHDWTResult_down;
			int mLow_h;
			int mHigh_h;
			int mWidth;
			uint_8 mYParity;
		}DecodeVDWTInput, * P_DecodeHVWTInput;

		typedef struct  DecodeMCTInput_s
		{
			unsigned char * mComp[4];
			int mWidth;
			int mHeight;
			int mBitDepth;
			int mCheckoutComp;
			OutputFormat	mOutFormat;
			int		outPizelSizeinBytes;
		}DecodeMCTInput, * P_DecodeMCTnput;

		typedef	struct	DecodeDwtMctJob_s
		{
			DwtMctSubType		mSubType;
			int		mWordLength;
			unsigned char *	mResult;
			int		mResultPitch;
 			int		mSubJobId;
			int		mTileid;

			union
			{
				DecodeHDWTInput		mHDWT;
				DecodeVDWTInput		mVDWT;
				DecodeMCTInput		mMCT;
				bool	mLastSyncJob;
			};

		}DecodeDWTJob, * P_DecodeDWTJob;

	private:

		//////////////////////////////////////////////////////////////////////////
		//	Private Function
		//////////////////////////////////////////////////////////////////////////
		int GetIndex(CodeStreamHandler handler) const;
		int	GetNextAvailableResultSlot();

		const DecodeJobConfig & GetJobConfig(int inputIndex) {return mInput[inputIndex].mConfig;}

		// Parse the input buffer, fill the properties
		// If unknown format or any other error return false
		bool	ParseCodeStream(int inputIndex);

		// process decoding has to be run in single thread, such as T2
		// If any error return false
		Status	PreDecode(int inputIndex);

		Status	ThreadSafeChangeStatus(int inputIndex, Status rewStatus);

		void	MultiThread_decode(int  inputIndex);
// 		void	OnDecodeMsg(WPARAM wParam, LPARAM lParam);

		void	CreateJobQueue( int inputIndex);
// 		void	StartChildThreads();
// 		int		GetChildThreadIndex(unsigned long threadId);

		bool GetUndoDWTJobsAndDispatchToChildThread(int tileId = INVALID);

		typedef	struct	DecodeT1Job_s
		{
			int mTileId;
			int	mComponentId;
			int mSubband_id;
			int mYOffsetInBand;
			int mCodeBlockStripeId;

// 			bool mDeq97;

			stru_code_block *mCodeBlock;
			short			mCurrentBandId;	
			const unsigned char * mContextTable;
			int				mCodeBlockNumInAStripe;		

		}DecodeT1Job, *P_DecodeT1Job;


		typedef	enum	CudaJobType_e
		{
			CUDA_UPLOAD,
			CUDA_DOWNLOAD,
			CUDA_DWT,
			CUDA_MCT_ICT,
			CUDA_JOB_UNKNOWN
		}CudaJobType;

		typedef	struct UploadPara_s
		{	
			CudaBuf * mHostBuf;
			int		mTileId;
			int		mCompId;
			int		mBandId;
			int		mLinesAvailable;
		}UploadPara;		
		
		typedef	struct	CudaAccessJob_s
		{
			CudaJobType mType;
			union
			{
				UploadPara	mUpload;
			};
			
		}CudaAccessJob, P_CudaAccessJob;


		static	THREAD_ENTRY WINAPI Thread_Decode(PVOID pParam);


		//////////////////////////////////////////////////////////////////////////
		//	Private data
		//////////////////////////////////////////////////////////////////////////


		stru_dec_param		mDecodePara;
		void				* mT1DecodeBuffer;
		int					mT1DecodeBufSize[2];
		int					mJustFinishedPictureHandler;


		EnginConfig			mEnginConfig;
		volatile P_J2K_InputStream	mInput;
		volatile P_DecodeResult	mResult;

		DecodeJobConfig	mCurrentJobConfig;
		int				mNextInputSlot;
		int				mNextResultSlot;


		P_ThreadInfo		mChildThread;

		SEMAPHORE_HANDLE			mDecodeThreadCallback;
		SemaphoreControledJobQueue	mJobQueue;

		MUTEX_HANDLE			mStatusMutex;

		SEMAPHORE_HANDLE	mDwtSyncSemaphore;

	};	


	class EncoderConcrete : public Encoder, public Singleton<EncoderConcrete>
	{
	public:
		EncoderConcrete();
		virtual	~EncoderConcrete();

		//////////////////////////////////////////////////////////////////////////
		//		Function derived from Encoder
		//////////////////////////////////////////////////////////////////////////

		virtual	int Init(const P_EnginConfig enginConfig = NULL);

		virtual	CodeStreamHandler Encode(const void * inputBuffer, int bufSize, InputProperties	* inputInfor = NULL);
		virtual CodeStreamHandler Encode(const void * inputBuffer, int bufSize, double & time, InputProperties	* inputInfor = NULL);

		void GetVersion(int & mainVersion, int & subvVrsion, int &branchVersion, int &year, int &month, int &date) 
		{ 
			branchVersion = ENGINE_VERSION % 100;
			subvVrsion = (ENGINE_VERSION / 100 ) % 100;
			mainVersion = (ENGINE_VERSION / 10000 ) % 100;

			date = RELEASE_DATE % 100;
			month = (RELEASE_DATE / 100 ) % 100;
			year = (RELEASE_DATE / 10000 ) % 100 + 2000;

		}


		virtual	Status	CheckStatus(CodeStreamHandler handler) const;
		virtual	bool	CheckProperties(CodeStreamHandler handler, CodeStreamProperties & properties) const;
		virtual	const void *	GetResult(CodeStreamHandler handler, int & bufSize);
		virtual void	Release(CodeStreamHandler handler);

		virtual void	SetJobConfig(const EncodeJobConfig & config) {		 mCurrentJobConfig = config; }
		virtual	void	GetJobConfig(EncodeJobConfig & config, CodeStreamHandler handler = INVALID) const;



		typedef	struct EncodeResult_s
		{
			EncodeResult_s():
			mBuf(NULL),
			mBufSize(INVALID),
			mLocked(false)
			{}

			const void				* mBuf;
			unsigned int			mBufSize;
			CodeStreamHandler		mHandler;
			bool					mLocked;
			EncodeJobConfig			mConfig;

		}EncodeResult, *P_EncodeResult;


	private:

		//////////////////////////////////////////////////////////////////////////
		//	Private Function
		//////////////////////////////////////////////////////////////////////////
		static	THREAD_ENTRY WINAPI Thread_Encode(PVOID pParam);

		//////////////////////////////////////////////////////////////////////////
		//	Private data
		//////////////////////////////////////////////////////////////////////////

		EncodeJobConfig	mCurrentJobConfig;
		int				mNextInputSlot;
		int				mNextResultSlot;
		// 		uint_32			mIndex;

		volatile P_InputStream	mInput;
		volatile P_EncodeResult		mResult;
// 
// 		P_ThreadInfo		mChildThread;
// 
// 		ThreadInfo			mMainThread;
// 		JobQueue		mJobQueue;
// 
// 		// 		unsigned char		* mDecodeBuf;
// 		// 		unsigned	int		mDecodeBufElementSize;
// 
// 
// // 		MUTEX_HANDLE			mJobIdMutex;
// 		MUTEX_HANDLE			mStatusMutex;
// 
// 		SEMAPHORE_HANDLE	mDwtSyncSemaphore;
// 
// 		stru_dec_param		mDecodePara;
// 		void				* mT1DecodeBuffer;
// 		int					mT1DecodeBufSize[2];
// 		int					mJustFinishedPictureHandler;


		EnginConfig			mEnginConfig;


	};	
}
