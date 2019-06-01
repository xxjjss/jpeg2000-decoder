#pragma once

#include "platform.h"

// #if defined _CONSOLE
//#undef _WINDOWS
//#endif

#define		PRINT_SCREEN(x)	printf x
#define		WRITE_LOG(x)  dbg_write_log x


#if defined _FINAL
	#define		PRINT(x)
#elif _WINDOWS
	#define		PRINT(x)		dbg_error_printf x
#else
	#define		PRINT(x)		printf x
#endif	// _WINDOWS

#if defined _DEBUG
	#if _WINDOWS
		#define		DEBUG_PRINT(x)						dbg_error_printf x
		#define		DEBUG_CONDITION_PRINT(c,p)		if (c)   dbg_error_printf p;
		#define		DEBUG_SELECT_PRINT(c,t,f)		(c) ?  dbg_error_printf t : dbg_error_printf f
	#else
		#define		DEBUG_PRINT(x)						printf x
		#define		DEBUG_CONDITION_PRINT(c,p)		if (c)   printf p;
		#define		DEBUG_SELECT_PRINT(c,t,f)		(c) ? printf t : printf f
	#endif
	#define		WRITE_DBG_LOG(x) WRITE_LOG(x) 
	#define DEBUG_PAUSE(x)		dbg_pause((x))
#else
	#define		DEBUG_PRINT(x)				//		dbg_error_printf x
	#define		DEBUG_CONDITION_PRINT(c,p)	//	if (c)   dbg_error_printf p;
	#define		DEBUG_SELECT_PRINT(c,t,f)	//	(c) ?  dbg_error_printf t : dbg_error_printf f
	#define		WRITE_DBG_LOG(x)
	#define DEBUG_PAUSE(x)
#endif

#if  _DEBUG
	#define BMI_ASSERT_MSG(x, msg)	if (!(x)) {PRINT(("BMI_ASSERT fail!!!!\n\t %s\n",msg));		DEBUG_PAUSE(1);	}				
	#define BMI_ASSERT(x)	if (!(x)) {PRINT(("BMI_ASSERT fail!!!!\n"));	DEBUG_PAUSE(1);	}				
	#define BMI_WARNING(x)	if (!(x)) {PRINT(("BMI_WARNING!!!!\n"));		DEBUG_PAUSE(1);	}			
#else
	#ifdef _FINAL
		#define BMI_ASSERT_MSG(x, msg)	if (!(x)) { WRITE_LOG(("ASSERT fail on %s, line %d !!! \n\t %s\n", __FILE__, __LINE__, msg)); PRINT_SCREEN(("ASSERT fail on %s, line %d !!! \n\t %s\n", __FILE__, __LINE__, msg));}		
		#define BMI_ASSERT(x)	if (!(x)) {WRITE_LOG(("BMI_ASSERT fail on %s, line %d\n", __FILE__, __LINE__));		PRINT_SCREEN(("BMI_ASSERT fail on %s, line %d\n", __FILE__, __LINE__));	}				
		#define BMI_WARNING(x)	if (!(x)) {WRITE_LOG(("BMI_WARNING on %s, line %d\n", __FILE__, __LINE__));	}				
	#else
		#define BMI_ASSERT_MSG(x, msg)	if (!(x)) { WRITE_LOG(("ASSERT fail on %s, line %d !!! \n\t %s\n", __FILE__, __LINE__, msg)); PRINT_SCREEN(("ASSERT fail on %s, line %d !!! \n\t %s\n", __FILE__, __LINE__, msg));exit(-1);	}		
		#define BMI_ASSERT(x)	if (!(x)) {WRITE_LOG(("BMI_ASSERT fail on %s, line %d\n", __FILE__, __LINE__));		PRINT_SCREEN(("BMI_ASSERT fail on %s, line %d\n", __FILE__, __LINE__)); exit(-1);	}				
		#define BMI_WARNING(x)	if (!(x)) {PRINT(("BMI_WARNING on %s, line %d\n", __FILE__, __LINE__));	}				
	#endif
#endif



#if _WINDOWS
#define BMI_PRAGMA_MSG(x)	#x
#define BMI_PRAGMA_MSG_TMP00(x)	BMI_PRAGMA_MSG(x)
#define BMI_PRAGMA_MSG_TMP	__FILE__ "(" BMI_PRAGMA_MSG_TMP00(__LINE__) ") : TODO: "
#define BMI_PRAGMA_TODO_MSG(x)		 message(BMI_PRAGMA_MSG_TMP x)
#else
#define BMI_PRAGMA_TODO_MSG(x)
#endif


#ifdef __cplusplus
extern "C" {
#endif
#if _DEBUG
BMI_J2K_CODEC_EXPORTS void dbg_pause(int condition);
#endif

#if _WINDOWS
BMI_J2K_CODEC_EXPORTS	void dbg_error_printf(const char *fmt, ...);
#endif

BMI_J2K_CODEC_EXPORTS	void dbg_write_log(const char *fmt, ...);

#ifdef __cplusplus
	}
#endif