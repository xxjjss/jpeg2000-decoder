#pragma once

#include "codec.h"
#include "singleton.h"
#include "codecconcrete.h"
#include "codec_base.h"
#include "bmi_jp2dec.h"			// MAX_COMPONENT
#include "cudaInterface.h"		// borrow the definition for picture-tile-component-subband
#include "debug.h"

//#if _WINDOWS
#include <xmmintrin.h>
#include <emmintrin.h>
//#endif



// #define		BMI_DEQ_FIX_MUL(x, y)					((static_cast<BMI_INT_64>(x) * static_cast<BMI_INT_64>(y)) >> 13)
#define		SET_BOUNDARY(min_v, v, max_v)		((v) < (min_v) ? (min_v) : ((v) > (max_v) ? (max_v) : (v)))
#define		DOUBLE_TO_FIX(x)				(BMI_INT_64)((x) * (double)(0x2000))		//	0x2000 = 1 << 13
#define		FIX_MUL(x, y)					(int_32)(((BMI_INT_64)(x) * (BMI_INT_64)(y)) >> 13)
#define		LIFT(v, inDeep, outDeep)		(((inDeep) >= (outDeep)) ? ((v) >> ((inDeep)-(outDeep))) : ((v) << ((outDeep) - (inDeep))))

namespace BMI
{

	//////////////////////////////////////////////////////////////////////////
	// The following are the filter value we used in DWT
	//////////////////////////////////////////////////////////////////////////

	const	float	IDWT_ALPHA	=	-1.58613434205992f;
	const	float	IDWT_BETA	=	-0.05298011857296f;
	const	float	IDWT_GAMMA	=	 0.88291107553093f;
	const	float	IDWT_DELTA	=	 0.44350685204397f;

	const	float	IDWT_LO_GAIN =	 1.23017410558578f;
	const	float	IDWT_HI_GAIN =	 1.62578613134411f;		// (2.0 / IDWT_LO_GAIN)
	const	float	IDWT_RENORM	 =	 8192.0f;			// 1 << 13
	const	float	IDWT_NORM	 =	 0.0001220703125f;	// (1.0 / IDWT_RENORM)
	const	float	IDWT_LO_GAIN_NORM =	 IDWT_LO_GAIN * IDWT_NORM;
	const	float	IDWT_HI_GAIN_NORM =	 IDWT_HI_GAIN * IDWT_NORM;

#if  INTEL_ASSEMBLY_32 || AT_T_ASSEMBLY
	typedef __m128 FLOAT_REG;
#else
	typedef float FLOAT_REG;
#endif


#if AT_T_ASSEMBLY
#define 	FILL128(x)	_mm_set_ps1(x)
#else
#define 	FILL128(x)	x
#endif

#if  INTEL_ASSEMBLY_32
	//////////////////////////////////////////////////////////////////////////
	// Extent the above value to 128 bits for SIMD
	//////////////////////////////////////////////////////////////////////////

	const __m128 IDWT_LO_GAIN_NORM_128 = _mm_set_ps1(IDWT_LO_GAIN_NORM);
	const __m128 IDWT_HI_GAIN_NORM_128 = _mm_set_ps1(IDWT_HI_GAIN_NORM);
	const __m128 IDWT_LO_GAIN_128 = _mm_set_ps1(IDWT_LO_GAIN);
	const __m128 IDWT_HI_GAIN_128 = _mm_set_ps1(IDWT_HI_GAIN);
	const __m128 IDWT_DELTA_128 = _mm_set_ps1(IDWT_DELTA);
	const __m128 IDWT_GAMMA_128 = _mm_set_ps1(IDWT_GAMMA);
	const __m128 IDWT_BETA_128 = _mm_set_ps1(IDWT_BETA);
	const __m128 IDWT_ALPHA_128 = _mm_set_ps1(IDWT_ALPHA);
#else
#define	 IDWT_LO_GAIN_NORM_128	IDWT_LO_GAIN_NORM
#define  IDWT_HI_GAIN_NORM_128	IDWT_HI_GAIN_NORM,
#define  IDWT_LO_GAIN_128  IDWT_LO_GAIN 
#define  IDWT_HI_GAIN_128  IDWT_HI_GAIN 
#define  IDWT_DELTA_128  IDWT_DELTA 
#define  IDWT_GAMMA_128  IDWT_GAMMA 
#define  IDWT_BETA_128  IDWT_BETA 
#define  IDWT_ALPHA_128  IDWT_ALPHA 
#endif


	enum JobSlotAvailibity
	{
		SLOT_NO_INIT,
		SLOT_AVAILABLE,
		SLOT_IN_USED
	};

	enum	ResolutionPosition
	{
		POS_LL = 0,
		POS_LH = 1,
		POS_HL = 2,
		POS_HH = 3,
		POS_IGNORE
	};



	/***********************************************************************************
	Function Name:	memcpy2D
	Description:	This is a helper function for copy a 2D memory
	IN:
	src	: the pointer to source memory
	srcCol : the col of 2d memory we need to copy, count in bytes
	srcRow : the row of 2d memory we need to copy
	sourcePitch : The pitch of the source memory, counted in bytes
	dest: the pointer to the destine memory
	destPitch : The pitch of the destine memory, counted in bytes
	x_off : x offset in the destine memory
	y_off : y offset in the destine memory
	wrap : optional, if set to yes, when the x_off + srcCol > destPitch, the remaining data in the row will wrap and copy into beginning of next row, otherwise discard them
	OUT:
	dest : The destine memory filled with new data from source
	RETURN:
	N/A
	***********************************************************************************/
	void memcpy2D(const void * src, int srcCol, int srcRow, int sourcePitch, void * dest, int destPitch, int x_off, int y_off, bool wrap = false);


