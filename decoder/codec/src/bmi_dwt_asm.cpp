
#include "bmi_cpu_dwt.h"
#include "debug.h"

#if ENABLE_ASSEMBLY_OPTIMIZATION

#if INTEL_ASSEMBLY_32

#if _DEBUG
#define		DEBUG_XMM_VAL		__m128 debugQ
#define		CHECK_VAL(x)		MOVDQA debugQ, x
#else
#define		DEBUG_XMM_VAL
#define		CHECK_VAL(x)	
#endif


#define DWT_PIXELS_128(a,b,c,filter)	\
	MOVDQA XMM6, XMM##b		\
	ADDPS  XMM6, XMM##c		\
	MULPS  XMM6, filter		\
	SUBPS  XMM##a, XMM6
#define LOAD_SIX_PIXELS
#define LOAD_TWO_PIXELS_TO_45
#define LOAD_TWO_PIXELS_TO_23
#define LOAD_TWO_PIXELS_TO_01
namespace	BMI
{

#define 	LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2, P1, xmmt)		\
	__asm	MOVAPS	xmmt, [addr]					\
	__asm	ADD addr, 0x10						\
	__asm	MOVAPS	xmmm, maskSign				\
	__asm	PANDN	xmmm, xmmt					\
	__asm	CVTDQ2PS xmmm, xmmm					\
	__asm	PAND	xmmt, maskSign				\
	__asm	PSRLD	xmmt, 31					\
	__asm	CVTDQ2PS xmmt, xmmt					\
	__asm	MULPS	xmmt, N2					\
	__asm	ADDPS	xmmt, P1					\
	__asm	MULPS	xmmm, xmmt					\
	__asm	MULPS	xmmm, coef					

#define 	LOAD_NO_TRANSFER_FORWARD(xmmm, addr, coef)		\
	__asm	MOVAPS	xmmm, [addr]					\
	__asm	ADD addr, 0x10						\
	__asm	MULPS	xmmm, coef


#define 	STORE_FORWARD(xmma, addr)		\
	__asm	MOVAPS	[addr], xmma				\
	__asm	ADD addr, 0x10

	void	hdwt_analyst_even(float * result, int loopLength, int longExtra, __m128 coef128,  int low_width, int lhParity, float *  restoreResult = NULL, int restoreLength = INVALID)
	{

		DEBUG_XMM_VAL;

		if (loopLength)
		{
			if (lhParity)
			{
				int tail = loopLength * 4 - low_width;
				if (tail >= 0)
				{
					result[loopLength * 8 - tail - 1 ] = result[loopLength * 8 - tail - 2];		// mirror the band_h last one
				}
				else
				{
					result[loopLength * 8 + 4] = result[loopLength * 8 - 1];		// mirror the band_h last one
				}
			}

			loopLength += longExtra;

			if (!restoreResult)
			{

				__asm
				{
					MOVAPS XMM7, coef128

					MOV ECX, loopLength
					MOV ESI, result
					MOVAPS	XMM0, [ESI + 0x10]

analyst_loop:
					MOVAPS	XMM1,[ESI]	// L0L1L2L3
					CHECK_VAL(XMM1)
					MOVAPS	XMM2, [ESI + 0x10]	// H0H1H2H3
					MOVAPS	XMM3, XMM2
					CHECK_VAL(XMM2)
					PSLLDQ	XMM2, 4		
					CHECK_VAL(XMM2)
					MOVSS	XMM2, XMM0		// H(-1) H0H1H2
					CHECK_VAL(XMM2)
					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
					CHECK_VAL(XMM2)
					MULPS	XMM2, XMM7		
					CHECK_VAL(XMM2)
					SUBPS	XMM1, XMM2
					CHECK_VAL(XMM1)
					MOVAPS  XMM0, XMM3
					PSRLDQ	XMM0, 12		

					MOVAPS  [ESI], XMM1

					ADD ESI, 0x20
					LOOPNZ analyst_loop
				}		
			}
			else	// repack
			{

				BMI_ASSERT(restoreLength != INVALID);
				BMI_ASSERT(restoreLength <= loopLength * 8 + longExtra && restoreLength >= low_width * 2 - 1);
				int tailPixels = restoreLength + 8 - loopLength * 8;
				float last8Pixels[8];


				__asm{
					MOVAPS XMM7, coef128
	
					MOV ESI, result
					MOV EDI, restoreResult
					MOVAPS	XMM0, [ESI + 16]
					MOV ECX, loopLength
					DEC ECX
					JZ last_4_pixels_repack

analyst_loop_repeck:
					MOVAPS	XMM1,[ESI]	// L0L1L2L3
					CHECK_VAL(XMM1)
					MOVAPS	XMM2, [ESI + 0x10]	// H0H1H2H3
					MOVAPS	XMM3, XMM2
					CHECK_VAL(XMM2)
					PSLLDQ	XMM2, 4		
					CHECK_VAL(XMM2)
					MOVSS	XMM2, XMM0		// H(-1) H0H1H2
					CHECK_VAL(XMM2)
					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
					CHECK_VAL(XMM2)
					MULPS	XMM2, XMM7		
					CHECK_VAL(XMM2)
					SUBPS	XMM1, XMM2
					CHECK_VAL(XMM1)

					MOVAPS  XMM0, XMM3		// previous for next loop
					PSRLDQ	XMM0, 12		
					MOVAPS XMM2, XMM1
					PUNPCKLDQ	XMM1, XMM3
					PUNPCKHDQ	XMM2, XMM3

					MOVAPS  [EDI], XMM1
					MOVAPS  [EDI + 0x10], XMM2
					ADD ESI, 0x20
					ADD EDI, 0x20
					LOOPNZ analyst_loop_repeck

last_4_pixels_repack:
					MOVAPS	XMM1,[ESI]	// L0L1L2L3
					CHECK_VAL(XMM1)
					MOVAPS	XMM2, [ESI + 0x10]	// H0H1H2H3
					MOVAPS	XMM3, XMM2
					CHECK_VAL(XMM2)
					PSLLDQ	XMM2, 4		
					CHECK_VAL(XMM2)
					MOVSS	XMM2, XMM0		// H(-1) H0H1H2
					CHECK_VAL(XMM2)
					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
					CHECK_VAL(XMM2)
					MULPS	XMM2, XMM7		
					CHECK_VAL(XMM2)
					SUBPS	XMM1, XMM2
					CHECK_VAL(XMM1)

					MOVAPS XMM2, XMM1
					PUNPCKLDQ	XMM1, XMM3
					PUNPCKHDQ	XMM2, XMM3

					LEA EDI, last8Pixels
					MOVUPS  [EDI], XMM1
					MOVUPS  [EDI + 0x10], XMM2
				}
				memcpy(restoreResult +(loopLength - 1) * 8, last8Pixels, tailPixels * sizeof(float));

			}
		}

	}

	void	hdwt_analyst_odd(float * result, int loopLength, int longExtra,  __m128 coef128,  int low_width, int lhParity, float * restoreResult = NULL, int restoreLength = INVALID)
	{

		DEBUG_XMM_VAL;

		if (loopLength)
		{

			if (!lhParity)
			{
				int calWidth = ALIGN_TO_SSE_DWORDS(low_width);
				if (calWidth != low_width)
				{
					calWidth -= low_width;
					result[loopLength * 8 - calWidth - 4 ] = result[loopLength * 8 - calWidth - 5];		// mirror the band_l last one
				}		
			}

			__m128i	maskLast	= _mm_set_epi32(0xffffffff, 0, 0, 0);

			if (!restoreResult)
			{
				__asm
				{
					MOVAPS XMM7, coef128

					MOV ECX, loopLength
					MOV ESI, result


		analyst_loop:
					CMP ECX, 1
					JNG	last_4_pixels
					MOVAPS XMM0, [ESI + 0x20]
					CHECK_VAL(XMM0)
					PSLLDQ	XMM0, 12		
					JMP calculate_4_pixels
		last_4_pixels:
					CMP longExtra, 1
					JE last_4_pixels_long_extra
					MOVAPS XMM0, [ESI]
					PAND   XMM0, maskLast		
					JMP calculate_4_pixels
		last_4_pixels_long_extra:
					MOVAPS XMM0, [ESI + 0x20]
					PSLLDQ	XMM0, 12		
		calculate_4_pixels:
					CHECK_VAL(XMM0)
					MOVAPS	XMM2,[ESI]	// L0L1L2L3
					MOVAPS	XMM3, XMM2
					MOVAPS	XMM1, [ESI + 0x10]	// H0H1H2H3
					CHECK_VAL(XMM1)
					CHECK_VAL(XMM2)
					PSRLDQ	XMM2, 4		
					CHECK_VAL(XMM2)
					ORPS	XMM2, XMM0
					CHECK_VAL(XMM2)
					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
					CHECK_VAL(XMM2)
					MULPS	XMM2, XMM7		
					CHECK_VAL(XMM2)
					SUBPS	XMM1, XMM2
					CHECK_VAL(XMM1)
					MOVAPS  [ESI + 0x10], XMM1

					ADD ESI, 0x20
					LOOPNZ analyst_loop
				}		
			}
			else	// repack 
			{
				BMI_ASSERT(restoreLength != INVALID);
				BMI_ASSERT(restoreLength <= loopLength * 8 + longExtra && restoreLength >= low_width * 2 - 1);
				int tailPixels = restoreLength + 8 - loopLength * 8;
				float last8Pixels[9];

				__asm
				{

					MOVAPS XMM7, coef128

					MOV ESI, result
					MOV EDI, restoreResult
					MOV ECX, loopLength
					DEC ECX
					JZ last_4_pixels_repack

analyst_loop_repack:
					MOVAPS XMM0, [ESI + 0x20]
					PSLLDQ	XMM0, 12		
					CHECK_VAL(XMM0)

					MOVAPS	XMM2,[ESI]	// L0L1L2L3
					MOVAPS	XMM3, XMM2
					MOVAPS	XMM1, [ESI + 0x10]	// H0H1H2H3
					CHECK_VAL(XMM1)
					CHECK_VAL(XMM2)
					PSRLDQ	XMM2, 4		
					CHECK_VAL(XMM2)
					ORPS	XMM2, XMM0
					CHECK_VAL(XMM2)
					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
					CHECK_VAL(XMM2)
					MULPS	XMM2, XMM7		
					CHECK_VAL(XMM2)
					SUBPS	XMM1, XMM2
					CHECK_VAL(XMM1)
					// repack XMM1 and XMM3

					MOVAPS XMM2, XMM3
					PUNPCKLDQ	XMM2, XMM1
					PUNPCKHDQ	XMM3, XMM1

					MOVAPS  [EDI], XMM2
					MOVAPS  [EDI + 0x10], XMM3
					ADD ESI, 0x20
					ADD EDI, 0x20
					LOOPNZ analyst_loop_repack

						// tail repeck
		last_4_pixels_repack:
					CMP longExtra, 1
					JE last_4_pixels_repack_long_extra
					MOVAPS XMM0, [ESI]
					PAND   XMM0, maskLast		
					JMP calculate_4_pixels_equal
		last_4_pixels_repack_long_extra:
					MOVAPS XMM0, [ESI + 0x20]
					PSLLDQ	XMM0, 12		

		calculate_4_pixels_equal:
					MOVAPS	XMM2,[ESI]	// L0L1L2L3
					MOVAPS	XMM3, XMM2
					MOVAPS	XMM1, [ESI + 0x10]	// H0H1H2H3
					CHECK_VAL(XMM1)
					CHECK_VAL(XMM2)
					PSRLDQ	XMM2, 4		
					CHECK_VAL(XMM2)
					ORPS	XMM2, XMM0
					CHECK_VAL(XMM2)
					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
					CHECK_VAL(XMM2)
					MULPS	XMM2, XMM7		
					CHECK_VAL(XMM2)
					SUBPS	XMM1, XMM2
					CHECK_VAL(XMM1)
					// repack XMM1 and XMM3

					MOVAPS XMM2, XMM3
					PUNPCKLDQ	XMM2, XMM1
					PUNPCKHDQ	XMM3, XMM1

					LEA EDI, last8Pixels
					MOVUPS  [EDI], XMM2
					MOVUPS  [EDI + 0x10], XMM3
					CMP longExtra, 1
					JNE analyst_odd_repack_done
					MOV EAX, [ESI+0x20]
					MOV [EDI+0x20], EAX
analyst_odd_repack_done:
				}
				memcpy(restoreResult +(loopLength - 1) * 8, last8Pixels, tailPixels * sizeof(float));

			}
		}

	}

	//							analystSrc
	//			analystLeft			|		analystRight
	//						\		|		/
	//							analystTarget
	void	vdwt_analyst(unsigned char * analystSrc, unsigned char * analystLeft, unsigned char * analystRight, unsigned char * analystTarget,unsigned char * lastRight, int loopY, int loopX,  __m128 coef, int sourceLineSkip, int middleLineSkip, int targetLineSkip)
	{
		__asm
		{

			MOVAPS XMM7, coef
			MOV ESI, analystSrc
			MOV EDI, analystTarget
			MOV EAX, analystLeft
			MOV EDX, analystRight
			MOV ECX, loopY
			DEC ECX
			JZ last_line
y_loop:
			PUSH ECX
			PUSH EDX		// push right
			MOV  ECX, loopX
x_loop:
			MOVAPS XMM0, [EAX]
			MOVAPS XMM1, [EDX]
			ADDPS XMM0, XMM1
			MULPS XMM0, XMM7
			MOVAPS XMM1, [ESI]
			SUBPS XMM1, XMM0
			MOVAPS [EDI], XMM1
			ADD EAX, 0x10
			ADD EDX, 0x10
			ADD ESI, 0x10
			ADD EDI, 0x10

			LOOPNZ x_loop
			POP EAX		// pop to left
			POP ECX
			ADD ESI, sourceLineSkip
			ADD EDX, middleLineSkip
			ADD EDI, targetLineSkip
			LOOPNZ y_loop
last_line:
			MOV EDX, lastRight
			MOV ECX, loopX
last_line_loop:
			MOVAPS XMM0, [EAX]
			MOVAPS XMM1, [EDX]
			ADDPS XMM0, XMM1
			MULPS XMM0, XMM7
			MOVAPS XMM1, [ESI]
			SUBPS XMM1, XMM0
			MOVAPS [EDI], XMM1
			ADD EAX, 0x10
			ADD EDX, 0x10
			ADD ESI, 0x10
			ADD EDI, 0x10

			LOOPNZ last_line_loop
		}
	}

	void vdwt_analyst_line_func(unsigned char * dst, unsigned char * src, unsigned char * left, unsigned char * right, int loopX, FLOAT_REG coef)
	{
		__asm
		{
			MOVAPS XMM7, coef
			MOV ESI, src
			MOV EDI, dst
			MOV EAX, left
			MOV EDX, right
			MOV ECX, loopX

vdwt_analyst_line_loop:
			MOVAPS XMM0, [EAX]
			MOVAPS XMM1, [EDX]
			ADDPS XMM0, XMM1
			MULPS XMM0, XMM7
			MOVAPS XMM1, [ESI]
			SUBPS XMM1, XMM0
			MOVAPS [EDI], XMM1
			ADD EAX, 0x10
			ADD EDX, 0x10
			ADD ESI, 0x10
			ADD EDI, 0x10

			LOOPNZ vdwt_analyst_line_loop
		}

	}


