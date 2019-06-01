#pragma  once

#include "platform.h"

//////////////////////////////////////////////////////////////////////////
//	Version Information
//////////////////////////////////////////////////////////////////////////
#define		ENGINE_VERSION	40501
#define		RELEASE_DATE	100729

//////////////////////////////////////////////////////////////////////////
// Set the Expire date
//////////////////////////////////////////////////////////////////////////
#define DEMO_EXPIRE_DATE	1

#if DEMO_EXPIRE_DATE
#include <time.h>
#define EXPIRE_YEAR		2010
#define EXPIRE_MONTH	9
#define EXPIRE_DATE		1
#endif


#define		GPU_T1_TESTING		0



#if  _MAC_OS
	#define		ENABLE_ASSEMBLY_OPTIMIZATION	1
	#define		AT_T_ASSEMBLY	ENABLE_ASSEMBLY_OPTIMIZATION
	#define		INTEL_ASSEMBLY_32	0
	#define		INTEL_ASSEMBLY_64	0
	#define		SSE_OPTIMIZED		ENABLE_ASSEMBLY_OPTIMIZATION

#elif _LINUX
#define		ENABLE_ASSEMBLY_OPTIMIZATION	1
	#define		AT_T_ASSEMBLY	ENABLE_ASSEMBLY_OPTIMIZATION
	#define		INTEL_ASSEMBLY_32	0
	#define		INTEL_ASSEMBLY_64	0
	#define		SSE_OPTIMIZED		0 // ENABLE_ASSEMBLY_OPTIMIZATION

#else
	#if _WIN32_ 
		#define		ENABLE_ASSEMBLY_OPTIMIZATION	1
		#define		INTEL_ASSEMBLY_32	ENABLE_ASSEMBLY_OPTIMIZATION
		#define		INTEL_ASSEMBLY_64	0
		#define		AT_T_ASSEMBLY		0
		#define		SSE_OPTIMIZED		ENABLE_ASSEMBLY_OPTIMIZATION
	#elif _WIN64_
		#define		ENABLE_ASSEMBLY_OPTIMIZATION	0
		#define		INTEL_ASSEMBLY_64	ENABLE_ASSEMBLY_OPTIMIZATION
		#define		INTEL_ASSEMBLY_32	0
		#define		AT_T_ASSEMBLY		0
		#define		SSE_OPTIMIZED		0
	#endif
#endif

#if SSE_OPTIMIZED
	#define		SSE_ELEMENT_BYTES			16
	#define		ALIGN_TO_SSE_DWORDS(x)		(((x) + 3) & 0xfffffffc)
#else
	#define		SSE_ELEMENT_BYTES		1
	#define		ALIGN_TO_SSE_DWORDS(x)		(x)
#endif

#define		SSE_ALIGN_BYTES			(SSE_ELEMENT_BYTES - 1)

#define SOFTWARE_MULTI_THREAD	1
#if SOFTWARE_MULTI_THREAD
	#define		BMI_BLOCK_DECODER			1
#else
	#define		BMI_BLOCK_DECODER			0
#endif


#define USE_FAST_MACROS 1


#define INT_32_MAX	(int_32)0x7fffffff

//////////////////////////////////////////////////////////////////////////
//  Defined some macro for MAC, those was defined in Windows vc/include/xxxxx.h
//////////////////////////////////////////////////////////////////////////
#if _MAC_OS || _LINUX
typedef	void * PVOID;
typedef int LONGLONG;	// was useing in class testClock
#define WINAPI
typedef		long BMI_INT_64;
typedef		unsigned long UINT64;
#else
typedef		_int64	  BMI_INT_64;
#endif
//////////////////////////////////////////////////////////////////////////
//	MACRO for multi-threads
//////////////////////////////////////////////////////////////////////////
#if _MAC_OS
#include <string.h>
#include <pthread.h>
typedef	pthread_t					THREAD_HANDLE;
#define END_THREAD(x)				pthread_cancel(x)
#define SAME_THREAD(x,y)			(pthread_equal((x), (y)) == 0)

#include "macsemaphore.h"
typedef	Mac_sem_t 					SEMAPHORE_HANDLE;
#define CREATE_SEMAPHORE(x,initVal,maxVal)		Mac_sem_init(&(x),0, initVal)
#define WAIT_SEMAPHORE(x)		Mac_sem_wait(&(x))
#define INCREASE_SEMAPHORE(x)		Mac_sem_post(&(x))
#define DESTROY_SEMAPHORE(x)		Mac_sem_destroy(&(x))
#define	GET_SEMAPHORE_VALUE(x, v)	Mac_sem_getvalue(&(x), v)


typedef pthread_mutex_t				MUTEX_HANDLE;
#define CREATE_MUTEX(x)				pthread_mutex_init(&(x), NULL)
#define LOCK_MUTEX(x)				pthread_mutex_lock(&(x))
#define RELEASE_MUTEX(mutex)		pthread_mutex_unlock(&(mutex))
#define RELEASE_MUTEX_THREAD(mutex,threadNum)		if( (threadNum) > 1 ) pthread_mutex_unlock(&(mutex))
#define DESTROY_MUTEX(x)			pthread_mutex_destroy(&(x))
#define MUTEX_INIT(x,v)				pthread_mutex_init(&(x),(v))