	/************************************************************************/
	//							   src
	//				     left		|		right
	//						\		|		/
	//							   dst	
	//	FORMULA : dst = src - coef * (left + right)
	// 	Function Name:	vdwt_analyst_line
	//  Description:	This is a SSE function to implement one line vertical DWT
	//  IN:
	//  src	left, right, dst: the pointer to the lines going to be implemented
    //  loopX : looping time needed for whole line, loopX = ALLIGNED_TO_SSE_DWORD(width) / 4
    //  coef : the coefficient used in above formula
	// RETURN:
	// 	N/A
	/************************************************************************/
	// source code in bmi_dwt_asm.cpp or bmi_cpu_dwt.cpp
	void vdwt_analyst_line_func(unsigned char * dst, unsigned char * src, unsigned char * left, unsigned char * right, int loopX, FLOAT_REG coef);

#if INTEL_ASSEMBLY_32 && SSE_OPTIMIZED
#define vdwt_analyst_line_macro(dst_ex, src_ex, left_ex,  right_ex, loopX_ex, coef, x)		\
	{										\
		unsigned char * src = src_ex;		\
		unsigned char * dst = dst_ex;		\
		unsigned char * left = left_ex;		\
		unsigned char * right = right_ex;	\
		__asm								\
		{									\
			__asm MOVAPS XMM7, coef			\
			__asm MOV ESI, src				\
			__asm MOV EDI, dst				\
			__asm MOV EAX, left				\
			__asm MOV EDX, right			\
			__asm MOV ECX, loopX			\
			__asm vdwt_analyst_line_loop_##x:\
			__asm MOVAPS XMM0, [EAX]		\
			__asm MOVAPS XMM1, [EDX]		\
			__asm ADDPS XMM0, XMM1			\
			__asm MULPS XMM0, XMM7			\
			__asm MOVAPS XMM1, [ESI]		\
			__asm SUBPS XMM1, XMM0			\
			__asm MOVAPS [EDI], XMM1		\
			__asm ADD EAX, 0x10				\
			__asm ADD EDX, 0x10				\
			__asm ADD ESI, 0x10				\
			__asm ADD EDI, 0x10				\
			__asm LOOPNZ vdwt_analyst_line_loop_##x	\
		}									\
	}
#elif AT_T_ASSEMBLY && SSE_OPTIMIZED
#define		vdwt_analyst_line_macro(dst, src, left, right, loopX, coef, x)			vdwt_analyst_line_func(dst, src, left,  right, loopX, coef)

#else

#if SSE_OPTIMIZED
#define vdwt_analyst_line_func_loop  (loopX * 4)
#else
#define vdwt_analyst_line_func_loop  loopX
#endif

#define		vdwt_analyst_line_macro(dst, src, left, right, loopX, coef, x)			\
	{																									\
		float * S = (float *) src, * D = (float *)dst, * L = (float *)left, * R = (float *)right;		\
		for (int i = 0; i < vdwt_analyst_line_func_loop; ++i, ++S, ++D, ++L, ++R)												\
			*D = *S - coef *(*L + *R);																	\
	}
#endif

#if USE_FAST_MACROS
#define vdwt_analyst_line(dst, src, left,  right, loopX, coef, x)		vdwt_analyst_line_macro(dst, src, left,  right, loopX, coef, x)
// #define vdwt_analyst_line(dst, src, left,  right, loopX, coef, x)		vdwt_analyst_line_func(dst, src, left,  right, loopX, coef)
#else
#define vdwt_analyst_line(dst, src, left,  right, loopX, coef, x)		vdwt_analyst_line_func(dst, src, left,  right, loopX, coef)
#endif

	///////////////////////////////////////////////////////////////////////////
	// sThumbnailBuf : struct of buffer used for storing and generating the thumbnail buffer
	//////////////////////////////////////////////////////////////////////////
	typedef	struct  sThumbnailBuf
	{
		sThumbnailBuf():
		mDataBuf(NULL),
		mTileNum(0),
		mThumbnailTileOffset(NULL),
		mThumbnailTileSize(NULL),
		mSize(0),
		mTrueSize(0),
		mPic(NULL),
		mMCTDone(false)
	{
		mComp[0] = mComp[1] = mComp[2] = NULL;
	}

	~sThumbnailBuf()
	{
		SAFE_DELETE_ARRAY(mThumbnailTileOffset);
		SAFE_DELETE_ARRAY(mThumbnailTileSize);
		SAFE_FREE(mDataBuf);
	}

	/***********************************************************************************
	Function Name:	InitBuf
	Description:	Init this buffer by the size needed for storing thumbnail
	IN:
	width	: width of the thumbnail
	height: : height of the thumbnail
	wordLength : wordLength of the input data
	OUT:
	N/A
	RETURN:
	N/A
	***********************************************************************************/
	void InitBuf(P_PictureInfo pic, int wordLength);

	/***********************************************************************************
	Function Name:	UploadThumbnailBufferLines
	Description:	Upload T1 result into thumbnail buffer
	IN:
	buf : T1 result
	tileId, compId : tile and components' id
	y_offset : Y-Offset in subbband
	lines : lines number to be copied
	OUT:
	N/A
	RETURN:
	N/A
	***********************************************************************************/
	void UploadThumbnailBufferLines( unsigned char * buf, int tileId, int compId, int y_offset, int lines );

	/***********************************************************************************
	Function Name:	GetBufSize
	Description:	get the thumbnail buffer's size
	IN:
	N/A
	OUT:
	N/A
	RETURN:
	The size of the buffer
	***********************************************************************************/
	int GetBufSize() const	{return (mWidth * mHeight * UINT_32_BITS);}

	/***********************************************************************************
	Function Name:	GetBuf
	Description:	get the pointer to this thumbnail result buffer
	IN:
	N/A
	OUT:
	N/A
	RETURN:
	The pointer to the buffer
	***********************************************************************************/
	unsigned char * GetBuf();	

	private: 

		//////////////////////////////////////////////////////////////////////////
		// mDataBuf : The buffer we allocated for the picture
		// mComp;	The buffer for 3 Components
		// mSize : size of the buffer we needed
		// mTempBuf : The temp Buffer we used in multiTile picture, merge different tiles into a picture
		// mTempSize : Size of the temp Buffer
		//////////////////////////////////////////////////////////////////////////
		unsigned char  *mDataBuf;
		unsigned char  *mComp[3];

		int mTileNum;
		Pos * mThumbnailTileOffset;
		Size * mThumbnailTileSize;
		

		int	 mSize;
		int  mWidth;
		int  mHeight;
		WordLength	mWordLength;

		///////////////////////////////////////////////////////////////////////////
		// mTrueSize : the size of the mdataBuf we allocated, it's equal or greater than mSize
		//////////////////////////////////////////////////////////////////////////
		int mTrueSize;

		P_PictureInfo mPic;
		bool mMCTDone;

	}ThumbnailBuf, *P_ThumbnailBuf;