	void	CpuDwtDecoder::VDWT_R97_FLOAT_ASM_32(unsigned char * target,unsigned char * H_result_up, unsigned char * H_result_down, int low_h, int high_h, int width, int linesOffsetInlow, int linesDecodeInLow, uint_8 yParity, int threadId)
	{

// 		BMI_ASSERT(linesOffsetInlow == 0 || linesOffsetInlow > 2);
// 		BMI_ASSERT(linesOffsetInlow+ linesDecodeInLow == low_h || low_h - linesOffsetInlow - linesDecodeInLow > 2);
// 
// 		int LineSkip = ALIGN_TO_SSE_DWORDS(width) * sizeof(float);
// 
// 		unsigned char * overlayBuf_Head	= mSubThreadDwtOverLayMemory + threadId * mSubThreadDwtOverLayMemoryUnitSize;
// 		unsigned char * overlayBuf_Tail = overlayBuf_Head + LineSkip;
// 		unsigned char * overlayBuf_Temp = overlayBuf_Tail + LineSkip;
// 
// 		bool isBegin, isEnd;
// 
// 		if (linesOffsetInlow)
// 		{
// 			isBegin = false;
// 		}
// 		else
// 		{
// 			isBegin = true;
// 		}
// 
// 		if (linesOffsetInlow + linesDecodeInLow == low_h)
// 		{
// 			isEnd = true;
// 		}
// 		else
// 		{
// 			isEnd = false;
// 
// 		}
// 
// 		H_result_up +=  LineSkip * linesOffsetInlow;
// 		H_result_down += LineSkip * linesOffsetInlow;
// 		target += LineSkip * linesOffsetInlow * 2;
// 
// 		int loopX =  ALIGN_TO_SSE_DWORDS(width) / 4;
// 		int loopY = linesDecodeInLow;
// 		unsigned char * src, * dst, * left, * right, * last;
// 
// 		// step 3:
// 		if (!yParity)
// 		{
// 			src = H_result_up;
// 			dst = target;
// 			right = H_result_down;
// 			left = right;
// 
// 			if (!isBegin)
// 			{
// 				left -= LineSkip;
// 			}
// 
// 			if (isEnd)
// 			{
// 				if (low_h == high_h)
// 				{
// 					last = H_result_down + LineSkip * (linesDecodeInLow - 1) ;
// 				}
// 				else
// 				{
// 					last = H_result_down + LineSkip * (linesDecodeInLow - 2);
// 				}
// 
// 			}
// 			else
// 			{
// 				last = H_result_down + LineSkip *  linesDecodeInLow;
// 			}
// 		}
// 		else
// 		{
// 		}
// 
// 		vdwt_analyst(src, left, right, dst, last, loopY, loopX, IDWT_DELTA_128, 0, 0, LineSkip);
// 
// 		// step 4
// 		if (!yParity)
// 		{
// 			src = H_result_down;
// 			left = target;
// 			dst = target + LineSkip;
// 			right = target + LineSkip * 2;
// 			if (isEnd)
// 			{
// 				last = target + (linesDecodeInLow - 1) * LineSkip * 2;
// 			}
// 			else
// 			{
// 
// 				vdwt_analyst_line(overlayBuf_Tail,
// 								H_result_up + LineSkip * linesDecodeInLow, 
// 								H_result_down + LineSkip * (linesDecodeInLow - 1),
// 								H_result_down + LineSkip * linesDecodeInLow,
// 								loopX, IDWT_DELTA_128);
// 
// 				last = overlayBuf_Tail;
// 			}
// 		}
// 		else
// 		{
// 
// 		}
// 		vdwt_analyst(src, left, right, dst, last, loopY, loopX, IDWT_GAMMA_128, 0, LineSkip, LineSkip);
// 
// 
// 		// step 5
// 		if (!yParity)
// 		{
// 			src = target;
// 			dst = src;
// 			right = target + LineSkip;
// 			if (isBegin)
// 			{
// 				left = right;
// 			}
// 			else
// 			{
// 
// 				vdwt_analyst_line(overlayBuf_Head,
// 								H_result_up - LineSkip,
// 								H_result_down - LineSkip * 2,
// 								H_result_down - LineSkip,
// 								loopX,
// 								IDWT_DELTA_128);
// 				vdwt_analyst_line(overlayBuf_Head,
// 								H_result_down - LineSkip,
// 								overlayBuf_Head,
// 								target,
// 								loopX,
// 								IDWT_GAMMA_128);
// 				left = overlayBuf_Head;
// 			}
// 
// 			if (isEnd)
// 			{
// 				if (low_h == high_h)
// 				{
// 					last = target + LineSkip * (linesDecodeInLow * 2 - 1) ;
// 				}
// 				else
// 				{
// 					last = target + LineSkip * (linesDecodeInLow * 2 - 3);
// 				}			
// 			}
// 			else
// 			{
// 				last = target + LineSkip * (linesDecodeInLow * 2 - 1) ;
// 			}
// 
// 		}
// 		else
// 		{
// 
// 		}
// 		vdwt_analyst(src, left, right, dst, last, loopY, loopX, IDWT_BETA_128, LineSkip, LineSkip, LineSkip);
// 
// 		//step 6
// 		if (!yParity)
// 		{
// 			src = target + LineSkip;
// 			dst = src;
// 			left = target;
// 			right = left + LineSkip;
// 			if (isEnd)
// 			{
// 				last = target + (linesDecodeInLow - 1) * LineSkip * 2;
// 
// 			}
// 			else
// 			{
// 				vdwt_analyst_line(overlayBuf_Temp, 
// 								H_result_up + (linesDecodeInLow + 1) * LineSkip,
// 								H_result_down + LineSkip * linesDecodeInLow ,
// 								H_result_down + LineSkip * (linesDecodeInLow + 1),
// 								loopX, IDWT_DELTA_128);
// 				vdwt_analyst_line(overlayBuf_Temp,
// 								H_result_down + LineSkip * linesDecodeInLow,
// 								overlayBuf_Tail,
// 								overlayBuf_Temp,
// 								loopX, IDWT_GAMMA_128);
// 				vdwt_analyst_line(overlayBuf_Tail,
// 								overlayBuf_Tail,
// 								target + LineSkip * (linesDecodeInLow * 2 - 1),
// 								overlayBuf_Temp,
// 								loopX, IDWT_BETA_128);
// 				last = overlayBuf_Tail;
// 
// 			}
// 		}
// 		else
// 		{
// 
// 		}
// 		vdwt_analyst(src, left, right, dst, last, loopY, loopX, IDWT_ALPHA_128, LineSkip, LineSkip, LineSkip);

	}


	void CpuDwtDecoder::HDWT_R97_FLOAT_ASM_1Line(float * result, float  * band_l, float  * band_h, float * tempBuf, int low_w, int high_w, float lowgain, float highgain, uint_8 xParity,  bool isLowBandNeedTransfer)
	{


		static const __m128i maskS = _mm_set1_epi32(0x80000000);
		static const __m128i maskV = _mm_set1_epi32(0x7fffffff);
		static const __m128	floatN2 = _mm_set_ps1(-2.0f);
		static const __m128	floatP1 = _mm_set_ps1(1.0f);

		int lhParity = (low_w == high_w ? 0 : 1);
		int width = low_w + high_w;

		float * lineL;
		float * lineH;
		__m128 lowGain;
		__m128 highGain;
		int loopLength, longExtra = 0;


		if (xParity)
		{
			lineL = band_h;
			lineH = band_l;
			highGain = _mm_set_ps1(lowgain);
			lowGain = _mm_set_ps1(highgain);
			loopLength = (ALIGN_TO_SSE_DWORDS(low_w)) / 4;
			longExtra = (ALIGN_TO_SSE_DWORDS(high_w)) / 4 - loopLength;
		}
		else
		{
			lineL = band_l;
			lineH = band_h;
			lowGain = _mm_set_ps1(lowgain);
			highGain = _mm_set_ps1(highgain);
			loopLength = (ALIGN_TO_SSE_DWORDS(high_w)) / 4;
			longExtra = (ALIGN_TO_SSE_DWORDS(low_w)) / 4 - loopLength;
		}

		BMI_ASSERT (!((int)band_l & 0x03) && !((int)band_h & 0x03 ));


		if (isLowBandNeedTransfer)
		{
			__asm
			{

				MOV EAX, lineL
				MOV EDX, lineH
				MOV EDI, tempBuf
				MOV ECX, loopLength
				MOVAPS XMM6, lowGain
				MOVAPS XMM7, highGain
				MOVAPS XMM2, maskS
				MOVAPS XMM3, floatN2
				MOVAPS XMM4, floatP1

L_transfer_load_line:
				// setp 1 and 2
				LOAD_TRANSFER_FORWARD(XMM0, EAX, XMM6, XMM2, XMM3,XMM4, XMM5)
				LOAD_TRANSFER_FORWARD(XMM1, EDX, XMM7, XMM2, XMM3,XMM4, XMM5)
				STORE_FORWARD(XMM0, EDI)
				STORE_FORWARD(XMM1, EDI)
				LOOPNZ L_transfer_load_line

				CMP longExtra, 0
				JE L_transfer_load_line_done
				LOAD_TRANSFER_FORWARD(XMM0, EAX, XMM6, XMM2, XMM3,XMM4, XMM5)
				MOVAPS [EDI], XMM0
L_transfer_load_line_done:
			}
		}
		else
		{
			if (!xParity)
			{
				__asm
				{

					MOV EAX, lineL
					MOV EDX, lineH
					MOV EDI, tempBuf
					MOV ECX, loopLength
					MOVAPS XMM6, lowGain
					MOVAPS XMM7, highGain
					MOVAPS XMM2, maskS
					MOVAPS XMM3, floatN2
					MOVAPS XMM4, floatP1

L_no_transfer_load_line:
					// setp 1 and 2
					LOAD_NO_TRANSFER_FORWARD(XMM0, EAX, XMM6)
					LOAD_TRANSFER_FORWARD(XMM1, EDX, XMM7, XMM2, XMM3,XMM4, XMM5)
					STORE_FORWARD(XMM0, EDI)
					STORE_FORWARD(XMM1, EDI)
					LOOPNZ L_no_transfer_load_line

					CMP longExtra, 0
					JE L_no_transfer_load_line_done
					LOAD_NO_TRANSFER_FORWARD(XMM0, EAX, XMM6)
					MOVAPS [EDI], XMM0
L_no_transfer_load_line_done:

				}
			}
			else
			{
				// x parity, switched L and H
				__asm
				{

					MOV EAX, lineL
					MOV EDX, lineH
					MOV EDI, tempBuf
					MOV ECX, loopLength
					MOVAPS XMM6, lowGain
					MOVAPS XMM7, highGain
					MOVAPS XMM2, maskS
					MOVAPS XMM3, floatN2
					MOVAPS XMM4, floatP1

L_parity_no_transfer_load_line:
					// setp 1 and 2
					LOAD_TRANSFER_FORWARD(XMM0, EAX, XMM6, XMM2, XMM3,XMM4, XMM5)
					LOAD_NO_TRANSFER_FORWARD(XMM1, EDX, XMM7)
					STORE_FORWARD(XMM0, EDI)
					STORE_FORWARD(XMM1, EDI)
					LOOPNZ L_parity_no_transfer_load_line

					CMP longExtra, 0
					JE L_parity__no_transfer_load_line_done
					LOAD_TRANSFER_FORWARD(XMM0, EAX, XMM6, XMM2, XMM3,XMM4, XMM5)
					MOVAPS [EDI], XMM0
L_parity__no_transfer_load_line_done:

				}
			}

		}

		if (!xParity)
		{
			hdwt_analyst_even(tempBuf, loopLength, longExtra, IDWT_DELTA_128, low_w, lhParity);
			hdwt_analyst_odd(tempBuf, loopLength, longExtra, IDWT_GAMMA_128, low_w, lhParity);
			hdwt_analyst_even(tempBuf, loopLength, longExtra, IDWT_BETA_128, low_w, lhParity);
			hdwt_analyst_odd(tempBuf, loopLength, longExtra,IDWT_ALPHA_128,low_w, lhParity, result, low_w+high_w );	// repack the result
		}
		else
		{
			hdwt_analyst_odd(tempBuf, loopLength, longExtra, IDWT_DELTA_128, high_w, lhParity);
			hdwt_analyst_even(tempBuf, loopLength, longExtra , IDWT_GAMMA_128, high_w, lhParity);
			hdwt_analyst_odd(tempBuf, loopLength, longExtra, IDWT_BETA_128, high_w, lhParity);
			hdwt_analyst_even(tempBuf, loopLength, longExtra, IDWT_ALPHA_128,high_w, lhParity, result, low_w+high_w);	// repack the result
		}

	}



