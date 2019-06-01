#include "codecconcrete.h"
#include "debug.h"
#include "testclock.h"
#include "cudaInterface.h"
#include "bmi_cpu_dwt.h"

namespace	BMI
{

	//////////////////////////////////////////////////////////////////////////
	// constructor and destructor
	//////////////////////////////////////////////////////////////////////////
	EncoderConcrete::EncoderConcrete()
	{

	}

	EncoderConcrete::~EncoderConcrete()
	{

	}

	//////////////////////////////////////////////////////////////////////////
	//	public member function
	//////////////////////////////////////////////////////////////////////////
	int EncoderConcrete::Init(const P_EnginConfig enginConfig /*= NULL*/)
	{

		return	0;
	}

	CodeStreamHandler EncoderConcrete::Encode(const void * codeStreamBuffer, int bufSize, InputProperties	* inputInfor/* = NULL*/)
	{
		return 0;

	}

	 CodeStreamHandler EncoderConcrete::Encode(const void * codeStreamBuffer, int bufSize, double & time, InputProperties	* inputInfor/* = NULL*/)
	 {
		 return 0;

	 }


	Status	EncoderConcrete::CheckStatus(CodeStreamHandler handler) const
	{
		return INVALID_STATUS;
	}

	bool	EncoderConcrete::CheckProperties(CodeStreamHandler handler, CodeStreamProperties & properties) const
	{
		return true;
	}

	const void *	EncoderConcrete::GetResult(CodeStreamHandler handler, int & bufSize)
	{
		return NULL;
	}

	 void	EncoderConcrete::Release(CodeStreamHandler handler)
	 {

	 }

	void	EncoderConcrete::GetJobConfig(EncodeJobConfig & config, CodeStreamHandler handler/* = INVALID*/) const
	{

	}


	THREAD_ENTRY WINAPI EncoderConcrete::Thread_Encode(PVOID pParam)
	{

		return 0;
	}
}