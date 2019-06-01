// For testing only
#pragma  once
#include "platform.h"

#define _TIME_TRACKING_		1


#if _WINDOWS
#else
#include <sys/time.h> 
typedef		long INT64;
#endif

#if defined _FINAL && _TIME_TRACKING_
#define _TIME_TRACK_FIRST_CLOCK_ONLY	1
#else
#define _TIME_TRACK_FIRST_CLOCK_ONLY	0
#endif

#if _TIME_TRACKING_
#define		RESETALLCLOCK		resetClock(-1)

#if _TIME_TRACK_FIRST_CLOCK_ONLY
#define		CREAT_CLOCK(x)		CreatClock(1)
#define		STARTCLOCK(x)		if (!(x)) startClock(x)
#define		STOPCLOCK(x)		if (!(x)) stopClock(x)
#define		GETCLOCKTIME(x)		((x) ? 0 : getClockTime(x))
#define		GETCLOCKHIT(x)		((x) ? 0 : getClockHit(x))
#define		GETCLOCKCYCLE(x)	((x) ? 0 : getClockCycle(x))
#define		RESETCLOCK(x)		if (!(x)) resetClock(x)

#else
#define		CREAT_CLOCK(x)		CreatClock(x)
#define		STARTCLOCK(x)		startClock(x)
#define		STOPCLOCK(x)		stopClock(x)
#define		GETCLOCKTIME(x)		getClockTime(x)
#define		GETCLOCKHIT(x)		getClockHit(x)
#define		GETCLOCKCYCLE(x)	getClockCycle(x)
#define		RESETCLOCK(x)		resetClock(x)
#endif // _TIME_TRACK_FIRST_CLOCK_ONLY
#else
#define		CREAT_CLOCK(x)
#define		STARTCLOCK(x)
#define		STOPCLOCK(x)
#define		RESETCLOCK(x)
#define		GETCLOCKTIME(x)		0
#define		GETCLOCKCYCLE(x)	0
#define		GETCLOCKHIT(x)		0
#define		RESETALLCLOCK		
#endif

#if _TIME_TRACKING_

BMI_J2K_CODEC_EXPORTS void resetClock(int id);

BMI_J2K_CODEC_EXPORTS INT64 getClockCycle(int id);

BMI_J2K_CODEC_EXPORTS double getClockTime(int id);
BMI_J2K_CODEC_EXPORTS	int	getClockHit(int id);

BMI_J2K_CODEC_EXPORTS void startClock(int id);

BMI_J2K_CODEC_EXPORTS void stopClock(int id);

BMI_J2K_CODEC_EXPORTS void CreatClock(int num = 0);

#endif