	void	CpuDwtDecoder::RMCT_R97_FLOAT_ASM(unsigned char  * result, float * comp0, float * comp1, float * comp2, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat, short checkOutComp)
	{

		if (outFormat != IMAGE_FOTMAT_32BITS_ARGB && outFormat != IMAGE_FOTMAT_24BITS_RGB)
		{
			RICT_97_FLOAT(result, comp0, comp1, comp2, resultPitch, width, height, bitDepth, outFormat, checkOutComp);
			return;
		}
#pragma BMI_PRAGMA_TODO_MSG("Implement MCT 97 in Assemblly for other format")
#pragma BMI_PRAGMA_TODO_MSG("Implement MCT 97 in Assemblly use alligned memory for better performance, replace MOVUPS with MOVAPS")



		static __m128	c2_c0_ratio_128 = _mm_set_ps1(1.402f);
		static __m128	c1_c1_ratio_128 = _mm_set_ps1(-0.34413f);
		static __m128	c2_c1_ratio_128 = _mm_set_ps1(-0.71414f);
		static __m128	c2_c0_c1_ratio_128 = _mm_set_ps1(-0.71414f / 1.402f);
		static __m128	c1_c2_ratio_128 = _mm_set_ps1(1.772f);
		static __m128	c1_c1_c2_ratio_128 = _mm_set_ps1(1.772f / (-0.34413f));

		static __m128	float_0 =  _mm_set_ps1(0.0f);
		static __m128	lift_128 = _mm_set_ps1(128.0f);
		static __m128	float_255 =  _mm_set_ps1(255.999f);


		DEBUG_XMM_VAL;

		int inPitch = ALIGN_TO_SSE_DWORDS(width);
		int tail = 4 + width - inPitch;
		int h = height;

		switch(outFormat)
		{
		case IMAGE_FOTMAT_32BITS_ARGB:
			{
				int destSkip = (resultPitch - width) * 4;
				__m128	convert_to_8_bits = bitDepth > 8 ? _mm_set_ps1(1.0f / static_cast<float>(1 << (bitDepth - 8))) : _mm_set_ps1(static_cast<float>(1 << (8 - bitDepth)));

				if (checkOutComp == INVALID)
				{
					__asm
					{
						MOV ESI, comp0
						MOV EAX, comp1
						MOV EDX, comp2
						MOV EDI, result

						MOVAPS	XMM4, c2_c0_ratio_128
						CHECK_VAL(XMM4)
						MULPS	XMM4, convert_to_8_bits
						CHECK_VAL(XMM4)
						MOVAPS	XMM5, c1_c1_ratio_128
						MULPS	XMM5, convert_to_8_bits
						MOVAPS	XMM6, float_0
						MOVAPS	XMM7, float_255

height_loop_32bits_argb:
						MOV ECX, inPitch
						SHR ECX, 2		// ecx /= 4
						DEC ECX			// inpitch - 1

width_loop_32bits_argb:
						MOVAPS	XMM0, [ESI]	// component 0
						CHECK_VAL(XMM0)
						MULPS	XMM0, convert_to_8_bits
						CHECK_VAL(XMM0)
						ADDPS	XMM0, lift_128	// do the lifting first
						ADD ESI, 0x10
						MOVUPS	XMM1, [EAX]	// component 1
						CHECK_VAL(XMM1)
						ADD EAX, 0x10
						MOVUPS	XMM2, [EDX]	// component 2
						CHECK_VAL(XMM2)
						ADD EDX, 0x10

						MULPS	XMM2, XMM4
						CHECK_VAL(XMM2)
						CHECK_VAL(XMM4)
						MOVAPS	XMM3, XMM0
						ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
						CHECK_VAL(XMM3)
						MINPS	XMM3, XMM7	// clipping
						MAXPS	XMM3, XMM6
						CVTTPS2DQ XMM3, XMM3
						CHECK_VAL(XMM3)

						MULPS	XMM2, c2_c0_c1_ratio_128
						MULPS	XMM1, XMM5
						ADDPS	XMM2, XMM1
						ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
						MINPS	XMM2, XMM7	// clipping
						MAXPS	XMM2, XMM6
						CVTTPS2DQ XMM2, XMM2
						CHECK_VAL(XMM2)
						PSLLD	XMM3, 8	
						CHECK_VAL(XMM3)
						POR		XMM2, XMM3		// R + G

						MULPS	XMM1, c1_c1_c2_ratio_128
						ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
						MINPS	XMM1, XMM7	// clipping
						MAXPS	XMM1, XMM6
						CVTTPS2DQ XMM1, XMM1
						PSLLD	XMM2, 8
						CHECK_VAL(XMM2)
						POR	XMM1, XMM2		// R + G + B
						CHECK_VAL(XMM1)

						MOVUPS	[EDI], XMM1
						ADD EDI, 0x10

						DEC ECX
						JNZ	width_loop_32bits_argb

						// last 4-pixels
						MOVAPS	XMM0, [ESI]	// component 0
						CHECK_VAL(XMM0)
						MULPS	XMM0, convert_to_8_bits
						CHECK_VAL(XMM0)
						ADDPS	XMM0, lift_128	// do the lifting first
						ADD ESI, 0x10
						MOVUPS	XMM1, [EAX]	// component 1
						CHECK_VAL(XMM1)
						ADD EAX, 0x10
						MOVUPS	XMM2, [EDX]	// component 2
						CHECK_VAL(XMM2)
						ADD EDX, 0x10

						MULPS	XMM2, XMM4
						CHECK_VAL(XMM2)
						CHECK_VAL(XMM4)
						MOVAPS	XMM3, XMM0
						ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
						CHECK_VAL(XMM3)
						MINPS	XMM3, XMM7	// clipping
						MAXPS	XMM3, XMM6
						CVTTPS2DQ XMM3, XMM3
						CHECK_VAL(XMM3)

						MULPS	XMM2, c2_c0_c1_ratio_128
						MULPS	XMM1, XMM5
						ADDPS	XMM2, XMM1
						ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
						MINPS	XMM2, XMM7	// clipping
						MAXPS	XMM2, XMM6
						CVTTPS2DQ XMM2, XMM2
						CHECK_VAL(XMM2)
						PSLLD	XMM3, 8	
						CHECK_VAL(XMM3)
						POR		XMM2, XMM3		// R + G

						MULPS	XMM1, c1_c1_c2_ratio_128
						ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
						MINPS	XMM1, XMM7	// clipping
						MAXPS	XMM1, XMM6
						CVTTPS2DQ XMM1, XMM1
						PSLLD	XMM2, 8
						CHECK_VAL(XMM2)
						POR	XMM1, XMM2		// R + G + B
						CHECK_VAL(XMM1)

						CMP tail, 4
						JE tail_4_pixels__32bits_argb
						CMP tail, 3
						JE tail_3_pixels__32bits_argb
						CMP tail, 2
						JE tail_2_pixels__32bits_argb
						JMP tail_1_pixels__32bits_argb

tail_3_pixels__32bits_argb:
						MOVD	[EDI], XMM1
						PSRlDQ	XMM1, 4
						ADD EDI, 4
tail_2_pixels__32bits_argb:
						MOVD	[EDI], XMM1
						PSRlDQ	XMM1, 4
						ADD EDI, 4
tail_1_pixels__32bits_argb:
						MOVD	[EDI], XMM1
						ADD EDI, 4
						JMP width_loop_32bits_argb_done
tail_4_pixels__32bits_argb:
						MOVUPS	[EDI], XMM1
						ADD EDI, 16

width_loop_32bits_argb_done:
						ADD EDI, destSkip
						DEC h
						JNZ height_loop_32bits_argb
					}
				}
				else if (checkOutComp == 0 || checkOutComp == 2)		// checkout component 0
				{

					__asm
					{
						MOV EDI, result
						MOV ESI, comp0

						MOVAPS XMM3, convert_to_8_bits
						MOVAPS XMM5, lift_128
						MOVAPS XMM6, float_0
						MOVAPS XMM7, float_255

						CMP checkOutComp, 0
						JE check_out_comp0_32bits_argb

// check_out_comp2_32bits_argb:
						MOV EDX, comp1
						MOVAPS XMM4, c1_c2_ratio_128
						MULPS	XMM4, XMM3
						JMP height_loop_0_2_32bits_argb

check_out_comp0_32bits_argb:
						MOV EDX, comp2
						MOVAPS XMM4, c2_c0_ratio_128
						MULPS	XMM4, XMM3

height_loop_0_2_32bits_argb:
						MOV ECX, inPitch
						SHR ECX, 2		// ecx /= 4
						DEC ECX			// inpitch - 1

width_loop_0_2_32bits_argb:
						MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
						MULPS	XMM0, XMM3
						ADDPS	XMM0, XMM5	// do the lifting
						MOVAPS	XMM2, [EDX]	// component 2 or 1
						ADD EDX, 0x10

						MULPS	XMM2, XMM4
						ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
						MINPS	XMM0, XMM7	// clipping
						MAXPS	XMM0, XMM6
						CVTTPS2DQ XMM0, XMM0		// convert to integer
						MOVAPS	XMM1, XMM0		// convert to grey scaled
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0

						MOVUPS	[EDI], XMM1
						ADD EDI, 0x10

						LOOPNZ	width_loop_0_2_32bits_argb


						// last 4-pixels
						MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
						MULPS	XMM0, XMM3
						ADDPS	XMM0, XMM5	// do the lifting
						MOVUPS	XMM2, [EDX]	// component 2 or 1
						ADD EDX, 0x10

						MULPS	XMM2, XMM4
						ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
						MINPS	XMM0, XMM7	// clipping
						MAXPS	XMM0, XMM6
						CVTTPS2DQ XMM0, XMM0		// convert to integer
						MOVAPS	XMM1, XMM0		// convert to grey scaled
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0


						CMP tail, 4
						JE tail_4_pixels__32bits_argb_0_2
						CMP tail, 3
						JE tail_3_pixels__32bits_argb_0_2
						CMP tail, 2
						JE tail_2_pixels__32bits_argb_0_2
						JMP tail_1_pixels__32bits_argb_0_2


tail_3_pixels__32bits_argb_0_2:
						MOVD	[EDI], XMM1
							PSRlDQ	XMM1, 4
							ADD EDI, 4
tail_2_pixels__32bits_argb_0_2:
						MOVD	[EDI], XMM1
							PSRlDQ	XMM1, 4
							ADD EDI, 4
tail_1_pixels__32bits_argb_0_2:
						MOVD	[EDI], XMM1
							ADD EDI, 4
							JMP width_loop_32bits_argb_done_0_2
tail_4_pixels__32bits_argb_0_2:
						MOVUPS	[EDI], XMM1
							ADD EDI, 16

width_loop_32bits_argb_done_0_2:
						ADD EDI, destSkip
						DEC h
						JNZ height_loop_0_2_32bits_argb	

					}

				}
				else if (checkOutComp == 1)
				{

					__asm
					{
						MOV EDI, result
						MOV ESI, comp0
						MOV EAX, comp1
						MOV EDX, comp2

						MOVAPS XMM2, lift_128
						MOVAPS XMM3, convert_to_8_bits
						MOVAPS XMM4, float_0
						MOVAPS XMM7, float_255

						MOVAPS XMM5, c1_c1_ratio_128
						MULPS	XMM5, XMM3
						MOVAPS XMM6, c2_c1_ratio_128
						MULPS	XMM6, XMM3

height_loop_1_32bits_argb:
						MOV ECX, inPitch
						SHR ECX, 2		// ecx /= 4
						DEC ECX			// inpitch - 1

width_loop_1_32bits_argb:
						MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
						MULPS	XMM0, XMM3
						ADDPS	XMM0, XMM2	// do the lifting
						MOVUPS	XMM1, [EAX]	// component 1
						ADD EAX, 0x10
						MULPS	XMM1, XMM5
						ADDPS	XMM0, XMM1

						MOVAPS	XMM1, [EDX]	// component 2
						ADD EDX, 0x10
						MULPS	XMM1, XMM6
						ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);

						MINPS	XMM0, XMM7	// clipping
						MAXPS	XMM0, XMM4
						CVTTPS2DQ XMM0, XMM0		// convert to integer
						MOVAPS	XMM1, XMM0		// convert to grey scaled
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0

						MOVUPS	[EDI], XMM1
						ADD EDI, 0x10

						LOOPNZ	width_loop_1_32bits_argb

						// last 4-pixels
						MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
						MULPS	XMM0, XMM3
						ADDPS	XMM0, XMM2	// do the lifting
						MOVUPS	XMM1, [EAX]	// component 1
						ADD EAX, 0x10
						MULPS	XMM1, XMM5
						ADDPS	XMM0, XMM1

						MOVAPS	XMM1, [EDX]	// component 2
						ADD EDX, 0x10
						MULPS	XMM1, XMM6
						ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
						MINPS	XMM0, XMM7	// clipping
						MAXPS	XMM0, XMM4
						CVTTPS2DQ XMM0, XMM0		// convert to integer
						MOVAPS	XMM1, XMM0		// convert to grey scaled
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0
						PSLLDQ XMM0, 1
						POR	XMM1, XMM0

						CMP tail, 4
						JE tail_4_pixels__32bits_argb_1
						CMP tail, 3
						JE tail_3_pixels__32bits_argb_1
						CMP tail, 2
						JE tail_2_pixels__32bits_argb_1
						JMP tail_1_pixels__32bits_argb_1

tail_3_pixels__32bits_argb_1:
						MOVD	[EDI], XMM1
							PSRlDQ	XMM1, 4
							ADD EDI, 4
tail_2_pixels__32bits_argb_1:
						MOVD	[EDI], XMM1
							PSRlDQ	XMM1, 4
							ADD EDI, 4
tail_1_pixels__32bits_argb_1:
						MOVD	[EDI], XMM1
							ADD EDI, 4
							JMP width_loop_32bits_argb_done_1

tail_4_pixels__32bits_argb_1:
						MOVUPS	[EDI], XMM1
							ADD EDI, 16

width_loop_32bits_argb_done_1:
						ADD EDI, destSkip
						DEC h
						JNZ height_loop_1_32bits_argb	
					}

				}
				else
				{
					BMI_ASSERT(0);
				}

			}
			break;

		case IMAGE_FOTMAT_24BITS_RGB:
			{
				int destSkip = (resultPitch - width) * 3;
				__m128	convert_to_8_bits = bitDepth > 8 ? _mm_set_ps1(1.0f / static_cast<float>(1 << (bitDepth - 8))) : _mm_set_ps1(static_cast<float>(1 << (8 - bitDepth)));

				if (checkOutComp == INVALID)
				{
					__asm
					{
						MOV ESI, comp0
						MOV EAX, comp1
						MOV EDX, comp2
						MOV EDI, result

						MOVAPS	XMM4, c2_c0_ratio_128
						CHECK_VAL(XMM4)
						MULPS	XMM4, convert_to_8_bits
						CHECK_VAL(XMM4)
						MOVAPS	XMM5, c1_c1_ratio_128
						MULPS	XMM5, convert_to_8_bits
						MOVAPS	XMM6, float_0
						MOVAPS	XMM7, float_255

height_loop_24bits_rgb:
						MOV ECX, inPitch
						SHR ECX, 2		// ecx /= 4
						DEC ECX			// inpitch - 1

width_loop_24bits_rgb:
						MOVAPS	XMM0, [ESI]	// component 0
						CHECK_VAL(XMM0)
						MULPS	XMM0, convert_to_8_bits
						CHECK_VAL(XMM0)
						ADDPS	XMM0, lift_128	// do the lifting first
						ADD ESI, 0x10
						MOVUPS	XMM1, [EAX]	// component 1
						CHECK_VAL(XMM1)
						ADD EAX, 0x10
						MOVUPS	XMM2, [EDX]	// component 2
						CHECK_VAL(XMM2)
						ADD EDX, 0x10

							MULPS	XMM2, XMM4
							CHECK_VAL(XMM2)
							CHECK_VAL(XMM4)
							MOVAPS	XMM3, XMM0
							ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
							CHECK_VAL(XMM3)
							MINPS	XMM3, XMM7	// clipping
							MAXPS	XMM3, XMM6
							CVTTPS2DQ XMM3, XMM3
							CHECK_VAL(XMM3)

							MULPS	XMM2, c2_c0_c1_ratio_128
							MULPS	XMM1, XMM5
							ADDPS	XMM2, XMM1
							ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
							MINPS	XMM2, XMM7	// clipping
							MAXPS	XMM2, XMM6
							CVTTPS2DQ XMM2, XMM2
							CHECK_VAL(XMM2)
							PSLLD	XMM3, 8	
							CHECK_VAL(XMM3)
							POR		XMM2, XMM3		// R + G

							MULPS	XMM1, c1_c1_c2_ratio_128
							ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
							MINPS	XMM1, XMM7	// clipping
							MAXPS	XMM1, XMM6
							CVTTPS2DQ XMM1, XMM1
							PSLLD	XMM2, 8
							CHECK_VAL(XMM2)
							POR	XMM1, XMM2		// R + G + B
							CHECK_VAL(XMM1)
// 							PSLLD	XMM1, 8		// 0RGB -> RGB0
							CHECK_VAL(XMM1)


							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							CHECK_VAL(XMM1)
							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3

							DEC ECX
							JNZ	width_loop_24bits_rgb

							// last 4-pixels
						MOVAPS	XMM0, [ESI]	// component 0
						CHECK_VAL(XMM0)
						MULPS	XMM0, convert_to_8_bits
						CHECK_VAL(XMM0)
						ADDPS	XMM0, lift_128	// do the lifting first
						ADD ESI, 0x10
						MOVUPS	XMM1, [EAX]	// component 1
						CHECK_VAL(XMM1)
						ADD EAX, 0x10
							MOVUPS	XMM2, [EDX]	// component 2
						CHECK_VAL(XMM2)
							ADD EDX, 0x10

							MULPS	XMM2, XMM4
							CHECK_VAL(XMM2)
							CHECK_VAL(XMM4)
							MOVAPS	XMM3, XMM0
							ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
							CHECK_VAL(XMM3)
							MINPS	XMM3, XMM7	// clipping
							MAXPS	XMM3, XMM6
							CVTTPS2DQ XMM3, XMM3
							CHECK_VAL(XMM3)

							MULPS	XMM2, c2_c0_c1_ratio_128
							MULPS	XMM1, XMM5
							ADDPS	XMM2, XMM1
							ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
							MINPS	XMM2, XMM7	// clipping
							MAXPS	XMM2, XMM6
							CVTTPS2DQ XMM2, XMM2
							CHECK_VAL(XMM2)
							PSLLD	XMM3, 8	
							CHECK_VAL(XMM3)
							POR		XMM2, XMM3		// R + G

							MULPS	XMM1, c1_c1_c2_ratio_128
							ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
							MINPS	XMM1, XMM7	// clipping
							MAXPS	XMM1, XMM6
							CVTTPS2DQ XMM1, XMM1
							PSLLD	XMM2, 8
							CHECK_VAL(XMM2)
							POR	XMM1, XMM2		// R + G + B
							CHECK_VAL(XMM1)
// 							PSLLD	XMM1, 8		// 0RGB -> RGB0


							CMP tail, 4
							JE tail_4_pixels__24bits_rgb
							CMP tail, 3
							JE tail_3_pixels__24bits_rgb
							CMP tail, 2
							JE tail_2_pixels__24bits_rgb
							JMP last_pizel_24bits_rgb

tail_4_pixels__24bits_rgb:
						MOVD	[EDI], XMM1
						ADD EDI,3
						PSRlDQ	XMM1, 4
tail_3_pixels__24bits_rgb:
						MOVD	[EDI], XMM1
						ADD EDI,3
						PSRlDQ	XMM1, 4
tail_2_pixels__24bits_rgb:
						CHECK_VAL(XMM1)
						MOVD	[EDI], XMM1
						ADD EDI,3
						PSRlDQ	XMM1, 4
last_pizel_24bits_rgb:
						CHECK_VAL(XMM1)
						MOVD ECX, XMM1
						MOV [EDI], CX
						SHR ECX, 16
						MOV [EDI+2], CL
						ADD EDI, 3


						ADD EDI, destSkip
						DEC h
						JNZ height_loop_24bits_rgb
					}
				}
				else if (checkOutComp == 0 || checkOutComp == 2)		// checkout component 0
				{

					__asm
					{
						MOV EDI, result
							MOV ESI, comp0

							MOVAPS XMM3, convert_to_8_bits
							MOVAPS XMM5, lift_128
							MOVAPS XMM6, float_0
							MOVAPS XMM7, float_255

							CMP checkOutComp, 0
							JE check_out_comp0_24bits_rgb

						MOV EDX, comp1
							MOVAPS XMM4, c1_c2_ratio_128
							MULPS	XMM4, XMM3
							JMP height_loop_0_2_24bits_rgb

check_out_comp0_24bits_rgb:
						MOV EDX, comp2
							MOVAPS XMM4, c2_c0_ratio_128
							MULPS	XMM4, XMM3

height_loop_0_2_24bits_rgb:
						MOV ECX, inPitch
							SHR ECX, 2		// ecx /= 4
							DEC ECX			// inpitch - 1

width_loop_0_2_24bits_rgb:
						MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
							MULPS	XMM0, XMM3
							ADDPS	XMM0, XMM5	// do the lifting
							MOVAPS	XMM2, [EDX]	// component 2 or 1
						ADD EDX, 0x10

							MULPS	XMM2, XMM4
							ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
							MINPS	XMM0, XMM7	// clipping
							MAXPS	XMM0, XMM6
							CVTTPS2DQ XMM0, XMM0		// convert to integer
							MOVAPS	XMM1, XMM0		// convert to grey scaled
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0

// 							PSLLD	XMM1, 8		// 0RGB -> RGB0

							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3

							LOOPNZ	width_loop_0_2_24bits_rgb

							MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
							MULPS	XMM0, XMM3
							ADDPS	XMM0, XMM5	// do the lifting
							MOVAPS	XMM2, [EDX]	// component 2 or 1
						ADD EDX, 0x10

							MULPS	XMM2, XMM4
							ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
							MINPS	XMM0, XMM7	// clipping
							MAXPS	XMM0, XMM6
							CVTTPS2DQ XMM0, XMM0		// convert to integer
							MOVAPS	XMM1, XMM0		// convert to grey scaled
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0
// 							PSLLD	XMM1, 8		// 0RGB -> RGB0

							CMP tail, 4
							JE tail_4_pixels__24bits_rgb_0_2
							CMP tail, 3
							JE tail_3_pixels__24bits_rgb_0_2
							CMP tail, 2
							JE tail_2_pixels__24bits_rgb_0_2
							JMP last_pizel_24bits_rgb_0_2

tail_4_pixels__24bits_rgb_0_2:
						MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
tail_3_pixels__24bits_rgb_0_2:
						MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
tail_2_pixels__24bits_rgb_0_2:
						MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
last_pizel_24bits_rgb_0_2:
						MOVD ECX, XMM1
							MOV [EDI], CX
							SHR ECX, 16
							MOV [EDI+2], CL
							ADD EDI, 3


							ADD EDI, destSkip
							DEC h
							JNZ height_loop_0_2_24bits_rgb
						}

				}
				else if (checkOutComp == 1)
				{

					__asm
					{
						MOV EDI, result
							MOV ESI, comp0
							MOV EAX, comp1
							MOV EDX, comp2

							MOVAPS XMM2, lift_128
							MOVAPS XMM3, convert_to_8_bits
							MOVAPS XMM4, float_0
							MOVAPS XMM7, float_255

							MOVAPS XMM5, c1_c1_ratio_128
							MULPS	XMM5, XMM3
							MOVAPS XMM6, c2_c1_ratio_128
							MULPS	XMM6, XMM3

height_loop_1_24bits_rgb:
						MOV ECX, inPitch
							SHR ECX, 2		// ecx /= 4
							DEC ECX			// inpitch - 1

width_loop_1_24bits_rgb:
						MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
							MULPS	XMM0, XMM3
							ADDPS	XMM0, XMM2	// do the lifting
							MOVUPS	XMM1, [EAX]	// component 1
						ADD EAX, 0x10
							MULPS	XMM1, XMM5
							ADDPS	XMM0, XMM1

							MOVAPS	XMM1, [EDX]	// component 2
						ADD EDX, 0x10
							MULPS	XMM1, XMM6
							ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);

							MINPS	XMM0, XMM7	// clipping
							MAXPS	XMM0, XMM4
							CVTTPS2DQ XMM0, XMM0		// convert to integer
							MOVAPS	XMM1, XMM0		// convert to grey scaled
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0

// 							PSLLD	XMM1, 8		// 0RGB -> RGB0

							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
							MOVD	[EDI], XMM1
							ADD EDI,3

							LOOPNZ	width_loop_1_24bits_rgb

							// last 4-pixels
							MOVAPS	XMM0, [ESI]	// component 0
						ADD ESI, 0x10
							MULPS	XMM0, XMM3
							ADDPS	XMM0, XMM2	// do the lifting
							MOVUPS	XMM1, [EAX]	// component 1
						ADD EAX, 0x10
							MULPS	XMM1, XMM5
							ADDPS	XMM0, XMM1

							MOVAPS	XMM1, [EDX]	// component 2
						ADD EDX, 0x10
							MULPS	XMM1, XMM6
							ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
							MINPS	XMM0, XMM7	// clipping
							MAXPS	XMM0, XMM4
							CVTTPS2DQ XMM0, XMM0		// convert to integer
							MOVAPS	XMM1, XMM0		// convert to grey scaled
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0
							PSLLDQ XMM0, 1
							POR	XMM1, XMM0

							CMP tail, 4
							JE tail_4_pixels__24bits_rgb_1
							CMP tail, 3
							JE tail_3_pixels__24bits_rgb_1
							CMP tail, 2
							JE tail_2_pixels__24bits_rgb_1
							JMP last_pixel_24bits_rgb_1

							// One float

tail_4_pixels__24bits_rgb_1:
						MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
tail_3_pixels__24bits_rgb_1:
						MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
tail_2_pixels__24bits_rgb_1:
						MOVD	[EDI], XMM1
							ADD EDI,3
							PSRlDQ	XMM1, 4
last_pixel_24bits_rgb_1:
						MOVD ECX, XMM1
							MOV [EDI], CX
							SHR ECX, 16
							MOV [EDI+2], CL
							ADD EDI, 3

						ADD EDI, destSkip
							DEC h
							JNZ height_loop_1_24bits_rgb	
					}

				}
				else
				{
					BMI_ASSERT(0);
				}

			}
			break;

