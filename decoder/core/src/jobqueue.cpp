//#include "codecconcrete.h"
#include "debug.h"
#include "testclock.h"
#include "jobqueue.h"
#include "codec_base.h"

#define FORWARD_HEADINDEX	BMI_ASSERT(mHeadIndex != mTailIndex); ++mHeadIndex; if (mHeadIndex == mCapacity) mHeadIndex = 0;
#define FORWARD_TAILINDEX	++mTailIndex; if (mTailIndex == mCapacity) mTailIndex = 0; BMI_ASSERT(mHeadIndex != mTailIndex); 

namespace	BMI
{

	const int	MAX_JOBS_IN_QUEUE	= 1000;	

	void		JobQueue::init(int reservedSize,unsigned int unitSize )
	{
		if (!mInited)
		{
			mUnitSize = unitSize + 4;		// Add a int as the handler to the element head
			mQueue = reinterpret_cast<unsigned char*>(malloc((reservedSize + 1) * mUnitSize));
			BMI_ASSERT(mQueue);
			mHeadIndex = 0;
			mTailIndex = 0;
			mCapacity = reservedSize;
			mInited = true;
		}
		else
		{
			BMI_ASSERT(0);
			// currently don't support adjustment
		}
	}

	bool	JobQueue::pop_front()
	{
		BMI_ASSERT(mInited);
		bool ret = false;
		if (mHeadIndex != mTailIndex)
		{
			ret = true;
			++mHeadIndex;
			if (mHeadIndex == mCapacity)	
			{
				mHeadIndex = 0;
			}
			--mSize;
			// 			WRITE_LOG(("Capacity %d size %d head %d tail %d\n", mCapacity, mSize, mHeadIndex,mTailIndex));
		}
		// 		BMI_ASSERT((  mTailIndex + mCapacity - mHeadIndex == mSize) || (mTailIndex - mHeadIndex  == mSize));
		return ret;
	}
	bool			JobQueue::pop_back()
	{
		BMI_ASSERT(mInited);
		bool ret = false;
		if (mHeadIndex != mTailIndex)
		{
			ret = true;
			--mTailIndex;
			if (mTailIndex < 0)	
			{
				mTailIndex = mCapacity - 1;;
			}
			--mSize;
			// 			WRITE_LOG(("Capacity %d size %d head %d tail %d\n", mCapacity, mSize, mHeadIndex,mTailIndex));

		}
		// 		BMI_ASSERT((  mTailIndex + mCapacity - mHeadIndex == mSize) || (mTailIndex - mHeadIndex  == mSize));

		return ret;
	}

	bool			JobQueue::push_front(void * job, int handler)
	{
		BMI_ASSERT(mInited);

		--mHeadIndex;
		if (mHeadIndex < 0)
		{
			mHeadIndex = mCapacity - 1;
		}
		if (mHeadIndex == mTailIndex)
		{
			WRITE_LOG(("Job insert failed!!!index = %d:%d mSize = %d\n", mHeadIndex,mTailIndex, mSize));
			BMI_ASSERT(0);
		}
		memcpy(GET_ELEMENT(mHeadIndex), job, mUnitSize);
		ELEMENT_HANDLER(mHeadIndex) = handler;
		++mSize;
		// 		WRITE_LOG(("Capacity %d size %d head %d tail %d\n", mCapacity, mSize, mHeadIndex,mTailIndex));

		// 		BMI_ASSERT((  mTailIndex + mCapacity - mHeadIndex == mSize) || (mTailIndex - mHeadIndex  == mSize));

		return true;
	}

	bool			JobQueue::push_back(void * job, int handler)
	{
		BMI_ASSERT(mInited);

		memcpy(GET_ELEMENT(mTailIndex), job, mUnitSize);
		ELEMENT_HANDLER(mTailIndex) = handler;

		++mTailIndex;
		if (mTailIndex == mCapacity)
		{
			mTailIndex = 0;
		}
		if (mHeadIndex == mTailIndex)
		{
			WRITE_LOG(("Job insert failed!!!index = %d:%d mSize = %d\n", mHeadIndex,mTailIndex, mSize));
			BMI_ASSERT(0);
		}
		++mSize;

		return true;
	}

