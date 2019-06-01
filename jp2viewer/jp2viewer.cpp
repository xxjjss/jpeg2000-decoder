
#include "platform.h"
#include "codec_base.h"
#include "testclock.h"
#include "debug.h"
#include "imagesaver.h"

#include <xmmintrin.h>

const char * fileName; 
#if _DEBUG
int repeat	= 30;
#else
int repeat	= 300;
#endif


bool ENABLE_GPU_DWT	= true;

BMI::ImageSavingFileType	saveType = BMI::IMAGEFILE_NONE;

int main(int argc, char * argv[])
{

	BMI::Decoder * decoder = BMI::Decoder::Get();
	int mainver, subver, branchver;
	int releaseY, releaseM, releaseD;
	decoder->GetVersion(mainver, subver, branchver,  releaseY, releaseM, releaseD);
	PRINT_SCREEN(("BMI Jpeg2000 decoder version %d.%d.%d\nReleased on %d - %d - %d\n", mainver, subver, branchver, releaseY, releaseM, releaseD));

	int threadNum = 0;

	if (argc >= 2)
	{
		fileName = argv[1];
	}
	else
	{
		PRINT_SCREEN(("Usage :\n"));
		PRINT_SCREEN(("jp2viewer filename [enableGPU] [threadNum] [repeatTime] [saveoutput] [DWT_level] [Component]\n"));
		PRINT_SCREEN(("filename		: Input jpeg2000 format file\n"));
		PRINT_SCREEN(("enableGpu	: Optional, default 1: enable GPU decoding to speedup\n\t\t  0: disable GPU, using pure CPU decoding\n"));
		PRINT_SCREEN(("threadNum	: Optional, create how many decoding thread\n\t\t  default 0 : will base on CPU number\n"));
		PRINT_SCREEN(("repeatTime	: Optional, run decode for how many time? default 300\n"));
		PRINT_SCREEN(("saveoutput	: Optional, 1: save bmp output 2: save tiff output, default 0: not saving\n"));
		PRINT_SCREEN(("DWT_level	: Optional, decode DWT level, default -1. If input number\n\t\t  equal or greater than DWT level or less than 0, ignore\n"));
		PRINT_SCREEN(("Component	: Optional, check out component, Will generate grey picture\n\t\t  use the component. default -1 will use the first 3 components\n"));

		exit(0);
	}

	BMI::EnginConfig enginConfig;

	if (argc >= 3)
	{
		enginConfig.mUsingGpu = (atoi(argv[2]) != 0);
	}

	if (argc >= 4)
	{
		threadNum = atoi(argv[3]);
	}

	if (argc >= 5)
	{
		int fm = atoi(argv[4]);
		if (fm != 0)
		{
			repeat = fm;
		}
	}

	if (argc >= 6)
	{
		int t = atoi(argv[5]);
		if (t <= 0 || t >= BMI::IMAGEFILE_UNKNOWN)
		{
			saveType  = BMI::IMAGEFILE_NONE;
		}
		else
		{
			saveType =  (BMI::ImageSavingFileType)t;
		}

	}


	enginConfig.mThreadNum = threadNum;
	enginConfig.mInputQueueLen = 1;
	enginConfig.mOutputQueueLen = 1;

	threadNum = decoder->Init(&enginConfig);		// for single frame
	ENABLE_GPU_DWT = enginConfig.mUsingGpu;

	BMI::DecodeJobConfig config;
	decoder->GetJobConfig(config);
	if (saveType)
	{
		config.mOutputFormat = IMAGE_FOTMAT_24BITS_RGB;
	} 
	config.mCheckOutComponent = -1;
	config.mDwtDecodeTopLevel = -1;
//	config.mIgnoreComponents = 0xf0;	
	if (argc >= 7)
	{
		config.mDwtDecodeTopLevel = atoi(argv[6]);
		if (config.mDwtDecodeTopLevel <= 0)
		{
			config.mDwtDecodeTopLevel = -1;
		}
	}
// 	config.mDwtDecodeTopLevel = 4;
	if (argc >= 8)
	{
		config.mCheckOutComponent = atoi(argv[7]);
	}
	decoder->SetJobConfig(config);

	FILE * fp;
	FOPEN(fp, fileName, "rb");
	if (fp)
	{
		PRINT(("file %s opened\n", fileName));
		fseek(fp, 0, SEEK_END);
		int bufSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		void * buf = malloc(bufSize);

		FREAD_S(buf, bufSize, 1, bufSize, fp);
		fclose(fp);


		double t, totalT = 0.0f, minT = 999.9f, maxT = 0.0f;
#if _TIME_TRACKING_	&& !_TIME_TRACK_FIRST_CLOCK_ONLY
		unsigned int total_T1 = 0;
		unsigned int total_Thread_0 = 0;
		unsigned int total_Thread_1= 0;
		unsigned int total_dwt_tail = 0;
		unsigned int total_cuda_time = 0;
		unsigned int cuda_waiting = 0;
		unsigned int cuda_buf_copy = 0;
		unsigned int cuda_buf_sync = 0;
		unsigned int cuda_cal = 0;
		unsigned int cuda_cal_sync = 0;
		unsigned int cuda_init_quit = 0;
		unsigned int cuda_jobs	=0;
		double ave_thread_waiting = 0.0f;
		int ave_dwt_waiting;
		double ave_T1;

#endif	// _TIME_TRACKING_

#ifndef  _DEBUG
		int handler	= decoder->Decode(buf, bufSize, t);
		decoder->Release(handler);
#else
		int handler;
#endif

		RESETALLCLOCK;

		int outSizeX, outSizeY;

		

		for (int i = 1; i <= repeat; ++i)
		{


			handler = decoder->Decode(buf, bufSize, t);

			if (handler == BMI::INVALID)
			{
				PRINT(("skip %d\n", i));
			}

			totalT += t;
			minT = minT<t ? minT : t;
			maxT = maxT>t ? maxT : t;

			PRINT_SCREEN(("[%d] : cost %f msec\n", i, t));
			BMI::Status status = decoder->CheckStatus(handler);


			if (status != BMI::READY)
			{
				PRINT_SCREEN(("Status error  with code : %d !!\n", status));
				decoder->Release(handler);
			}
			else
			{

				PRINT_SCREEN(("Decode SUCCEED!!\n"));
				BMI::CodeStreamProperties properties;
				decoder->CheckProperties(handler, properties);
				int picSize;

 				const void * resultBuf = NULL;
				resultBuf =  decoder->GetResult(handler, picSize);
//				resultBuf = (void *)decoder->GetDecodedComponentFloat(handler,picSize, 1);
//				resultBuf = (void *)decoder->GetDecodedComponentInt(handler,picSize, 3);
//				resultBuf =  decoder->GetResult(handler, picSize);

				outSizeX = properties.mOutputSize.x;
				outSizeY = properties.mOutputSize.y;

			

#if _TIME_TRACKING_ && !_TIME_TRACK_FIRST_CLOCK_ONLY
				// T1 time
				double ave = 0;
				for (int k = 0; k < threadNum; ++k)
				{
					total_T1 += static_cast<unsigned int>(GETCLOCKTIME(1+k) * 100.0f);
					ave += GETCLOCKTIME(1+k);
					PRINT_SCREEN(("Thread %d call decode_cb_stripe %d times cost %f\n",k, GETCLOCKHIT(1+k), GETCLOCKTIME(1+k)));
				}
				if (threadNum > 1)
				{
					PRINT_SCREEN((" Ave a thread cost %f in T1\n",  ave /threadNum ));
				}
				ave_T1 = ave /threadNum;

				// thread_waiting 0:
				ave = 0.0;
				if (enginConfig.mUsingGpu)
				{
					ave_thread_waiting = 0.0;
					int  t1 = 0/*, t2 = 0, t3 = 0*/; 
					for (int k = 0; k < threadNum; ++k)
					{
						t1 = static_cast<unsigned int>(GETCLOCKTIME(10 + k) * 100.0f);
						total_Thread_0 += t1 /*+ t3*/;//static_cast<unsigned int>(GETCLOCKTIME(10 + k) * 100.0f);
						ave += GETCLOCKTIME(10 + k);
						PRINT_SCREEN(("Thread %d waste  %f in geting buffer \n",k, t1/100.0f));
					}
					ave_thread_waiting = ave / threadNum;
					PRINT_SCREEN(("Ave each thread waste %f msec in geting buffer\n", ave_thread_waiting));

					// thread_waiting 1:
					ave = 0.0;

					for (int k = 0; k < threadNum; ++k)
					{
						total_Thread_1 += static_cast<unsigned int>(GETCLOCKTIME(20 + k) * 100.0f);
						ave += GETCLOCKTIME(20 + k);
						PRINT_SCREEN(("Thread %d waste  %f in waiting for waiting mutex\n",k, GETCLOCKTIME(20 + k)));
					}
					PRINT_SCREEN(("Ave each thread waste %f msec\n", ave / threadNum));


					ave_thread_waiting += ave / threadNum;
					// DWT tail
					ave_dwt_waiting = (int)GETCLOCKTIME(29);
					total_dwt_tail += static_cast<unsigned int>(GETCLOCKTIME(29) * 100.0f);
					PRINT_SCREEN(("Wait %f msec for cuda finish DWT\n",GETCLOCKTIME(29) ));
					RESETCLOCK(29);

					// Cuda job time
					total_cuda_time += static_cast<unsigned int>(GETCLOCKTIME(19) * 100.0f);
					cuda_buf_copy	+=  static_cast<unsigned int>(GETCLOCKTIME(30) * 100.0f);
					cuda_buf_sync	+=  static_cast<unsigned int>(GETCLOCKTIME(31) * 100.0f);
					cuda_cal		+=  static_cast<unsigned int>(GETCLOCKTIME(32) * 100.0f);
					cuda_cal_sync	+=  static_cast<unsigned int>(GETCLOCKTIME(33) * 100.0f);
					cuda_init_quit	+= static_cast<unsigned int>(GETCLOCKTIME(34) * 100.0f);
					cuda_waiting	+= static_cast<unsigned int>(GETCLOCKTIME(35) * 100.0f);
					cuda_jobs		+= GETCLOCKHIT(35);
					PRINT_SCREEN(("Cuda thread cost %f msec CPU time \n copy_buf sync_buf calculat cal_sync mutex_wt(job_num)\n   %2.4f   %2.4f   %2.4f   %2.4f   %2.4f(%d))\n",
						GETCLOCKTIME(19), GETCLOCKTIME(30), GETCLOCKTIME(31), GETCLOCKTIME(32), GETCLOCKTIME(33), GETCLOCKTIME(35), GETCLOCKHIT(35) ));
					PRINT_SCREEN(("\n"));

				}
				else
				{
					for (int k = 0; k < threadNum; ++k)
					{
						ave += GETCLOCKTIME(10 + k);
						PRINT_SCREEN(("Thread %d cost %f in upload buf\n",k, GETCLOCKTIME(10 + k)));
					}
					PRINT_SCREEN(("Ave each thread cost %f msec in upload buf and DWT \n", ave / threadNum));

					ave = 0;
					for (int k = 0; k < threadNum; ++k)
					{
						total_Thread_1 += static_cast<unsigned int>(GETCLOCKTIME(20 + k) * 100.0f);
						ave += GETCLOCKTIME(20 + k);
						PRINT_SCREEN(("Thread %d cost %f in DWT\n",k, GETCLOCKTIME(20 + k)));
					}
					PRINT_SCREEN(("Ave each thread cost %f msec in unfinished DWT\n", ave / threadNum));

					ave = 0;
					for (int k = 0; k < threadNum; ++k)
					{
						total_Thread_1 += static_cast<unsigned int>(GETCLOCKTIME(30 + k) * 100.0f);
						ave += GETCLOCKTIME(30 + k);
						PRINT_SCREEN(("Thread %d cost %f in MCT\n",k, GETCLOCKTIME(30 + k)));
					}
					PRINT_SCREEN(("Ave each thread cost %f msec in MCT\n", ave / threadNum));

					PRINT_SCREEN(("Main thread cost cost %f msec in job sync\n", GETCLOCKTIME(40)));

					PRINT_SCREEN(("\tMain\t0\t1\t2\t3\t4\t5\t6\n"));
					for (int k = -1; k <= threadNum-2 && k <=  6; ++k)
					{
						double m;
						if (k == -1)
						{
							m = GETCLOCKTIME(threadNum) + GETCLOCKTIME(10 + threadNum - 1) + GETCLOCKTIME(20 + threadNum - 1) + GETCLOCKTIME(30 + threadNum - 1) + GETCLOCKTIME(40);
						}
						else
						{
							m = GETCLOCKTIME(1+k) + GETCLOCKTIME(10 + k) + GETCLOCKTIME(20 + k) + GETCLOCKTIME(30 + k);
						}
						PRINT_SCREEN(("\t%3.3f", m ));
					}

					PRINT_SCREEN(("\n=========================================\n\n"));

				}

				RESETALLCLOCK;

#endif


				if (resultBuf && saveType && (config.mOutputFormat == IMAGE_FOTMAT_24BITS_RGB || config.mOutputFormat == IMAGE_FOTMAT_48BITS_RGB 
					|| config.mOutputFormat == IMAGE_FOTMAT_24BITS_3COMPONENTS 
					|| config.mOutputFormat == IMAGE_FOTMAT_48BITS_3COMPONENTS))
				{
// 					static bool saved = false;
// 					if ( !saved)
					{
						char filename[255];
						SPRINTF_S((filename, 255, "decode_out%04d.%s",i,(saveType == BMI::IMAGEFILE_BMP ? "bmp" : (filename,saveType == BMI::IMAGEFILE_TIFF ? "tiff" : "") )));

						BMI::ImageSavingResult ret = BMI::SaveImage(properties , filename, saveType, resultBuf, picSize);
						if ( ret == BMI::IMAGESAVE_SUCCEED)
						{
// 							saved = true;
							PRINT_SCREEN(("save result into %s\n", filename));
						}
						else
						{
							PRINT_SCREEN(("save result error :"));
							switch (ret)
							{
							case BMI::IMAGESAVE_FILE_OPEN_FAILED:
								PRINT_SCREEN(("FILE OPEN FAILED\n"));
								break;;
							case BMI::IMAGESAVE_FILE_TYPE_UNKNOWN:
								PRINT_SCREEN(("UNKNOWN FILE TYPE\n"));
								break;
							case BMI::IMAGESAVE_PARAMETERS_ERROR:
								PRINT_SCREEN(("PARAMETERS ERROR"));
							default:
								break;
							}

						}
					}
				}

				decoder->Release(handler);

			}
		}


		PRINT_SCREEN(("min: %f  ave: %f  max: %f \n", minT, totalT / repeat, maxT));
#if _TIME_TRACKING_ && !_TIME_TRACK_FIRST_CLOCK_ONLY
		PRINT_SCREEN(("Each thread ave cost %f in T1 decoding\n",total_T1/ (100.0f * threadNum * repeat) ));
		if (enginConfig.mUsingGpu)
		{
			PRINT_SCREEN(("Each thread ave waste %f in waiting\n",(total_Thread_0 + total_Thread_1) / (100.0f * threadNum * repeat) ));
			PRINT_SCREEN(("Aveage wait CUDA finished DWT cost %f \n",total_dwt_tail / (100.0f * repeat) ));
			PRINT_SCREEN(("Aveage CUDA thread use %f CPU time\n",total_cuda_time / (100.0f * repeat) ));
			PRINT_SCREEN(("  CUDA async copy %2.4f copy_sync %2.4f calculate %2.4f cal_sync %2.4f \n  Cuda init_quit %2.4f  waiting mutex %2.4f\n",
				cuda_buf_copy/ (100.0f * repeat),
				cuda_buf_sync/ (100.0f * repeat),
				cuda_cal/ (100.0f * repeat),
				cuda_cal_sync/ (100.0f * repeat),
				cuda_init_quit/ (100.0f * repeat),
				cuda_waiting/ (100.0f * repeat)));
		}
#endif


	}
	else
	{
		PRINT_SCREEN(("file %s opened failed\n", fileName));

	}

	decoder->Terminate();

	return 0;
	
}