		case IMAGE_FOTMAT_36BITS_RGB:
		case IMAGE_FOTMAT_48BITS_ARGB:
		case IMAGE_FOTMAT_48BITS_RGB:
		case IMAGE_FOTMAT_64BITS_ARGB:
#pragma BMI_PRAGMA_TODO_MSG("To implement MCT assembly for more format")
		default:
			BMI_ASSERT(0);
			break;
		}

		return;
	}


	void	CpuDwtDecoder::RMCT_R53_INT32_ASM(void * resultBuf, int * redBuf, int * greenBuf, int * blueBuf, int bufPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp)
	{

		if (((int)resultBuf & 0x0f) || ((int)redBuf & 0x0f) || ((int)greenBuf & 0x0f) || ((int)blueBuf & 0x0f) || outFormat != IMAGE_FOTMAT_32BITS_ARGB)
		{
			RRct_53<int_32>(resultBuf, redBuf, greenBuf, blueBuf, bufPitch, width, height, bitDepth, outFormat,checkOutComp);
			return;
		}
#pragma BMI_PRAGMA_TODO_MSG("Implement MCT 53 in Assemblly for other format")

		__m128i	int_255_128bits	= _mm_set1_epi32(255);
		__m128i	int_128_128bits	= _mm_set1_epi32(128);
		__m128i	int_0_128bits	= _mm_set1_epi32(0);

		int rowSkip = (bufPitch-width)<<2;

		__asm
		{

			MOV EAX, width

				MOV ESI, redBuf
				MOV ECX, greenBuf
				MOV EDX, blueBuf

				MOV EDI, resultBuf

				MOVDQA	XMM5, int_255_128bits
				MOVDQA	XMM6, int_128_128bits
				MOVDQA	XMM7, int_0_128bits

mct_loop:

			MOVDQA	XMM0, [ESI]
			MOVDQA	XMM1, [ECX]
			MOVDQA	XMM2, [EDX]

			MOVDQA	XMM3, XMM1
				PADDD	XMM3, XMM2
				PSRAD	XMM3, 2

				PSUBD	XMM0, XMM3
				PADDD	XMM0, XMM6	// new_g = r - (g + b) / 4 + lift
				PADDD	XMM1, XMM0	// new_b = g + new_r
				PADDD	XMM2, XMM0	// new_r = b + new_g

				PSLLD	XMM0, 8
				PADDD	XMM0, XMM1
				PSLLD	XMM2, 16
				PADDD	XMM0, XMM2
				MOVDQA	[EDI], XMM0

				ADD ESI, 0x10
				ADD ECX, 0x10
				ADD EDX, 0x10
				ADD EDI, 0x10

				SUB EAX, 4
				JNZ mct_loop
				ADD EDI, rowSkip
				MOV EAX, height
				DEC	EAX
				JZ  done
				MOV height, EAX
				MOV EAX, width
				JMP mct_loop
done:

		}

	}

};

//#elif  AT_T_ASSEMBLY



void	hdwt_analyst_even(float * result, int loopLength, int longExtra, __m128 coef128,  int low_width, int lhParity, float *  restoreResult = NULL, int restoreLength = INVALID)
{


	if (loopLength)
	{
		if (lhParity)
		{
			int tail = loopLength * 4 - low_width;
			if (tail >= 0)
			{
				result[loopLength * 8 - tail - 1 ] = result[loopLength * 8 - tail - 2];		// mirror the band_h last one
			}
			else
			{
				result[loopLength * 8 + 4] = result[loopLength * 8 - 1];		// mirror the band_h last one
			}
		}

		loopLength += longExtra;

		__m128 * Pcoef128 = & coef128;

		if (!restoreResult)
		{

			__asm__ volatile (
				"movaps	(%0), %%xmm7;"		// 				MOVAPS XMM7, coef128
				// 				MOV ECX, loopLength
				// 				MOV ESI, result
				"movaps 0x10(%2), %%xmm0;"	// 				MOVAPS	XMM0, [ESI + 0x10]
"analyst_loop:;"
				"movaps (%2), %%xmm1;"		// 				MOVAPS	XMM1,[ESI]	// L0L1L2L3
				"movaps 0x10(%2), %%xmm2;"	// 				MOVAPS	XMM2, [ESI + 0x10]	// H0H1H2H3
				"movaps %%xmm2, %%xmm3;"	// 				MOVAPS	XMM3, XMM2
				"pslldq	$4, %%xmm2;"		// 					PSLLDQ	XMM2, 4		
				"movss %%xmm0, %%xmm2;"		// 					MOVSS	XMM2, XMM0		// H(-1) H0H1H2
				"addps %%xmm3, %%xmm2;"		// 					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"		// 					MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"		// 					SUBPS	XMM1, XMM2
				"movaps %%xmm3, %%xmm0;"	// 					MOVAPS  XMM0, XMM3
				"psrldq $12, %%xmm0;"		// 					PSRLDQ	XMM0, 12		
				"movaps %%xmm1, (%2);"		// 					MOVAPS  [ESI], XMM1
				"addl $0x20, %2;"			// 					ADD ESI, 0x20
				"decl %1;"
				"jnz analyst_loop;"			// 					LOOPNZ analyst_loop
				//output
				:
				// input
				:
				"r"(Pcoef128),		// %0
				"r"(loopLength),	// %1
				"r"(result)		// %2
			);
		}
		else	// repack
		{

			BMI_ASSERT(restoreLength != INVALID);
			BMI_ASSERT(restoreLength <= loopLength * 8 + longExtra && restoreLength >= low_width * 2 - 1);
			int tailPixels = restoreLength + 8 - loopLength * 8;
			float last8Pixels[9];
			float * Plast8Pixels = &(last8Pixels[0]);
			__asm__ volatile (

				"movaps	(%0), %%xmm7;"		// 				MOVAPS XMM7, coef128
				// 					MOV ESI, result
				// 					MOV EDI, restoreResult
				"movaps 0x10(%2), %%xmm0;"	// 				MOVAPS	XMM0, [ESI + 0x10]
				// 				MOV ECX, loopLength
				"decl %1;"					// 				DEC ECX
				"jz last_4_pixels_repack;"	//				JZ last_4_pixels_repack

"analyst_loop_repack:;"

				"movaps (%2), %%xmm1;"		// 				MOVAPS	XMM1,[ESI]	// L0L1L2L3
				"movaps 0x10(%2), %%xmm2;"	// 				MOVAPS	XMM2, [ESI + 0x10]	// H0H1H2H3
				"movaps %%xmm2, %%xmm3;"	// 				MOVAPS	XMM3, XMM2
				"pslldq	$4, %%xmm2;"		// 					PSLLDQ	XMM2, 4		
				"movss %%xmm0, %%xmm2;"		// 					MOVSS	XMM2, XMM0		// H(-1) H0H1H2
				"addps %%xmm3, %%xmm2;"		// 					ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"		// 					MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"		// 					SUBPS	XMM1, XMM2
				"movaps %%xmm3, %%xmm0;"	// 					MOVAPS  XMM0, XMM3
				"psrldq $12, %%xmm0;"		// 					PSRLDQ	XMM0, 12		

				"movaps %%xmm1, %%xmm2;"	// 					MOVAPS XMM2, XMM1
				"punpckldq %%xmm3, %%xmm1;"	// 					PUNPCKLDQ	XMM1, XMM3
				"punpckhdq %%xmm3, %%xmm2;"	// 					PUNPCKHDQ	XMM2, XMM3
				"movaps %%xmm1, (%3);"		// 					MOVAPS  [EDI], XMM1
				"movaps %%xmm2, 0x10(%3);"	// 					MOVAPS  [EDI + 0x10], XMM2

				"addl $0x20, %2;"			// 					ADD ESI, 0x20
				"addl $0x20, %3;"			// 					ADD ESI, 0x20
				"decl %1;"
				"jnz analyst_loop_repack;"	// 					LOOPNZ analyst_loop_repeck

"last_4_pixels_repack:;"
				"movaps (%2), %%xmm1;"		// 				MOVAPS	XMM1,[ESI]	// L0L1L2L3
				"movapd 0x10(%2), %%xmm2;"	// 				MOVAPS	XMM2, [ESI + 0x10]	// H0H1H2H3
				"movaps %%xmm2, %%xmm3;"	// 				MOVAPS	XMM3, XMM2
				"pslldq	$4, %%xmm2;"		// 				PSLLDQ	XMM2, 4		
				"movss %%xmm0, %%xmm2;"		// 				MOVSS	XMM2, XMM0		// H(-1) H0H1H2
				"addps %%xmm3, %%xmm2;"		// 				ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"		// 				MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"		// 				SUBPS	XMM1, XMM2

				"movaps %%xmm1, %%xmm2;"	// 				MOVAPS XMM2, XMM1
				"punpckldq %%xmm3, %%xmm1;"	// 				PUNPCKLDQ	XMM1, XMM3
				"punpckhdq %%xmm3, %%xmm2;"	// 				PUNPCKHDQ	XMM2, XMM3
			
				//			LEA EDI, last8Pixels
				"movups %%xmm1, (%4);"		// 				MOVUPS  [EDI], XMM1
				"movups %%xmm2, 0x10(%4);"	// 				MOVUPS  [EDI + 0x10], XMM2
				//output
				:
				// input
				:
				"r" (Pcoef128),		// %0
				"r"(loopLength),	// %1
				"r"(result),		// %2
				"r"(restoreResult),	// %3
				"r"(Plast8Pixels)	// %4
			);

			memcpy(restoreResult +(loopLength - 1) * 8, last8Pixels, tailPixels * sizeof(float));
		}
	}

}