	void		JobQueue::remove_frame(int handler)
	{
		if (mHeadIndex != mTailIndex)
		{
			while (ELEMENT_HANDLER(mHeadIndex) == handler )
			{
				ELEMENT_HANDLER(mHeadIndex) = INVALID;
				mHeadIndex++;
				if (mHeadIndex == MAX_JOBS_IN_QUEUE)
				{
					mHeadIndex = 0;
				}	
			}
		}
		// 		BMI_ASSERT((  mTailIndex + mCapacity - mHeadIndex == mSize) || (mTailIndex - mHeadIndex  == mSize));
	}


	int		JobQueue::ThreadSafeAccessJobQueue(MUTEX_HANDLE mutex, void * &job, int &handler, JobQueueAccessMode mode, int & para)
	{
		BMI_ASSERT(mInited);

#if _WINDOWS
		if (mutex)
#endif
		{
			LOCK_MUTEX(mutex);
		}
		int ret = mSize - mSkipedSlot;
		switch(mode)
		{
		case GET_JOB_COUNT:
			break;

		case GET_A_JOB:
			{
				job = NULL;
				if (para == INVALID)
				{
					para = 0;
				}
				BMI_ASSERT(para < mSize);

				int index = mHeadIndex + para;
				int skip = 0;
				if (ret)
				{

					while ((handler = ELEMENT_HANDLER(index)) == INVALID)
					{
						++skip;
						if (index == mTailIndex)
						{
							break;
						}
						++index;
						if (index >= mCapacity)
						{
							index = 0;
						}
					}

					job = GET_ELEMENT(index);
					if (handler != INVALID)
					{
						--ret ;
					}

					if (para == 0)	// Get a job from the head
					{
						++mHeadIndex;
						if (mHeadIndex >= mCapacity)
						{
							mHeadIndex = 0;
						}
						mSize -= skip + 1;
						mSkipedSlot -= skip;
					}
					else if (para == mSize - 1)	// Get a job from the tail of the queue
					{
						--mTailIndex;
						if (mTailIndex < 0)
						{
							mTailIndex = mCapacity - 1;
						}
						--mSize;
						mSkipedSlot -= skip;
					}
					else	// Get a job from the middle of the queue
					{
						ELEMENT_HANDLER(index) = INVALID;
						++mSkipedSlot;
					}

				}
			}
			break;

		case INSERT_A_JOB:
			{
				if (para == INVALID)
				{
					para = 0;
				}
				else if (para >= mSize)
				{
					para = mSize;
				}
				BMI_ASSERT(job && handler != INVALID);

				int targetIndex = mHeadIndex + para;
				int moveSize = mSize - para;
				bool moveForward = true;
				if (moveSize >= mSize / 2)
				{
					moveSize = mSize - moveSize;
					moveForward = false;
					--targetIndex;
				}

				if (moveSize)
				{
					BMI_ASSERT(0);	
#pragma BMI_PRAGMA_TODO_MSG("to support insert job in the middle of the queue")
				}


				if (moveForward)
				{
					++mTailIndex;
					if (mTailIndex >= mCapacity)
					{
						mTailIndex = 0;
					}
				}
				else
				{
					--mHeadIndex;
					if (mHeadIndex < 0)
					{
						mHeadIndex = mCapacity - 1;
					}
				}

				BMI_ASSERT(mHeadIndex != mTailIndex);

				memcpy(GET_ELEMENT(targetIndex), job, mUnitSize);
				ELEMENT_HANDLER(targetIndex) = handler;

				++mSize;
			}
			break;

		case ATTACH_A_JOB:
			++mTailIndex;
			if (mTailIndex >= mCapacity)
			{
				mTailIndex = 0;
			}
			BMI_ASSERT(mHeadIndex != mTailIndex);
			memcpy(GET_ELEMENT(mTailIndex), job, mUnitSize);
			ELEMENT_HANDLER(mTailIndex) = handler;
			break;


		case REMOVE_A_JOB:
			{
				if (para == INVALID)
				{
					para =0;
				}

//				int index = mHeadIndex;
				if (ret)
				{

					while ((handler = ELEMENT_HANDLER(mHeadIndex)) == INVALID)
					{
						++mHeadIndex;
						if (mHeadIndex >= mCapacity)
						{
							mHeadIndex = 0;
						}
						--mSkipedSlot;
					}

					job = GET_ELEMENT(mHeadIndex);
					++mHeadIndex;
					if (mHeadIndex >= mCapacity)
					{
						mHeadIndex = 0;
					}
					--mSize;
				}
			}
		case CHECK_CURRENT_FRAME_DONE:
		case REMOVE_CURRENT_FRAME:
		case CLEAN_UP_QUEUE:
		default:
			BMI_ASSERT(0);
			break;

		}
#if _WINDOWS
		if (mutex)
#endif
		{
			RELEASE_MUTEX(mutex);
		}
		return ret;
	}

