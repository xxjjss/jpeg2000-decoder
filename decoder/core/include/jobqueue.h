#pragma  once

#include "codec.h"
#include "codec_base.h"

#define		GET_ELEMENT(x)		reinterpret_cast<void *>(mQueue + (x) * mUnitSize + 4)
#define		ELEMENT_HANDLER(x)	*reinterpret_cast<int *>(mQueue + (x) * mUnitSize)


const int MAX_LENGTH_OF_JOB_QUEUE	= 50000;	

namespace	BMI
{

	enum	JobQueueAccessMode
	{
		// 		INIT_JOB_QUEUE,
		GET_JOB_COUNT,
		GET_A_JOB,
		INSERT_A_JOB,
		ATTACH_A_JOB,
		REMOVE_A_JOB,
		CHECK_CURRENT_FRAME_DONE,
		REMOVE_CURRENT_FRAME,
		CLEAN_UP_QUEUE,
		UNKNOWN_MODE,

		TRAVELSAL_DO_FUNC,
		TRAVELSAL_REMOVE
	};


	class JobQueue 
	{
	public:
		JobQueue():
		mQueue(NULL),
		mHeadIndex(-1),
		mTailIndex(-1),
		mCapacity(0),
		mSize(0),
		mSkipedSlot(0),
		mUnitSize(0),
		mInited(false)
		// 			  ,mMaxSize(0)
		{}

		  ~JobQueue()
		  {
			  SAFE_FREE(mQueue);
		  }


		  int		ThreadSafeAccessJobQueue(MUTEX_HANDLE mutex, void * &job,  int &handler,JobQueueAccessMode mode, int & para);
		  void		init(int reservedSize, unsigned int unitSize);
		  bool		isInited() const {return mInited;}
		  int		size() const  {   int ret = mTailIndex - mHeadIndex; ret = ret < 0 ? ret + mCapacity : ret; return ret;  }
		  int		capacity() const	  {		  return mCapacity;		  }

		  void *	get_element(int index) {return GET_ELEMENT(index);};
		  void *	begin()	{ return ((mHeadIndex != mTailIndex) ? GET_ELEMENT(mHeadIndex) : NULL); }
		  int		get_head_index() const {return mHeadIndex;}
		  int		get_tail_index() const {return mTailIndex;}

		  bool		pop_front();
		  bool		pop_back();
		  bool		push_front(void * job, int handler);
		  bool		push_back(void * job, int handler);
		  void		clear()
		  {
			  mHeadIndex = 0;
			  mTailIndex = 0;
			  mSize = 0;
		  }

		  bool		check_frame(int handler)	{return ((mHeadIndex != mTailIndex) ? ELEMENT_HANDLER(mHeadIndex)==handler : false);}
		  void		remove_frame(int handler);

		  // 		  int mMaxSize;
	private:
		unsigned char *		mQueue;
		int			mHeadIndex;
		int			mTailIndex;
		int			mCapacity;
		int			mSize;
		int			mSkipedSlot;
		unsigned int			mUnitSize;
		bool		mInited;
	};

	typedef		JobQueue *		P_JobQueue;


	class SemaphoreControledJobQueue
	{
	public:
		SemaphoreControledJobQueue():
#if _WINDOWS
		mQueueSemaphore(NULL),
		mInited(false),
#endif
		mCapacity(0),
		mSize(0),
		mHeadIndex(0),
		mTailIndex(0),
		mUnitSize(0),
		mSpace(NULL)
		{
		}

		~SemaphoreControledJobQueue()
		{
			if (mInited)
			{
				SAFE_FREE(mSpace);
				DESTROY_MUTEX(mQueueMutex);
				DESTROY_SEMAPHORE(mQueueSemaphore);
			}
		}

		bool	Init(int	elementNum, int unitSize);
		int		Access(void * unit, JobQueueAccessMode mode,  int inputSlot = -1,  bool threadSafe = true,	void (* func)(int, void *) = NULL);

		int		GetunitSize() const  {return mUnitSize;}

	private:

		MUTEX_HANDLE		mQueueMutex;
		SEMAPHORE_HANDLE	mQueueSemaphore;

		bool	mInited;

		int		mCapacity;
		int		mSize;
		int		mHeadIndex;
		int		mTailIndex;
		int		mUnitSize;

		unsigned char	* mSpace;
	};
}