void	hdwt_analyst_odd(float * result, int loopLength, int longExtra,  __m128 coef128,  int low_width, int lhParity, float * restoreResult = NULL, int restoreLength = INVALID)
{


	if (loopLength)
	{

		if (!lhParity)
		{
			int calWidth = ALIGN_TO_SSE_DWORDS(low_width);
			if (calWidth != low_width)
			{
				calWidth -= low_width;
				result[loopLength * 8 - calWidth - 4 ] = result[loopLength * 8 - calWidth - 5];		// mirror the band_l last one
			}		
		}

		__m128i	maskLast	= _mm_set_epi32(0xffffffff, 0, 0, 0);

		__m128 * Pcoef128 = & coef128;
		__m128i * PmaskLast = & maskLast;


		if (!restoreResult)
		{
			__asm__ volatile (

				"movaps	(%0), %%xmm7;"		// 				MOVAPS XMM7, coef128
				// 				MOV ESI, result
				// 				MOV ECX, loopLength
"analyst_loop_odd:;"
				"cmpl $1, %1;"				// 				CMP ECX, 1
				"jng last_4_pixels_odd;"	// 					JNG	last_4_pixels
				"movaps 0x20(%2), %%xmm0;"	// 					MOVAPS XMM0, [ESI + 0x20]
				"pslldq	$12, %%xmm0;"		// 					PSLLDQ	XMM0, 12
				"jmp calculate_4_pixels_odd;"// 					JMP calculate_4_pixels
"last_4_pixels_odd:;"
				"cmp $1, %4;"				// 				CMP longExtra, 1
				"je last_4_pixels_long_extra_odd;"	// 					JE last_4_pixels_long_extra
				"movaps (%2), %%xmm0;"		// 					MOVAPS XMM0, [ESI]
				"pand (%3), %%xmm0;"		// 				PAND   XMM0, maskLast		
				"jmp calculate_4_pixels_odd;"// 					JMP calculate_4_pixels

"last_4_pixels_long_extra_odd:;"
				"movaps 0x20(%2), %%xmm0;"	// 					MOVAPS XMM0, [ESI + 0x20]
				"pslldq	$12, %%xmm0;"		// 					PSLLDQ	XMM0, 12

"calculate_4_pixels_odd:;"
				"movaps (%2), %%xmm2;"		// 				MOVAPS	XMM2,[ESI]	// L0L1L2L3
				"movaps %%xmm2, %%xmm3;"	// 				MOVAPS	XMM3, XMM2
				"movaps 0x10(%2), %%xmm1;"	// 				MOVAPS	XMM1, [ESI + 0x10]	// H0H1H2H3
				"psrldq	$4, %%xmm2;"		//				PSRLDQ	XMM2, 4		
				"orps %%xmm0, %%xmm2;"		//				ORPS	XMM2, XMM0
				"addps %%xmm3, %%xmm2;"		//				ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"		//				MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"		//				SUBPS	XMM1, XMM2
				"movaps %%xmm1, 0x10(%2);"	//				MOVAPS  [ESI + 0x10], XMM1

				"addl $0x20, %2;"			// 					ADD ESI, 0x20
				"decl %1;"
				"jnz analyst_loop_odd;"		// 					LOOPNZ analyst_loop

				//output
				:
				// input
				:
				"r" (Pcoef128),		// %0
				"r"(loopLength),	// %1
				"r"(result),		// %2
				"r"(PmaskLast),		// %3
				"r"(longExtra)		// %4

			);

		}
		else	// repack 
		{
			BMI_ASSERT(restoreLength != INVALID);
			BMI_ASSERT(restoreLength <= loopLength * 8 + longExtra && restoreLength >= low_width * 2 - 1);
			int tailPixels = restoreLength + 8 - loopLength * 8;
			float last8Pixels[9];
			float * Plast8Pixels = &(last8Pixels[0]);
			int loop = loopLength;
			__asm__ volatile (
				"movaps	(%0), %%xmm7;"		// 				MOVAPS XMM7, coef128
				// 				MOV ESI, result
				// 				MOV EDI, restoreResult
				// 				MOV ECX, loopLength
				"decl %1;"					// 				DEC ECX
				"jz last_4_pixels_repack_odd;"	//				JZ last_4_pixels_repack

"analyst_loop_repack_odd:;"
				"movaps 0x20(%2), %%xmm0;"		// 				MOVAPS XMM0, [ESI + 0x20]
				"pslldq $12, %%xmm0;"			// 				PSLLDQ	XMM0, 12		
				"movaps (%2), %%xmm2;"			// 				MOVAPS	XMM2,[ESI]	// L0L1L2L3
				"movaps %%xmm2, %%xmm3;"		// 				MOVAPS	XMM3, XMM2
				"movaps 0x10(%2), %%xmm1;"		//				MOVAPS	XMM1, [ESI + 0x10]	// H0H1H2H3
				"psrldq	$4, %%xmm2;"			// 				PSRLDQ	XMM2, 4		
				"orps %%xmm0, %%xmm2;"			// 				ORPS	XMM2, XMM0
				"addps %%xmm3, %%xmm2;"			//				ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"			// 				MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"			// 				SUBPS	XMM1, XMM2

				// repack XMM1 and XMM3

				"movaps %%xmm3, %%xmm2;"		//				MOVAPS XMM2, XMM3
				"punpckldq %%xmm1, %%xmm2;"		//				PUNPCKLDQ	XMM2, XMM1
				"punpckhdq %%xmm1, %%xmm3;"		// 				PUNPCKHDQ	XMM3, XMM1
				"movaps %%xmm2, (%3);"			// 				MOVAPS  [EDI], XMM2
				"movaps %%xmm3, 0x10(%3);"		// 				MOVAPS  [EDI + 0x10], XMM3

				"addl $0x20, %2;"				// 				ADD ESI, 0x20
				"addl $0x20, %3;"				// 				ADD ESI, 0x20
				"decl %1;"
				"jnz analyst_loop_repack_odd;"	// 				LOOPNZ analyst_loop_repack

				// 					// tail repeck
"last_4_pixels_repack_odd:;"
				"cmpl $1, %5;"					// 			CMP longExtra, 1
				"je last_4_pixels_repack_long_extra_odd;"	//	JE last_4_pixels_repack_long_extra
				"movaps (%2), %%xmm0;"			//			MOVAPS XMM0, [ESI]
				"pand (%6), %%xmm0;"				// 			PAND   XMM0, maskLast		
				"jmp calculate_4_pixels_equal_odd;"	//		JMP calculate_4_pixels_equal

"last_4_pixels_repack_long_extra_odd:;"
				"movaps 0x20(%2), %%xmm0;"		// 				MOVAPS XMM0, [ESI + 0x20]
				"pslldq $12, %%xmm0;"			// 				PSLLDQ	XMM0, 12		

				
"calculate_4_pixels_equal_odd:;"
				"movaps (%2), %%xmm2;"		// 				MOVAPS	XMM2,[ESI]	// L0L1L2L3
				"movapd 0x10(%2), %%xmm1;"	// 				MOVAPS	XMM1, [ESI + 0x10]	// H0H1H2H3
				"movaps %%xmm2, %%xmm3;"	// 				MOVAPS	XMM3, XMM2
				"psrldq	$4, %%xmm2;"		// 				PSRLDQ	XMM2, 4		

				"movss %%xmm0, %%xmm2;"		// 				MOVSS	XMM2, XMM0		// H(-1) H0H1H2
				"addps %%xmm3, %%xmm2;"		// 				ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"		// 				MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"		// 				SUBPS	XMM1, XMM2
				"orps %%xmm0, %%xmm2;"		// 				ORPS	XMM2, XMM0
				"addps %%xmm3, %%xmm2;"		// 				ADDPS	XMM2, XMM3		// H(-1)+H0; H0+H1; H1 + H2;  H2 + H3
				"mulps %%xmm7, %%xmm2;"		// 				MULPS	XMM2, XMM7		
				"subps %%xmm2, %%xmm1;"		// 				SUBPS	XMM1, XMM2
				// 					// repack XMM1 and XMM3
				"movaps %%xmm3, %%xmm2;"	//				MOVAPS XMM2, XMM3
				"punpckldq %%xmm1, %%xmm2;"	//				PUNPCKLDQ	XMM2, XMM1
				"punpckhdq %%xmm1, %%xmm3;"	// 				PUNPCKHDQ	XMM3, XMM1


				// 					LEA EDI, last8Pixels
 				"movups %%xmm2, (%4);"		// 				MOVUPS  [EDI], XMM2
 				"movups %%xmm3, 0x10(%4);"	// 				MOVUPS  [EDI + 0x10], XMM3

				"cmpl $1, %5;"				// 				CMP longExtra, 1
				"jne analyst_odd_repack_done_odd;"	//		JNE analyst_odd_repack_done
				"pushl %%eax;"
				"movl 0x20(%2), %%eax;"		//				MOV EAX, [ESI+0x20]
				"movl %%eax, 0x20(%4);"		// 				MOV [EDI+0x20], EAX
"analyst_odd_repack_done_odd:;"

				//output
				:
				// input
				:
				"r" (Pcoef128),		// %0
				"g"(loop),	// %1
				"r"(result),		// %2
				"r"(restoreResult),	// %3
				"r"(Plast8Pixels),	// %4
				"g"(longExtra),		// %5
				"r"(PmaskLast)		// %6
				);

			memcpy(restoreResult +(loopLength - 1) * 8, last8Pixels, tailPixels * sizeof(float));

		}
	}

}