	struct sDwtJob;
	//////////////////////////////////////////////////////////////////////////
	// The struct used in DWT for storing DWT buffer
	//////////////////////////////////////////////////////////////////////////
	typedef	struct  sDwtBuf
	{
		sDwtBuf():
		mBuf(NULL),
		mRealBuf(NULL),
		mSizeOfBuf(0),
		mRealSizeOfBuf(0),
		mTopDwtLevel(INVALID)
		{	}

		~sDwtBuf()
		{
			SAFE_FREE(mRealBuf);
		}

		/***********************************************************************************
		Function Name:	InitBuf
		Description:	Init this DWT buffer by the picture and the top DET level
		IN:
		pic	: all the necessary picture's information
		tileID	: This dwt buffer is initialized for this tile
		dwtTopLevel	: The top dwt level we need to decode, if this one is greater than the picture's DWT level or less than zero, ingore it and set it to the picture's DWT level
		wordLen : The word length we used in DWT, in current version we only support 32 bits
		OUT:
		N/A
		RETURN:
		N/A
		***********************************************************************************/
		void InitBuf(const sDwtJob *  curJob, int tileId);

		/***********************************************************************************
		Function Name:	GetSubBandBuf
		Description:	Get the buffer for indicated subband in current tile
		IN:
		compId  : Component ID
		subbandID	: Subband ID
		OUT:
		size : if this pointer has been set will return the size of the this buffer
		RETURN:
		The buffer's pointer
		***********************************************************************************/
		unsigned char * GetSubBandBuf(int compId, int subbandId, int * size = NULL);
		
		/***********************************************************************************
		Function Name:	GetResolutionSubbandBuf
		Description:	Will return the buffer for current resolution
		IN:
		compId  : Component ID
		resolutionId	: resolution ID, from 0 to DWT level minue 1
		pos		: the position of the subband, LL, LH, HL or HH. If LL for the DWT level greater than 0, we know there is not subband for it. It will return result buffer from the previous DWT level
		OUT:
		N/A
		RETURN:
		The buffer's pointer
		***********************************************************************************/
		unsigned char * GetResolutionSubbandBuf(int compId, int resolutionId, ResolutionPosition pos);

		/***********************************************************************************
		Function Name:	GetImgBuf
		Description:	Get the image buffer for current tile, indicated component. This buffer is the result from DWT
		IN:
		compId  : Component ID
		OUT:
		N/A
		RETURN:
		The buffer's pointer
		***********************************************************************************/
		unsigned char * GetImgBuf(int compId)	{	return mTileImgBuf[compId];	}

		/***********************************************************************************
		Function Name:	GetImgBufSize
		Description:	Get the image buffer's size
		IN:
		compId  : Component ID
		OUT:
		N/A
		RETURN:
		The buffer's size
		***********************************************************************************/
		int GetImgBufSize(int compId);

		/***********************************************************************************
		Function Name:	Shift_GetImgBuf
		Description:	Get the image buffer for the tile, indicated component. This buffer is the result from DWT and after DC shifting and converted into integer
		IN:
		compId  : Component ID
		picture : The picture;s information
		OUT:
		bufSize : Size of the return buffer
		width (optional) : size of the component, counted in pixel
		height (optional) : size of the component, counted in pixel
		RETURN:
		The buffer's pointer
		***********************************************************************************/
		unsigned char * Shift_GetImgBuf(int compId,unsigned int &bufSize, const P_PictureInfo picture, int * width = NULL, int * height = NULL);

		/***********************************************************************************
		Function Name:	GetTmpBuf
		Description:	Get the tmp buffer we used for DWT calculation, This buffer we saved the H-DWT's result and used for V-DWT calculation, the V-DWT result will saved back to subband / img buffer
		IN:
		compId  : Component ID
		OUT:
		N/A
		RETURN:
		The buffer's pointer
		***********************************************************************************/
		unsigned char * GetTmpBuf(int compId)	{	return mTempImgBuf[compId];	}

		/***********************************************************************************
		Function Name:	GetTmpBuf
		Description:	Get the tmp buffer we used for DWT calculation, This buffer we saved the H-DWT's result and used for V-DWT calculation, the V-DWT result will saved back to subband / img buffer
		IN:
		compId  : Component ID
		OUT:
		N/A
		RETURN:
		The buffer's pointer
		***********************************************************************************/
		unsigned char * GetTmpResolutionBottomBuf(int compId, int ResolutionId)	{	return mTempResolutionBottomBuf[compId][ResolutionId];	}

		/***********************************************************************************
		Function Name:	ImgBufShifted
		Description:	Check current component's image buffer (DWT result) has been shift or not
		IN:
		compId  : Component ID
		OUT:
		N/A
		RETURN:
		True: has been shifted 
		***********************************************************************************/
		bool ImgBufShifted(int compId)	{	return mTileImgBufShifted[compId];	}

	private:

		//////////////////////////////////////////////////////////////////////////
		// mTileImgBuf : image buffer for each componet, saved the DWT result
		// mTempImgBuf : temp buffer used for storing H-DWT's result, also in W9X7, this buffer used for saving the shifted and converted buffer
		// mTileImgBufShifted : flag used for indicated the above buffer has been shifter or not
		// mTileImgBufSize : size of the buffers: mTileImgBuf and mTempImgBuf
		//////////////////////////////////////////////////////////////////////////
		unsigned char *	mTileImgBuf[MAX_COMPONENT];	
		bool			mTileImgBufShifted[MAX_COMPONENT];
		unsigned char *	mTempImgBuf[MAX_COMPONENT];
		unsigned char * mTempResolutionBottomBuf[MAX_COMPONENT][MAX_DWT_LEVEL];
		unsigned int		mTileImgBufSize;

		//////////////////////////////////////////////////////////////////////////
		// mTileSize : size of current tile
		//////////////////////////////////////////////////////////////////////////
		s2d		mTileSize;

		//////////////////////////////////////////////////////////////////////////
		//	mSubbanBuf : buffer for each subband
		// mSubbanBufSize : buffer size for each subband
		//////////////////////////////////////////////////////////////////////////
		unsigned char *	mSubbanBuf[MAX_COMPONENT][MAX_DWT_LEVEL * 4];
		int		mSubbanBufSize[MAX_COMPONENT][MAX_DWT_LEVEL * 4];

		//////////////////////////////////////////////////////////////////////////
		// mBuf : the real buffer that allocated, all the above buffer just a pointer point to within this buffer
		// mSizeOfBuf : The size of mBuf we allocated
		//////////////////////////////////////////////////////////////////////////
		unsigned char *	mBuf;
		unsigned char * mRealBuf;
		int		mSizeOfBuf;
		int		mRealSizeOfBuf;

		//////////////////////////////////////////////////////////////////////////
		// mTopDwtLevel : top DWT level we decode to
		//////////////////////////////////////////////////////////////////////////
		int mTopDwtLevel;

	}DwtBuf, *P_DwtBuf;