	bool	SemaphoreControledJobQueue::Init(int	elementNum, int unitSize)
	{
		// The first 4 bytes indicated this item's availability
		if (!mInited)
		{
			CREATE_SEMAPHORE(mQueueSemaphore, 0, MAX_LENGTH_OF_JOB_QUEUE );
			CREATE_MUTEX(mQueueMutex);
			mInited = true;
			if (unitSize > 0 && elementNum > 0)
			{
				mSpace = reinterpret_cast<unsigned char *>(malloc(unitSize * elementNum));
				BMI_ASSERT(mSpace);
				mCapacity = elementNum;
				mUnitSize = unitSize;
			}
		}
		else		// re_init 
		{
			while (mSize)	// release all semaphore
			{
				WAIT_SEMAPHORE(mQueueSemaphore);
				--mSize;
			}
			if (mCapacity * mUnitSize < unitSize * elementNum)
			{
				SAFE_FREE(mSpace);
				mSpace = reinterpret_cast<unsigned char *>(malloc(unitSize * elementNum));
			}
			mCapacity = elementNum;
			mUnitSize = unitSize;
			mHeadIndex = 0;
			mTailIndex = 0;
			DEBUG_PRINT(("Set job queue length %d unit %d bytes\n", mCapacity, mUnitSize));
		}

		return mInited;
	}

	int		SemaphoreControledJobQueue::Access(void * unit, JobQueueAccessMode mode, int inputSlot, bool threadSafe /*= true*/, void (* func)(int, void *)/* = NULL*/)
	{
		int	ret = INVALID;
		bool	Locked = false;
		switch (mode)
		{
		case	GET_JOB_COUNT:
			ret = mSize;
			break;

		case	GET_A_JOB:
			WAIT_SEMAPHORE(mQueueSemaphore);
			if (threadSafe)
			{
				LOCK_MUTEX(mQueueMutex);
				Locked = true;
			}
			memcpy(unit, reinterpret_cast<void *>(mSpace + mHeadIndex * mUnitSize), mUnitSize);
			FORWARD_HEADINDEX;
			--mSize;
			break;


		case ATTACH_A_JOB:
			if (threadSafe)
			{
				LOCK_MUTEX(mQueueMutex);
				Locked = true;
			}
			memcpy(reinterpret_cast<void *>(mSpace + mTailIndex * mUnitSize), unit, mUnitSize);
			INCREASE_SEMAPHORE(mQueueSemaphore);
//			DEBUG_PRINT(("+++++  [%d] Insert a Job on %d\n", mSize,mTailIndex));
			FORWARD_TAILINDEX;
			++mSize;
			break;

		case	CLEAN_UP_QUEUE:
			if (threadSafe)
			{
				LOCK_MUTEX(mQueueMutex);
				Locked = true;
			}
			while (mSize)	// release all semaphore
			{
				WAIT_SEMAPHORE(mQueueSemaphore);
				--mSize;
			}
			mHeadIndex = 0;
			mTailIndex = 0;
			break;

		case	TRAVELSAL_DO_FUNC:
			if (threadSafe)
			{
				LOCK_MUTEX(mQueueMutex);
				Locked = true;
			}

			BMI_ASSERT(func);
			for (int i = mHeadIndex; i != mTailIndex; ++i )
			{
				if (i == mCapacity)
				{
					i = 0;
				}
				(*func)(inputSlot, reinterpret_cast<void *>(mSpace + i * mUnitSize));
			}
			break;

		default:
			break;
		}
		if (Locked)
		{
			RELEASE_MUTEX(mQueueMutex);
		}

		return ret;

	}


}