void CpuDwtDecoder::HDWT_R97_FLOAT_ASM_1Line(float * result, float  * band_l, float  * band_h, float * tempBuf, int low_w, int high_w, float lowgain, float highgain, uint_8 xParity,  bool isLowBandNeedTransfer)
{

	typedef	struct s_HDWT_paras
	{
		__m128 lowGain;												// 0
		__m128 highGain;											// 16

		__m128i	maskS;		// = _mm_set1_epi32(0x80000000);		// 32
		__m128i	maskV;		// = _mm_set1_epi32(0x7fffffff);		// 48
		__m128	floatN2;	// = _mm_set_ps1(-2.0f);			// 64
		__m128	floatP1;	// = _mm_set_ps1(1.0f);				// 80

	}HDWT_paras;

	HDWT_paras paras;
	HDWT_paras * P_paras = & paras;

	paras.maskS = _mm_set1_epi32(0x80000000);		// 32
	paras.maskV	= _mm_set1_epi32(0x7fffffff);		// 48
	paras.floatN2	= _mm_set_ps1(-2.0f);			// 64
	paras.floatP1	= _mm_set_ps1(1.0f);			// 80


	int lhParity = (low_w == high_w ? 0 : 1);
	int width = low_w + high_w;

	float * lineL;
	float * lineH;
	int loopLength, longExtra = 0;


	if (xParity)
	{
		lineL = band_h;
		lineH = band_l;
		paras.highGain = _mm_set_ps1(lowgain);
		paras.lowGain = _mm_set_ps1(highgain);
		loopLength = (ALIGN_TO_SSE_DWORDS(low_w)) / 4;
		longExtra = (ALIGN_TO_SSE_DWORDS(high_w)) / 4 - loopLength;
	}
	else
	{
		lineL = band_l;
		lineH = band_h;
		paras.lowGain = _mm_set_ps1(lowgain);
		paras.highGain = _mm_set_ps1(highgain);
		loopLength = (ALIGN_TO_SSE_DWORDS(high_w)) / 4;
		longExtra = (ALIGN_TO_SSE_DWORDS(low_w)) / 4 - loopLength;
	}

	BMI_ASSERT (!((int)band_l & 0x03) && !((int)band_h & 0x03 ));



	if (isLowBandNeedTransfer)
	{
		__asm__ volatile (
// 			MOV EAX, lineL
// 			MOV EDX, lineH
// 			MOV EDI, tempBuf
// 			MOV ECX, loopLength
			"movaps (%0), %%xmm6;"			//				MOVAPS XMM6, lowGain
			"movaps 16(%0), %%xmm7;"		// 				MOVAPS XMM7, highGain
			"movaps	32(%0),	%%xmm2;"		// 				MOVAPS XMM2, maskS
			"movaps 64(%0), %%xmm3;"		// 				MOVAPS XMM3, floatN2
			"movaps	80(%0), %%xmm4;"		// 				MOVAPS XMM4, floatP1
"L_transfer_load_line:;"
			// 			// setp 1 and 2
// 			LOAD_TRANSFER_FORWARD(XMM0, EAX,  XMM6, XMM2,     XMM3,XMM4, XMM5)
// 			LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2,  P1,   xmmt)		
			"movaps (%1), %%xmm5;"			//		__asm	MOVAPS	xmmt, [addr]					
			"addl	$0x10, %1;"				//		__asm	ADD addr, 0x10						
			"movaps %%xmm2, %%xmm0;"		//		__asm	MOVAPS	xmmm, maskSign				
			"pandn %%xmm5, %%xmm0;"			//		__asm	PANDN	xmmm, xmmt				
			"cvtdq2ps %%xmm0, %%xmm0;"		//		__asm	CVTDQ2PS xmmm, xmmm					
			"pand	%%xmm2, %%xmm5;"		//		__asm	PAND	xmmt, maskSign				
			"psrld	$31, %%xmm5;"			//		__asm	PSRLD	xmmt, 31					
			"cvtdq2ps %%xmm5, %%xmm5;"		//		__asm	CVTDQ2PS xmmt, xmmt					
			"mulps %%xmm3, %%xmm5;"			//		__asm	MULPS	xmmt, N2					
			"addps	%%xmm4, %%xmm5;"		//		__asm	ADDPS	xmmt, P1					
			"mulps %%xmm5, %%xmm0;"			//		__asm	MULPS	xmmm, xmmt					
			"mulps	%%xmm6, %%xmm0;"		//		__asm	MULPS	xmmm, coef					

// 			LOAD_TRANSFER_FORWARD(XMM1, EDX, XMM7,  XMM2,    XMM3, XMM4, XMM5)
// 			LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2,  P1,   xmmt)		
			"movaps (%2), %%xmm5;"			//		__asm	MOVAPS	xmmt, [addr]					
			"addl	$0x10, %2;"				//		__asm	ADD addr, 0x10						
			"movaps %%xmm2, %%xmm1;"		//		__asm	MOVAPS	xmmm, maskSign				
			"pandn %%xmm5, %%xmm1;"			//		__asm	PANDN	xmmm, xmmt				
			"cvtdq2ps %%xmm1, %%xmm1;"		//		__asm	CVTDQ2PS xmmm, xmmm					
			"pand	%%xmm2, %%xmm5;"		//		__asm	PAND	xmmt, maskSign				
			"psrld	$31, %%xmm5;"			//		__asm	PSRLD	xmmt, 31					
			"cvtdq2ps %%xmm5, %%xmm5;"		//		__asm	CVTDQ2PS xmmt, xmmt					
			"mulps %%xmm3, %%xmm5;"			//		__asm	MULPS	xmmt, N2					
			"addps	%%xmm4, %%xmm5;"		//		__asm	ADDPS	xmmt, P1					
			"mulps %%xmm5, %%xmm1;"			//		__asm	MULPS	xmmm, xmmt					
			"mulps	%%xmm7, %%xmm1;"		//		__asm	MULPS	xmmm, coef					

			// 				STORE_FORWARD(XMM0, EDI)
			"movaps %%xmm0, (%3);"			//		__asm	MOVAPS	[addr], xmma				
			"addl $0x10, %3;"				//		__asm	ADD addr, 0x10

			// 				STORE_FORWARD(XMM1, EDI)
			"movaps %%xmm1, (%3);"			//		__asm	MOVAPS	[addr], xmma				
			"addl $0x10, %3;"				//		__asm	ADD addr, 0x10

			"decl %4;"
			"jnz L_transfer_load_line;"		//		LOOPNZ L_transfer_load_line

			"cmpl $0, %5;"					//		CMP longExtra, 0
			"je L_transfer_load_line_done;"	//		JE L_transfer_load_line_done

// 			LOAD_TRANSFER_FORWARD(XMM0, EAX,  XMM6, XMM2,    XMM3, XMM4, XMM5)
// 			LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2,  P1,   xmmt)		
			"movaps (%1), %%xmm5;"			//		__asm	MOVAPS	xmmt, [addr]					
			"addl	$0x10, %1;"				//		__asm	ADD addr, 0x10						
			"movaps %%xmm2, %%xmm0;"		//		__asm	MOVAPS	xmmm, maskSign				
			"pandn %%xmm5, %%xmm0;"			//		__asm	PANDN	xmmm, xmmt				
			"cvtdq2ps %%xmm0, %%xmm0;"		//		__asm	CVTDQ2PS xmmm, xmmm					
			"pand	%%xmm2, %%xmm5;"		//		__asm	PAND	xmmt, maskSign				
			"psrld	$31, %%xmm5;"			//		__asm	PSRLD	xmmt, 31					
			"cvtdq2ps %%xmm5, %%xmm5;"		//		__asm	CVTDQ2PS xmmt, xmmt					
			"mulps %%xmm3, %%xmm5;"			//		__asm	MULPS	xmmt, N2					
			"addps	%%xmm4, %%xmm5;"		//		__asm	ADDPS	xmmt, P1					
			"mulps %%xmm5, %%xmm0;"			//		__asm	MULPS	xmmm, xmmt					
			"mulps	%%xmm6, %%xmm0;"		//		__asm	MULPS	xmmm, coef					

			"movaps	%%xmm0, (%3);"			//		MOVAPS [EDI], XMM0
"L_transfer_load_line_done:;"

		// output
		:
		// input
		: "r"(P_paras),			// %0
		  "r"(lineL),			// %1	EAX
 		  "r"(lineH),			// %2	EDX
 		  "r"(tempBuf),			// %3	EDI
 		  "r"(loopLength),		// %4	ECX
		  "g"(longExtra)		// %5
		: 
			);
	}
	else
	{
		if (!xParity)
		{
			__asm__ volatile (
				// 				MOV EAX, lineL
				// 				MOV EDX, lineH
				// 				MOV EDI, tempBuf
				// 				MOV ECX, loopLength
				"movaps (%0), %%xmm6;"			//				MOVAPS XMM6, lowGain
				"movaps 16(%0), %%xmm7;"		// 				MOVAPS XMM7, highGain
				"movaps	32(%0),	%%xmm2;"		// 				MOVAPS XMM2, maskS
				"movaps 64(%0), %%xmm3;"		// 				MOVAPS XMM3, floatN2
				"movaps	80(%0), %%xmm4;"		// 				MOVAPS XMM4, floatP1
"L_no_transfer_load_line:;"
 				// setp 1 and 2
				//		LOAD_NO_TRANSFER_FORWARD(XMM0, EAX, XMM6)
				//		LOAD_NO_TRANSFER_FORWARD(xmmm, addr, coef)		
				"movaps (%1), %%xmm0;"			//		__asm	MOVAPS	xmmm, [addr]
				"addl $0x10, %1;"				//		__asm	ADD addr, 0x10						
				"mulps %%xmm6, %%xmm0;"			//		__asm	MULPS	xmmm, coef

				// 			LOAD_TRANSFER_FORWARD(XMM1, EDX, XMM7,  XMM2,    XMM3, XMM4, XMM5)
				// 			LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2,  P1,   xmmt)		
				"movaps (%2), %%xmm5;"			//		__asm	MOVAPS	xmmt, [addr]					
				"addl	$0x10, %2;"				//		__asm	ADD addr, 0x10						
				"movaps %%xmm2, %%xmm1;"		//		__asm	MOVAPS	xmmm, maskSign				
				"pandn %%xmm5, %%xmm1;"			//		__asm	PANDN	xmmm, xmmt				
				"cvtdq2ps %%xmm1, %%xmm1;"		//		__asm	CVTDQ2PS xmmm, xmmm					
				"pand	%%xmm2, %%xmm5;"		//		__asm	PAND	xmmt, maskSign				
				"psrld	$31, %%xmm5;"			//		__asm	PSRLD	xmmt, 31					
				"cvtdq2ps %%xmm5, %%xmm5;"		//		__asm	CVTDQ2PS xmmt, xmmt					
				"mulps %%xmm3, %%xmm5;"			//		__asm	MULPS	xmmt, N2					
				"addps	%%xmm4, %%xmm5;"		//		__asm	ADDPS	xmmt, P1					
				"mulps %%xmm5, %%xmm1;"			//		__asm	MULPS	xmmm, xmmt					
				"mulps	%%xmm7, %%xmm1;"		//		__asm	MULPS	xmmm, coef			

				// 				STORE_FORWARD(XMM0, EDI)
				"movaps %%xmm0, (%3);"			//		__asm	MOVAPS	[addr], xmma				
				"addl $0x10, %3;"				//		__asm	ADD addr, 0x10

				// 				STORE_FORWARD(XMM1, EDI)
				"movaps %%xmm1, (%3);"			//		__asm	MOVAPS	[addr], xmma				
				"addl $0x10, %3;"				//		__asm	ADD addr, 0x10

				"decl %4;"
				"jnz L_no_transfer_load_line;"		//		LOOPNZ L_no_transfer_load_line


				"cmpl $0, %5;"					//		CMP longExtra, 0
				"je L_no_transfer_load_line_done;"	//		JE L_no_transfer_load_line_done

				// 		LOAD_NO_TRANSFER_FORWARD(XMM0, EAX,  XMM6)
				//		LOAD_NO_TRANSFER_FORWARD(xmmm, addr, coef)		
				"movaps (%1), %%xmm0;"			//		__asm	MOVAPS	xmmm, [addr]
				"addl $0x10, %1;"				//		__asm	ADD addr, 0x10						
				"mulps %%xmm6, %%xmm0;"			//		__asm	MULPS	xmmm, coef

				"movaps	%%xmm0, (%3);"			//		MOVAPS [EDI], XMM0

"L_no_transfer_load_line_done:;"
			// output
			:
			// input
			:	"r"(P_paras),		// %0
				"r"(lineL),			// %1	EAX
				"r"(lineH),			// %2	EDX
				"r"(tempBuf),		// %3	EDI
				"r"(loopLength),	// %4	ECX
				"g"(longExtra)		// %5
			: 
			);
		}
		else
		{
			// x parity, switched L and H
			__asm__ volatile (
				// 				MOV EAX, lineL
				// 				MOV EDX, lineH
				// 				MOV EDI, tempBuf
				// 				MOV ECX, loopLength
				"movaps (%0), %%xmm6;"			//				MOVAPS XMM6, lowGain
				"movaps 16(%0), %%xmm7;"		// 				MOVAPS XMM7, highGain
				"movaps	32(%0),	%%xmm2;"		// 				MOVAPS XMM2, maskS
				"movaps 64(%0), %%xmm3;"		// 				MOVAPS XMM3, floatN2
				"movaps	80(%0), %%xmm4;"		// 				MOVAPS XMM4, floatP1

"L_parity_no_transfer_load_line:;"
				// setp 1 and 2
				// 			LOAD_TRANSFER_FORWARD(XMM0, EAX,  XMM6, XMM2,    XMM3, XMM4, XMM5)
				// 			LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2,  P1,   xmmt)		
				"movaps (%1), %%xmm5;"			//		__asm	MOVAPS	xmmt, [addr]					
				"addl	$0x10, %1;"				//		__asm	ADD addr, 0x10						
				"movaps %%xmm2, %%xmm0;"		//		__asm	MOVAPS	xmmm, maskSign				
				"pandn %%xmm5, %%xmm0;"			//		__asm	PANDN	xmmm, xmmt				
				"cvtdq2ps %%xmm0, %%xmm0;"		//		__asm	CVTDQ2PS xmmm, xmmm					
				"pand	%%xmm2, %%xmm5;"		//		__asm	PAND	xmmt, maskSign				
				"psrld	$31, %%xmm5;"			//		__asm	PSRLD	xmmt, 31					
				"cvtdq2ps %%xmm5, %%xmm5;"		//		__asm	CVTDQ2PS xmmt, xmmt					
				"mulps %%xmm3, %%xmm5;"			//		__asm	MULPS	xmmt, N2					
				"addps	%%xmm4, %%xmm5;"		//		__asm	ADDPS	xmmt, P1					
				"mulps %%xmm5, %%xmm0;"			//		__asm	MULPS	xmmm, xmmt					
				"mulps	%%xmm6, %%xmm0;"		//		__asm	MULPS	xmmm, coef			

				// 		LOAD_NO_TRANSFER_FORWARD(XMM1, EDX, XMM7)
				//		LOAD_NO_TRANSFER_FORWARD(xmmm, addr, coef)		
				"movaps (%2), %%xmm1;"			//		__asm	MOVAPS	xmmm, [addr]
				"addl $0x10, %2;"				//		__asm	ADD addr, 0x10						
				"mulps %%xmm7, %%xmm1;"			//		__asm	MULPS	xmmm, coef


				// 				STORE_FORWARD(XMM0, EDI)
				"movaps %%xmm0, (%3);"			//		__asm	MOVAPS	[addr], xmma				
				"addl $0x10, %3;"				//		__asm	ADD addr, 0x10

				// 				STORE_FORWARD(XMM1, EDI)
				"movaps %%xmm1, (%3);"			//		__asm	MOVAPS	[addr], xmma				
				"addl $0x10, %3;"				//		__asm	ADD addr, 0x10

				"decl %4;"
				"jnz L_parity_no_transfer_load_line;"		//		LOOPNZ L_parity_no_transfer_load_line


				"cmpl $0, %5;"					//		CMP longExtra, 0
				"je L_parity_no_transfer_load_line_done;"	//		JE L_parity_no_transfer_load_line_done

				// 		LOAD_TRANSFER_FORWARD(XMM0, EAX, XMM6, XMM2, XMM3,XMM4, XMM5)
				// 		LOAD_TRANSFER_FORWARD(xmmm, addr, coef, maskSign, N2,  P1,   xmmt)		
				"movaps (%1), %%xmm5;"			//		__asm	MOVAPS	xmmt, [addr]					
				"addl	$0x10, %1;"				//		__asm	ADD addr, 0x10						
				"movaps %%xmm2, %%xmm0;"		//		__asm	MOVAPS	xmmm, maskSign				
				"pandn %%xmm5, %%xmm0;"			//		__asm	PANDN	xmmm, xmmt				
				"cvtdq2ps %%xmm0, %%xmm0;"		//		__asm	CVTDQ2PS xmmm, xmmm					
				"pand	%%xmm2, %%xmm5;"		//		__asm	PAND	xmmt, maskSign				
				"psrld	$31, %%xmm5;"			//		__asm	PSRLD	xmmt, 31					
				"cvtdq2ps %%xmm5, %%xmm5;"		//		__asm	CVTDQ2PS xmmt, xmmt					
				"mulps %%xmm3, %%xmm5;"			//		__asm	MULPS	xmmt, N2					
				"addps	%%xmm4, %%xmm5;"		//		__asm	ADDPS	xmmt, P1					
				"mulps %%xmm5, %%xmm0;"			//		__asm	MULPS	xmmm, xmmt					
				"mulps	%%xmm6, %%xmm0;"		//		__asm	MULPS	xmmm, coef			

				"movaps	%%xmm0, (%3);"			//		MOVAPS [EDI], XMM0

"L_parity_no_transfer_load_line_done:;"
				// output
				:
			// input
			:	"r"(P_paras),		// %0
				"r"(lineL),			// %1	EAX
				"r"(lineH),			// %2	EDX
				"r"(tempBuf),		// %3	EDI
				"r"(loopLength),	// %4	ECX
				"g"(longExtra)		// %5
				: 
			);
		}

	}

	if (!xParity)
	{
		hdwt_analyst_even(tempBuf, loopLength, longExtra, _mm_set_ps1(IDWT_DELTA), low_w, lhParity);
		hdwt_analyst_odd(tempBuf, loopLength, longExtra, _mm_set_ps1(IDWT_GAMMA), low_w, lhParity);
		hdwt_analyst_even(tempBuf, loopLength, longExtra, _mm_set_ps1(IDWT_BETA), low_w, lhParity);
		hdwt_analyst_odd(tempBuf, loopLength, longExtra,_mm_set_ps1(IDWT_ALPHA),low_w, lhParity, result, low_w+high_w );	// repack the result
	}
	else
	{
		hdwt_analyst_odd(tempBuf, loopLength, longExtra, _mm_set_ps1(IDWT_DELTA), high_w, lhParity);
		hdwt_analyst_even(tempBuf, loopLength, longExtra , _mm_set_ps1(IDWT_GAMMA), high_w, lhParity);
		hdwt_analyst_odd(tempBuf, loopLength, longExtra, _mm_set_ps1(IDWT_BETA), high_w, lhParity);
		hdwt_analyst_even(tempBuf, loopLength, longExtra, _mm_set_ps1(IDWT_ALPHA),high_w, lhParity, result, low_w+high_w);	// repack the result
	}

}


void BMI::vdwt_analyst_line_func(unsigned char * dst, unsigned char * src, unsigned char * left, unsigned char * right, int loopX, FLOAT_REG coef)
{
	FLOAT_REG * Pcoef = &coef;

	__asm__ volatile (


		"pushl %%eax;"
		"movl %0, %%eax;"
		"movaps (%%eax), %%xmm7;"
		"popl %%eax;"

// 			MOV ESI, src
// 			MOV EDI, dst
// 			MOV EAX, left
// 			MOV EDX, right
// 			MOV ECX, loopX
"vdwt_analyst_line_loop:;"
		"movaps (%3), %%xmm0;"		// 		MOVAPS XMM0, [EAX]
		"movaps (%4), %%xmm1;"		// 		MOVAPS XMM1, [EDX]
		"addps %%xmm1, %%xmm0;"		// 		ADDPS XMM0, XMM1
		"mulps %%xmm7, %%xmm0;"		// 			MULPS XMM0, XMM7
		"movaps	(%1), %%xmm1;"		// 			MOVAPS XMM1, [ESI]
		"subps %%xmm0, %%xmm1;"		// 		SUBPS XMM1, XMM0
		"movaps %%xmm1, (%2);"		// 			MOVAPS [EDI], XMM1
		
		"addl $0x10, %1;"
		"addl $0x10, %2;"
		"addl $0x10, %3;"
		"addl $0x10, %4;"
// 			ADD EAX, 0x10
// 			ADD EDX, 0x10
// 			ADD ESI, 0x10
// 			ADD EDI, 0x10

		"dec %5;"
		"jnz vdwt_analyst_line_loop;"	// 			LOOPNZ vdwt_analyst_line_loop
		// output
		:
		// input
		:
		"g"(Pcoef),	// %0
		"r"(src),	// %1
		"r"(dst),	// %2
		"r"(left),	// %3
		"r"(right),	// %4
		"r"(loopX)	// %5
		:

		);

}

#elif  0 //AT_T_ASSEMBLY		// #if INTEL_ASSEMBLY_32

namespace	BMI
{

	//							analystSrc
	//			analystLeft			|		analystRight
	//						\		|		/
	//							analystTarget
// 	void	vdwt_analyst(unsigned char * analystSrc, unsigned char * analystLeft, unsigned char * analystRight, unsigned char * analystTarget,unsigned char * lastRight, int loopY, int loopX,  __m128 coef, int sourceLineSkip, int middleLineSkip, int targetLineSkip)
// 	{
// 		__asm
// 		{
// 
// 			MOVAPS XMM7, coef
// 				MOV ESI, analystSrc
// 				MOV EDI, analystTarget
// 				MOV EAX, analystLeft
// 				MOV EDX, analystRight
// 				MOV ECX, loopY
// 				DEC ECX
// 				JZ last_line
// y_loop:
// 			PUSH ECX
// 				PUSH EDX		// push right
// 				MOV  ECX, loopX
// x_loop:
// 			MOVAPS XMM0, [EAX]
// 			MOVAPS XMM1, [EDX]
// 			ADDPS XMM0, XMM1
// 				MULPS XMM0, XMM7
// 				MOVAPS XMM1, [ESI]
// 			SUBPS XMM1, XMM0
// 				MOVAPS [EDI], XMM1
// 				ADD EAX, 0x10
// 				ADD EDX, 0x10
// 				ADD ESI, 0x10
// 				ADD EDI, 0x10
// 
// 				LOOPNZ x_loop
// 				POP EAX		// pop to left
// 				POP ECX
// 				ADD ESI, sourceLineSkip
// 				ADD EDX, middleLineSkip
// 				ADD EDI, targetLineSkip
// 				LOOPNZ y_loop
// last_line:
// 			MOV EDX, lastRight
// 				MOV ECX, loopX
// last_line_loop:
// 			MOVAPS XMM0, [EAX]
// 			MOVAPS XMM1, [EDX]
// 			ADDPS XMM0, XMM1
// 				MULPS XMM0, XMM7
// 				MOVAPS XMM1, [ESI]
// 			SUBPS XMM1, XMM0
// 				MOVAPS [EDI], XMM1
// 				ADD EAX, 0x10
// 				ADD EDX, 0x10
// 				ADD ESI, 0x10
// 				ADD EDI, 0x10
// 
// 				LOOPNZ last_line_loop
// 		}
// 	}