	///////////////////////////////////////////////////////////////////////////
	// sSimpleBuf : struct of simple buffer use for storing the result of the generate bmp picture
	//////////////////////////////////////////////////////////////////////////
	typedef	struct  sSimpleBuf
	{
		sSimpleBuf():
		mDataBuf(NULL),
		mRealBuf(NULL),
		mSize(0),
		mTrueSize(0),
		mFlag(0)
		{
		}

		~sSimpleBuf()
		{
			SAFE_FREE(mRealBuf);
		}


		/***********************************************************************************
		Function Name:	InitBuf
		Description:	Init this result buffer by the picture and the top DET level
		IN:
		BufSize	: Sizes need for this buffer
		OUT:
		N/A
		RETURN:
		N/A
		***********************************************************************************/
		void InitBuf(unsigned int bufSize, bool alignSSE = false)
		{
			mFlag = 0;
			mSize = bufSize;
			if (mTrueSize < mSize + (alignSSE ? SSE_ALIGN_BYTES : 0))
			{
				SAFE_FREE(mRealBuf);
				mTrueSize = mSize + (alignSSE ? SSE_ALIGN_BYTES : 0);
				mRealBuf = malloc(mTrueSize);
				if (alignSSE)
				{
// 					mDataBuf = (void *)(((unsigned int)mRealBuf + SSE_ALIGN_BYTES) & 0xfffffff0);
					mDataBuf = reinterpret_cast<void *>(((PointerValue)mRealBuf + SSE_ALIGN_BYTES) / (SSE_ALIGN_BYTES + 1) * (SSE_ALIGN_BYTES + 1) );
				}
				else
				{
					mDataBuf = mRealBuf;
				}
			}
		}	

		/***********************************************************************************
		Function Name:	GetBufSize
		Description:	get the result buffer's size
		IN:
		N/A
		OUT:
		N/A
		RETURN:
		The size of the buffer
		***********************************************************************************/
		unsigned int GetBufSize() const	{return mSize;}

		/***********************************************************************************
		Function Name:	GetBuf
		Description:	get the pointer to this result buffer
		IN:
		N/A
		OUT:
		N/A
		RETURN:
		The pointer to the buffer
		***********************************************************************************/
		void * GetBuf()			{return mDataBuf;}

		// provide interface allow user to access the flag
		UINT64 GetFlag()	const	{	return mFlag;	}
		void SetFlag(UINT64 f)	{	mFlag = f;		}
	private: 

		//////////////////////////////////////////////////////////////////////////
		// mDataBuf : The buffer we allocated for the picture
		// mSize : size of the buffer we needed
		//////////////////////////////////////////////////////////////////////////
		void * mDataBuf;
		void * mRealBuf;
		unsigned int	mSize;

		///////////////////////////////////////////////////////////////////////////
		// mTrueSize : the size of the mdataBuf we allocated, it's equal or greater than mSize
		//////////////////////////////////////////////////////////////////////////
		unsigned int mTrueSize;

		//////////////////////////////////////////////////////////////////////////
		// mFlag : reserved , user can use it to do some recording whenever they like
		//////////////////////////////////////////////////////////////////////////
		UINT64	mFlag;

	}SimpleBuf, *P_SimpleBuf;



	//////////////////////////////////////////////////////////////////////////
	// DWT_STATUS : the status of DWT buffer
	// switching : 
	// DWT_STATUS_NONE -> DWT_STATUS_READY_FOR_HDWT -> DWT_STATUS_CHANGING -> DWT_STATUS_READY_FOR_VDWT -> DWT_STATUS_CHANGING -> DWT_STATUS_DWT_DONE
	//////////////////////////////////////////////////////////////////////////
	enum	DWT_STATUS
	{
		DWT_STATUS_NONE = 0,
		DWT_STATUS_READY_FOR_HDWT,
		DWT_STATUS_READY_FOR_VDWT,
		DWT_STATUS_DWT_DONE,
		DWT_STATUS_CHANGING
	};

	//////////////////////////////////////////////////////////////////////////
	// DWT_CBSTRIBE_STATUS :  the struct for tracking code block stripe buffer's status
	//////////////////////////////////////////////////////////////////////////
	struct DWT_CBSTRIBE_STATUS
	{
		int_16	mYOffsetInBand;		// The Y offset of this codeblock in current subband
		int_16 mNumLines;			// Number of rows in the stripe
		DWT_STATUS	mStatus;		// current dwt status of this buffer
	};

	//////////////////////////////////////////////////////////////////////////
	// DWT_CBSTRIBE_STATUS :  the struct for tracking current resoltion's status
	//////////////////////////////////////////////////////////////////////////
	typedef struct s_DwtResolutionStatusTracer
	{
		DWT_STATUS		mCurStatus;				// DWT_STATUS_NONE -> (mJobCounter reduce to zero) -> DWT_STATUS_READY_FOR_VDWT -> (VDWT processing)DWT_STATUS_CHANGING -> DWT_STATUS_DWT_DONE
		int_16			mJobCounter;			// the job counter for current resolution, it's the sum of the code block stripe number of all the subband in this resolution
	}DwtResolutionStatusTracer, *P_DwtResolutionStatusTracer;


	//////////////////////////////////////////////////////////////////////////
	// DwtStatus : The class to create a table to tracking and maintain the DWT status for all code block stribe
	//////////////////////////////////////////////////////////////////////////
	class DwtStatus
	{
	public:

		DwtStatus():
		mDwtStatusTable(NULL),
		mSizeOfDwtTable(0),
		mStatusTableSubband(NULL),
		mStatusTableDwtResolution(NULL),
		mTileNum(0),
		mCompNum(0),
		mTotalSubband(0),
		mSubBandNumPreComp(0),
		mDwtTopLevel(INVALID),
		mStripeNumTable(NULL)
		{
			CREATE_MUTEX(mDwtStatusMutex);
		}

		~DwtStatus()
		{
			SAFE_FREE(mDwtStatusTable);
			SAFE_FREE(mStatusTableSubband);
			SAFE_FREE(mStatusTableDwtResolution);
			SAFE_FREE(mStripeNumTable);
			DESTROY_MUTEX(mDwtStatusMutex);
		}

		/***********************************************************************************
		Function Name:	InitDwtStatus
		Description:	Create and initialize the DWT table for current job 
		IN:
		curJob	: current job
		s_opt	: The T2 result of current job
		opt_size : the size of the buffer s_opt
		OUT:
		N/A
		RETURN:
		N/A
		***********************************************************************************/
		void InitDwtStatus(void * curJob, const stru_opt * s_opt, int opt_size);

		/***********************************************************************************
		Function Name:	ThreadSafeCheckDwtStatus
		Description:	Check the indicated codeblock buffer status
		IN:
		tileId	: tile ID
		compId	: component id
		subbandid : subband Id
		cbStripeId : code block stripe Id
		OUT:
		N/A
		RETURN:
		Current status of this buffer
		***********************************************************************************/
		DWT_STATUS ThreadSafeCheckDwtStatus(int tileId, int compId, int subBandId, int cbStribeId)	
		{
			DWT_CBSTRIBE_STATUS * tmp =  mStatusTableSubband[mSubBandNumPreComp * mCompNum * tileId + mSubBandNumPreComp * compId + subBandId];
			LOCK_MUTEX(mDwtStatusMutex);
			DWT_STATUS ret = tmp[cbStribeId].mStatus;
			RELEASE_MUTEX(mDwtStatusMutex);
			return ret;
		}

		/***********************************************************************************
		Function Name:	UnfinishedHDWT
		Description:	Check the indicated subband to see which buffer hasn't processed H-DWT
		IN:
		curJob : current job
		tileId	: tile ID
		compId	: component id
		subbandid : subband Id
		OUT:
		lineoff : The y-off of an unfinished code block stripe buffer we first found
		RETURN:
		the pointer to the fist code block strip that H-DWT unfinished in this subband
		***********************************************************************************/
		unsigned char * UnfinishedHDWT(void * curJob,int tileId, int compId, int subBandId, int & lineOff, int & numLines);	

