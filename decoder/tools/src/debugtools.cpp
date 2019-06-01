#include "debugtools.h"
#include "stdio.h"
#include "singleton.h"
#include "debug.h"
#include "codec.h"
#include "codec_base.h"

#if _ENABLE_MEMORY_CHECK

class MemoryCheck : public Singleton<MemoryCheck>
{
public:

	MemoryCheck():
	  mResult(NULL),
	  mSize(NULL),
	  mPairNum(0)
	  {
	  }

	~MemoryCheck() 
	{
		SAFE_DELETE_ARRAY(mResult);
		SAFE_DELETE_ARRAY(mSize);
	}

	void	init(int pairNum)
	{
		if (pairNum != mPairNum && pairNum > 0)
		{
			mPairNum = pairNum;
			SAFE_DELETE_ARRAY(mResult);
			SAFE_DELETE_ARRAY(mSize);
			mResult = new unsigned int[mPairNum];
			mSize = new unsigned int[mPairNum];
		}

		if (mPairNum)
		{
			memset(mSize, 0, mPairNum*sizeof(unsigned int));
		}
	}

	void	set(int id, const void * buf, unsigned int bufSize)
	{
		BMI_ASSERT(buf && bufSize > 0 && id < mPairNum);
		mResult[id] = MemoryCheck::getCCRValue(buf, bufSize);
		mSize[id] = bufSize;
	}

	bool		compare(int id, const void * buf, unsigned int bufSize)
	{
		BMI_ASSERT(buf && bufSize > 0&& id < mPairNum);
		bool ret = false;
		if (mSize[id] == bufSize)
		{
			ret = (MemoryCheck::getCCRValue(buf, bufSize) == mResult[id]);
		}

		return ret;
	}


private:

	unsigned int	* mResult;
	unsigned int	* mSize;
	int		mPairNum;
public:
	static unsigned int getCCRValue(const void * buf, unsigned int bufSize)
	{
		unsigned int val = 0;
		const unsigned int * intbuf = reinterpret_cast< const unsigned int *>(buf);
		int intSize = bufSize / (sizeof(unsigned int) / sizeof(unsigned char));
		if (intSize)
		{
			for (int i = 0; i < intSize; ++i)
			{
				val ^= *(intbuf++);
			}
		}

		int byteSize = bufSize % (sizeof(unsigned int) / sizeof(unsigned char));
		if (byteSize)
		{
			const unsigned char * byteBuf = reinterpret_cast<const unsigned char *>(intbuf);
			unsigned int v = 0;
			for (int i = 0; i < byteSize; ++i)
			{
				v <<= 8;
				v += *(byteBuf++);
			}

			val ^= v;
		}

		return val;
	}
};

unsigned int CheckCCR(const void * buf, unsigned int bSize)
{
	return MemoryCheck::getCCRValue(buf, bSize) ;
}


void CreateMemoryCheck(int num /*= 10*/)
{
	if (!MemoryCheck::IsCreated())
	{
		MemoryCheck::Create();
	}
	MemoryCheck::GetInstance()->init(num);
}

void MemoryCheckSet(int id, const void * buf, unsigned int bufSize)
{
	BMI_ASSERT(MemoryCheck::IsCreated());
	MemoryCheck::GetInstance()->set(id, buf, bufSize);
}

bool MemoryCheckCompare(int id, const void * buf, unsigned int bufSize)
{
	BMI_ASSERT(MemoryCheck::IsCreated());
	return MemoryCheck::GetInstance()->compare(id, buf, bufSize);
}
#endif	// _ENABLE_MEMORY_CHECK

#if _WINDOWS
const char * LogFileName = ".\\jp2codecLog.txt";
#else
const char * LogFileName = "./jp2codecLog.txt";
#endif

void WriteLog(const char * logString)
{

	static bool needInit = true;
	static MUTEX_HANDLE fileMutex;
	if (needInit)
	{
		CREATE_MUTEX(fileMutex);
		needInit = false;
	}

	FILE * fLog = NULL;

	LOCK_MUTEX(fileMutex);
#if _MAC_OS || _LINUX
	fLog = fopen(LogFileName, "at+");
#else
	fopen_s(&fLog, LogFileName, "at+");
#endif
	if (fLog)
	{
// #if _WINDOWS
// 		fprintf(fLog, "[Thread %8d] %s", GetCurrentThreadId(),logString);
// #else
		fprintf(fLog, "%s", logString);
// #endif
		fclose(fLog);
		DEBUG_PRINT(("%s", logString));
	}
	else
	{
		
		printf("%s open failed => LOG:: %s\n",LogFileName,  logString);
	}
	RELEASE_MUTEX(fileMutex);

}


// template	<class T1/*, class T2*/>
// void print_buf_to_file(const T1 * buf, unsigned int buf_size_b, const char * filename, int cols, bool lineNumber/* = false*/, bool colNumber /*= false*/)
void print_buf_to_file(const int * buf, unsigned int buf_size_b, const char * filename, int pitch, int cols, bool lineNumber/* = false*/, bool colNumber /*= false*/)

{
	FILE * fs; 
	int step, wordwidth;
	unsigned int offset = 0, line = 0;

	BMI_ASSERT(cols < 9999 && cols > 0);


#if _MAC_OS || _LINUX
	fs = fopen(filename,"wb");
#else	
	fopen_s(&fs, filename,"wb");
#endif

	if (fs)
	{

// 		step = sizeof(T1);
 		step = sizeof(int);
		wordwidth = 9;// (sizeof(T2)<<1) + 1;
		char formatstring[100];

		if (colNumber)
		{
			if (lineNumber)
			{
				fprintf(fs, "      ");	// 7 spaces for line number
			}
#if _MAC_OS || _LINUX
			sprintf(formatstring, "%%%dd\0", wordwidth);
#else
			sprintf_s(formatstring,100, "%%%dd\0", wordwidth);
#endif
			for (int i = 1; i <= cols; ++i)
			{
				fprintf(fs, formatstring, i);
			}
			fprintf(fs, "\n");
		}

		int colId = 0;
		while(offset < buf_size_b)
		{
			if (lineNumber && colId == 0)
			{
				fprintf(fs, "%5d:", ++line);
			}
#if _MAC_OS || _LINUX
			sprintf(formatstring, " %%0%dx\0 ", wordwidth-1);
#else
			sprintf_s(formatstring,100, " %%0%dx\0 ", wordwidth-1);
#endif
			

			//			T2 dout = (T2)*buf++
// 			fprintf(fs, formatstring, static_cast<T2>(*buf++));
 			fprintf(fs, formatstring, static_cast<int>(*buf++));
			offset += step;
			++colId;
			if (colId >= cols)
			{
				fprintf(fs, "\n");
				colId = 0;
				buf += pitch - cols;
				fflush(fs);
			}

		}

		fprintf(fs, "\n\n");
		fclose(fs);
	}		
}