	void vdwt_analyst_line_func(unsigned char * dst, unsigned char * src, unsigned char * left, unsigned char * right, int loopX, FLOAT_REG coef)
	{

		//							    S
		//				       L		|		 R
		//						\		|		/
		//							    D	
		float * S = (float *) src, * D = (float *)dst, * L = (float *)left, * R = (float *)right;

		for (int i = 0; i < loopX * 4; ++i, ++S, ++D, ++L, ++R)
		{
			*D = *S - coef *(*L + *R);
		}

// 		__asm
// 		{
// 			MOVAPS XMM7, coef
// 				MOV ESI, src
// 				MOV EDI, dst
// 				MOV EAX, left
// 				MOV EDX, right
// 				MOV ECX, loopX
// 
// vdwt_analyst_line_loop:
// 			MOVAPS XMM0, [EAX]
// 			MOVAPS XMM1, [EDX]
// 			ADDPS XMM0, XMM1
// 				MULPS XMM0, XMM7
// 				MOVAPS XMM1, [ESI]
// 			SUBPS XMM1, XMM0
// 				MOVAPS [EDI], XMM1
// 				ADD EAX, 0x10
// 				ADD EDX, 0x10
// 				ADD ESI, 0x10
// 				ADD EDI, 0x10
// 
// 				LOOPNZ vdwt_analyst_line_loop
// 		}

	}


	void	CpuDwtDecoder::VDWT_R97_FLOAT_ASM_32(unsigned char * target,unsigned char * H_result_up, unsigned char * H_result_down, int low_h, int high_h, int width, int linesOffsetInlow, int linesDecodeInLow, uint_8 yParity, int threadId)
	{

		BMI_ASSERT(0);
	}



	void	CpuDwtDecoder::RMCT_R97_FLOAT_ASM(unsigned char  * result, float * comp0, float * comp1, float * comp2, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat, short checkOutComp)
	{

// 		if (outFormat != IMAGE_FOTMAT_32BITS_ARGB && outFormat != IMAGE_FOTMAT_24BITS_RGB)
// 		{
			RICT_97_FLOAT(result, comp0, comp1, comp2, resultPitch, width, height, bitDepth, outFormat, checkOutComp);
			return;
// 		}
// #pragma BMI_PRAGMA_TODO_MSG("Implement MCT 97 in Assemblly for other format")
// #pragma BMI_PRAGMA_TODO_MSG("Implement MCT 97 in Assemblly use alligned memory for better performance, replace MOVUPS with MOVAPS")
// 
// 
// 
// 		static __m128	c2_c0_ratio_128 = _mm_set_ps1(1.402f);
// 		static __m128	c1_c1_ratio_128 = _mm_set_ps1(-0.34413f);
// 		static __m128	c2_c1_ratio_128 = _mm_set_ps1(-0.71414f);
// 		static __m128	c2_c0_c1_ratio_128 = _mm_set_ps1(-0.71414f / 1.402f);
// 		static __m128	c1_c2_ratio_128 = _mm_set_ps1(1.772f);
// 		static __m128	c1_c1_c2_ratio_128 = _mm_set_ps1(1.772f / (-0.34413f));
// 
// 		static __m128	float_0 =  _mm_set_ps1(0.0f);
// 		static __m128	lift_128 = _mm_set_ps1(128.0f);
// 		static __m128	float_255 =  _mm_set_ps1(255.999f);
// 
// 
// 		DEBUG_XMM_VAL;
// 
// 		int inPitch = ALIGN_TO_SSE_DWORDS(width);
// 		int tail = 4 + width - inPitch;
// 		int h = height;
// 
// 		switch(outFormat)
// 		{
// 		case IMAGE_FOTMAT_32BITS_ARGB:
// 			{
// 				int destSkip = (resultPitch - width) * 4;
// 				__m128	convert_to_8_bits = bitDepth > 8 ? _mm_set_ps1(1.0f / static_cast<float>(1 << (bitDepth - 8))) : _mm_set_ps1(static_cast<float>(1 << (8 - bitDepth)));
// 
// 				if (checkOutComp == INVALID)
// 				{
// 					__asm
// 					{
// 						MOV ESI, comp0
// 							MOV EAX, comp1
// 							MOV EDX, comp2
// 							MOV EDI, result
// 
// 							MOVAPS	XMM4, c2_c0_ratio_128
// 							CHECK_VAL(XMM4)
// 							MULPS	XMM4, convert_to_8_bits
// 							CHECK_VAL(XMM4)
// 							MOVAPS	XMM5, c1_c1_ratio_128
// 							MULPS	XMM5, convert_to_8_bits
// 							MOVAPS	XMM6, float_0
// 							MOVAPS	XMM7, float_255
// 
// height_loop_32bits_argb:
// 						MOV ECX, inPitch
// 							SHR ECX, 2		// ecx /= 4
// 							DEC ECX			// inpitch - 1
// 
// width_loop_32bits_argb:
// 						MOVAPS	XMM0, [ESI]	// component 0
// 						CHECK_VAL(XMM0)
// 							MULPS	XMM0, convert_to_8_bits
// 							CHECK_VAL(XMM0)
// 							ADDPS	XMM0, lift_128	// do the lifting first
// 							ADD ESI, 0x10
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						CHECK_VAL(XMM1)
// 							ADD EAX, 0x10
// 							MOVUPS	XMM2, [EDX]	// component 2
// 						CHECK_VAL(XMM2)
// 							ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							CHECK_VAL(XMM2)
// 							CHECK_VAL(XMM4)
// 							MOVAPS	XMM3, XMM0
// 							ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
// 							CHECK_VAL(XMM3)
// 							MINPS	XMM3, XMM7	// clipping
// 							MAXPS	XMM3, XMM6
// 							CVTTPS2DQ XMM3, XMM3
// 							CHECK_VAL(XMM3)
// 
// 							MULPS	XMM2, c2_c0_c1_ratio_128
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM2, XMM1
// 							ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 							MINPS	XMM2, XMM7	// clipping
// 							MAXPS	XMM2, XMM6
// 							CVTTPS2DQ XMM2, XMM2
// 							CHECK_VAL(XMM2)
// 							PSLLD	XMM3, 8	
// 							CHECK_VAL(XMM3)
// 							POR		XMM2, XMM3		// R + G
// 
// 							MULPS	XMM1, c1_c1_c2_ratio_128
// 							ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM1, XMM7	// clipping
// 							MAXPS	XMM1, XMM6
// 							CVTTPS2DQ XMM1, XMM1
// 							PSLLD	XMM2, 8
// 							CHECK_VAL(XMM2)
// 							POR	XMM1, XMM2		// R + G + B
// 							CHECK_VAL(XMM1)
// 
// 							MOVUPS	[EDI], XMM1
// 							ADD EDI, 0x10
// 
// 							DEC ECX
// 							JNZ	width_loop_32bits_argb
// 
// 							// last 4-pixels
// 							MOVAPS	XMM0, [ESI]	// component 0
// 						CHECK_VAL(XMM0)
// 							MULPS	XMM0, convert_to_8_bits
// 							CHECK_VAL(XMM0)
// 							ADDPS	XMM0, lift_128	// do the lifting first
// 							ADD ESI, 0x10
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						CHECK_VAL(XMM1)
// 							ADD EAX, 0x10
// 							MOVUPS	XMM2, [EDX]	// component 2
// 						CHECK_VAL(XMM2)
// 							ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							CHECK_VAL(XMM2)
// 							CHECK_VAL(XMM4)
// 							MOVAPS	XMM3, XMM0
// 							ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
// 							CHECK_VAL(XMM3)
// 							MINPS	XMM3, XMM7	// clipping
// 							MAXPS	XMM3, XMM6
// 							CVTTPS2DQ XMM3, XMM3
// 							CHECK_VAL(XMM3)
// 
// 							MULPS	XMM2, c2_c0_c1_ratio_128
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM2, XMM1
// 							ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 							MINPS	XMM2, XMM7	// clipping
// 							MAXPS	XMM2, XMM6
// 							CVTTPS2DQ XMM2, XMM2
// 							CHECK_VAL(XMM2)
// 							PSLLD	XMM3, 8	
// 							CHECK_VAL(XMM3)
// 							POR		XMM2, XMM3		// R + G
// 
// 							MULPS	XMM1, c1_c1_c2_ratio_128
// 							ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM1, XMM7	// clipping
// 							MAXPS	XMM1, XMM6
// 							CVTTPS2DQ XMM1, XMM1
// 							PSLLD	XMM2, 8
// 							CHECK_VAL(XMM2)
// 							POR	XMM1, XMM2		// R + G + B
// 							CHECK_VAL(XMM1)
// 
// 							CMP tail, 4
// 							JE tail_4_pixels__32bits_argb
// 							CMP tail, 3
// 							JE tail_3_pixels__32bits_argb
// 							CMP tail, 2
// 							JE tail_2_pixels__32bits_argb
// 							JMP tail_1_pixels__32bits_argb
// 
// tail_3_pixels__32bits_argb:
// 						MOVD	[EDI], XMM1
// 							PSRlDQ	XMM1, 4
// 							ADD EDI, 4
// tail_2_pixels__32bits_argb:
// 						MOVD	[EDI], XMM1
// 							PSRlDQ	XMM1, 4
// 							ADD EDI, 4
// tail_1_pixels__32bits_argb:
// 						MOVD	[EDI], XMM1
// 							ADD EDI, 4
// 							JMP width_loop_32bits_argb_done
// tail_4_pixels__32bits_argb:
// 						MOVUPS	[EDI], XMM1
// 							ADD EDI, 16
// 
// width_loop_32bits_argb_done:
// 						ADD EDI, destSkip
// 							DEC h
// 							JNZ height_loop_32bits_argb
// 					}
// 				}
// 				else if (checkOutComp == 0 || checkOutComp == 2)		// checkout component 0
// 				{
// 
// 					__asm
// 					{
// 						MOV EDI, result
// 							MOV ESI, comp0
// 
// 							MOVAPS XMM3, convert_to_8_bits
// 							MOVAPS XMM5, lift_128
// 							MOVAPS XMM6, float_0
// 							MOVAPS XMM7, float_255
// 
// 							CMP checkOutComp, 0
// 							JE check_out_comp0_32bits_argb
// 
// 							// check_out_comp2_32bits_argb:
// 							MOV EDX, comp1
// 							MOVAPS XMM4, c1_c2_ratio_128
// 							MULPS	XMM4, XMM3
// 							JMP height_loop_0_2_32bits_argb
// 
// check_out_comp0_32bits_argb:
// 						MOV EDX, comp2
// 							MOVAPS XMM4, c2_c0_ratio_128
// 							MULPS	XMM4, XMM3
// 
// height_loop_0_2_32bits_argb:
// 						MOV ECX, inPitch
// 							SHR ECX, 2		// ecx /= 4
// 							DEC ECX			// inpitch - 1
// 
// width_loop_0_2_32bits_argb:
// 						MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM5	// do the lifting
// 							MOVAPS	XMM2, [EDX]	// component 2 or 1
// 						ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM6
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 							MOVUPS	[EDI], XMM1
// 							ADD EDI, 0x10
// 
// 							LOOPNZ	width_loop_0_2_32bits_argb
// 
// 
// 							// last 4-pixels
// 							MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM5	// do the lifting
// 							MOVUPS	XMM2, [EDX]	// component 2 or 1
// 						ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM6
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 
// 							CMP tail, 4
// 							JE tail_4_pixels__32bits_argb_0_2
// 							CMP tail, 3
// 							JE tail_3_pixels__32bits_argb_0_2
// 							CMP tail, 2
// 							JE tail_2_pixels__32bits_argb_0_2
// 							JMP tail_1_pixels__32bits_argb_0_2
// 
// 
// tail_3_pixels__32bits_argb_0_2:
// 						MOVD	[EDI], XMM1
// 							PSRlDQ	XMM1, 4
// 							ADD EDI, 4
// tail_2_pixels__32bits_argb_0_2:
// 						MOVD	[EDI], XMM1
// 							PSRlDQ	XMM1, 4
// 							ADD EDI, 4
// tail_1_pixels__32bits_argb_0_2:
// 						MOVD	[EDI], XMM1
// 							ADD EDI, 4
// 							JMP width_loop_32bits_argb_done_0_2
// tail_4_pixels__32bits_argb_0_2:
// 						MOVUPS	[EDI], XMM1
// 							ADD EDI, 16
// 
// width_loop_32bits_argb_done_0_2:
// 						ADD EDI, destSkip
// 							DEC h
// 							JNZ height_loop_0_2_32bits_argb	
// 
// 					}
// 
// 				}
// 				else if (checkOutComp == 1)
// 				{
// 
// 					__asm
// 					{
// 						MOV EDI, result
// 							MOV ESI, comp0
// 							MOV EAX, comp1
// 							MOV EDX, comp2
// 
// 							MOVAPS XMM2, lift_128
// 							MOVAPS XMM3, convert_to_8_bits
// 							MOVAPS XMM4, float_0
// 							MOVAPS XMM7, float_255
// 
// 							MOVAPS XMM5, c1_c1_ratio_128
// 							MULPS	XMM5, XMM3
// 							MOVAPS XMM6, c2_c1_ratio_128
// 							MULPS	XMM6, XMM3
// 
// height_loop_1_32bits_argb:
// 						MOV ECX, inPitch
// 							SHR ECX, 2		// ecx /= 4
// 							DEC ECX			// inpitch - 1
// 
// width_loop_1_32bits_argb:
// 						MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM2	// do the lifting
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						ADD EAX, 0x10
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM0, XMM1
// 
// 							MOVAPS	XMM1, [EDX]	// component 2
// 						ADD EDX, 0x10
// 							MULPS	XMM1, XMM6
// 							ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM4
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 							MOVUPS	[EDI], XMM1
// 							ADD EDI, 0x10
// 
// 							LOOPNZ	width_loop_1_32bits_argb
// 
// 							// last 4-pixels
// 							MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM2	// do the lifting
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						ADD EAX, 0x10
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM0, XMM1
// 
// 							MOVAPS	XMM1, [EDX]	// component 2
// 						ADD EDX, 0x10
// 							MULPS	XMM1, XMM6
// 							ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM4
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 							CMP tail, 4
// 							JE tail_4_pixels__32bits_argb_1
// 							CMP tail, 3
// 							JE tail_3_pixels__32bits_argb_1
// 							CMP tail, 2
// 							JE tail_2_pixels__32bits_argb_1
// 							JMP tail_1_pixels__32bits_argb_1
// 
// tail_3_pixels__32bits_argb_1:
// 						MOVD	[EDI], XMM1
// 							PSRlDQ	XMM1, 4
// 							ADD EDI, 4
// tail_2_pixels__32bits_argb_1:
// 						MOVD	[EDI], XMM1
// 							PSRlDQ	XMM1, 4
// 							ADD EDI, 4
// tail_1_pixels__32bits_argb_1:
// 						MOVD	[EDI], XMM1
// 							ADD EDI, 4
// 							JMP width_loop_32bits_argb_done_1
// 
// tail_4_pixels__32bits_argb_1:
// 						MOVUPS	[EDI], XMM1
// 							ADD EDI, 16
// 
// width_loop_32bits_argb_done_1:
// 						ADD EDI, destSkip
// 							DEC h
// 							JNZ height_loop_1_32bits_argb	
// 					}
// 
// 				}
// 				else
// 				{
// 					BMI_ASSERT(0);
// 				}
// 
// 			}
// 			break;
// 
// 		case IMAGE_FOTMAT_24BITS_RGB:
// 			{
// 				int destSkip = (resultPitch - width) * 3;
// 				__m128	convert_to_8_bits = bitDepth > 8 ? _mm_set_ps1(1.0f / static_cast<float>(1 << (bitDepth - 8))) : _mm_set_ps1(static_cast<float>(1 << (8 - bitDepth)));
// 
// 				if (checkOutComp == INVALID)
// 				{
// 					__asm
// 					{
// 						MOV ESI, comp0
// 							MOV EAX, comp1
// 							MOV EDX, comp2
// 							MOV EDI, result
// 
// 							MOVAPS	XMM4, c2_c0_ratio_128
// 							CHECK_VAL(XMM4)
// 							MULPS	XMM4, convert_to_8_bits
// 							CHECK_VAL(XMM4)
// 							MOVAPS	XMM5, c1_c1_ratio_128
// 							MULPS	XMM5, convert_to_8_bits
// 							MOVAPS	XMM6, float_0
// 							MOVAPS	XMM7, float_255
// 
// height_loop_24bits_rgb:
// 						MOV ECX, inPitch
// 							SHR ECX, 2		// ecx /= 4
// 							DEC ECX			// inpitch - 1
// 
// width_loop_24bits_rgb:
// 						MOVAPS	XMM0, [ESI]	// component 0
// 						CHECK_VAL(XMM0)
// 							MULPS	XMM0, convert_to_8_bits
// 							CHECK_VAL(XMM0)
// 							ADDPS	XMM0, lift_128	// do the lifting first
// 							ADD ESI, 0x10
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						CHECK_VAL(XMM1)
// 							ADD EAX, 0x10
// 							MOVUPS	XMM2, [EDX]	// component 2
// 						CHECK_VAL(XMM2)
// 							ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							CHECK_VAL(XMM2)
// 							CHECK_VAL(XMM4)
// 							MOVAPS	XMM3, XMM0
// 							ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
// 							CHECK_VAL(XMM3)
// 							MINPS	XMM3, XMM7	// clipping
// 							MAXPS	XMM3, XMM6
// 							CVTTPS2DQ XMM3, XMM3
// 							CHECK_VAL(XMM3)
// 
// 							MULPS	XMM2, c2_c0_c1_ratio_128
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM2, XMM1
// 							ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 							MINPS	XMM2, XMM7	// clipping
// 							MAXPS	XMM2, XMM6
// 							CVTTPS2DQ XMM2, XMM2
// 							CHECK_VAL(XMM2)
// 							PSLLD	XMM3, 8	
// 							CHECK_VAL(XMM3)
// 							POR		XMM2, XMM3		// R + G
// 
// 							MULPS	XMM1, c1_c1_c2_ratio_128
// 							ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM1, XMM7	// clipping
// 							MAXPS	XMM1, XMM6
// 							CVTTPS2DQ XMM1, XMM1
// 							PSLLD	XMM2, 8
// 							CHECK_VAL(XMM2)
// 							POR	XMM1, XMM2		// R + G + B
// 							CHECK_VAL(XMM1)
// 							// 							PSLLD	XMM1, 8		// 0RGB -> RGB0
// 							CHECK_VAL(XMM1)
// 
// 
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							CHECK_VAL(XMM1)
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 
// 							DEC ECX
// 							JNZ	width_loop_24bits_rgb
// 
// 							// last 4-pixels
// 							MOVAPS	XMM0, [ESI]	// component 0
// 						CHECK_VAL(XMM0)
// 							MULPS	XMM0, convert_to_8_bits
// 							CHECK_VAL(XMM0)
// 							ADDPS	XMM0, lift_128	// do the lifting first
// 							ADD ESI, 0x10
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						CHECK_VAL(XMM1)
// 							ADD EAX, 0x10
// 							MOVUPS	XMM2, [EDX]	// component 2
// 						CHECK_VAL(XMM2)
// 							ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							CHECK_VAL(XMM2)
// 							CHECK_VAL(XMM4)
// 							MOVAPS	XMM3, XMM0
// 							ADDPS	XMM3, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	
// 							CHECK_VAL(XMM3)
// 							MINPS	XMM3, XMM7	// clipping
// 							MAXPS	XMM3, XMM6
// 							CVTTPS2DQ XMM3, XMM3
// 							CHECK_VAL(XMM3)
// 
// 							MULPS	XMM2, c2_c0_c1_ratio_128
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM2, XMM1
// 							ADDPS	XMM2, XMM0	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 							MINPS	XMM2, XMM7	// clipping
// 							MAXPS	XMM2, XMM6
// 							CVTTPS2DQ XMM2, XMM2
// 							CHECK_VAL(XMM2)
// 							PSLLD	XMM3, 8	
// 							CHECK_VAL(XMM3)
// 							POR		XMM2, XMM3		// R + G
// 
// 							MULPS	XMM1, c1_c1_c2_ratio_128
// 							ADDPS	XMM1, XMM0	// Blue : *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM1, XMM7	// clipping
// 							MAXPS	XMM1, XMM6
// 							CVTTPS2DQ XMM1, XMM1
// 							PSLLD	XMM2, 8
// 							CHECK_VAL(XMM2)
// 							POR	XMM1, XMM2		// R + G + B
// 							CHECK_VAL(XMM1)
// 							// 							PSLLD	XMM1, 8		// 0RGB -> RGB0
// 
// 
// 							CMP tail, 4
// 							JE tail_4_pixels__24bits_rgb
// 							CMP tail, 3
// 							JE tail_3_pixels__24bits_rgb
// 							CMP tail, 2
// 							JE tail_2_pixels__24bits_rgb
// 							JMP last_pizel_24bits_rgb
// 
// tail_4_pixels__24bits_rgb:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// tail_3_pixels__24bits_rgb:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// tail_2_pixels__24bits_rgb:
// 						CHECK_VAL(XMM1)
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// last_pizel_24bits_rgb:
// 						CHECK_VAL(XMM1)
// 							MOVD ECX, XMM1
// 							MOV [EDI], CX
// 							SHR ECX, 16
// 							MOV [EDI+2], CL
// 							ADD EDI, 3
// 
// 
// 							ADD EDI, destSkip
// 							DEC h
// 							JNZ height_loop_24bits_rgb
// 					}
// 				}
// 				else if (checkOutComp == 0 || checkOutComp == 2)		// checkout component 0
// 				{
// 
// 					__asm
// 					{
// 						MOV EDI, result
// 							MOV ESI, comp0
// 
// 							MOVAPS XMM3, convert_to_8_bits
// 							MOVAPS XMM5, lift_128
// 							MOVAPS XMM6, float_0
// 							MOVAPS XMM7, float_255
// 
// 							CMP checkOutComp, 0
// 							JE check_out_comp0_24bits_rgb
// 
// 							MOV EDX, comp1
// 							MOVAPS XMM4, c1_c2_ratio_128
// 							MULPS	XMM4, XMM3
// 							JMP height_loop_0_2_24bits_rgb
// 
// check_out_comp0_24bits_rgb:
// 						MOV EDX, comp2
// 							MOVAPS XMM4, c2_c0_ratio_128
// 							MULPS	XMM4, XMM3
// 
// height_loop_0_2_24bits_rgb:
// 						MOV ECX, inPitch
// 							SHR ECX, 2		// ecx /= 4
// 							DEC ECX			// inpitch - 1
// 
// width_loop_0_2_24bits_rgb:
// 						MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM5	// do the lifting
// 							MOVAPS	XMM2, [EDX]	// component 2 or 1
// 						ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM6
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 							// 							PSLLD	XMM1, 8		// 0RGB -> RGB0
// 
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 
// 							LOOPNZ	width_loop_0_2_24bits_rgb
// 
// 							MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM5	// do the lifting
// 							MOVAPS	XMM2, [EDX]	// component 2 or 1
// 						ADD EDX, 0x10
// 
// 							MULPS	XMM2, XMM4
// 							ADDPS	XMM0, XMM2	// Red : *comp0 + lift + FIX_MUL(C2_C0_RATIO , *comp2);	 or Blue: *comp0 + lift + FIX_MUL(C1_C2_RATIO , *comp1);
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM6
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							// 							PSLLD	XMM1, 8		// 0RGB -> RGB0
// 
// 							CMP tail, 4
// 							JE tail_4_pixels__24bits_rgb_0_2
// 							CMP tail, 3
// 							JE tail_3_pixels__24bits_rgb_0_2
// 							CMP tail, 2
// 							JE tail_2_pixels__24bits_rgb_0_2
// 							JMP last_pizel_24bits_rgb_0_2
// 
// tail_4_pixels__24bits_rgb_0_2:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// tail_3_pixels__24bits_rgb_0_2:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// tail_2_pixels__24bits_rgb_0_2:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// last_pizel_24bits_rgb_0_2:
// 						MOVD ECX, XMM1
// 							MOV [EDI], CX
// 							SHR ECX, 16
// 							MOV [EDI+2], CL
// 							ADD EDI, 3
// 
// 
// 							ADD EDI, destSkip
// 							DEC h
// 							JNZ height_loop_0_2_24bits_rgb
// 					}
// 
// 				}
// 				else if (checkOutComp == 1)
// 				{
// 
// 					__asm
// 					{
// 						MOV EDI, result
// 							MOV ESI, comp0
// 							MOV EAX, comp1
// 							MOV EDX, comp2
// 
// 							MOVAPS XMM2, lift_128
// 							MOVAPS XMM3, convert_to_8_bits
// 							MOVAPS XMM4, float_0
// 							MOVAPS XMM7, float_255
// 
// 							MOVAPS XMM5, c1_c1_ratio_128
// 							MULPS	XMM5, XMM3
// 							MOVAPS XMM6, c2_c1_ratio_128
// 							MULPS	XMM6, XMM3
// 
// height_loop_1_24bits_rgb:
// 						MOV ECX, inPitch
// 							SHR ECX, 2		// ecx /= 4
// 							DEC ECX			// inpitch - 1
// 
// width_loop_1_24bits_rgb:
// 						MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM2	// do the lifting
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						ADD EAX, 0x10
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM0, XMM1
// 
// 							MOVAPS	XMM1, [EDX]	// component 2
// 						ADD EDX, 0x10
// 							MULPS	XMM1, XMM6
// 							ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM4
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 							// 							PSLLD	XMM1, 8		// 0RGB -> RGB0
// 
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// 							MOVD	[EDI], XMM1
// 							ADD EDI,3
// 
// 							LOOPNZ	width_loop_1_24bits_rgb
// 
// 							// last 4-pixels
// 							MOVAPS	XMM0, [ESI]	// component 0
// 						ADD ESI, 0x10
// 							MULPS	XMM0, XMM3
// 							ADDPS	XMM0, XMM2	// do the lifting
// 							MOVUPS	XMM1, [EAX]	// component 1
// 						ADD EAX, 0x10
// 							MULPS	XMM1, XMM5
// 							ADDPS	XMM0, XMM1
// 
// 							MOVAPS	XMM1, [EDX]	// component 2
// 						ADD EDX, 0x10
// 							MULPS	XMM1, XMM6
// 							ADDPS	XMM0, XMM1	// Green : *comp0 + lift + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);
// 							MINPS	XMM0, XMM7	// clipping
// 							MAXPS	XMM0, XMM4
// 							CVTTPS2DQ XMM0, XMM0		// convert to integer
// 							MOVAPS	XMM1, XMM0		// convert to grey scaled
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 							PSLLDQ XMM0, 1
// 							POR	XMM1, XMM0
// 
// 							CMP tail, 4
// 							JE tail_4_pixels__24bits_rgb_1
// 							CMP tail, 3
// 							JE tail_3_pixels__24bits_rgb_1
// 							CMP tail, 2
// 							JE tail_2_pixels__24bits_rgb_1
// 							JMP last_pixel_24bits_rgb_1
// 
// 							// One float
// 
// tail_4_pixels__24bits_rgb_1:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// tail_3_pixels__24bits_rgb_1:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// tail_2_pixels__24bits_rgb_1:
// 						MOVD	[EDI], XMM1
// 							ADD EDI,3
// 							PSRlDQ	XMM1, 4
// last_pixel_24bits_rgb_1:
// 						MOVD ECX, XMM1
// 							MOV [EDI], CX
// 							SHR ECX, 16
// 							MOV [EDI+2], CL
// 							ADD EDI, 3
// 
// 							ADD EDI, destSkip
// 							DEC h
// 							JNZ height_loop_1_24bits_rgb	
// 					}
// 
// 				}
// 				else
// 				{
// 					BMI_ASSERT(0);
// 				}
// 
// 			}
// 			break;
// 
// 		case IMAGE_FOTMAT_36BITS_RGB:
// 		case IMAGE_FOTMAT_48BITS_ARGB:
// 		case IMAGE_FOTMAT_48BITS_RGB:
// 		case IMAGE_FOTMAT_64BITS_ARGB:
// #pragma BMI_PRAGMA_TODO_MSG("To implement MCT assembly for more format")
// 		default:
// 			BMI_ASSERT(0);
// 			break;
// 		}
// 
// 		return;
	}


