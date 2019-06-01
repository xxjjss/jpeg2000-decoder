//
// For debug
#pragma  once
#include "platform.h"


#define		_ENABLE_MEMORY_CHECK	1

#if _ENABLE_MEMORY_CHECK
#define		CHECK_MEMORY_CREATE(num)			CreateMemoryCheck((num))
#define		CHECK_MEMORY_SET(id,buf,bufSize)	MemoryCheckSet((id), reinterpret_cast<const void *>(buf), static_cast<unsigned int>(bufSize))
#define		CHECK_MEMORY_CMP(id,buf,bufSize)	MemoryCheckCompare((id), reinterpret_cast<const void *>(buf), static_cast<unsigned int>(bufSize))
#define		CHECK_MEMORY_CCR(buf,bufSize)		CheckCCR(reinterpret_cast<const void *>(buf), static_cast<unsigned int>(bufSize))

#else
#define		CHECK_MEMORY_CREATE(num)	
#define		CHECK_MEMORY_SET(id,x,s)	
#define		CHECK_MEMORY_CMP(id,x,s)	true
#endif	// _ENABLE_MEMORY_CHECK

#if _ENABLE_MEMORY_CHECK
BMI_J2K_CODEC_EXPORTS	unsigned int CheckCCR(const void * buf, unsigned int bSize);
BMI_J2K_CODEC_EXPORTS void CreateMemoryCheck(int num = 10);
BMI_J2K_CODEC_EXPORTS void MemoryCheckSet(int id, const void * buf, unsigned int bufSize);
BMI_J2K_CODEC_EXPORTS bool MemoryCheckCompare(int id, const void * buf, unsigned int bufSize);
#endif	// _ENABLE_MEMORY_CHECK

void WriteLog(const char * String);



#if _DEBUG 
#define WRITE_TO_FILE	1
#else	// Release 
#define WRITE_TO_FILE	0
#endif		// _DEBUG

//template	<class T>
//BMI_J2K_CODEC_EXPORTS void print_buf_to_file(const T * buf, unsigned int buf_size_b, const char * filename, int cols, bool lineNumber = false, bool colNumber = false);
BMI_J2K_CODEC_EXPORTS void print_buf_to_file(const int * buf, unsigned int buf_size_b, const char * filename, int pitch, int cols, bool lineNumber = false, bool colNumber = false);


#if WRITE_TO_FILE	

#define PRINT_BUF_TO_FILE_4BYTES(x)	print_buf_to_file x
// #define PRINT_BUF_TO_FILE_4BYTES(x)	print_buf_to_file<int> x
// #define PRINT_BUF_TO_FILE_2BYTES(x)	print_buf_to_file<short> x
// #define PRINT_BUF_TO_FILE_BYTE(x)	print_buf_to_file<char> x

#else
#define PRINT_BUF_TO_FILE_4BYTES(x)	
#define PRINT_BUF_TO_FILE_2BYTES(x)	
#define PRINT_BUF_TO_FILE_BYTE(x)	
#endif		// WRITE_TO_FILE