typedef	void * 			THREAD_ENTRY;

#ifdef __cplusplus
extern "C" {
#endif

	#include <stdarg.h>
	void CreateSemaphore(SEMAPHORE_HANDLE sem, int initVal, int maxVal);

#ifdef __cplusplus
}
#endif

#elif _LINUX
#include <string.h>
#include <pthread.h>
typedef	pthread_t					THREAD_HANDLE;
#define END_THREAD(x)				pthread_cancel((x))
#define SAME_THREAD(x,y)			(pthread_equal((x), (y)) == 0)

#include "linuxsemaphore.h"
typedef	LINUX_sem_t 					SEMAPHORE_HANDLE;
#define CREATE_SEMAPHORE(x,initVal,maxVal)		LINUX_sem_init(&(x),0, initVal)
#define WAIT_SEMAPHORE(x)			LINUX_sem_wait(&(x))
#define INCREASE_SEMAPHORE(x)		LINUX_sem_post(&(x))
#define DESTROY_SEMAPHORE(x)		LINUX_sem_destroy(&(x))
#define	GET_SEMAPHORE_VALUE(x, v)	LINUX_sem_getvalue(&(x), v)


typedef pthread_mutex_t				MUTEX_HANDLE;
#define CREATE_MUTEX(x)				pthread_mutex_init(&(x), NULL)
#define LOCK_MUTEX(x)				pthread_mutex_lock(&(x))
#define RELEASE_MUTEX(mutex)		pthread_mutex_unlock(&(mutex))
#define RELEASE_MUTEX_THREAD(mutex,threadNum)		if( (threadNum) > 1 ) pthread_mutex_unlock(&(mutex))
#define DESTROY_MUTEX(x)			pthread_mutex_destroy(&(x))
#define MUTEX_INIT(x,v)				pthread_mutex_init(&(x),(v))

typedef	void * 			THREAD_ENTRY;

#ifdef __cplusplus
extern "C" {
#endif

	#include <stdarg.h>
	void CreateSemaphore(SEMAPHORE_HANDLE sem, int initVal, int maxVal);

#ifdef __cplusplus
}
#endif

#else

typedef	HANDLE						THREAD_HANDLE;
#define END_THREAD(x)				TerminateThread((x), 0);
#define SAME_THREAD(x,y)			((x) == (y))

typedef	HANDLE						SEMAPHORE_HANDLE;
#define CREATE_SEMAPHORE(x,initVal,maxVal)	(x) = CreateSemaphore(NULL,initVal, maxVal, NULL)
#define WAIT_SEMAPHORE(x)			WaitForSingleObject((x), INFINITE)
#define INCREASE_SEMAPHORE(x)		ReleaseSemaphore((x), 1, NULL);
#define DESTROY_SEMAPHORE(x)		if(x) CloseHandle((x))
#define	GET_SEMAPHORE_VALUE(x, v)

typedef	HANDLE						MUTEX_HANDLE;
#define CREATE_MUTEX(x)				(x) =  CreateMutex(NULL,FALSE,NULL)
#define LOCK_MUTEX(x)				WaitForSingleObject((x), INFINITE)
#define RELEASE_MUTEX(mutex)		ReleaseMutex((mutex))
#define RELEASE_MUTEX_THREAD(mutex,threadNum)		if( (threadNum) > 1 ) ReleaseMutex((mutex))
#define DESTROY_MUTEX(x)			if (x) CloseHandle((x))
#define MUTEX_INIT(x,v)				(x) = (v)
typedef	DWORD 			THREAD_ENTRY;
#endif


#if _WIN64_
typedef	UINT64		PointerValue;
#else
typedef	unsigned int	PointerValue;
#endif


//////////////////////////////////////////////////////////////////////////
// The following macro used in testing
//////////////////////////////////////////////////////////////////////////
#define		GPU_T1_TESTING		0

#if DEMO_EXPIRE_DATE	
/***********************************************************************************
Function Name:	EngineExpired
Description:	Check the engine's expire day
IN:
N/A
OUT:
N/A
RETURN:
true if the engine expired
***********************************************************************************/
static bool EngineExpired()
{

	bool ret;

	struct tm expire_time;
	time_t cur_time = time(NULL);
	expire_time.tm_year = EXPIRE_YEAR - 1900;
	expire_time.tm_mon  = EXPIRE_MONTH - 1;      // Jan = 0, Dec = 11
	expire_time.tm_mday = EXPIRE_DATE;
	expire_time.tm_hour = 0;
	expire_time.tm_min  = 0;
	expire_time.tm_sec  = 0;

	if((unsigned int)cur_time > (unsigned int)mktime(&expire_time))
	{
		ret = true;
	}
	else
	{
		ret = false;
	}

	return ret;
}

#define CHECK_EXPIRED_DATE	EngineExpired()
#else

#define CHECK_EXPIRED_DATE	false

#endif