	void	CpuDwtDecoder::RMCT_R53_INT32_ASM(void * resultBuf, int * redBuf, int * greenBuf, int * blueBuf, int bufPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp)
	{

// 		if (((int)resultBuf & 0x0f) || ((int)redBuf & 0x0f) || ((int)greenBuf & 0x0f) || ((int)blueBuf & 0x0f) || outFormat != IMAGE_FOTMAT_32BITS_ARGB)
// 		{
			RRct_53<int_32>(resultBuf, redBuf, greenBuf, blueBuf, bufPitch, width, height, bitDepth, outFormat,checkOutComp);
			return;
// 		}
// #pragma BMI_PRAGMA_TODO_MSG("Implement MCT 53 in Assemblly for other format")
// 
// 		__m128i	int_255_128bits	= _mm_set1_epi32(255);
// 		__m128i	int_128_128bits	= _mm_set1_epi32(128);
// 		__m128i	int_0_128bits	= _mm_set1_epi32(0);
// 
// 		int rowSkip = (bufPitch-width)<<2;
// 
// 		__asm
// 		{
// 
// 			MOV EAX, width
// 
// 				MOV ESI, redBuf
// 				MOV ECX, greenBuf
// 				MOV EDX, blueBuf
// 
// 				MOV EDI, resultBuf
// 
// 				MOVDQA	XMM5, int_255_128bits
// 				MOVDQA	XMM6, int_128_128bits
// 				MOVDQA	XMM7, int_0_128bits
// 
// mct_loop:
// 
// 			MOVDQA	XMM0, [ESI]
// 			MOVDQA	XMM1, [ECX]
// 			MOVDQA	XMM2, [EDX]
// 
// 			MOVDQA	XMM3, XMM1
// 				PADDD	XMM3, XMM2
// 				PSRAD	XMM3, 2
// 
// 				PSUBD	XMM0, XMM3
// 				PADDD	XMM0, XMM6	// new_g = r - (g + b) / 4 + lift
// 				PADDD	XMM1, XMM0	// new_b = g + new_r
// 				PADDD	XMM2, XMM0	// new_r = b + new_g
// 
// 				PSLLD	XMM0, 8
// 				PADDD	XMM0, XMM1
// 				PSLLD	XMM2, 16
// 				PADDD	XMM0, XMM2
// 				MOVDQA	[EDI], XMM0
// 
// 				ADD ESI, 0x10
// 				ADD ECX, 0x10
// 				ADD EDX, 0x10
// 				ADD EDI, 0x10
// 
// 				SUB EAX, 4
// 				JNZ mct_loop
// 				ADD EDI, rowSkip
// 				MOV EAX, height
// 				DEC	EAX
// 				JZ  done
// 				MOV height, EAX
// 				MOV EAX, width
// 				JMP mct_loop
// done:
// 
// 		}

	}


};

#endif

#endif		// ENABLE_ASSEMBLY_OPTIMIZATION