		/***********************************************************************************
		Function Name:	ThreadSafeSetDwtStatus
		Description:	Set the indicated codeblock buffer status
		IN:
		curJob : current job
		tileId	: tile ID
		compId	: component id
		subbandid : subband Id
		cbStribeId : code block stripe Id
		y_offset_in_band : current code block stripe's offset in subband
		status	: new status
		OUT:
		relatebandId : optional, 
			If we set this codeblock stripe status to DWT_STATUS_READY_FOR_HDWT, check the related code block stripe is also DWT_STATUS_READY_FOR_HDWT or not, if yes, set it to the related subband id
			If we set this codeblock stripe status to DWT_STATUS_DONE, check the next DWT level's related subband to see is there any stripe buffers are ready for processing H-DWT, if yes, set the subband id
		RETURN:
		NULL : done
		pointer : return the related code block buffer that we could continue processing DWT
		If we set this codeblock stripe status to DWT_STATUS_READY_FOR_HDWT/ DWT_STATUS_DONE return the related code block stripe buffer
		If we set this codeblock stripe status to DWT_STATUS_READY_FOR_VDWT, check the current resolution is it good to process V-DWT, if yes, return the resolution's buffer
		***********************************************************************************/
		unsigned char * ThreadSafeSetDwtStatus(void * curJob,int tileId, int compId, int subBandId, int cbStribeId, int y_offset_in_band, DWT_STATUS status, int * relatebandId = NULL);	

	private:
		//////////////////////////////////////////////////////////////////////////
		// The MUTEX we used for control the multi thread accessing to this table
		//////////////////////////////////////////////////////////////////////////
		MUTEX_HANDLE	mDwtStatusMutex;

		//////////////////////////////////////////////////////////////////////////
		// mDwtStatusTable : the memory block store the DWT status table
		// mSizeOfDwtTable : size of this table
		// mStatusTableSubband : an pointers' array for each code stripe's status
		// DwtResolutionStatusTracer : an pointers' array for resolution's DWT status
		//////////////////////////////////////////////////////////////////////////
		unsigned char	* mDwtStatusTable;
		int mSizeOfDwtTable;
		DWT_CBSTRIBE_STATUS ** mStatusTableSubband;
		DwtResolutionStatusTracer ** mStatusTableDwtResolution;

		//////////////////////////////////////////////////////////////////////////
		// mTileNum;	Tile Number
		// mCompNum;	Component number
		// mTotalSubband;	subband number for all
		// mSubBandNumPreComp; subband number in one componet
		// mDwtTopLevel;	Top dwt level
		// mStripeNum	number of stripes in each subband
		//////////////////////////////////////////////////////////////////////////
		int mTileNum;
		int mCompNum;
		int mTotalSubband;
		int mSubBandNumPreComp;
		int mDwtTopLevel;
		int * mStripeNumTable;

		/***********************************************************************************
		Function Name:	CalculateDwtStatusTableSize
		Description:	calculate the size we need for create the table for tracking DWT status
		IN:
		curJob	: current job
		s_opt	: The T2 result of current job
		opt_size : the size of the buffer s_opt
		OUT:
		N/A
		RETURN:
		memory size we need, count in bytes
		***********************************************************************************/
		int CalculateDwtStatusTableSize(void * curJob, const stru_opt * s_opt, int opt_size);
	};

	typedef struct	sDwtJob
	{

		sDwtJob():
		mAvailable(SLOT_NO_INIT),
		mWordLength(UINT_32_BITS),
		mDwtDecodingTopLevel(INVALID),
		mPicture(NULL),
		mDwtBuf(NULL),
		mJobConfig(NULL)
		{
		}

		~sDwtJob()
		{
			SAFE_DELETE(mPicture);
			SAFE_DELETE_ARRAY(mDwtBuf);
		}

		JobSlotAvailibity mAvailable;
		WordLength	mWordLength;

		union
		{
			short				mDwtDecodingTopLevel;
			short				mDwtEncodingLevel;
		};

	
		//////////////////////////////////////////////////////////////////////////
		// mPicture : current picture's information, 
		//////////////////////////////////////////////////////////////////////////
		P_PictureInfo	mPicture;

		//////////////////////////////////////////////////////////////////////////
		// mDwtBuf : DWT buffer for current job
		// mResultbuf : result buffer for storing current job
		// mThumbnailBuf : result buffer for storing thumbnail for current job
		// mDwtStatus : The DWT status table for current job
		//////////////////////////////////////////////////////////////////////////
		DwtBuf *		mDwtBuf;
		SimpleBuf		mResultbuf;
		SimpleBuf		mDecodedCompBuf[MAX_COMPONENT];
		ThumbnailBuf	mThumbnailBuf;
		DwtStatus		mDwtStatus;

		//////////////////////////////////////////////////////////////////////////
		// mThreadNum : thread number for current decoding procedure
		// mJobConfig : The job configuration for current decoding, it;s only a pointer point to the data in class decodeConcrete 
		//////////////////////////////////////////////////////////////////////////
		int			mThreadNum;
		DecodeJobConfig * mJobConfig;


	} DwtJob, * P_DwtJob;







	//////////////////////////////////////////////////////////////////////////
	// CpuDwtDecoder : The main class to hook up with decode concrete to implement all the DWT/MCT jobs
	//		This is a singleton class
	//////////////////////////////////////////////////////////////////////////
	class CpuDwtDecoder : public Singleton<CpuDwtDecoder>
	{
	public:

		CpuDwtDecoder();
		~CpuDwtDecoder();

		//////////////////////////////////////////////////////////////////////////
		// in class DecoderConcrete, need to access some private function for DWT processing
		//////////////////////////////////////////////////////////////////////////
		friend class DecoderConcrete;		

		/***********************************************************************************
		Function Name:	Init
		Description:	Init : Initialize this DWT processor base on the engine's configuration
		IN:
		enginConfig	: the configuration of current engine
		OUT:
		N/A
		RETURN:
		N/A
		***********************************************************************************/
		void	Init(EnginConfig & enginConfig);


		/***********************************************************************************
		Function Name:	InitPicture
		Description:	base on the picture's information, initialize current DWT jobs, including all ralted buffer and status
		IN:
		inStream	: input stream, includes all information
		outputIndex : the slot of current decoding job will occupy
		s_opt	: T2 result
		opt_size : Unit size for each tile in T1 result
		OUT:
		N/A
		RETURN:
		N/A
		***********************************************************************************/
		void	InitPicture(J2K_InputStream &inStream, int outputIndex, const stru_opt * s_opt, int opt_size);

		/***********************************************************************************
		Function Name:	ReleasePicture
		Description:	release this picture, make all resource reusable
		IN:
		outputIndex	: he slot of current decoding job occupied
		OUT:
		N/A
		RETURN:
		N/A
		***********************************************************************************/
		void	ReleasePicture(int outputIndex) {	mDwtJobs[outputIndex].mAvailable = SLOT_AVAILABLE; }

