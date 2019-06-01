
#include "codec_base.h"
#include "codecconcrete.h"

#include "debug.h"
#include "debugtools.h"

#ifdef __cplusplus
extern "C" {
#endif

#if _DEBUG
void dbg_pause(int condition)
{
	if (condition)
	{
		int set_break_point_here_for_debuging = 1;
	}
}
#endif

#if _WINDOWS
void dbg_error_printf(const char *fmt, ...)
{
	va_list ap;
 	va_start(ap, fmt);
	char dgbString[255];
	vsprintf_s(dgbString, 255, fmt, ap);
	OutputDebugStringA(dgbString);
	va_end(ap);
}
	
#endif

BMI_J2K_CODEC_EXPORTS	void dbg_write_log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char dgbString[255];
#if _WINDOWS
	vsprintf_s(dgbString, 255, fmt, ap);
#else
	vsprintf(dgbString, fmt, ap);
#endif
	WriteLog(dgbString);
	va_end(ap);

}

#ifdef __cplusplus
}
#endif

#if _MAC_OS || _LINUX
#if _MAC_OS
void mac_sprintf_s( char * buf, int buf_len, const char * fmt, ...)
#else
void linux_sprintf_s( char * buf, int buf_len, const char * fmt, ...)
#endif
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
}

#endif

namespace BMI
{
	//////////////////////////////////////////////////////////////////////////
	// Some function for initialization
	//////////////////////////////////////////////////////////////////////////


	// provide a concrete instance for external use
	Decoder *	Decoder::Get()
	{
		if (!DecoderConcrete::IsCreated())
		{
			DecoderConcrete::Create();
		}
		return reinterpret_cast<Decoder *>(DecoderConcrete::GetInstance());	
	}

	void	Decoder::Terminate()
	{
		if (DecoderConcrete::IsCreated())
		{
			DecoderConcrete::GetInstance()->Destory();
		}
	}



	// provide a concrete instance for external use
	Encoder *	Encoder::Get()
	{
		if (!EncoderConcrete::IsCreated())
		{
			EncoderConcrete::Create();
		}
		return reinterpret_cast<Encoder *>(EncoderConcrete::GetInstance());
	}

	void	Encoder::Terminate()
	{
		if (EncoderConcrete::IsCreated())
		{
			EncoderConcrete::GetInstance()->Destory();
		}
	}

}
