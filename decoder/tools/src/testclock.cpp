// For testing onnly

#include "testclock.h"
#include "singleton.h"
#include "debug.h"
#include "memory.h"

#if _TIME_TRACKING_

#if _WINDOWS
typedef		LARGE_INTEGER					CLOCK_VAL;
#define		GET_CURRENT_TIME(x)				QueryPerformanceCounter(&(x))
#define		ADD_UP_TIME_DIFFERENCE(x,a,b)  ((x).QuadPart += (b).QuadPart - (a).QuadPart - TestClock::mStartStopPairClockClick) //QuadPart  ((x).LowPart += (b).LowPart - (a).LowPart);((x).HighPart += (b).HighPart - (a).HighPart);
#define		GET_MSEC(x)						(((double)(x).QuadPart * 1000.0f / (double)mFreq.QuadPart) ) //(((double)(x).LowPart / (double)mFreq.LowPart) * 1000.0f)
#else
typedef		struct timeval					CLOCK_VAL;
#define		GET_CURRENT_TIME(x)				gettimeofday(&(x), NULL)
#define		ADD_UP_TIME_DIFFERENCE(x,a,b)  ((x).tv_sec += (b).tv_sec - (a).tv_sec); ((x).tv_usec += (b).tv_usec - (a).tv_usec);
#define		GET_MSEC(x)						((x).tv_sec * 1000.0 +  (x).tv_usec / 1000.0 )

#endif
class TestClock : public Singleton<TestClock>
{
public:

	TestClock();
	~TestClock() {}

	void	start(int clockId);

	void	stop(int clockId);

	double	GetClockTime(int clockId) const;
	int		GetCloskHit(int clockId) const;

	void	reset(int clockId);

#if _WINDOWS
	const LARGE_INTEGER & GetFrequence() const
	{
		return mFreq;
	}


	LONGLONG GetClockCycles(int clockId) const
	{
		BMI_ASSERT(clockId < mClockNum);
		return mClockTime[clockId].QuadPart;
	}
#else
	const int GetFrequence() const
	{
		return 0;	// todo
	}

	int GetClockCycles(int clockId) const
	{
		return 0; // todo
	}
#endif

	void	Init(int clockNum);

private:
	void	reset();


	CLOCK_VAL *mClockTime;
	CLOCK_VAL *mClockStart, *mClockStop;
	bool		  * mClockInUse;
	unsigned int	* mClockHit;
	int			  mClockNum;
#if _WINDOWS
	CLOCK_VAL mFreq;

public:
	static LONGLONG mStartStopPairClockClick;
#endif

};

#if _WINDOWS
LONGLONG TestClock::mStartStopPairClockClick = 0;
#endif

const int defaultClockNum = 20;

TestClock::TestClock():
	mClockTime(NULL),
	mClockStart(NULL),
	mClockStop(NULL),
	mClockInUse(NULL),
	mClockHit(NULL),
	mClockNum(0)
	{
#if _WINDOWS
		QueryPerformanceFrequency(&mFreq);
#endif
	}

	
	void	TestClock::start(int clockId)
	{
		if (clockId < mClockNum)
		{
			if (mClockInUse[clockId] == false)
			{
				GET_CURRENT_TIME(mClockStart[clockId]); 
				mClockInUse[clockId] = true;
			}
			else
			{
				PRINT(("thread conflict when start clock %d\n ", clockId));
			}
			++mClockHit[clockId];
		}

	}

	void	TestClock::stop(int clockId)
	{
		if (clockId < mClockNum)
		{
			if (mClockInUse[clockId] == true)
			{
				GET_CURRENT_TIME(mClockStop[clockId]); 
				mClockInUse[clockId] = false;
				ADD_UP_TIME_DIFFERENCE(mClockTime[clockId], mClockStart[clockId], mClockStop[clockId]);
			}
			else
			{
				PRINT(("thread conflict when stop clock %d\n ", clockId));
			}
		}
	}



	double TestClock::GetClockTime(int clockId) const
	{
		if(clockId < mClockNum) 
		{
			return GET_MSEC(mClockTime[clockId]);
		}
		else 
		{
			return 0;
		}
	}

	int	TestClock::GetCloskHit(int clockId) const
	{
		if (clockId < mClockNum)
		{
			return mClockHit[clockId];
		}
		else
		{
			return 0;
		}

	}
	void	TestClock::reset(int clockId)
	{
		if (clockId == -1)
		{
			reset();
		}
		else 
		{
			if(clockId < mClockNum)
			{
				memset(&mClockTime[clockId], 0, sizeof(CLOCK_VAL));
				mClockInUse[clockId] = false;
				mClockHit[clockId] = 0;
			}
		}
	}

	void	TestClock::Init(int clockNum)
	{
		if (clockNum != mClockNum)
		{
			if (mClockNum != 0)
			{
				delete[] mClockTime;
				delete[] mClockStart;
				delete[] mClockStop;
				delete[] mClockInUse;
				delete[] mClockHit;

			}
			mClockTime = new CLOCK_VAL[clockNum];
			mClockStart = new CLOCK_VAL[clockNum];
			mClockStop = new CLOCK_VAL[clockNum];
			mClockInUse = new bool[clockNum];
			mClockHit	= new unsigned int[clockNum];
			mClockNum = clockNum;		
		}

		reset();

// #if _WINDOWS
// 		CLOCK_VAL tmp0, tmp1;
// 		TestClock::mStartStopPairClockClick = 0;
// 		QueryPerformanceCounter(&tmp0); 
// 		start(0); 
// 		QueryPerformanceCounter(&tmp1); 
// 		TestClock::mStartStopPairClockClick = tmp1.QuadPart - tmp0.QuadPart;
// 		QueryPerformanceCounter(&mClockTime[0]); 
// 		QueryPerformanceCounter(&tmp0); 
// 		TestClock::mStartStopPairClockClick -= tmp0.QuadPart - tmp1.QuadPart;
// 
// #endif

	}

	void	TestClock::reset()
	{
		memset(mClockTime, 0, sizeof(CLOCK_VAL)*mClockNum);
		memset(mClockHit, 0, sizeof(unsigned int)*mClockNum);
		memset(mClockInUse, false, sizeof(bool) * mClockNum);
	}


INT64 getClockCycle(int id)
{
	BMI_ASSERT(TestClock::IsCreated());
	return TestClock::GetInstance()->GetClockCycles(id);
}

double getClockTime(int id)
{
	BMI_ASSERT(TestClock::IsCreated());
	return TestClock::GetInstance()->GetClockTime(id);
}

int	getClockHit(int id)
{
	BMI_ASSERT(TestClock::IsCreated());
	return TestClock::GetInstance()->GetCloskHit(id);
}

void resetClock(int id)
{
	BMI_ASSERT(TestClock::IsCreated());
	TestClock::GetInstance()->reset(id);

}


void startClock(int id)
{
	BMI_ASSERT(TestClock::IsCreated());
	TestClock::GetInstance()->start(id);
}

void stopClock(int id)
{
	BMI_ASSERT(TestClock::IsCreated());
	TestClock::GetInstance()->stop(id);
}

void CreatClock(int num /*= 0*/)
{
	if (!TestClock::IsCreated())
	{
		TestClock::Create();
	}
	if (num)
	{
		TestClock::GetInstance()->Init(num);
	}
	else
	{
		TestClock::GetInstance()->Init(defaultClockNum);
	}

}

#endif // _TIME_TRACKING_