		/***********************************************************************************
		Function Name:	GetStripeBuffer
		Description:	find the stripe buffer that will be used by T1
		IN:
		pictureIndex	: The slot of current decoding job occupied
		tileId, compId, subbandId, y_offset_in_band : the T1 job parameter,use for locating this stripe buffer in DWT buffer
		OUT:
		buf_pitch : the DWT buffer pitch
		RETURN:
		The pointer to this buffer
		***********************************************************************************/
		unsigned char  * GetStripeBuffer(PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int y_offset_in_band, int &buf_pitch);

		/***********************************************************************************
		Function Name:	UploadStripeBuf
		Description:	Upload the stripe buffer that filled with T1 result to DWT processor, this buffer must be allocated by this DWT processor
		IN:
		pictureIndex	: The slot of current decoding job occupied
		tileId, compId, subbandId, y_offset_in_band : the T1 job parameter,use for locating this stripe buffer in DWT buffer
		OUT:
		buf_pitch : the DWT buffer pitch
		RETURN:
		The pointer to this buffer
		***********************************************************************************/		
		void	UploadStripeBuf(unsigned char * stripeBuf, const DecodeJobConfig & config, const PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int linesInserted, int y_offset_in_band,int codeBlockStripeId, int threadId);

		/***********************************************************************************
		Function Name:	GetComponentInt
		Description:	Return the DWT result of an single component, DC shifted
		IN:
		outputIndex	: The slot of current decoding job occupied
		componentId : component ID
		tileId : optional, if it set to a valid tileID, will return the componet only for this tile, otherwise, will merge all available tiles
		OUT:
		sizeofBuf : size of this buffer
		RETURN:
		The pointer to this buffer
		***********************************************************************************/	
		int  * GetComponentInt( CodeStreamHandler handler,unsigned int & sizeofBuf, int outputIndex, int componentId, int tileId = INVALID);

		/***********************************************************************************
		Function Name:	GetComponentFloat
		Description:	Return the DWT result of an single component, DC shifted
		IN:
		outputIndex	: The slot of current decoding job occupied
		componentId : component ID
		tileId : optional, if it set to a valid tileID, will return the componet only for this tile, otherwise, will merge all available tiles
		OUT:
		sizeofBuf : size of this buffer
		RETURN:
		The pointer to this buffer, in W5X3, will return NULL
		***********************************************************************************/	
		float * GetComponentFloat( CodeStreamHandler handler,unsigned int & sizeofBuf, int outputIndex, int componentId, int tileId = INVALID);

		/***********************************************************************************
		Function Name:	GetResultBuf
		Description:	Return the BMP buffer generated. 32bits with RGB in 8 bits depth
		IN:
		outputIndex	: The slot of current decoding job occupied
		OUT:
		bufSize : The size of the buffer
		RETURN:
		The pointer to result
		***********************************************************************************/	
		void  * GetResultBuf(int outputIndex, unsigned int & bufSize);	

		/***********************************************************************************
		Function Name:	GetThumbnail
		Description:	Return the BMP buffer of thumbnail generated. 32bits with RGB in 8 bits depth
		IN:
		outputIndex	: The slot of current decoding job occupied
		OUT:
		bufSzie : size of the thumbnail buffer, count in bytes
		RETURN:
		The pointer to thumbnail buffer
		***********************************************************************************/	
		const void * GetThumbnail(int outputIndex, int &bufSize);

		/***********************************************************************************
		Function Name:	DwtUnfinished
		Description:	To check if there are some DWT job need to insert into job queue. Most of the DWT job should be done in uploadStripeBuffer, in multi threads, some V-DWT jobs and MCT 
						need to insert into job queue after all T1 job done
		IN:
		N/A
		OUT:
		N?A
		RETURN:
		The job numbers of the unfinished DWT job
		***********************************************************************************/	
		int		DwtUnfinished();

		/***********************************************************************************
		Function Name:	GetUnfinishedDwtJobs
		Description:	To check in DWT processor's unfinished job queue and fetch the first job in the queue
		IN:
		tileId	: Get the first job for current tile, if this ID set to INVALID, ignore it, return the first job
		OUT:
		pictureHandler : the handler for current picture
		pictureIndex  : The job slot for current picture
		RETURN:
		the pointer to the first found job, fail to get the job will return NULL
		***********************************************************************************/	
		const DecoderConcrete::P_DecodeDWTJob		GetUnfinishedDwtJobs(int tileID, int &pictureHandler, int &pictureIndex);

// 		bool	CreateLastLevelDwtJob(DecoderConcrete::P_DecodeDWTJob dwtJob, int jobIndex, int tileId, int compId = INVALID);

	
		/***********************************************************************************
		Function Name:	HDWT_R97
		Description:	Implement reverse horizontal DWT W9X7 picture
		IN:
		H_result : the buffer for storing the DWT result
		lowband : the buffer of lower band for H-DWT
		highband : the buffer of higher band for H-DWT
		low_w	: lower band width
		high_w  : higher band width
		lines	: the number of the lines need to do H-DWT
		loGain / hiGain : the float value for DWT calculation, lowGain and highGain
		isLowBandNeedTransfer : bool value to decide should the lower band need to be transfer from fix point to float, if the lower band is the result from previous DWT, it's set to false
		OUT:
		H_result : Filled with DWT result (low_w+high_w) * lines
		RETURN:
		***********************************************************************************/	
		void	HDWT_R97(float * H_result, float  * lowBand,float  * highBand, int low_w, int high_w, int lines,  float loGain, float hiGain, uint_8 xParity, bool isLowBandNeedTransfer);

		/***********************************************************************************
		Function Name:	VDWT_R97
		Description:	Implement reverse vertical DWT W9X7 picture
		IN:
		target : the buffer for storing the DWT result
		H_result_up : the buffer from HDWT, storing the HDWT result upper part
		H_result_down : the buffer from HDWT, storing the HDWT result bottom part
		low_h	: lower band height
		high_h  : higher band height
		width :	width of the buffer, equal low_w + hight_w in HDWT
		linesOffset	: the lines offset in the lower band, used when we cut the buffer into some pieces and distributes to multi threads
		linesDecode : the number of the lines we need to decode, it's the lines number in lower band only, the real decoding line would be linesDecode*2 or linesDecode*2 - 1
		threadId : threadId of current thread, used for index and get the overlay buffer in multi threads
		OUT:
		H_result : Filled with DWT result 
		RETURN:
		***********************************************************************************/			
		void	VDWT_R97(unsigned char * target,unsigned char * H_result_up, unsigned char * H_result_down, int low_h, int high_h, int width, int linesOffset, int linesDecode, uint_8 yParity, int threadId);

		/***********************************************************************************
		Function Name:	HDWT_R53
		Description:	Implement reverse horizontal DWT W5X3 picture
		IN:
		result : the buffer for storing the DWT result
		in_pixel_low : the buffer of lower band for H-DWT
		in_pixel_high : the buffer of higher band for H-DWT
		low_w	: lower band width
		high_w  : higher band width
		lines	: the number of the lines need to do H-DWT
		isLowBandNeedTransfer : bool value to decide should the lower band need to be transfer from separate signed bit integer to integer, if the lower band is the result from previous DWT, it's set to false
		OUT:
		H_result : Filled with DWT result (low_w+high_w) * lines
		RETURN:
		***********************************************************************************/	
		void	HDWT_R53_INT(int * result, int  * in_pixel_low, int  * in_pixel_high, int low_w, int high_w, int lines, uint_8 xParity, bool isLowBandNeedTransfer);
		void	HDWT_R53_SHORT(short * result, short  * in_pixel_low, short  * in_pixel_high, int low_w, int high_w, int lines, uint_8 xParity, bool isLowBandNeedTransfer);

		/***********************************************************************************
		Function Name:	VDWT_R53
		Description:	Implement reverse vertical DWT W5X3 picture
		IN:
		target : the buffer for storing the DWT result
		H_result : the buffer from HDWT, storing the HDWT result
		low_h	: lower band height
		high_h  : higher band height
		width :	width of the buffer, equal low_w + hight_w in HDWT
		linesOffset	: the lines offset in the lower band, used when we cut the buffer into some pieces and distributes to multi threads
		linesDecode : the number of the lines we need to decode, it's the lines number in lower band only, the real decoding line would be linesDecode*2 or linesDecode*2 - 1
		threadId : threadId of current thread, used for index and get the overlay buffer in multi threads
		OUT:
		H_result : Filled with DWT result 
		RETURN:
		***********************************************************************************/			
		template<class T>	void	VDWT_R53(T * target, T * H_result, int low_h, int high_h, int width, int linesOffset, int linesDecode,uint_8 yParity,int threadId);

		/***********************************************************************************
		Function Name:	RRct_97
		Description:	Implement reverse RCT for W9X7 picture
		IN:
		result	: Pointer to the buffer for storing the MCT result (32bits/pixel 8 bits depth for each color)
		comp0, comp1, comp2 : the buffer of the inputed 3 channels
		bufPitch : pitch if the result buffer
		width/height : the size of the components
		bitDepth : the color's bit depth of the 3 components
		checkOutComp : set to -1, will generate the color output, set to 0-2, will generate the gray scale picture use the component indicated
		OUT:
		resultBuf : the buffer filled with generate BMP picture
		RETURN:
		N/A
		***********************************************************************************/	
		void	MCT_R97(unsigned char  * result, unsigned char * comp0, unsigned char * comp1, unsigned char * comp2, int resultPitch, int width, int height, short bitDepth, OutputFormat outFormat, short checkOutComp, int wordLen);

		/***********************************************************************************
		Function Name:	RRct_53
		Description:	Implement reverse RCT for W5X3 picture
		IN:
		resultBuf	: Pointer to the buffer for storing the MCT result (32bits/pixel 8 bits depth for each color)
		redBuf, greenBuf, blueBuf : the buffer of the inputed 3 channels
		bufPitch : pitch if the result buffer
		width/height : the size of the components
		bitDepth : the color's bit depth of the 3 components
		checkOutComp : set to -1, will generate the color output, set to 0-2, will generate the gray scale picture use the component indicated
		width : 
		OUT:
		resultBuf : the buffer filled with generate BMP picture
		RETURN:
		N/A
		***********************************************************************************/	
		template<class T>	void	RRct_53(void * resultBuf, T* redBuf, T* greenBuf, T* blueBuffer, int bufPitch, int width, int height, int bitDepth, OutputFormat outFormat, int checkOutComp);
	


		/***********************************************************************************
		Function Name:	MergeComponents
		Description:	Treat the 3 input components as RGB and merge them into BMP picture without doing MCT
		IN:
		resultBuf	: Pointer to the buffer for storing the result
		comp0, comp1, comp2 : the buffer of the inputed 3 channels
		resultPitch : pitch if the result buffer
		width/height : the size of the components
		bitDepth : the color's bit depth of the 3 components
		checkOutComp : set to -1, will generate the color output, set to 0-2, will generate the gray scale picture use the component indicated
		OUT:
		resultBuf : the buffer filled with generate BMP picture
		RETURN:
		N/A
		***********************************************************************************/	
		template <class T>
		void	MergeComponents(unsigned char * result, T * comp0, T * comp1, T * comp2,  T * comp3, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat, short checkOutComp);

	private:

		/***********************************************************************************
		Function Name:	RICT_97_FLOAT
		Description:	Do the MCT on the 3 given channels and output in the given formate
		IN:
		resultBuf	: Pointer to the buffer for storing the result
		comp0, comp1, comp2 : the buffer of the inputed 3 channels, float value
		resultPitch : pitch if the result buffer
		width/height : the size of the components
		bitDepth : the color's bit depth of the 3 components
		outFormat : the output format
		checkOutComp : set to -1, will generate the color output, set to 0-2, will generate the gray scale picture use the component indicated
		OUT:
		resultBuf : the buffer filled with generate output
		RETURN:
		N/A
		***********************************************************************************/	
		void	RICT_97_FLOAT(unsigned char  * result,  float * comp0, float * comp1, float * comp2, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat, short checkOutComp, int sourceSkip = 0);


		void	CleanUpUndoJobs() {mUndoDwtJobsNum = 0; mUndoDwtJobsCurrentIndex = 0;}

 		void	VDWT_R97_LINES(unsigned char  * taget, unsigned char 	*HResult_L, unsigned char  * HResult_H, unsigned char  ** overlaylines, int loopX, int decodeLine,uint_8 yParity, int threadId,  bool isStart, bool isEnd);

		template<class T> void	VDWT_R53_LINES(T * target, T	*HResult_L, T * HResult_H, T ** overlaylines, int width, int decodeLine,uint_8 yParity,int threadId,  bool isStart, bool isEnd);


// 		void	RMCT_97_INT32(void * result, int * comp0, int * comp1, int * comp2, int resultPitch, int width, int height, short bitDepth, OutputFormat outFormat,short checkOutComp);

		void	RMct(DwtJob & job, int tileId, int checkOutComp);

		void	MergeComponents(DwtJob & job,  int tileId, int checkOutComp);


		//////////////////////////////////////////////////////////////////////////
		//		ASM functions 
		//////////////////////////////////////////////////////////////////////////

#if ENABLE_ASSEMBLY_OPTIMIZATION
		/***********************************************************************************
		Function Name:	HDWT_R97_FLOAT_ASM
		Description:	reverse DWT for W9X7, implement in assembly language use SIMD
		IN:
			threadId : calling thread ID, used for getting the temporary buffer
			band_l	: lower band source buffer for reverse DWT, LL or Hl, data saved as 32bit float
			band_h	: higher band source buffer for reverse DWT, LH or HH, data saved as 32bit float
			low_w	: width of the lower band, count in pixel	
			high_w	: width of the high band, count in picel
			lines	: The number of the lines need to be decoded
			lowgain / highgain : low gain and high gain for the subband
			xparity : Is he HDWT parity set ? true : 1 false :0
			isLowBandNeedTransfer : If the lower band's data from last level DWt transition? If yes, we need to convert the data from fixed point to float
		OUT:
			result	: Reverse DWT result, saved in float, the size of this buffer need to be at least (low_w + high_w) * lines * 4 butes
		RETURN:
			N/A
		***********************************************************************************/
		void	HDWT_R97_FLOAT_ASM(int threadId, float * result, float  * band_l, float  * band_h, int low_w, int high_w,  int lines, float lowgain, float highgain, uint_8 xParity,  bool isLowBandNeedTransfer);
		void HDWT_R97_FLOAT_ASM_1Line(float * result, float  * band_l, float  * band_h, float * tempBuf, int low_w, int high_w, float lowgain, float highgain, uint_8 xParity,  bool isLowBandNeedTransfer);


		/***********************************************************************************
		Function Name:	VDWT_R97_FLOAT_ASM_32	VDWT_R97_FLOAT_ASM_ATT
		Description:	Implement reverse vertical DWT W9X7 picture
		IN:
		target : the buffer for storing the DWT result
		H_result_up : the buffer from HDWT, storing the HDWT result upper part
		H_result_down : the buffer from HDWT, storing the HDWT result bottom part
		low_h	: lower band height
		high_h  : higher band height
		width :	width of the buffer, equal low_w + hight_w in HDWT
		linesOffsetInLow	: the lines offset in the lower band, used when we cut the buffer into some pieces and distributes to multi threads
		linesDecodeInLow : the number of the lines we need to decode, it's the lines number in lower band only, the real decoding line would be linesDecode*2 or linesDecode*2 - 1
		threadId : threadId of current thread, used for index and get the overlay buffer in multi threads
		OUT:
		H_result : Filled with DWT result 
		RETURN:
		***********************************************************************************/	
#if INTEL_ASSEMBLY_32
		void	VDWT_R97_FLOAT_ASM_32(unsigned char * target,unsigned char * H_result_up, unsigned char * H_result_down, int low_h, int high_h, int width, int linesOffsetInLow, int linesDecodeInLow, uint_8 yParity, int threadId);
#else if AT_T_ASSEMBLY
		void	VDWT_R97_FLOAT_ASM_ATT(unsigned char * target,unsigned char * H_result_up, unsigned char * H_result_down, int low_h, int high_h, int width, int linesOffsetInLow, int linesDecodeInLow, uint_8 yParity, int threadId);
#endif
		/***********************************************************************************
		Function Name:	RMCT_R97_FLOAT_ASM
		Description:	reverse ICT for W9X7, implement in assembly language use SIMD
		IN:
			comp0	: the component buffer, Red channel, DWT done, saved in float format
			comp1	: the component buffer, Green channel, DWT done, saved in float format
			comp2	: the component buffer, Blue channel , DWT done, saved in float format
			resultPitch	: the pitch of the result buffer, count in pixel
			width	: width of the component, count in pixel
			height	: height of the component, count in pixel
			bitDepth : bitdepth of each component
			checkOutComp : set to 0-3 will generate the grey scale picture for the corresponding channel, set to INVALID or >=4, will ignore this and generate BMP buffer
		OUT:
			result	: ICT result, will clip each channel to 8 bits, the output format is for 32 bits BMP picture with 8 bits depth for each color, saved in ARGB, alpha channel is zero
		RETURN:
			N/A
		***********************************************************************************/
		void	RMCT_R97_FLOAT_ASM(unsigned char  * result, float * comp0, float * comp1, float * comp2, int resultPitch, int width, int height, short bitDepth, OutputFormat outFormat, short checkOutComp);

		/***********************************************************************************
		Function Name:	RMCT_R53_INT32_ASM
		Description:	reverse ICT for W5X3, implement in assembly language use SIMD
		IN:
		comp0	: the component buffer, Red channel, DWT done, saved in int format
		comp1	: the component buffer, Green channel, DWT done, saved in int format
		comp2	: the component buffer, Blue channel , DWT done, saved in int format
		resultPitch	: the pitch of the result buffer, count in pixel
		width	: width of the component, count in pixel
		height	: height of the component, count in pixel
		bitDepth : bitdepth of each component
		checkOutComp : set to 0-3 will generate the grey scale picture for the corresponding channel, set to INVALID or >=4, will ignore this and generate BMP buffer
		OUT:
		result	: ICT result, will clip each channel to 8 bits, the output format is for 32 bits BMP picture with 8 bits depth for each color, saved in ARGB, alpha channel is zero
		RETURN:
		return 0 succeed, -1 means the address is not aligned for SSE
		***********************************************************************************/
		void	RMCT_R53_INT32_ASM(void * result, int * comp0, int * comp1, int * comp2, int resultPitch, int width, int height, short bitDepth, OutputFormat outFormat, short checkOutComp);
		
#endif	//ENABLE_ASSEMBLY_OPTIMIZATION
		
		/***********************************************************************************
		Function Name:	MergeConponentsFloat_ASM / MergeConponentsint32_ASM
		Description:	Merge up to 3 components into a picture by the given output format
		IN:
		comp0	: the component buffer, Red channel, DWT done
		comp1	: the component buffer, Green channel, DWT done
		comp2	: the component buffer, Blue channel , DWT done
		resultPitch	: the pitch of the result buffer, count in pixel
		width	: width of the component, count in pixel
		height	: height of the component, count in pixel
		bitDepth : bitdepth of each component
		outFormat : output format defined in outputFormat
		checkOutComp : set to 0-3 will generate the grey scale picture for the corresponding channel, set to INVALID or >=4, will ignore this and generate BMP buffer
		OUT:
		outResult : The output buffer, must be big enough to hold all pixels
		RETURN:
		return true succeed, false means the address is not aligned for SSE
		***********************************************************************************/
#if INTEL_ASSEMBLY_32 || INTEL_ASSEMBLY_64
		void	MergeConponentsFloat_WIN_SSE(unsigned char * outResult, float * comp0,  float * comp1,  float * comp2, float * comp3, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp);
		void 	MergeConponentsInt32_WIN_SSE(unsigned char * outResult, int_32 * comp0, int_32 * comp1, int_32 * comp2, int_32 * comp3,int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp);
#endif

		bool	mInit;
// 		int		mThreadNum;
		EnginConfig * mEngineConfig;
		DwtJob	* mDwtJobs;

		MUTEX_HANDLE	mDwtCounterMutex;

		DecoderConcrete::P_DecodeDWTJob	mUndoDwtJobs;
		int								mUndoDwtJobsLen;
		int								mUndoDwtJobsNum;
		int								mUndoDwtJobsCurrentIndex;
		int								mCurrentUndoHandler;
		int								mCurrentUndoIndex;


		unsigned char	*	mSubThreadDwtOverLayRealMemory;
		unsigned char	*	mSubThreadDwtOverLayMemory;
		int			mSubThreadDwtOverLayMemoryUnitSize;

	};


};
