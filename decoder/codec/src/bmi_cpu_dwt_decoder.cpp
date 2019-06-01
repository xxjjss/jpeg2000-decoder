
#include "bmi_cpu_dwt.h"
#include "codec_base.h"
#include "debug.h"
#include "debugtools.h"
#define VDWT_MULTI_THREADS	1



namespace BMI
{


	//////////////////////////////////////////////////////////////////////////
	// member functions of class ThumbnailBuf
	//////////////////////////////////////////////////////////////////////////
	void ThumbnailBuf::InitBuf(P_PictureInfo pic, int wordLength)
	{

		mPic = pic;
		int tileNum = pic->mProperties.mTileNum.x * pic->mProperties.mTileNum.y;
		if (tileNum > mTileNum)
		{
			SAFE_DELETE_ARRAY(mThumbnailTileOffset);
			mThumbnailTileOffset = new Pos[tileNum];
			SAFE_DELETE_ARRAY(mThumbnailTileSize);
			mThumbnailTileSize = new Size[tileNum];
		}
		mTileNum = tileNum;
		mWordLength = static_cast<WordLength>(wordLength);


		int index = 0;
		mHeight = 0;
		for (int i = 0; i < pic->mProperties.mTileNum.y; ++i )
		{
			mWidth = 0;
			for (int j = 0; j < pic->mProperties.mTileNum.x; ++j)
			{
				mThumbnailTileOffset[index].x = mWidth;
				mThumbnailTileOffset[index].y = mHeight;
				mThumbnailTileSize[index].x = pic->mTileinfo[index].mCompInfo[0].mSubbandInfo[0].mSize.x;
				mThumbnailTileSize[index].y = pic->mTileinfo[index].mCompInfo[0].mSubbandInfo[0].mSize.y;

				mWidth += mThumbnailTileSize[index].x;
				++index;
			}
			mHeight += mThumbnailTileSize[index - 1].y;
		}

		pic->mProperties.mThumbOutputSize.x = mWidth;
		pic->mProperties.mThumbOutputSize.y = mHeight;

		mSize = mWidth * mHeight * (wordLength * 3/* 3 components */ +  UINT_32_BITS /* result of thumbnail generated as 32 bits*/);

		if (mTrueSize < mSize)
		{
			SAFE_FREE(mDataBuf);
			mTrueSize = mSize;
			mDataBuf = (unsigned char *)malloc(mTrueSize);
		}

		mComp[0] = mDataBuf + mWidth * mHeight * wordLength;
		mComp[1] = mComp[0] + mWidth * mHeight * wordLength;
		mComp[2] = mComp[1] + mWidth * mHeight * wordLength;
		mMCTDone = false;

	}	

	void ThumbnailBuf::UploadThumbnailBufferLines( unsigned char * buf, int tileId, int compId, int y_offset, int lines )
	{

		if ((mPic->mProperties.mComponentNum >= 3 && compId >= 3) || (mPic->mProperties.mComponentNum < 3 && compId != 0))
		{
			return;
		}

		if (mPic->mProperties.mMethod == Ckernels_W9X7)
		{
			BMI_ASSERT(mWordLength == UINT_32_BITS);
			float absStep = mPic->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[0].mfAbsStepNorm;

			int * src = (int *)buf;
			int * dst = reinterpret_cast<int *>(mComp[compId] + (mThumbnailTileOffset[tileId].x + (mThumbnailTileOffset[tileId].y + y_offset) * mWidth) * mWordLength);
			int skip = mWidth - mThumbnailTileSize[tileId].x;

			for (int y = 0; y < lines; ++y)
			{
				for (int x = 0; x < mThumbnailTileSize[tileId].x; ++x, ++dst, ++src)
				{

//					int t = ( *src & 0x80000000) ? ((*src - 1)^INT_32_MAX) : (*src & INT_32_MAX);
					*dst = (int)((( *src & 0x80000000) ? ((*src - 1)^INT_32_MAX) : (*src & INT_32_MAX)) * absStep);

				}
				dst += skip;
			}
		}
		else	// W5X3
		{
			memcpy2D(buf, mThumbnailTileSize[tileId].x*mWordLength,lines, mThumbnailTileSize[tileId].x*mWordLength,  mComp[compId], mWidth * mWordLength, mThumbnailTileOffset[tileId].x*mWordLength, mThumbnailTileOffset[tileId].y+y_offset);

		}

	}

	unsigned char * ThumbnailBuf::GetBuf()			
	{
		if (!mMCTDone)
		{
			if (mPic->mProperties.mMethod == Ckernels_W9X7)
			{
				BMI_ASSERT(mWordLength == UINT_32_BITS);

				if (mPic->mProperties.mCalculatedComponentNum >=3)
				{
					// MCT
					const	static float	C2_C0_RATIO	= (float)(1.402);
					const	static float	C1_C1_RATIO	= (float)(-0.34413);
					const	static float	C2_C1_RATIO	= (float)(-0.71414);
					const	static float	C1_C2_RATIO	= (float)(1.772);

					float tmp;
					int *comp0 = (int *)mComp[0];
					int *comp1 = (int *)mComp[1];
					int *comp2 = (int *)mComp[2];
					int * result = (int *)mDataBuf;

					int bitDepth = mPic->mProperties.mBitDepth;


					int_32 rgbValue, maxV =( 0x01 << bitDepth) - 1;

					for (int h = 0; h < mHeight; ++h)
					{
						for(int w = 0; w < mWidth; ++w, ++result, ++comp0, ++comp1, ++comp2)
						{

							tmp = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmp += 1 << (bitDepth  - 1);								// perform level shift
							rgbValue = (int)(SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);						// clipping
							rgbValue <<= 8;

							tmp = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmp += 1 << (bitDepth  - 1);
							rgbValue += (int)(SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);
							rgbValue <<= 8;

							tmp = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmp += 1 << (bitDepth  - 1);
							rgbValue += (int)(SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);
							* result = rgbValue;
						}
					}	

				}
				else
				{
					int *comp0 = (int *)mComp[0];
					int * result = (int *)mDataBuf;
					int bitDepth = mPic->mProperties.mBitDepth;


					int_32 rgbValue, maxV =( 0x01 << bitDepth) - 1;

					for (int h = 0; h < mHeight; ++h)
					{
						for(int w = 0; w < mWidth; ++w, ++result, ++comp0)
						{
							*comp0 += (1 << (bitDepth  - 1));								// perform level shift
							rgbValue = (int)(SET_BOUNDARY(0, *comp0, maxV)) >> (bitDepth - 8);						// clipping
							rgbValue = rgbValue | (rgbValue << 8) | (rgbValue << 16);
							* result = rgbValue;
						}
					}	

				}


			}
			else	// W5X3
			{

				if (mWordLength == UINT_32_BITS)
				{

					if (mPic->mProperties.mCalculatedComponentNum >= 3)
					{
						CpuDwtDecoder::GetInstance()->RRct_53<int_32>(
							(int *)mDataBuf,
							reinterpret_cast<int_32*>(mComp[0]), 
							reinterpret_cast<int_32*>(mComp[1]), 
							reinterpret_cast<int_32*>(mComp[2]), 
							mWidth, 
							mWidth, 
							mHeight,
							mPic->mProperties.mBitDepth,
							mPic->mProperties.mOutputFormat,
							-1);		

					}
					else
					{
						int_32 rgbValue;
						int bitDepth = mPic->mProperties.mBitDepth;
						int_32 dc_offset = 0x01 <<(bitDepth - 1);
						int_32 dc_max = (bitDepth << 1) - 1;
						int * result = (int *)mDataBuf;
						int *comp0 = (int *)mComp[0];

						for (int h = 0; h < mHeight; ++h)
						{
							for(int w = 0; w < mWidth; ++w, ++result, ++comp0)
							{

								*comp0 += dc_offset;
								rgbValue = SET_BOUNDARY(0, *comp0, dc_max) >> (bitDepth - 8);		// r
								rgbValue = rgbValue | (rgbValue << 8) | (rgbValue << 16);
								*result = rgbValue;		
							}
						}
					}

				}
				else
				{
#pragma BMI_PRAGMA_TODO_MSG("Support short length in W5X3 for thumbnail generation")
				}
			}

			mMCTDone = true;
		}

		return mDataBuf;
	}


	//////////////////////////////////////////////////////////////////////////
	//	member function for DwtBuf
	//////////////////////////////////////////////////////////////////////////
	void sDwtBuf::InitBuf(const sDwtJob * curJob, int tileId)
	{
		const P_PictureInfo pic = curJob->mPicture;
		mTileSize = pic->mTileinfo[tileId].mSize;

	
		int dwtTopLevel = curJob->mDwtDecodingTopLevel;
		mTopDwtLevel = dwtTopLevel;

		int  allocateX = 0; 
		for (int i = 0; i <= dwtTopLevel; ++i)
		{
			if (i)
			{
				allocateX += ALIGN_TO_SSE_DWORDS(pic->mTileinfo[tileId].mCompInfo[0].mSubbandInfo[3*i].mSize.x);
			}
			else
			{
				allocateX = ALIGN_TO_SSE_DWORDS(pic->mTileinfo[tileId].mCompInfo[0].mSubbandInfo[0].mSize.x);
			}
		}
		while (dwtTopLevel < pic->mProperties.mDwtLevel)
		{
			mTileSize.x = (mTileSize.x + 1) >> 1;
			mTileSize.y = (mTileSize.y + 1) >> 1;
			++dwtTopLevel;
		}
		int wordLen = curJob->mJobConfig->mWordLength;
		mTileImgBufSize = allocateX * mTileSize.y * wordLen;

//		mTileImgBufSize = mTileSize.x * mTileSize.y * wordLen;

		int totalSize = mTileImgBufSize * pic->mProperties.mCalculatedComponentNum * 2;	// the buf size needed

		if (totalSize > mSizeOfBuf)
		{
			SAFE_FREE(mRealBuf);
			mRealBuf = reinterpret_cast<unsigned char *>(malloc(totalSize + SSE_ALIGN_BYTES));
//			mBuf = (unsigned char *)(((unsigned int)mRealBuf + SSE_ALIGN_BYTES) & 0xfffffff0);
			mBuf = reinterpret_cast<unsigned char *>(((PointerValue)mRealBuf + SSE_ALIGN_BYTES) / (SSE_ALIGN_BYTES + 1) * (SSE_ALIGN_BYTES + 1) );
			mSizeOfBuf = totalSize;
			mRealSizeOfBuf = totalSize + SSE_ALIGN_BYTES;
		}


		unsigned char * buf = mBuf;

		for (int c = 0; c < pic->mProperties.mComponentNum; ++c)
		{

			if ((curJob->mJobConfig->mIgnoreComponents >> c) & 0x01)
			{
				continue;
			}

			mTileImgBufShifted[c] = false;
			mTileImgBuf[c] = buf;
			buf += mTileImgBufSize;
			mTempImgBuf[c] = buf;
			buf += mTileImgBufSize;

			unsigned int bufOffset =  ALIGN_TO_SSE_DWORDS(pic->mTileinfo[tileId].mCompInfo[0].mSubbandInfo[0].mSize.x) * pic->mTileinfo[tileId].mCompInfo[0].mSubbandInfo[0].mSize.y * wordLen;

			for ( int d = 0; d < mTopDwtLevel; ++d)
			{

				int lo_w = pic->mTileinfo[tileId].mCompInfo[c].mSubbandInfo[ 3 * (d + 1) - 1].mSize.x;
				int hi_w = pic->mTileinfo[tileId].mCompInfo[c].mSubbandInfo[ 3 * (d + 1)    ].mSize.x;
				int lo_h = pic->mTileinfo[tileId].mCompInfo[c].mSubbandInfo[ 3 * (d + 1) - 2].mSize.y;
				int hi_h = pic->mTileinfo[tileId].mCompInfo[c].mSubbandInfo[ 3 * (d + 1)    ].mSize.y;
				// subband buf
				mSubbanBuf[c][4 * d		] = mTileImgBuf[c];
				mSubbanBufSize[c][4 * d	    ] = ALIGN_TO_SSE_DWORDS(lo_w) * lo_h * wordLen;

				mSubbanBuf[c][4 * d	+ 1	] = mTileImgBuf[c] + bufOffset;
				mSubbanBufSize[c][4 * d	+ 1 ] = ALIGN_TO_SSE_DWORDS(hi_w) * lo_h * wordLen;
				bufOffset += mSubbanBufSize[c][4 *d + 1];

				mTempResolutionBottomBuf[c][d] = mTempImgBuf[c] + bufOffset;
				mSubbanBuf[c][4 * d	+ 2	] = mTileImgBuf[c] + bufOffset;
				mSubbanBufSize[c][4 * d	+ 2 ] = ALIGN_TO_SSE_DWORDS(lo_w) * hi_h * wordLen;
				bufOffset += mSubbanBufSize[c][4 *d + 2];

				mSubbanBuf[c][4 * d	+ 3	] = mTileImgBuf[c] + bufOffset;
				mSubbanBufSize[c][4 * d	+ 3 ] = ALIGN_TO_SSE_DWORDS(hi_w) * hi_h * wordLen;
				bufOffset += mSubbanBufSize[c][4 *d + 3];
			}
		}

	}

	unsigned char * sDwtBuf::GetSubBandBuf(int compId, int subbandId, int * size /*= NULL*/)
	{
		int Id;
		if (subbandId)
		{
			Id = (subbandId - 1) / 3;	// resolution ID
			Id *= 4;
			Id += (subbandId - 1) % 3 + 1;
		}
		else
		{
			Id = 0;
		}
		if (size)
		{
			*size = mSubbanBufSize[compId][Id];
		}
		return mSubbanBuf[compId][Id];
	}

	unsigned char * sDwtBuf::GetResolutionSubbandBuf(int compId, int resolutionId, ResolutionPosition pos)
	{
		unsigned char * ret ;
		BMI_ASSERT(resolutionId > 0);
		if (resolutionId && resolutionId <= mTopDwtLevel)
		{
			ret = mSubbanBuf[compId][4 * (resolutionId - 1) + pos];
		}
		else
		{
			// resolution level == 0; return the full image buf
			ret = mTileImgBuf[compId];
		}
		return ret;
	}


	unsigned char * sDwtBuf::Shift_GetImgBuf(int compId, unsigned int & compBufSize, const P_PictureInfo picture, int * width /*= NULL*/, int * height /*= NLL*/ )
	{
		if (!mTileImgBufShifted[compId])
		{

			unsigned char * img = mTileImgBuf[compId];
			unsigned int bufsize = mTileImgBufSize;
			if (width)
			{
				*width = mTileSize.x;
			}
			if (height)
			{
				*height = mTileSize.y;
			}
#if INTEL_ASSEMBLY_32
#pragma BMI_PRAGMA_TODO_MSG("Handle when the address is not alligned to SSE")
			unsigned char * shiftImg = mTempImgBuf[compId];
			unsigned int tailSize = bufsize & 0x03;

			bufsize -= tailSize;
			bufsize >>= 4;

			__m128i	int_lift_128bits	= _mm_set1_epi32(1 << (picture->mProperties.mBitDepth - 1));
			if (picture->mProperties.mMethod == Ckernels_W5X3)
			{
				__asm{
 					MOV ESI, img
					MOV EDI, shiftImg
					MOV ECX, bufsize
					MOVDQA XMM1, int_lift_128bits
loop_53:
					MOVAPS  XMM0, [ESI]
					PADDD	XMM0, XMM1
					MOVAPS   [EDI], XMM0
					ADD ESI, 0x10
					ADD EDI, 0x10
					LOOPNZ loop_53
				}
			}
			else if (picture->mProperties.mMethod == Ckernels_W9X7)
			{
				__asm{
					MOV ESI, img
					MOV EDI, shiftImg
					MOV ECX, bufsize
					MOVDQA XMM1, int_lift_128bits
loop_97:
					CVTPS2DQ XMM0, [ESI]
					PADDD	XMM0, XMM1
					MOVDQA   [EDI], XMM0
					ADD ESI, 0x10
					ADD EDI, 0x10
					LOOPNZ loop_97
					}
			}
			else BMI_ASSERT(0);

			BMI_ASSERT(tailSize == 0);
#else
			int * outBuf = (int *)img;

			bufsize /= sizeof(int_32);
			int lift = 1 << (picture->mProperties.mBitDepth - 1);
			if (picture->mProperties.mMethod == Ckernels_W5X3)
			{
				int * inBuf = (int *)img;
				for (unsigned int i = 0; i < bufsize; ++i, ++inBuf, ++outBuf)
				{
					
					*outBuf = *inBuf + lift;
				}
			}
			else
			{
				float * inBuf = (float *)img;
				for (unsigned int i = 0; i < bufsize; ++i,  ++inBuf, ++outBuf)
				{
					*outBuf = (int)*inBuf + lift;
				}
			}
#endif
			mTileImgBufShifted[compId] = true;
		}

		compBufSize = mTileImgBufSize;
		return mTempImgBuf[compId];
	}


	//////////////////////////////////////////////////////////////////////////
	//	member function for DwtStatus
	//////////////////////////////////////////////////////////////////////////

	void DwtStatus::InitDwtStatus(void * Job, const stru_opt * s_opt, int opt_size)
	{

		int tableSize  = CalculateDwtStatusTableSize(Job, s_opt, opt_size);
		if (tableSize > mSizeOfDwtTable )
		{
			SAFE_FREE(mDwtStatusTable);
			mSizeOfDwtTable = tableSize;
			mDwtStatusTable = reinterpret_cast<unsigned char *>(malloc(tableSize));
		}

		mSubBandNumPreComp = 3 * mDwtTopLevel + 1;

		int numTotalSubband = mTileNum * mCompNum * (mDwtTopLevel * 3 + 1);
		if (numTotalSubband > mTotalSubband)
		{
			SAFE_FREE(mStatusTableSubband);
			SAFE_FREE(mStatusTableDwtResolution);
			mTotalSubband  = numTotalSubband;
			mStatusTableSubband = reinterpret_cast<DWT_CBSTRIBE_STATUS ** >(malloc(numTotalSubband * sizeof(DWT_CBSTRIBE_STATUS *)));
			mStatusTableDwtResolution = reinterpret_cast<DwtResolutionStatusTracer **>(malloc(mTileNum * mCompNum * mDwtTopLevel  * sizeof(DwtResolutionStatusTracer *)));
			SAFE_FREE(mStripeNumTable);
			mStripeNumTable = (int*)malloc(numTotalSubband * 4);
			memset((void*)mStripeNumTable,0,numTotalSubband * 4);
		}

		DWT_CBSTRIBE_STATUS ** subbandStatus = mStatusTableSubband;
		DwtResolutionStatusTracer ** resolutionStatus = mStatusTableDwtResolution;
		int offset = 0;
		BMI_ASSERT(mDwtStatusTable);
		BMI_ASSERT(mStripeNumTable);

		memset(reinterpret_cast<void *>(mDwtStatusTable), 0, mSizeOfDwtTable);

		DWT_CBSTRIBE_STATUS	* tmpCbStribeDwtStatus;
		int * tmpStripeNumTable = mStripeNumTable;

		const stru_opt * tile_opt = s_opt;
		for (int t = 0; t < mTileNum; ++t)
		{
			const stru_tile_component * cur_comp = tile_opt->component;
			// traversal all tiles
			for (int c = 0; c < mCompNum; ++c, ++cur_comp)
			{
				// traversal all components
				const stru_subband * cur_band = cur_comp->band;
				for (int r = 0; r < mDwtTopLevel; ++r)
				{
					// traversal all DWT resolution
					*(resolutionStatus) = reinterpret_cast<P_DwtResolutionStatusTracer>(mDwtStatusTable + offset);
					offset += sizeof(DwtResolutionStatusTracer);

					(*resolutionStatus)->mCurStatus = DWT_STATUS_NONE;
					if (r == 0)
					{
						(*resolutionStatus)->mJobCounter = cur_band->ncb_bh;
						*tmpStripeNumTable++ = cur_band->ncb_bh;

						tmpCbStribeDwtStatus = reinterpret_cast<DWT_CBSTRIBE_STATUS *>(mDwtStatusTable + offset);
						*(subbandStatus++) = tmpCbStribeDwtStatus;

						tmpCbStribeDwtStatus->mNumLines = cur_band->codeblock[0].height;
						for (int bh = 1, YOff = cur_band->codeblock[0].height; bh < cur_band->ncb_bh; YOff += cur_band->codeblock[bh * cur_band->ncb_bw].height, ++bh) //***UUU this use of cbheight is not correct
						{
							(++tmpCbStribeDwtStatus)->mYOffsetInBand = YOff;
							tmpCbStribeDwtStatus->mNumLines = cur_band->codeblock[bh * cur_band->ncb_bw].height;							
						}
						offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh;
						++cur_band;
					}

					(*resolutionStatus)->mJobCounter += cur_band->ncb_bh;
					*tmpStripeNumTable++ = cur_band->ncb_bh;
					tmpCbStribeDwtStatus = reinterpret_cast<DWT_CBSTRIBE_STATUS *>(mDwtStatusTable + offset);
					*(subbandStatus++) = tmpCbStribeDwtStatus;
					tmpCbStribeDwtStatus->mNumLines = cur_band->codeblock[0].height;
					for (int bh = 1, YOff = cur_band->codeblock[0].height; bh < cur_band->ncb_bh; YOff += cur_band->codeblock[bh * cur_band->ncb_bw].height, ++bh)
					{						
						(++tmpCbStribeDwtStatus)->mYOffsetInBand = YOff;
						tmpCbStribeDwtStatus->mNumLines = cur_band->codeblock[bh * cur_band->ncb_bw].height;
					}

					offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh;
					++cur_band;
					(*resolutionStatus)->mJobCounter += cur_band->ncb_bh * 2;
					*tmpStripeNumTable++ = cur_band->ncb_bh;
					*tmpStripeNumTable++ = cur_band->ncb_bh;
					tmpCbStribeDwtStatus = reinterpret_cast<DWT_CBSTRIBE_STATUS *>(mDwtStatusTable + offset);
					*(subbandStatus++) = tmpCbStribeDwtStatus;

					tmpCbStribeDwtStatus->mNumLines = cur_band->codeblock[0].height;
					tmpCbStribeDwtStatus[cur_band->ncb_bh].mNumLines = cur_band->codeblock[0].height;
					for (int bh = 1, YOff = cur_band->codeblock[0].height; bh < cur_band->ncb_bh; YOff += cur_band->codeblock[bh * cur_band->ncb_bw].height, ++bh)
					{
						(++tmpCbStribeDwtStatus)->mYOffsetInBand = YOff;
						tmpCbStribeDwtStatus->mNumLines = cur_band->codeblock[bh * cur_band->ncb_bw].height;
						tmpCbStribeDwtStatus[cur_band->ncb_bh].mYOffsetInBand = YOff;
						tmpCbStribeDwtStatus[cur_band->ncb_bh].mNumLines = cur_band->codeblock[bh * cur_band->ncb_bw].height;
					}
					offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh;
					tmpCbStribeDwtStatus = reinterpret_cast<DWT_CBSTRIBE_STATUS *>(mDwtStatusTable + offset);
					*(subbandStatus++) = tmpCbStribeDwtStatus;
					offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh;
					cur_band += 2;
					resolutionStatus++;
				}
			}
			tile_opt =reinterpret_cast<const stru_opt *>(reinterpret_cast<const unsigned char *>(tile_opt) + opt_size);
		}

	}

	unsigned char * DwtStatus::ThreadSafeSetDwtStatus(void * curJob,int tileId, int compId, int subBandId, int cbStribeId, int y_offset_in_band, DWT_STATUS status, int * pRelateBandId /*= NULL*/)	// inline
		// set the status for corresponding stripe 
		// check the status for matched stripe or the dwt resolution, if good to process next step return the buf we need for next step
	{
		P_DwtJob job = reinterpret_cast<P_DwtJob>(curJob);
		unsigned char * ret = NULL;
		int relateBandId = INVALID;

		DWT_CBSTRIBE_STATUS * tmp =  mStatusTableSubband[mSubBandNumPreComp * mCompNum * tileId + mSubBandNumPreComp * compId + subBandId];
		int dwtResolution = (subBandId == 0 ? 0 : (subBandId - 1)/3);

		if (status == DWT_STATUS_READY_FOR_VDWT)
		{
			BMI_ASSERT(cbStribeId != INVALID);
			P_DwtResolutionStatusTracer tracer = mStatusTableDwtResolution[mDwtTopLevel * mCompNum * tileId + mDwtTopLevel * compId + dwtResolution];
 			LOCK_MUTEX(mDwtStatusMutex);
			--tracer->mJobCounter;
			BMI_ASSERT(tracer->mJobCounter >= 0);
			if (!tracer->mJobCounter)
			{
#if VDWT_MULTI_THREADS
				if (dwtResolution < job->mDwtDecodingTopLevel - 1 || job->mThreadNum == 1 || 
					(job->mPicture->mProperties.mMethod == Ckernels_W5X3 && dwtResolution == job->mDwtDecodingTopLevel-1  && compId < ( mCompNum - 1)))
				{
					tracer->mCurStatus = DWT_STATUS_CHANGING;
					ret = reinterpret_cast<unsigned char*>(job->mDwtBuf[tileId].GetResolutionSubbandBuf(compId, dwtResolution + 1,POS_LL));
				}
				else
				{
					// last resolution level VDWT in unfinished jobs queue
					tracer->mCurStatus = DWT_STATUS_READY_FOR_VDWT;
				}
#else
				BMI_ASSERT(dwtResolution <= job->mDwtDecodingTopLevel - 1);
				tracer->mCurStatus = DWT_STATUS_CHANGING;
				ret = reinterpret_cast<unsigned char*>(job->mDwtBuf[tileId].GetResolutionSubbandBuf(compId, dwtResolution + 1,POS_LL));
#endif
			}
			tmp[cbStribeId].mStatus = status;
 			RELEASE_MUTEX(mDwtStatusMutex);

		}
		else if (status == DWT_STATUS_READY_FOR_HDWT)
		{
			BMI_ASSERT(cbStribeId != INVALID && y_offset_in_band != INVALID);

			if (dwtResolution == 0 || subBandId % 3 != 1)
			{
				if (dwtResolution == 0)
				{
					relateBandId = subBandId ^ 1;		// 0 - 1; 2 - 3
				}
				else
				{
					relateBandId = subBandId + ((subBandId % 3) ? 1 : -1);		// 5 - 6; 8 - 9 ......
				}
				LOCK_MUTEX(mDwtStatusMutex);
				if  ( mStatusTableSubband[mSubBandNumPreComp * mCompNum * tileId + mSubBandNumPreComp * compId + relateBandId][cbStribeId].mStatus  == DWT_STATUS_READY_FOR_HDWT)
				{
					mStatusTableSubband[mSubBandNumPreComp * mCompNum * tileId + mSubBandNumPreComp * compId + relateBandId][cbStribeId].mStatus  = DWT_STATUS_CHANGING;
					tmp[cbStribeId].mStatus = DWT_STATUS_CHANGING;
 					RELEASE_MUTEX(mDwtStatusMutex);
					int pitch;
					ret = job->mDwtBuf[tileId].GetSubBandBuf(compId, relateBandId, &pitch);

					pitch /= job->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[relateBandId].mSize.y;
					ret += y_offset_in_band * pitch;
				}
				else
				{
					tmp[cbStribeId].mStatus = status;
 					RELEASE_MUTEX(mDwtStatusMutex);
				}

			}
			else
			{
 				LOCK_MUTEX(mDwtStatusMutex);
				if  ( mStatusTableDwtResolution[mDwtTopLevel * mCompNum * tileId + mDwtTopLevel * compId + dwtResolution - 1]->mCurStatus == DWT_STATUS_DWT_DONE)
				{
					// good to process H-DWT with previous resolution
					tmp[cbStribeId].mStatus = DWT_STATUS_CHANGING;
 					RELEASE_MUTEX(mDwtStatusMutex);
					ret = reinterpret_cast<unsigned char*>(job->mDwtBuf[tileId].GetResolutionSubbandBuf(compId, dwtResolution,POS_LL));
					int pitch = ALIGN_TO_SSE_DWORDS(job->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subBandId + 1].mSize.x);
					ret += y_offset_in_band * pitch * job->mWordLength;

				}
				else
				{
					tmp[cbStribeId].mStatus = status;
 					RELEASE_MUTEX(mDwtStatusMutex);
				}
			}
		}
		else if (status == DWT_STATUS_DWT_DONE)
		{
			BMI_ASSERT(cbStribeId == INVALID);
 			LOCK_MUTEX(mDwtStatusMutex);
			mStatusTableDwtResolution[mDwtTopLevel * mCompNum * tileId + mDwtTopLevel * compId + dwtResolution]->mCurStatus = DWT_STATUS_DWT_DONE;
 			RELEASE_MUTEX(mDwtStatusMutex);
			if (dwtResolution < job->mDwtDecodingTopLevel - 1)
			{
				relateBandId = dwtResolution * 3 + 4;
			}
		}
		else
		{
			BMI_ASSERT(0);
// 			LOCK_MUTEX(mDwtStatusMutex);
// 			tmp[cbStribeId] = status;
// 			RELEASE_MUTEX(mDwtStatusMutex);
		}

		if (pRelateBandId)
		{
			*pRelateBandId = relateBandId;
		}
		return ret;
	}


	unsigned char * DwtStatus::UnfinishedHDWT(void * curJob,int tileId, int compId, int subBandId, int & linesOff, int & numLines)
	{
		BMI_ASSERT(subBandId % 3 == 1 && subBandId >= 4);
		P_DwtJob job = reinterpret_cast<P_DwtJob>(curJob);
		DWT_CBSTRIBE_STATUS * tmp =  mStatusTableSubband[mSubBandNumPreComp * mCompNum * tileId + mSubBandNumPreComp * compId + subBandId];
		int cbStripeNum = mStripeNumTable[mSubBandNumPreComp * mCompNum * tileId + mSubBandNumPreComp * compId + subBandId];//(job->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subBandId].mSize.y + job->mPicture->mProperties.mCodeBlockSize.y - 1) / job->mPicture->mProperties.mCodeBlockSize.y;
		unsigned char * ret = NULL;  //***UUU above use of mCodeBlockSize may cause problems since the actual codeblock size depends on the grid too
		int cbStribeId = 0;
		LOCK_MUTEX(mDwtStatusMutex);
		while(cbStribeId < cbStripeNum)
		{

			if (tmp[cbStribeId].mStatus == DWT_STATUS_READY_FOR_HDWT)
			{
				tmp[cbStribeId].mStatus = DWT_STATUS_CHANGING;
				RELEASE_MUTEX(mDwtStatusMutex);
				int pitch;
				ret = reinterpret_cast<unsigned char*>(job->mDwtBuf[tileId].GetSubBandBuf(compId, subBandId, &pitch));
				pitch /= job->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subBandId].mSize.y;
// 				int y_offset_in_band = job->mPicture->mProperties.mCodeBlockSize.y * cbStribeId;
				linesOff = tmp[cbStribeId].mYOffsetInBand;
				numLines = tmp[cbStribeId].mNumLines;
				ret +=  linesOff * pitch;
				break;
			}
			cbStribeId++;
		}
		if (!ret)
		{
			RELEASE_MUTEX(mDwtStatusMutex);
		}
		return ret;
	}

	int DwtStatus::CalculateDwtStatusTableSize(void * Job, const stru_opt * s_opt, int opt_size)
	{

		P_DwtJob curJob = reinterpret_cast<P_DwtJob>(Job);

		mCompNum = curJob->mPicture->mProperties.mComponentNum;
		mDwtTopLevel = curJob->mDwtDecodingTopLevel;
		mTileNum = curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y;

		int offset = 0;

		const stru_opt * tile_opt = s_opt;
		
		for (int t = 0; t < mTileNum; ++t)
		{
			// traversal all tiles
			const stru_tile_component * cur_comp = tile_opt->component;
			for (int c = 0; c < mCompNum; ++c, ++cur_comp)
			{
				// traversal all components

				const stru_subband * cur_band = cur_comp->band;
				for (int r = 0; r < mDwtTopLevel; ++r)
				{
					// traversal all DWT resolution
					offset += sizeof(DwtResolutionStatusTracer);
// 					int lowCodeBlockHeight  = cur_comp->band
// 						//(curJob->mPicture->mTileinfo[t].mCompInfo[c].mSubbandInfo[3 * r + 1].mSize.y + curJob->mPicture->mProperties.mCodeBlockSize.y - 1) / curJob->mPicture->mProperties.mCodeBlockSize.y;
// 					int highCodeBlockHHeight  =(curJob->mPicture->mTileinfo[t].mCompInfo[c].mSubbandInfo[3 * r + 1].mSize.y + curJob->mPicture->mProperties.mCodeBlockSize.y - 1) / curJob->mPicture->mProperties.mCodeBlockSize.y;
					if (r == 0)
					{
						offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh; //lowCodeBlockHeight;
						++cur_band;

					}
					offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh; // lowCodeBlockHeight;
					++cur_band;
					offset +=	sizeof(DWT_CBSTRIBE_STATUS) * cur_band->ncb_bh * 2;//highCodeBlockHHeight;
					BMI_ASSERT(cur_band->ncb_bh == cur_band[1].ncb_bh);
 					cur_band += 2;
				}
			}
			tile_opt =reinterpret_cast<const stru_opt *>(reinterpret_cast<const unsigned char *>(tile_opt) + opt_size);
		}

		return offset;

	}


	void  memcpy2D(const void * src, int srcCol, int srcRow, int srcPitch, void * dest, int destPitch, int x_off, int y_off, bool wrap /*= false*/ )
	{

		BMI_ASSERT(destPitch >= srcRow);

		unsigned char * target = (unsigned char *)dest + y_off * destPitch + x_off;
		const unsigned char *source = (const unsigned char *)src;

		int cpyLength = wrap ? srcCol : ((x_off + srcCol > destPitch) ? destPitch - x_off : srcCol);
	
		for (int i = 0; i < srcRow; ++i)
		{
			memcpy(target, source, cpyLength);
			target += destPitch;
			source += srcPitch;
		}
	}


	//////////////////////////////////////////////////////////////////////////
	//	member function for CpuDwtDecoder
	//////////////////////////////////////////////////////////////////////////

	CpuDwtDecoder::CpuDwtDecoder():
	mInit(false),
	mEngineConfig(NULL),
	mDwtJobs(NULL),
	mUndoDwtJobs(NULL),
	mUndoDwtJobsLen(0),
	mUndoDwtJobsNum(0),
	mUndoDwtJobsCurrentIndex(0),
	mSubThreadDwtOverLayRealMemory(NULL),
	mSubThreadDwtOverLayMemory(NULL),
	mSubThreadDwtOverLayMemoryUnitSize(0)

	{
		MUTEX_INIT(mDwtCounterMutex,NULL);

	}
	

	CpuDwtDecoder::~CpuDwtDecoder()
	{

		if (mInit)
		{
			SAFE_DELETE_ARRAY(mUndoDwtJobs);
			SAFE_FREE(mSubThreadDwtOverLayRealMemory);
			SAFE_DELETE_ARRAY(mDwtJobs);
			DESTROY_MUTEX(mDwtCounterMutex);
		}


	}

	void	CpuDwtDecoder::Init(EnginConfig & enginConfig)
	{

		mEngineConfig = &enginConfig;

		if (!mInit)
		{
			CREATE_MUTEX(mDwtCounterMutex);
			mDwtJobs = new DwtJob[mEngineConfig->mInputQueueLen];
		}

		mInit = true;
		return;
	}

	void	CpuDwtDecoder::InitPicture(J2K_InputStream &inStream, int outputIndex, const stru_opt * s_opt, int opt_size)

	{
		// need to init stripe buf and the temp_buf and output buf base on the picture's information
		BMI_ASSERT(mInit);

		int jobIndex = outputIndex;

		// find next available job slot
// 		BMI_ASSERT (jobIndex < mMaxJobNum && mDwtJobs[jobIndex].mAvailable != SLOT_IN_USED);	// no released??
		if (!(jobIndex < mEngineConfig->mOutputQueueLen && mDwtJobs[jobIndex].mAvailable != SLOT_IN_USED))
		{
			PRINT(("jobIndex == %d  max %d status %d\n", jobIndex,mEngineConfig->mOutputQueueLen, mDwtJobs[jobIndex].mAvailable ));
			BMI_ASSERT(0);
		}

		P_DwtJob curJob = &(mDwtJobs[jobIndex]);
		curJob->mJobConfig = &(inStream.mConfig);

		if (curJob->mAvailable == SLOT_NO_INIT)
		{
			curJob->mPicture = new PictureInfo;
		}

		curJob->mWordLength = inStream.mConfig.mWordLength;
		curJob->mThreadNum	= mEngineConfig->mThreadNum;
		curJob->mDwtDecodingTopLevel = inStream.mConfig.mDwtDecodeTopLevel;
		if (curJob->mDwtDecodingTopLevel == INVALID)
		{
			curJob->mDwtDecodingTopLevel = inStream.mProperties.mDwtLevel;
		}		

	
		P_PictureInfo cur_picture = curJob->mPicture;
		inStream.mDwtPictureInfo = reinterpret_cast<void * >(cur_picture);

		int tileNum = inStream.mProperties.mTileNum.x * inStream.mProperties.mTileNum.y;
		if (cur_picture->mProperties.mTileNum.x * cur_picture->mProperties.mTileNum.y != tileNum)
		{
			cur_picture->mProperties.mTileNum = inStream.mProperties.mTileNum;
			SAFE_DELETE_ARRAY(cur_picture->mTileinfo);
			SAFE_DELETE_ARRAY(curJob->mDwtBuf);
			cur_picture->mProperties.mComponentNum = 0;	// all components deleted
			cur_picture->mProperties.mCalculatedComponentNum = 0;	// all components deleted
			cur_picture->mTileinfo = new TileInfo[tileNum];
			curJob->mDwtBuf = new DwtBuf[tileNum];
		}

		for (int i = 0; i < tileNum; ++i)
		{
			cur_picture->mTileinfo[i].mOff.x = (i % inStream.mProperties.mTileNum.x) * inStream.mProperties.mTileSize.x;
			cur_picture->mTileinfo[i].mOff.y = (i / inStream.mProperties.mTileNum.x) * inStream.mProperties.mTileSize.y;
			cur_picture->mTileinfo[i].mSize.x = (cur_picture->mTileinfo[i].mOff.x + inStream.mProperties.mTileSize.x > inStream.mProperties.mSize.x) 
				? (inStream.mProperties.mSize.x - cur_picture->mTileinfo[i].mOff.x)
				: inStream.mProperties.mTileSize.x;
			cur_picture->mTileinfo[i].mSize.y = (cur_picture->mTileinfo[i].mOff.y + inStream.mProperties.mTileSize.y > inStream.mProperties.mSize.y) 
				? (inStream.mProperties.mSize.y - cur_picture->mTileinfo[i].mOff.y)
				: inStream.mProperties.mTileSize.y;

			for (int d = inStream.mProperties.mDwtLevel - 1, xOff = cur_picture->mTileinfo[i].mOff.x, yOff = cur_picture->mTileinfo[i].mOff.y;
				d >=0; --d)
			{

				cur_picture->mTileinfo[i].mXParity[d] = xOff & 1;
				cur_picture->mTileinfo[i].mYParity[d] = yOff & 1;
				xOff =  BMI_CEILDIV2(xOff);
				yOff =  BMI_CEILDIV2(yOff);
			}
		}


		P_TileInfo curTile;

//		if (cur_picture->mProperties.mComponentNum != inStream.mProperties.mComponentNum)
// 		if (cur_picture->mProperties.mComponentNum != inStream.mProperties.mComponentNum || 
// 			cur_picture->mProperties.mCalculatedComponentNum != inStream.mProperties.mCalculatedComponentNum ||
// 			(cur_picture->mProperties.mCalculatedComponentNum == inStream.mProperties.mCalculatedComponentNum && inStream.mConfig.mIgnoreComponents))
		{
			for (int i = 0; i < tileNum; ++i)
			{
				 curTile = &cur_picture->mTileinfo[i];
				CompInfo* cmpPtr = (CompInfo*)curTile->mCompInfo;
				SAFE_DELETE_ARRAY(cmpPtr);
				cur_picture->mProperties.mDwtLevel = 0;	//	all subband deleted
				curTile->mCompInfo = new CompInfo[inStream.mProperties.mComponentNum];
			}
		}

// 		for (int i = 0; i < tileNum; ++i)
// 		{
// 			 curTile = &cur_picture->mTileinfo[i];
// 
// // 			for (int j = 0; j < inStream.mProperties.mComponentNum; ++j)
// // 			{
// // 				curTile->mCompInfo[i].mSubbandFinishedBitmap = 0;
// // 				curTile->mCompInfo[j].mNextDwtLevel = 1;
// // 			}
// 		}

		if (cur_picture->mProperties.mDwtLevel != inStream.mProperties.mDwtLevel)
		{
			const unsigned char * opt_memory = reinterpret_cast<const unsigned char *>(s_opt);
			for (int i = 0; i < tileNum; ++i)
			{
//				const stru_opt * tile_opt =  reinterpret_cast<const stru_opt *>(opt_memory);
				for (int j = 0; j < inStream.mProperties.mComponentNum; ++j)
				{
					if (((curJob->mJobConfig->mIgnoreComponents >> j) & 0x01) && (j != 0))
						// current component ignored and it's not components 0
						// we need to get information from component 0 somewhere
					{
						continue;
					}

					P_CompInfo curComp = reinterpret_cast<P_CompInfo>(&cur_picture->mTileinfo[i].mCompInfo[j]);
					SubbandInfo * subbPtr = (SubbandInfo *)curComp->mSubbandInfo;
					SAFE_DELETE_ARRAY(subbPtr);
// 					curComp->mNextDwtLevel = 1;
					curComp->mSubbandInfo = new  SubbandInfo[inStream.mProperties.mDwtLevel * 3 + 1];

					for (int k = 0; k <= inStream.mProperties.mDwtLevel - 1; ++k)
					{
//						const stru_tile_component * comp = &tile_opt->component[j];

						for (int s = (k == 0 ? 0 : 1); s < 4; ++s)
						{
							int id = k*3 + s;
							curComp->mSubbandInfo[id].mId = id;
						}
					}

				}
				opt_memory += opt_size;	// next tile opt memory space
			}
		}


		const unsigned char * opt_memory = reinterpret_cast<const unsigned char *>(s_opt);
		for (int i = 0; i < tileNum; ++i)
		{
			const stru_opt * tile_opt =  reinterpret_cast<const stru_opt *>(opt_memory);
			for (int j = 0; j < inStream.mProperties.mComponentNum; ++j)
			{
				if ((curJob->mJobConfig->mIgnoreComponents >> j) & 0x01)
				{
					// current component skipped
					continue;
				}
				P_CompInfo curComp = reinterpret_cast<P_CompInfo>(&cur_picture->mTileinfo[i].mCompInfo[j]);

				for (int k = 0; k <= inStream.mProperties.mDwtLevel - 1; ++k)
				{
					const stru_tile_component * comp = &tile_opt->component[j];

					for (int s = (k == 0 ? 0 : 1); s < 4; ++s)
					{
						int id = k*3 + s;
						stru_subband * band = &comp->band[id];

						stru_code_block * cb = band->codeblock;
						curComp->mSubbandInfo[id].mSize.x = cb->width;
						curComp->mSubbandInfo[id].mSize.y = cb->height;
						for (int bw = 1; bw < band->ncb_bw; ++bw)
						{	
							curComp->mSubbandInfo[id].mSize.x += cb[bw].width;
						}
						for (int bh = 1; bh < band->ncb_bh; ++bh)
						{
							curComp->mSubbandInfo[id].mSize.y += cb[band->ncb_bw*bh].height;
						}

						curComp->mSubbandInfo[id].mOff.x = 
							curComp->mSubbandInfo[id].mOff.y = 0;

						if (s == 3)	// 1 & 3
						{
							curComp->mSubbandInfo[id].mOff.x = 
								curComp->mSubbandInfo[id-2].mOff.x = curComp->mSubbandInfo[id - 1].mSize.x;

							curComp->mSubbandInfo[id].mOff.y = 
								curComp->mSubbandInfo[id-1].mOff.y = curComp->mSubbandInfo[id - 2].mSize.y;
						}


						if (inStream.mProperties.mMethod == Ckernels_W9X7)
						{
							const unsigned int * qqsteps = tile_opt->qcc_quant_step[j];

							int absStepSize = ((qqsteps[id] & 0x7ff)<< 2) | 0x2000;	// 0x2000 = 0x01 << 13
							int n = inStream.mProperties.mBitDepth  - (qqsteps[id] >> 11);
							absStepSize = (n > 0) ? (absStepSize << n) : (absStepSize >> (-n));

							curComp->mSubbandInfo[id].mfAbsStepNorm = static_cast<float>(absStepSize) * IDWT_NORM;
						}
					}
				}
			}
			opt_memory += opt_size;	// next tile opt memory space

		}


		cur_picture->mProperties = inStream.mProperties;


		curJob->mThumbnailBuf.InitBuf(cur_picture,curJob->mWordLength );	//  Init thumbnail buffer
		inStream.mProperties = cur_picture->mProperties;

		for (int i = 0; i < tileNum; ++i)
		{
			curJob->mDwtBuf[i].InitBuf(curJob, i);
		}

		cur_picture->mProperties.mOutputFormat = curJob->mJobConfig->mOutputFormat;

		if (curJob->mJobConfig->mOutputFormat != IMAGE_FOTMAT_NONE)
		{
			switch (curJob->mJobConfig->mOutputFormat)
			{
			case	IMAGE_FOTMAT_32BITS_ARGB:
			case	IMAGE_FOTMAT_32BITS_4COMPONENTS:
				curJob->mResultbuf.InitBuf(	cur_picture->mProperties.mOutputSize.x * cur_picture->mProperties.mOutputSize.y * sizeof(ARGB_32BITS), true);	//  result is saved as 32 bits ARGB
				break;

			case	IMAGE_FOTMAT_48BITS_RGB:
			case	IMAGE_FOTMAT_48BITS_3COMPONENTS:
				curJob->mResultbuf.InitBuf(	cur_picture->mProperties.mOutputSize.x * cur_picture->mProperties.mOutputSize.y * sizeof(RGB_48BITS), true );	//  result is saved as 48 bits RGB
				break;
				
			case	IMAGE_FOTMAT_24BITS_RGB:
			case	IMAGE_FOTMAT_24BITS_3COMPONENTS:
				curJob->mResultbuf.InitBuf(	cur_picture->mProperties.mOutputSize.x * cur_picture->mProperties.mOutputSize.y * sizeof(RGB_24BITS), true);	//  result is saved as 48 bits RGB
				break;
				
			case IMAGE_FOTMAT_64BITS_4COMPONENTS:
			case IMAGE_FOTMAT_64BITS_ARGB:
				curJob->mResultbuf.InitBuf(	cur_picture->mProperties.mOutputSize.x * cur_picture->mProperties.mOutputSize.y * sizeof(FOUR_COMPONENTS_64BITS), true);	//  result is saved as 48 bits RGB
				break;

			case	IMAGE_FOTMAT_36BITS_RGB:
			case	IMAGE_FOTMAT_48BITS_ARGB:
			default:
				#pragma	BMI_PRAGMA_TODO_MSG("for supporting more format")
				BMI_ASSERT(0);
				break;
			}
		}

		curJob->mAvailable = SLOT_IN_USED;		// Occupy current slot

		curJob->mDwtStatus.InitDwtStatus(reinterpret_cast<void *>(curJob), s_opt, opt_size);
#if VDWT_MULTI_THREADS

		if (mEngineConfig->mThreadNum > 1)
		{
			CleanUpUndoJobs();
			mCurrentUndoHandler = inStream.mHandler;
			mCurrentUndoIndex = outputIndex;

			BMI_ASSERT(curJob->mDwtDecodingTopLevel > 0 && curJob->mDwtDecodingTopLevel <= curJob->mPicture->mProperties.mDwtLevel);

			DwtMctSubType subtype = SUBTYPE_UNKNOWN;

			int pizelSizeInByte;
			bool rgbOut = false;
			if (curJob->mJobConfig->mOutputFormat < IMAGE_FOTMAT_NONE)
			{
				switch (curJob->mJobConfig->mOutputFormat)
				{
				case	IMAGE_FOTMAT_48BITS_RGB:
				case	IMAGE_FOTMAT_48BITS_ARGB:
					rgbOut = true;
				case	IMAGE_FOTMAT_48BITS_3COMPONENTS:
					pizelSizeInByte = sizeof(RGB_48BITS);
					break;

				case	IMAGE_FOTMAT_32BITS_ARGB:
					rgbOut = true;
				case	IMAGE_FOTMAT_32BITS_4COMPONENTS:
					pizelSizeInByte = sizeof(ARGB_32BITS);
					break;

				case	IMAGE_FOTMAT_24BITS_RGB:
					rgbOut = true;
				case	IMAGE_FOTMAT_24BITS_3COMPONENTS:
					pizelSizeInByte = sizeof(RGB_24BITS);
					break;

				case IMAGE_FOTMAT_64BITS_ARGB:
					rgbOut = true;
				case IMAGE_FOTMAT_64BITS_4COMPONENTS:
					pizelSizeInByte = sizeof(ARGB_64BITS);
					break;

				case	IMAGE_FOTMAT_36BITS_RGB:
				default:
#pragma	BMI_PRAGMA_TODO_MSG("for supporting more picture format")
					BMI_ASSERT(0);
					break;
				}


				if (rgbOut)
				{
					if (curJob->mPicture->mProperties.mCalculatedComponentNum >= 3)
					{
						subtype = (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7) ? SUBTYPE_97_ICT : SUBTYPE_53_MCT;
					}
					else
					{
						subtype = (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7) ? SUBTYPE_97_MERGE : SUBTYPE_53_MERGE;
					}
				}
				else
				{
					subtype = (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7) ? SUBTYPE_97_MERGE : SUBTYPE_53_MERGE;
				}
		}

 			mUndoDwtJobsNum =	(curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y) * 
					( curJob->mPicture->mProperties.mCalculatedComponentNum + (subtype == SUBTYPE_UNKNOWN ? 0 : 2) );	// One VDWT for each component and one sync plus one MCT for each tile


			if (mUndoDwtJobsLen < mUndoDwtJobsNum)
			{
				mUndoDwtJobsLen = mUndoDwtJobsNum;
				SAFE_DELETE_ARRAY(mUndoDwtJobs);
				mUndoDwtJobs = new DecoderConcrete::DecodeDWTJob[mUndoDwtJobsLen];
			}

			int jobIndex = 0;

			for(int t = 0; t < (curJob->mPicture->mProperties.mTileNum.x * curJob->mPicture->mProperties.mTileNum.y); ++t)
			{
				for (int i = 0; i < curJob->mPicture->mProperties.mComponentNum ; ++i)
				{
					if ((curJob->mJobConfig->mIgnoreComponents >> i) & 0x01)
					{
						continue;
					}

					mUndoDwtJobs[jobIndex].mSubType = ((curJob->mPicture->mProperties.mMethod == Ckernels_W9X7) ? SUBTYPE_VDWT_97 : SUBTYPE_VDWT_53);
					mUndoDwtJobs[jobIndex].mTileid = t;
					mUndoDwtJobs[jobIndex].mWordLength = curJob->mWordLength;
					mUndoDwtJobs[jobIndex].mVDWT.mHDWTResult_up = curJob->mDwtBuf[t].GetTmpBuf(i);
					mUndoDwtJobs[jobIndex].mVDWT.mHDWTResult_down = curJob->mDwtBuf[t].GetTmpResolutionBottomBuf(i, curJob->mDwtDecodingTopLevel - 1 );
					mUndoDwtJobs[jobIndex].mResult = curJob->mDwtBuf[t].GetImgBuf(i);	
					mUndoDwtJobs[jobIndex].mVDWT.mWidth  = curJob->mPicture->mTileinfo[t].mCompInfo[i].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3 - 1].mSize.x
						+ curJob->mPicture->mTileinfo[t].mCompInfo[i].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3    ].mSize.x;
					mUndoDwtJobs[jobIndex].mVDWT.mLow_h  = curJob->mPicture->mTileinfo[t].mCompInfo[i].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3 - 2].mSize.y;
					mUndoDwtJobs[jobIndex].mVDWT.mHigh_h = curJob->mPicture->mTileinfo[t].mCompInfo[i].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3    ].mSize.y;
					mUndoDwtJobs[jobIndex].mVDWT.mYParity = curJob->mPicture->mTileinfo[t].mYParity[curJob->mDwtDecodingTopLevel - 1];
					mUndoDwtJobs[jobIndex].mResultPitch = mUndoDwtJobs[jobIndex].mVDWT.mWidth;

					++jobIndex;

				}

				if (subtype != SUBTYPE_UNKNOWN)	// sync and MCT
				{
					mUndoDwtJobs[jobIndex].mTileid = t;
					mUndoDwtJobs[jobIndex].mSubType = SUBTYPE_SYNC_THREADS;
					mUndoDwtJobs[jobIndex].mLastSyncJob = false;
					++jobIndex;
					// SYNC job

					mUndoDwtJobs[jobIndex].mSubType = subtype;
					mUndoDwtJobs[jobIndex].mMCT.mOutFormat = curJob->mJobConfig->mOutputFormat;
					mUndoDwtJobs[jobIndex].mMCT.outPizelSizeinBytes = pizelSizeInByte;
					mUndoDwtJobs[jobIndex].mTileid = t;

					if(		inStream.mConfig.mCheckOutComponent == INVALID				// no check out
						||	inStream.mConfig.mCheckOutComponent >= curJob->mPicture->mProperties.mComponentNum		// check out component doesn't exist
						||	((curJob->mJobConfig->mIgnoreComponents >> inStream.mConfig.mCheckOutComponent) & 0x01) )	// check out component ignored
					{
						mUndoDwtJobs[jobIndex].mMCT.mCheckoutComp = INVALID;
						if (curJob->mPicture->mProperties.mCalculatedComponentNum < 3 && rgbOut)
						{
							// output the first component
							for (int c = 0; c < curJob->mPicture->mProperties.mComponentNum; ++c)
							{
								if (!((curJob->mJobConfig->mIgnoreComponents >> c) & 0x01))
								{
									mUndoDwtJobs[jobIndex].mMCT.mComp[0] = curJob->mDwtBuf[t].GetImgBuf(c);
									break;
								}
							}
							mUndoDwtJobs[jobIndex].mMCT.mComp[1] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mComp[2] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mComp[3] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mCheckoutComp = 0;
						}
						else if (rgbOut)
						{
							for (int c = 0, cal = 0; c < curJob->mPicture->mProperties.mComponentNum; ++c)
							{
								if (!((curJob->mJobConfig->mIgnoreComponents >> c) & 0x01))
								{
									mUndoDwtJobs[jobIndex].mMCT.mComp[cal] = curJob->mDwtBuf[t].GetImgBuf(c);
									++cal;
									if (cal == 3)
									{
										break;
									}
								}
							}
							mUndoDwtJobs[jobIndex].mMCT.mComp[3] = NULL;
						}
						else	// !rgbout
						{
							// merge

							mUndoDwtJobs[jobIndex].mMCT.mComp[1] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mComp[2] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mComp[3] = NULL;

							for (int c = 0, cal = 0; c < curJob->mPicture->mProperties.mComponentNum; ++c)
							{
								if (!((curJob->mJobConfig->mIgnoreComponents >> c) & 0x01))
								{
									mUndoDwtJobs[jobIndex].mMCT.mComp[cal] = curJob->mDwtBuf[t].GetImgBuf(c);
									++cal;
									if (cal > 3)
									{
										break;
									}
								}
							}
						}
					}
					else	// inStream.mConfig.mCheckOutComponent != INVALID
					{
						if (	curJob->mPicture->mProperties.mComponentNum != 3
							||	curJob->mPicture->mProperties.mComponentNum == 3 && (!(curJob->mJobConfig->mIgnoreComponents & 0x07)))
							// not RGB or one of the first 3 components ignored
						{
							mUndoDwtJobs[jobIndex].mMCT.mComp[0] = curJob->mDwtBuf[t].GetImgBuf(inStream.mConfig.mCheckOutComponent);
							mUndoDwtJobs[jobIndex].mMCT.mComp[1] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mComp[2] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mComp[3] = NULL;
							mUndoDwtJobs[jobIndex].mMCT.mCheckoutComp = 0;
						}
						else
						{
							for (int i = 0; i < 3; ++i)
							{
								mUndoDwtJobs[jobIndex].mMCT.mComp[i] = curJob->mDwtBuf[t].GetImgBuf(i);
							}								
							if (!rgbOut)
							{
								mUndoDwtJobs[jobIndex].mMCT.mComp[3] = curJob->mDwtBuf[t].GetImgBuf(3);
							}
							else
							{
								mUndoDwtJobs[jobIndex].mMCT.mComp[3] = NULL;
							}
							mUndoDwtJobs[jobIndex].mMCT.mCheckoutComp = inStream.mConfig.mCheckOutComponent;
						}

					}

					mUndoDwtJobs[jobIndex].mWordLength = curJob->mWordLength;
					mUndoDwtJobs[jobIndex].mResult = reinterpret_cast<unsigned char *>(curJob->mResultbuf.GetBuf());
					mUndoDwtJobs[jobIndex].mResultPitch = curJob->mPicture->mProperties.mOutputSize.x;
					int x_off = curJob->mPicture->mTileinfo[t].mOff.x;
					int y_off = curJob->mPicture->mTileinfo[t].mOff.y;
					if (curJob->mDwtDecodingTopLevel < curJob->mPicture->mProperties.mDwtLevel)
					{
						int dwtLevel = curJob->mDwtDecodingTopLevel;


						while(dwtLevel < curJob->mPicture->mProperties.mDwtLevel)
						{
							x_off = (x_off + 1)>>1;
							y_off = (y_off + 1)>>1;
							++dwtLevel;
						}
					}
					mUndoDwtJobs[jobIndex].mResult += (mUndoDwtJobs[jobIndex].mResultPitch * y_off + x_off) * pizelSizeInByte;

					int c = 0;
					while((curJob->mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
					--c;
					mUndoDwtJobs[jobIndex].mMCT.mWidth = curJob->mPicture->mTileinfo[t].mCompInfo[c].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3 - 1].mSize.x + curJob->mPicture->mTileinfo[t].mCompInfo[c].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3    ].mSize.x;
					mUndoDwtJobs[jobIndex].mMCT.mHeight = curJob->mPicture->mTileinfo[t].mCompInfo[c].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3 - 2].mSize.y + curJob->mPicture->mTileinfo[t].mCompInfo[c].mSubbandInfo[curJob->mDwtDecodingTopLevel * 3    ].mSize.y;
					mUndoDwtJobs[jobIndex].mMCT.mBitDepth = curJob->mPicture->mProperties.mBitDepth;

					++jobIndex;
				}
			}
		}
		if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)
		{
			int unitSize = ALIGN_TO_SSE_DWORDS(curJob->mPicture->mProperties.mTileSize.x) *  sizeof(float) * 2;
			// we need 2 lines overlay for 97 VDWT and one line temp for HDWT

			if (unitSize > mSubThreadDwtOverLayMemoryUnitSize)
			{
				mSubThreadDwtOverLayMemoryUnitSize = unitSize;
				SAFE_FREE(mSubThreadDwtOverLayRealMemory);
				mSubThreadDwtOverLayRealMemory = reinterpret_cast<unsigned char *>(malloc(mEngineConfig->mThreadNum * mSubThreadDwtOverLayMemoryUnitSize + SSE_ALIGN_BYTES));
// 				mSubThreadDwtOverLayMemory = (unsigned char *)((int)(mSubThreadDwtOverLayRealMemory +  SSE_ALIGN_BYTES) & 0xfffffff0);
				mSubThreadDwtOverLayMemory = reinterpret_cast<unsigned char *>(((PointerValue)mSubThreadDwtOverLayRealMemory + SSE_ALIGN_BYTES) / (SSE_ALIGN_BYTES + 1) * (SSE_ALIGN_BYTES + 1) );

			}
		}
#endif

	}

	unsigned char  * CpuDwtDecoder::GetStripeBuffer(PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int y_offset_in_band, int &buf_pitch_in_bytes)
	{
		// return the stripe buf for T1
		
		BMI_ASSERT(!((mDwtJobs[pictureIndex.mInputSlot].mJobConfig->mIgnoreComponents >> compId ) & 0x01));

		unsigned char * ret = mDwtJobs[pictureIndex.mInputSlot].mDwtBuf[tileId].GetSubBandBuf(compId, subbandId, &buf_pitch_in_bytes);

		buf_pitch_in_bytes /= mDwtJobs[pictureIndex.mInputSlot].mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mSize.y;

		ret += y_offset_in_band * buf_pitch_in_bytes;
		return ret;
	}

	void	CpuDwtDecoder::UploadStripeBuf(unsigned char * stripeBuf, const DecodeJobConfig & config,const PictureIndex & pictureIndex, int tileId, int compId, int subbandId, int linesinserted, int y_offset_in_band, int codeBlockStripeId, int threadId)
	{
		// T1 finished, the data already put into the dwt buffer
		// We need to check can we start DWT jobs and start doing dwt if we could
		BMI_ASSERT(!((mDwtJobs[pictureIndex.mInputSlot].mJobConfig->mIgnoreComponents >> compId ) & 0x01));

		DwtJob * curJob = &mDwtJobs[pictureIndex.mInputSlot];

		if (subbandId == 0 )		// LL : used for generate thumbnail
		{

			curJob->mThumbnailBuf.UploadThumbnailBufferLines(stripeBuf, tileId, compId, y_offset_in_band, linesinserted);
		}

// 		DEBUG_PAUSE(tileId >= 1);
// #if _DEBUG
// 		{
// 			char filename[255] = {0};
// 			SubbandInfo_c * sb = &(curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId]);
// 			sprintf_s(filename,255, "c://temp//T%d_C%d_S%02d_L%02d.txt", tileId,compId,subbandId,y_offset_in_band);
// 			PRINT_BUF_TO_FILE_4BYTES(((int *)stripeBuf, sb->mSize.x * linesinserted, filename, sb->mSize.x, true, true));
// 		}
// #endif

		int nextStep = INVALID;

		int relateBandId ;
		unsigned char * nextStepBuf = curJob->mDwtStatus.ThreadSafeSetDwtStatus(reinterpret_cast<void *>(curJob), tileId, compId, subbandId, codeBlockStripeId, y_offset_in_band, DWT_STATUS_READY_FOR_HDWT, &relateBandId);
		if (nextStepBuf)
		{
			if (curJob->mWordLength == UINT_32_BITS)
			{
				unsigned char * tmpBuf =  curJob->mDwtBuf[tileId].GetTmpBuf(compId);
				unsigned char * hDWTResultBuf = tmpBuf;
				unsigned char  * band0;
				unsigned char  * band1;

				float lowGain, hiGain;
				CompInfo_c * curComp = &(curJob->mPicture->mTileinfo[tileId].mCompInfo[compId]);
				int low_width,high_width;
				int lowHeight, hiHeight;
				int linesOff = y_offset_in_band;


				if (subbandId % 3 != 1 && subbandId != 0)	// HL and HH band
				{
					hiHeight = curComp->mSubbandInfo[subbandId].mSize.y;
					lowHeight = curComp->mSubbandInfo[subbandId - (subbandId + 2) % 3].mSize.y;
// 					linesOff += lowHeight;
					hDWTResultBuf = curJob->mDwtBuf[tileId].GetTmpResolutionBottomBuf(compId, (subbandId-1)/3);
				}
				else 	// low band	 LL / LH band
				{
					lowHeight = curComp->mSubbandInfo[subbandId].mSize.y;
					hiHeight = curComp->mSubbandInfo[subbandId + 2].mSize.y;
				}


				if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)
				{

					if (subbandId % 3 != 2 && subbandId != 0)	// high band: LH / HH
					{
						band1 = stripeBuf;
						band0 = nextStepBuf;
						high_width = curComp->mSubbandInfo[subbandId].mSize.x;
						hiGain = curComp->mSubbandInfo[subbandId].mfAbsStepNorm * IDWT_HI_GAIN;
						if (relateBandId == INVALID)
						{
							low_width = curComp->mSubbandInfo[subbandId + 1].mSize.x;
							lowGain = IDWT_LO_GAIN;
						}
						else
						{
							low_width = curComp->mSubbandInfo[relateBandId].mSize.x;
							lowGain = curComp->mSubbandInfo[relateBandId].mfAbsStepNorm * IDWT_LO_GAIN;
						}

					}
					else 	// low band		LL / Hl
					{
						BMI_ASSERT(relateBandId != INVALID);
						band0 = stripeBuf;
						band1 = nextStepBuf;
						low_width = curComp->mSubbandInfo[subbandId].mSize.x;
						high_width = curComp->mSubbandInfo[relateBandId].mSize.x;
						lowGain = curComp->mSubbandInfo[subbandId].mfAbsStepNorm * IDWT_LO_GAIN;
						hiGain = curComp->mSubbandInfo[relateBandId].mfAbsStepNorm * IDWT_HI_GAIN;
					}
					if (subbandId == 0 || (subbandId % 3 == 1))		// LL or Lh
					{
						lowGain *= IDWT_LO_GAIN;	// for VDWT, lowband
						hiGain *= IDWT_LO_GAIN;
					}
					else	// HL or HH
					{
						lowGain *= IDWT_HI_GAIN;	// for VDWT, high band
						hiGain *= IDWT_HI_GAIN;
					}
#if INTEL_ASSEMBLY_32 || AT_T_ASSEMBLY
					HDWT_R97_FLOAT_ASM(
						threadId, 
#else
					HDWT_R97(
#endif
						reinterpret_cast<float*>(hDWTResultBuf +  linesOff * ALIGN_TO_SSE_DWORDS(low_width + high_width) * 4),
						reinterpret_cast<float *>(band0),
						reinterpret_cast<float *>(band1),
						low_width,
						high_width,
						linesinserted,
						lowGain,
						hiGain,
						curJob->mPicture->mTileinfo[tileId].mXParity[subbandId ? (subbandId - 1) / 3 : 0],
						(relateBandId != INVALID));
				}
				else		// W5X3
				{
					if (subbandId % 3 != 2 && subbandId != 0)	// high band: LH / HH
					{
						band1 = stripeBuf;
						band0 = nextStepBuf;
						high_width = curComp->mSubbandInfo[subbandId].mSize.x;
						low_width = curComp->mSubbandInfo[(relateBandId == INVALID ? subbandId + 1 : relateBandId)].mSize.x;

					}
					else 	// low band		LL / Hl
					{
						BMI_ASSERT(relateBandId != INVALID);
						band0 =stripeBuf;
						band1 = nextStepBuf;
						low_width = curComp->mSubbandInfo[subbandId].mSize.x;
						high_width = curComp->mSubbandInfo[relateBandId].mSize.x;
					}

					if (curJob->mWordLength == 4)
					{
						HDWT_R53_INT(
// 							reinterpret_cast<int_32*>(tmpBuf +  linesOff * (low_width + high_width) * 4),
							reinterpret_cast<int_32*>(hDWTResultBuf +  linesOff * ALIGN_TO_SSE_DWORDS(low_width + high_width) * 4),
							reinterpret_cast<int_32 *>(band0),
							reinterpret_cast<int_32 *>(band1),
							low_width,
							high_width,
							linesinserted,
							curJob->mPicture->mTileinfo[tileId].mXParity[subbandId ? (subbandId - 1) / 3 : 0],
							(relateBandId != INVALID));				
					}
					else
					{
						HDWT_R53_SHORT(
//							reinterpret_cast<int_16*>(tmpBuf +  linesOff * (low_width + high_width) * 4),
							reinterpret_cast<int_16*>(hDWTResultBuf +  linesOff * ALIGN_TO_SSE_DWORDS(low_width + high_width) * 2),
							reinterpret_cast<int_16 *>(band0),
							reinterpret_cast<int_16 *>(band1),
							low_width,
							high_width,
							linesinserted,
							curJob->mPicture->mTileinfo[tileId].mXParity[subbandId ? (subbandId - 1) / 3 : 0],
							(relateBandId != INVALID));		
					}

				}
// 				DEBUG_PRINT(("HDWT done for tile %d comp %d subband %d lines %d linesoff %d\n", tileId, compId, subbandId, linesinserted, linesOff));

				if (relateBandId != INVALID)
				{
					nextStepBuf = curJob->mDwtStatus.ThreadSafeSetDwtStatus(reinterpret_cast<void *>(curJob), tileId, compId, relateBandId, codeBlockStripeId, INVALID,  DWT_STATUS_READY_FOR_VDWT);
					BMI_ASSERT(nextStepBuf == NULL);
				}
				nextStepBuf = curJob->mDwtStatus.ThreadSafeSetDwtStatus(reinterpret_cast<void *>(curJob), tileId, compId, subbandId, codeBlockStripeId, INVALID, DWT_STATUS_READY_FOR_VDWT);

				while (nextStepBuf)
				{
					BMI_ASSERT(nextStepBuf == curJob->mDwtBuf[tileId].GetResolutionSubbandBuf(compId, subbandId == 0 ? 1 : (subbandId + 2) / 3,POS_LL));
					// current resolution is good for processing VDWT
					if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)
					{
						VDWT_R97(
							nextStepBuf, 
							tmpBuf, 
							curJob->mDwtBuf[tileId].GetTmpResolutionBottomBuf(compId, (subbandId == 0 ? 0 : (subbandId-1)/3)), 
							lowHeight, hiHeight, 
							low_width+ high_width, 
							0, lowHeight, 
							curJob->mPicture->mTileinfo[tileId].mYParity[subbandId ? (subbandId - 1) / 3 : 0], threadId);
					}
					else	// w5x3
					{
						if (curJob->mWordLength == 4)
						{
							VDWT_R53<int_32>(
								reinterpret_cast<int_32*>(nextStepBuf), 
								reinterpret_cast<int_32*>(tmpBuf), 
								lowHeight, hiHeight, low_width + high_width, 0,lowHeight, 
								curJob->mPicture->mTileinfo[tileId].mYParity[subbandId ? (subbandId - 1) / 3 : 0],
								threadId );
						}
						else
						{
							VDWT_R53<int_16>(
								reinterpret_cast<int_16*>(nextStepBuf), 
								reinterpret_cast<int_16*>(tmpBuf), 
								lowHeight, hiHeight, low_width + high_width, 0,lowHeight,
								curJob->mPicture->mTileinfo[tileId].mYParity[subbandId ? (subbandId - 1) / 3 : 0],
								threadId );

						}
					}
// 					DEBUG_PRINT(("VDWT done for tile %d comp %d resolution %d\n", tileId, compId,( subbandId == 0 ? 0 : (subbandId  - 1) / 3)));

					curJob->mDwtStatus.ThreadSafeSetDwtStatus(reinterpret_cast<void *>(curJob), tileId, compId, subbandId, INVALID, INVALID, DWT_STATUS_DWT_DONE, &subbandId);

					if (subbandId != INVALID)
					{
						band0 = nextStepBuf;
						codeBlockStripeId = 0;
						BMI_ASSERT(subbandId % 3 == 1);
						low_width = curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId + 1].mSize.x;
						high_width = curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mSize.x;
						lowHeight = curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId].mSize.y;
						hiHeight  = curJob->mPicture->mTileinfo[tileId].mCompInfo[compId].mSubbandInfo[subbandId + 2].mSize.y;
						if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)
						{
							hiGain = curComp->mSubbandInfo[subbandId].mfAbsStepNorm * IDWT_HI_GAIN * IDWT_LO_GAIN;		// all here are for low_band in VDWT
						}

						while(1)
						{
							nextStepBuf = curJob->mDwtStatus.UnfinishedHDWT(reinterpret_cast<void *>(curJob), tileId, compId, subbandId, linesOff, linesinserted);
							if (!nextStepBuf)
							{
								break;
							}
							//linesinserted = linesOff + curJob->mPicture->mProperties.mCodeBlockSize.y > lowHeight ? (lowHeight - linesOff) : curJob->mPicture->mProperties.mCodeBlockSize.y;
							//****UUUU this use of mCodeBlockSize may not be correct
							BMI_ASSERT(linesinserted >= 0);
							if (curJob->mPicture->mProperties.mMethod == Ckernels_W9X7)
							{
#if INTEL_ASSEMBLY_32 || AT_T_ASSEMBLY
								HDWT_R97_FLOAT_ASM(
									threadId,
#else
								HDWT_R97(
#endif								
									reinterpret_cast<float*>(tmpBuf +  linesOff * ALIGN_TO_SSE_DWORDS(low_width + high_width) * 4),
									reinterpret_cast<float*>(band0 + linesOff * ALIGN_TO_SSE_DWORDS(low_width) * 4),
									reinterpret_cast<float *>(nextStepBuf),
									low_width,
									high_width,
									linesinserted,
									IDWT_LO_GAIN * IDWT_LO_GAIN,
									hiGain,
									curJob->mPicture->mTileinfo[tileId].mXParity[subbandId ? (subbandId - 1) / 3 : 0],
									false);
							}
							else
							{
								if (curJob->mWordLength == 4)
								{
									HDWT_R53_INT(
										reinterpret_cast<int_32*>(tmpBuf +  linesOff * ALIGN_TO_SSE_DWORDS(low_width + high_width) * 4),
										reinterpret_cast<int_32*>(band0 + linesOff * ALIGN_TO_SSE_DWORDS(low_width) * 4),
										reinterpret_cast<int_32 *>(nextStepBuf),
										low_width,
										high_width,
										linesinserted,
										curJob->mPicture->mTileinfo[tileId].mXParity[subbandId ? (subbandId - 1) / 3 : 0],
										false);								
								}
								else
								{
									HDWT_R53_SHORT(
										reinterpret_cast<int_16*>(tmpBuf +  linesOff * ALIGN_TO_SSE_DWORDS(low_width + high_width) * 2),
										reinterpret_cast<int_16*>(band0 + linesOff * ALIGN_TO_SSE_DWORDS(low_width) * 2),
										reinterpret_cast<int_16 *>(nextStepBuf),
										low_width,
										high_width,
										linesinserted,
										curJob->mPicture->mTileinfo[tileId].mXParity[subbandId ? (subbandId - 1) / 3 : 0],
										false);								

								}

							}
// 							DEBUG_PRINT(("HDWT done for tile %d comp %d subband %d lines %d linesoff %d\n", tileId, compId, subbandId, linesinserted, linesOff));

							nextStepBuf = curJob->mDwtStatus.ThreadSafeSetDwtStatus(reinterpret_cast<void *>(curJob), tileId, compId, subbandId, codeBlockStripeId, INVALID, DWT_STATUS_READY_FOR_VDWT);
							if (nextStepBuf)
							{
								// next resolution
// 								subbandId += 3;
								break;
							}

						}

					}			
					else // subbandID == INVALID
					{
						// finished the top level DWT
						LOCK_MUTEX(mDwtCounterMutex);
						++curJob->mPicture->mTileinfo[tileId].mComFinishedCounter;
						if (curJob->mPicture->mTileinfo[tileId].mComFinishedCounter == curJob->mPicture->mProperties.mCalculatedComponentNum)
						{
							// all components done
							curJob->mPicture->mTileinfo[tileId].mComFinishedCounter = 0;
							nextStep = tileId;
						}
						RELEASE_MUTEX(mDwtCounterMutex);
						nextStepBuf = NULL;

						if (curJob->mJobConfig->mOutputFormat < IMAGE_FOTMAT_NONE)
						{
							// all components decoded, need to do something if we want to generate a output picture

							switch (curJob->mJobConfig->mOutputFormat)
							{
							case IMAGE_FOTMAT_32BITS_ARGB:
							case IMAGE_FOTMAT_48BITS_RGB:
							case IMAGE_FOTMAT_24BITS_RGB:
							case IMAGE_FOTMAT_64BITS_ARGB:
								{
									if (nextStep != INVALID)
									{
										int checkOutComp = config.mCheckOutComponent;
										if (	checkOutComp >= curJob->mPicture->mProperties.mComponentNum 
											||	checkOutComp < 0
											||	((curJob->mJobConfig->mIgnoreComponents >> checkOutComp) & 0x01))
										{
											checkOutComp = INVALID;
										}
										if (curJob->mPicture->mProperties.mComponentNum == 3 && !(curJob->mJobConfig->mIgnoreComponents && 0x07))
										{
											RMct(*curJob, tileId, checkOutComp);
										}
										else 
										{
											if (curJob->mPicture->mProperties.mCalculatedComponentNum >= 3)
											{
												// merge the first 3 component as output
												RMct(*curJob, tileId, checkOutComp);
											}
											else
											{
												// less than 3 components?  use the first calculated component as output
												int c = 0;
												while ((curJob->mJobConfig->mIgnoreComponents >> c) & 0x01)
												{
													++c;
												};
												MergeComponents(*curJob, tileId, c);
											}
										}
									}
								}
								break;
							case IMAGE_FOTMAT_48BITS_3COMPONENTS:
							case IMAGE_FOTMAT_64BITS_4COMPONENTS:
							case IMAGE_FOTMAT_32BITS_4COMPONENTS:
							case IMAGE_FOTMAT_24BITS_3COMPONENTS:

								{
									if (nextStep != INVALID)
									{
										int checkOutComp = config.mCheckOutComponent;
										if (	checkOutComp >= curJob->mPicture->mProperties.mComponentNum 
											||	checkOutComp < 0
											||	((curJob->mJobConfig->mIgnoreComponents >> checkOutComp) & 0x01))
										{
											checkOutComp = INVALID;
										}

										MergeComponents(*curJob, tileId, checkOutComp);
									}
								}
								break;

							case	IMAGE_FOTMAT_36BITS_RGB:
							case	IMAGE_FOTMAT_48BITS_ARGB:
							default:
#pragma	BMI_PRAGMA_TODO_MSG("for supporting more picture format")
								BMI_ASSERT(0); // todo for supporting more format
							}
						}
					}
				}
			}
			else
			{
				BMI_ASSERT(0);
#pragma	BMI_PRAGMA_TODO_MSG("Using short interger for DWT transfer")

				// to do
			}
		}
		return;
	}

	struct cpuDwtComponentFlag
	{
		int		mHandler;
		short	mTileId;
		short	mType;
	};
	enum	cpuDwtComponentFlagContent
	{
		CONTENT_INT,
		CONTENT_FLOAT
	};

	float * CpuDwtDecoder::GetComponentFloat( CodeStreamHandler handler,unsigned int & sizeofBuf, int outputIndex, int componentId, int tileId)
	{
		float * retBuf = NULL;
		sizeofBuf = 0;

		BMI_ASSERT( mDwtJobs[outputIndex].mPicture->mProperties.mMethod == Ckernels_W9X7);
		{
			int tileNum = mDwtJobs[outputIndex].mPicture->mProperties.mTileNum.x * mDwtJobs[outputIndex].mPicture->mProperties.mTileNum.y;
			if (	tileId <  tileNum && 
				tileId >= INVALID && 
				outputIndex < mEngineConfig->mOutputQueueLen && 
				componentId < mDwtJobs[outputIndex].mPicture->mProperties.mComponentNum &&
				!((mDwtJobs[outputIndex].mJobConfig->mIgnoreComponents >> componentId) & 0x01))
			{
				UINT64 iflag = mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetFlag();
				cpuDwtComponentFlag * flag = reinterpret_cast<cpuDwtComponentFlag *>(&iflag);
				// in component buffer's flag, we use the flag top 32 bits for the pic handler and the lower 32 bits, 16bits for tile ID, and left 16 bits to indicate it stored int( 0 ) or float ( 1 )

				if (tileId == INVALID)
				{
					sizeofBuf = mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.y * sizeof(float);
					if (tileNum == 1)
					{
						if (mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x*4 != ALIGN_TO_SSE_DWORDS(mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x*4))
						{
							if ( flag->mHandler != handler || (flag->mTileId != 0x0f && flag->mTileId != 0 ) || flag->mType != CONTENT_FLOAT)
							{
								mDwtJobs[outputIndex].mDecodedCompBuf[componentId].InitBuf(sizeofBuf);
							}
							retBuf = (float *)mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetBuf();
							
							memcpy2D(mDwtJobs[outputIndex].mDwtBuf[0].GetImgBuf(componentId),
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * 4,
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.y,
								ALIGN_TO_SSE_DWORDS(mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * 4),
								retBuf,
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * 4,
								0,
								0
								);

							flag->mHandler = handler;
							flag->mTileId = tileId;
							flag->mType = CONTENT_FLOAT;
							mDwtJobs[outputIndex].mDecodedCompBuf[componentId].SetFlag(iflag);

						}
						else
						{
							retBuf = (float *)mDwtJobs[outputIndex].mDwtBuf[0].GetImgBuf(componentId);
						}
					}
					else
					{

						if ( flag->mHandler != handler || flag->mTileId != 0x0f  || flag->mType != CONTENT_FLOAT)
						{
							mDwtJobs[outputIndex].mDecodedCompBuf[componentId].InitBuf(sizeofBuf);
						}
						retBuf = (float *)mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetBuf();
						int xOff = 0, yOff = 0;
						for( int i = 0; i < tileNum; ++i)
						{
							int srcWidth = mDwtJobs[outputIndex].mPicture->mTileinfo[i].mSize.x;
							int srcHigh  = mDwtJobs[outputIndex].mPicture->mTileinfo[i].mSize.y;
							int dwtLevel = mDwtJobs[outputIndex].mPicture->mProperties.mDwtLevel;
							unsigned char * srcBuf = mDwtJobs[outputIndex].mDwtBuf[i].GetImgBuf(componentId);
							if (mDwtJobs[outputIndex].mJobConfig->mDwtDecodeTopLevel >= 0)
							{
								while (mDwtJobs[outputIndex].mJobConfig->mDwtDecodeTopLevel < dwtLevel)
								{
									--dwtLevel;
									srcWidth = (srcWidth + 1)>>1;
									srcHigh = (srcHigh + 1)>>1;
								}
							}

							memcpy2D(	srcBuf,
								srcWidth * mDwtJobs[outputIndex].mWordLength,
								srcHigh,
								ALIGN_TO_SSE_DWORDS(srcWidth * mDwtJobs[outputIndex].mWordLength),
								retBuf,
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * mDwtJobs[outputIndex].mWordLength,
								xOff * mDwtJobs[outputIndex].mWordLength,
								yOff);	

							xOff += srcWidth;

							if ( i < tileNum - 1 && mDwtJobs[outputIndex].mPicture->mTileinfo[i+1].mOff.x == 0)
							{
								// new line
								xOff = 0;
								yOff += srcHigh;
							}
						}
						flag->mHandler = handler;
						flag->mTileId = tileId;
						flag->mType = CONTENT_FLOAT;
						mDwtJobs[outputIndex].mDecodedCompBuf[componentId].SetFlag(iflag);

					}
				}
				else
				{

					int srcWidth = mDwtJobs[outputIndex].mPicture->mTileinfo[tileId].mSize.x;
					int srcHigh  = mDwtJobs[outputIndex].mPicture->mTileinfo[tileId].mSize.y;
					int dwtLevel = mDwtJobs[outputIndex].mPicture->mProperties.mDwtLevel;
					unsigned char * srcBuf = mDwtJobs[outputIndex].mDwtBuf[tileId].GetImgBuf(componentId);
					if (mDwtJobs[outputIndex].mJobConfig->mDwtDecodeTopLevel >= 0)
					{
						while (mDwtJobs[outputIndex].mJobConfig->mDwtDecodeTopLevel < dwtLevel)
						{
							--dwtLevel;
							srcWidth = (srcWidth + 1)>>1;
							srcHigh = (srcHigh + 1)>>1;
						}
					}

					sizeofBuf = srcWidth * srcHigh * sizeof(float);

					if (srcWidth*4 != ALIGN_TO_SSE_DWORDS(srcWidth*4))
					{
						if ( flag->mHandler != handler || flag->mTileId != tileId  || flag->mType != CONTENT_FLOAT)
						{
							mDwtJobs[outputIndex].mDecodedCompBuf[componentId].InitBuf(sizeofBuf);
						}
						retBuf = (float *)mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetBuf();

						memcpy2D(srcBuf,
							srcWidth * 4,
							srcHigh,
							ALIGN_TO_SSE_DWORDS(srcWidth * 4),
							retBuf,
							srcWidth * 4,
							0,
							0
							);
						flag->mHandler = handler;
						flag->mTileId = tileId;
						flag->mType = CONTENT_FLOAT;
						mDwtJobs[outputIndex].mDecodedCompBuf[componentId].SetFlag(iflag);


					}
					else
					{
						retBuf = (float *)srcBuf;
					}
				}

			}

		}

		return retBuf;
	}

	int  * CpuDwtDecoder::GetComponentInt( CodeStreamHandler handler,unsigned int & sizeofBuf, int outputIndex, int componentId, int tileId)
	{
		// return the result buffer

		int * retBuf = NULL;
		sizeofBuf = 0;

		BMI_ASSERT( mDwtJobs[outputIndex].mPicture->mProperties.mMethod == Ckernels_W5X3)
		{
			int tileNum = mDwtJobs[outputIndex].mPicture->mProperties.mTileNum.x * mDwtJobs[outputIndex].mPicture->mProperties.mTileNum.y;
			if (	tileId <  tileNum && 
				tileId >= INVALID && 
				outputIndex < mEngineConfig->mOutputQueueLen && 
				componentId < mDwtJobs[outputIndex].mPicture->mProperties.mComponentNum &&
				!((mDwtJobs[outputIndex].mJobConfig->mIgnoreComponents >> componentId) & 0x01))
			{
				UINT64 iflag = mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetFlag();
				cpuDwtComponentFlag * flag = reinterpret_cast<cpuDwtComponentFlag *>(&iflag);
				// in component buffer's flag, we use the flag top 32 bits for the pic handler and the lower 32 bits, 16bits for tile ID, and left 16 bits to indicate it stored int( 0 ) or float ( 1 )

				if (tileId == INVALID)
				{
					sizeofBuf = mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.y * sizeof(int);
					if (tileNum == 1)
					{
						if (mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x*4 != ALIGN_TO_SSE_DWORDS(mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x*4))
						{
							if ( flag->mHandler != handler || (flag->mTileId != 0x0f && flag->mTileId != 0 ) || flag->mType != CONTENT_INT)
							{
								mDwtJobs[outputIndex].mDecodedCompBuf[componentId].InitBuf(sizeofBuf);
							}
							retBuf = (int *)mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetBuf();

							memcpy2D(mDwtJobs[outputIndex].mDwtBuf[0].GetImgBuf(componentId),
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * 4,
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.y,
								ALIGN_TO_SSE_DWORDS(mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * 4),
								retBuf,
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * 4,
								0,
								0
								);

							flag->mHandler = handler;
							flag->mTileId = tileId;
							flag->mType = CONTENT_INT;
							mDwtJobs[outputIndex].mDecodedCompBuf[componentId].SetFlag(iflag);

						}
						else
						{
							retBuf = (int *)mDwtJobs[outputIndex].mDwtBuf[0].GetImgBuf(componentId);			
						}
					}
					else
					{

						if ( flag->mHandler != handler || flag->mTileId != 0x0f  || flag->mType != CONTENT_INT)
						{
							mDwtJobs[outputIndex].mDecodedCompBuf[componentId].InitBuf(sizeofBuf);
						}
						retBuf = (int *)mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetBuf();
						int xOff = 0, yOff = 0;
						for( int i = 0; i < tileNum; ++i)
						{
							int srcWidth = mDwtJobs[outputIndex].mPicture->mTileinfo[i].mSize.x;
							int srcHigh = mDwtJobs[outputIndex].mPicture->mTileinfo[i].mSize.y;
							unsigned char * srcBuf = mDwtJobs[outputIndex].mDwtBuf[i].GetImgBuf(componentId);

							memcpy2D(	srcBuf,
								srcWidth * mDwtJobs[outputIndex].mWordLength,
								srcHigh,
								ALIGN_TO_SSE_DWORDS(srcWidth * mDwtJobs[outputIndex].mWordLength),
								retBuf,
								mDwtJobs[outputIndex].mPicture->mProperties.mOutputSize.x * mDwtJobs[outputIndex].mWordLength,
								xOff * mDwtJobs[outputIndex].mWordLength,
								yOff);	

							xOff += srcWidth;

							if ( i < tileNum - 1 && mDwtJobs[outputIndex].mPicture->mTileinfo[i+1].mOff.x == 0)
							{
								// new line
								xOff = 0;
								yOff += srcHigh;
							}
						}
						flag->mHandler = handler;
						flag->mTileId = tileId;
						flag->mType = CONTENT_INT;
						mDwtJobs[outputIndex].mDecodedCompBuf[componentId].SetFlag(iflag);

					}
				}
				else
				{
					int srcWidth = mDwtJobs[outputIndex].mPicture->mTileinfo[tileId].mSize.x;
					int srcHigh  = mDwtJobs[outputIndex].mPicture->mTileinfo[tileId].mSize.y;
					int dwtLevel = mDwtJobs[outputIndex].mPicture->mProperties.mDwtLevel;
					unsigned char * srcBuf = mDwtJobs[outputIndex].mDwtBuf[tileId].GetImgBuf(componentId);
					if (mDwtJobs[outputIndex].mJobConfig->mDwtDecodeTopLevel >= 0)
					{
						while (mDwtJobs[outputIndex].mJobConfig->mDwtDecodeTopLevel < dwtLevel)
						{
							--dwtLevel;
							srcWidth = (srcWidth + 1)>>1;
							srcHigh = (srcHigh + 1)>>1;
						}
					}

					sizeofBuf = srcWidth * srcHigh * sizeof(int);

					if (srcWidth*4 != ALIGN_TO_SSE_DWORDS(srcWidth*4))
					{
						if ( flag->mHandler != handler || flag->mTileId != tileId  || flag->mType != CONTENT_INT)
						{
							mDwtJobs[outputIndex].mDecodedCompBuf[componentId].InitBuf(sizeofBuf);
						}
						retBuf = (int *)mDwtJobs[outputIndex].mDecodedCompBuf[componentId].GetBuf();

						memcpy2D(srcBuf,
							srcWidth * 4,
							srcHigh,
							ALIGN_TO_SSE_DWORDS(srcWidth * 4),
							retBuf,
							srcWidth * 4,
							0,
							0
							);
						flag->mHandler = handler;
						flag->mTileId = tileId;
						flag->mType = CONTENT_INT;
						mDwtJobs[outputIndex].mDecodedCompBuf[componentId].SetFlag(iflag);

					}
					else
					{
						retBuf = (int *)srcBuf;
					}
				}

			}
		}

		return retBuf;
	}

	void  * CpuDwtDecoder::GetResultBuf(int outputIndex,unsigned int &bufSize)
	{
		// return the result buffer
		bufSize = mDwtJobs[outputIndex].mResultbuf.GetBufSize();
		return mDwtJobs[outputIndex].mResultbuf.GetBuf();
	}

	const void * CpuDwtDecoder::GetThumbnail(int outputIndex, int &bufSize)
	{
		bufSize = mDwtJobs[outputIndex].mThumbnailBuf.GetBufSize();
		return mDwtJobs[outputIndex].mThumbnailBuf.GetBuf();
	}


	int	CpuDwtDecoder::DwtUnfinished()
	{
#if VDWT_MULTI_THREADS
		return mUndoDwtJobsNum;
#else
		return 0;
#endif

	}

	const DecoderConcrete::P_DecodeDWTJob		 CpuDwtDecoder::GetUnfinishedDwtJobs(int tileId, int &pictureHandler, int &pictureIndex)
	{

#if VDWT_MULTI_THREADS
		DecoderConcrete::P_DecodeDWTJob ret = NULL;

		if (mUndoDwtJobsNum)
		{
			ret = &mUndoDwtJobs[mUndoDwtJobsCurrentIndex];

			if (tileId == INVALID || ret->mTileid == tileId)
			{
				--mUndoDwtJobsNum;
				++mUndoDwtJobsCurrentIndex;

			}
			else
			{
				ret = NULL;
			}
		}
		else
		{
			mUndoDwtJobsCurrentIndex = INVALID;
		}

		pictureHandler = mCurrentUndoHandler;
		pictureIndex = mCurrentUndoIndex;

		return ret;
#else
		return NULL;
#endif
	}


	void	CpuDwtDecoder::RMct(DwtJob & job, int tileId, int checkOutComp)
	{

		BMI_ASSERT(job.mJobConfig->mOutputFormat < IMAGE_FOTMAT_NONE && job.mResultbuf.GetBuf());
		BMI_ASSERT(job.mPicture->mProperties.mCalculatedComponentNum >= 3);

		int c = 0;
		while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
		unsigned char * redBuf = job.mDwtBuf[tileId].GetImgBuf(c - 1); 
		while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
		unsigned char * greenBuf = job.mDwtBuf[tileId].GetImgBuf(c - 1); 
		while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
		unsigned char * blueBuf = job.mDwtBuf[tileId].GetImgBuf(c - 1); 
		unsigned char * resultBuf = reinterpret_cast<unsigned char *>(job.mResultbuf.GetBuf());

		int sizeOfPixel;
		switch (job.mJobConfig->mOutputFormat )
		{
			case IMAGE_FOTMAT_32BITS_ARGB:
				sizeOfPixel = sizeof(ARGB_32BITS);
				break;
			case IMAGE_FOTMAT_48BITS_RGB:
				sizeOfPixel = sizeof(RGB_48BITS);
				break;
			case IMAGE_FOTMAT_64BITS_ARGB:
				sizeOfPixel = sizeof(ARGB_64BITS);
				break;
			case IMAGE_FOTMAT_24BITS_RGB:
				sizeOfPixel = sizeof(RGB_24BITS);
				break;
			case IMAGE_FOTMAT_36BITS_RGB:
			case IMAGE_FOTMAT_48BITS_ARGB:
	#pragma BMI_PRAGMA_TODO_MSG("Support more output format")
			default:
				BMI_ASSERT(0);
				break;
		}

		int pitch = job.mPicture->mProperties.mOutputSize.x;
		int xoff = job.mPicture->mTileinfo[tileId].mOff.x;
		int yoff = job.mPicture->mTileinfo[tileId].mOff.y;
		int dwtLevel = job.mPicture->mProperties.mDwtLevel;
		int width = job.mPicture->mTileinfo[tileId].mSize.x;
		int height = job.mPicture->mTileinfo[tileId].mSize.y;

		while (dwtLevel-- > job.mDwtDecodingTopLevel )
		{
			xoff = (xoff + 1) >> 1;
			yoff = (yoff + 1) >> 1;
			width = (width + 1) >> 1;
			height = (height + 1) >> 1;
		}
		resultBuf += (xoff + yoff * pitch) * sizeOfPixel;


		if (job.mPicture->mProperties.mMethod == Ckernels_W9X7)
		{
			if (job.mWordLength == USHORT_16_BITS)
			{
				BMI_ASSERT(0);
#pragma	BMI_PRAGMA_TODO_MSG("doing RCT in short interger")

// 				MCT_R97_SHORT16(resultBuf,
// 					reinterpret_cast<int_16 *>(redBuf),
// 					reinterpret_cast<int_16 *>(greenBuf),
// 					reinterpret_cast<int_16 *>(blueBuf),
// 					pitch, width, height, job.mPicture->mProperties.mBitDepth, checkOutComp);


			}
			else if (job.mWordLength == UINT_32_BITS)
			{

#if INTEL_ASSEMBLY_32
				RMCT_R97_FLOAT_ASM(
#else 
				RICT_97_FLOAT(
#endif
					resultBuf,
					reinterpret_cast<float *>(redBuf),
					reinterpret_cast<float *>(greenBuf),
					reinterpret_cast<float *>(blueBuf),
					pitch, width, height, job.mPicture->mProperties.mBitDepth, 
					job.mJobConfig->mOutputFormat,
					checkOutComp);
			}

		}
		else
		{
			BMI_ASSERT(job.mPicture->mProperties.mMethod == Ckernels_W5X3);

			if (job.mWordLength == USHORT_16_BITS)
			{
				RRct_53<int_16>(
					reinterpret_cast<void *>(resultBuf),
					reinterpret_cast<int_16*>(redBuf), 
					reinterpret_cast<int_16*>(greenBuf), 
					reinterpret_cast<int_16*>(blueBuf), 
					pitch, 
					width, 
					height,
					job.mPicture->mProperties.mBitDepth,
					job.mJobConfig->mOutputFormat,
					checkOutComp);		

			}
			else if (job.mWordLength == UINT_32_BITS)
			{
#if INTEL_ASSEMBLY_32
				RMCT_R53_INT32_ASM(
#else
				RRct_53<int_32>(
#endif 
					reinterpret_cast<void *>(resultBuf),
					reinterpret_cast<int_32*>(redBuf), 
					reinterpret_cast<int_32*>(greenBuf), 
					reinterpret_cast<int_32*>(blueBuf), 
					pitch, 
					width, 
					height,
					job.mPicture->mProperties.mBitDepth,
					job.mJobConfig->mOutputFormat,
					checkOutComp);		

			}

		}


	}

	void	CpuDwtDecoder::HDWT_R97(float * result, float  * in_pixel_low, float  * in_pixel_high, int low_w, int high_w, int lines,float lowGain, float highGain,  uint_8 xParity,bool isLowBandNeedTransfer)
	{


#define FETCHPIXEL_INT(x)	((*(int_32 *)(x) & 0x80000000) ? ((*((int_32 *)(x)++)-1)^INT_32_MAX) : (*((int_32 *)(x)++) & INT_32_MAX))
#define FETCHPIXEL_FLOAT(x)	(*(x++))
#define FETCHPIXEL_HI(x)	FETCHPIXEL_INT(x)
#define FETCHPIXEL_LO(x)	(isLowBandNeedTransfer ? FETCHPIXEL_INT(x) : FETCHPIXEL_FLOAT(x))

		int width  , i;
		float * target =result;

		for (i = 0; i < lines; ++i)
		{
			width = low_w + high_w;
			if (width < 6)	
			{

				if (width == 1)
				{
					target[0] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
				}
				else
				{
					if (xParity)
					{
#pragma BMI_PRAGMA_TODO_MSG("Deal with HDWT when Xparitya nd less than 6 cols")
					}
					else
					{
						bool odd = (low_w != high_w);
						for (int i = 0; i < width; ++i)
						{
							target[i] = (i & 0x01) ? FETCHPIXEL_HI(in_pixel_high) * highGain : FETCHPIXEL_LO(in_pixel_low) * lowGain;
						}

						// step 3
						target[0] = target[0] - IDWT_DELTA * (target[1]	+ target[1]);
						for (int i = 2; i < width - 1; i+=2)
						{
							target[i] = target[i] - IDWT_DELTA * (target[i-1]	+ target[i+1]);
						}
						if (odd)
						{
							target[width - 1] = target[width - 1] - IDWT_DELTA * (target[width-2]	+ target[width - 2]);
						}
						// step 4
						for (int i = 1; i < width - 1; i+=2)
						{
							target[i] = target[i] - IDWT_GAMMA * (target[i-1] + target[i+1]);
						}
						if (!odd)
						{
							target[width - 1] = target[width - 1] - IDWT_GAMMA * (target[width-2]	+ target[width - 2]);
						}
						// step 5
						target[0] = target[0] - IDWT_BETA * (target[1]	+ target[1]);
						for (int i = 2; i < width - 1; i+=2)
						{
							target[i] = target[i] - IDWT_BETA * (target[i-1]	+ target[i+1]);
						}
						if (odd)
						{
							target[width - 1] = target[width - 1] - IDWT_BETA * (target[width-2]	+ target[width - 2]);
						}
						// step 6
						for (int i = 1; i < width - 1; i+=2)
						{
							target[i] = target[i] - IDWT_ALPHA * (target[i-1] + target[i+1]);
						}
						if (!odd)
						{
							target[width - 1] = target[width - 1] - IDWT_ALPHA * (target[width-2]	+ target[width - 2]);
						}

					}

				}
			}
			else
			{
				if (!xParity)
				{
					width -= 6;
					// step 1 and 2
					target[0] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
					target[1] = FETCHPIXEL_HI(in_pixel_high) * highGain;
					target[2] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
					target[3] = FETCHPIXEL_HI(in_pixel_high) * highGain;
					target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
					target[5] = FETCHPIXEL_HI(in_pixel_high) * highGain;
					// step 3
					target[0] = target[0] - IDWT_DELTA * (target[1]	+ target[1]);
					target[2] = target[2] - IDWT_DELTA * (target[1]	+ target[3]);
					target[4] = target[4] - IDWT_DELTA * (target[3] + target[5]);
					// setp 4
					target[1] = target[1] - IDWT_GAMMA * (target[0] + target[2]);
					target[3] = target[3] - IDWT_GAMMA * (target[2] + target[4]);
					// step 5
					target[0] = (target[0] - IDWT_BETA * (target[1]	+ target[1]));
					target[2] = (target[2] - IDWT_BETA * (target[1]	+ target[3]));
					// step 6
					target[1] = (target[1] - IDWT_ALPHA * (target[0] + target[2]));

				}
				else
				{
					width -= 5;
					--target;
					// step 1 and 2
					target[1] = FETCHPIXEL_HI(in_pixel_high) * highGain;
					target[2] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
					target[3] = FETCHPIXEL_HI(in_pixel_high) * highGain;
					target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
					target[5] = FETCHPIXEL_HI(in_pixel_high) * highGain;
					// step 3
					target[2] = target[2] - IDWT_DELTA * (target[1]	+ target[3]);
					target[4] = target[4] - IDWT_DELTA * (target[3] + target[5]);
					// setp 4
					target[1] = target[1] - IDWT_GAMMA * (target[2] + target[2]);
					target[3] = target[3] - IDWT_GAMMA * (target[2] + target[4]);
					// step 5
					target[2] = (target[2] - IDWT_BETA * (target[1]	+ target[3]));
					// step 6
					target[1] = (target[1] - IDWT_ALPHA * (target[2] + target[2]));

				}

				// now 0/1/2 are the final data

				while (width > 0)
				{

					target += 2;
					if (width >= 2)
					{


						// setp 1
						target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
						// setp 2
						target[5] = FETCHPIXEL_HI(in_pixel_high) * highGain;
						// setp 3
						target[4] = target[4] - IDWT_DELTA * (target[3] + target[5]);
					}
					else
					{
						// width == 1;
						// 						target += 2;

						// setp 1
						target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
						// step 2

						// setp 3
						target[4] = target[4] - IDWT_DELTA * (target[3] + target[3]);

					}
					// step 4
					target[3] = target[3] - IDWT_GAMMA * (target[2] + target[4]);
					// setp 5
					target[2] = (target[2] - IDWT_BETA * (target[1]	+ target[3]));
					// setp 6 
					target[1] = target[1] - IDWT_ALPHA * (target[0] + target[2]);	
					// two more pixel 1 & 2 done

					width -= 2;	// if width was 1, now become -1

				}

				// now there are 2 + width pixel left in the window

				if (width)
				{
					BMI_ASSERT(!xParity);
					// width == -1
					// step 5
					target[4] = target[4] - IDWT_BETA * (target[3] + target[3]);
					// step 6
					target[3] = target[3] - IDWT_ALPHA * (target[2] + target[4]);	
					// two more pixel 3 & 4 done

					target += 5;
				}
				else
				{
					// width = 0
					// setp 4
					target[5] = target[5] - IDWT_GAMMA * (target[4] + target[4]);	//mirror previous pixel 
					// step 5
					target[4] = target[4] - IDWT_BETA * (target[3]	+ target[5]) ;
					//step 6
					target[3] = target[3] - IDWT_ALPHA * (target[2] + target[4]);		
					// step 6
					target[5] = target[5] - IDWT_ALPHA * (target[4] + target[4]);			//mirror previous pixel
					target += 6;

				}
			}
		}	


	}

// 	void	CpuDwtDecoder::HDWT_R97_INT32(int_32 * result, int_32  * in_pixel_low, int_32  * in_pixel_high, int low_w, int high_w, int lines, float  lowGain, float highGain, bool isLowBandNeedTransfer)
// 	{
// 
// 
// #define FETCHPIXEL_INT(x)	((*(x) & 0x80000000) ? ((*((x)++)-1)^INT_32_MAX) : (*((x)++) & INT_32_MAX))
// #define FETCHPIXEL_FLOAT(x)	(*((float *)(x)++))
// #define FETCHPIXEL_HI(x)	FETCHPIXEL_INT(x)
// #define FETCHPIXEL_LO(x)	(isLowBandNeedTransfer ? FETCHPIXEL_INT(x) : FETCHPIXEL_FLOAT(x))
// 
// 		int width  , i;
// 		float * target = reinterpret_cast<float *>(result);
// 
// 		for (i = 0; i < lines; ++i)
// 		{
// 			width = low_w + high_w;
// 			if (width < 6)	
// 			{
// 				// to do
// 			}
// 			else
// 			{
// 				width -= 6;
// 				// step 1 and 2
// 				target[0] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
// 				target[1] = FETCHPIXEL_HI(in_pixel_high) * highGain;
// 				target[2] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
// 				target[3] = FETCHPIXEL_HI(in_pixel_high) * highGain;
// 				target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
// 				target[5] = FETCHPIXEL_HI(in_pixel_high) * highGain;
// 				// step 3
// 				target[0] = target[0] - IDWT_DELTA * (target[1]	+ target[1]);
// 				target[2] = target[2] - IDWT_DELTA * (target[1]	+ target[3]);
// 				target[4] = target[4] - IDWT_DELTA * (target[3] + target[5]);
// 				// setp 4
// 				target[1] = target[1] - IDWT_GAMMA * (target[0] + target[2]);
// 				target[3] = target[3] - IDWT_GAMMA * (target[2] + target[4]);
// 				// step 5
// 				target[0] = (target[0] - IDWT_BETA * (target[1]	+ target[1]));
// 				target[2] = (target[2] - IDWT_BETA * (target[1]	+ target[3]));
// 				// step 6
// 				target[1] = (target[1] - IDWT_ALPHA * (target[0] + target[2]));
// 
// 				// now 0/1/2 are the final data
// 
// 				while (width > 0)
// 				{
// 
// 					target += 2;
// 					if (width >= 2)
// 					{
// 
// 
// 						// setp 1
// 						target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
// 						// setp 2
// 						target[5] = FETCHPIXEL_HI(in_pixel_high) * highGain;
// 						// setp 3
// 						target[4] = target[4] - IDWT_DELTA * (target[3] + target[5]);
// 					}
// 					else
// 					{
// 						// width == 1;
// // 						target += 2;
// 
// 						// setp 1
// 						target[4] = FETCHPIXEL_LO(in_pixel_low) * lowGain;
// 						// step 2
// 
// 						// setp 3
// 						target[4] = target[4] - IDWT_DELTA * (target[3] + target[3]);
// 
// 					}
// 					// step 4
// 					target[3] = target[3] - IDWT_GAMMA * (target[2] + target[4]);
// 					// setp 5
// 					target[2] = (target[2] - IDWT_BETA * (target[1]	+ target[3]));
// 					// setp 6 
// 					target[1] = target[1] - IDWT_ALPHA * (target[0] + target[2]);	
// 					// two more pixel 1 & 2 done
// 
// 					width -= 2;	// if width was 1, now become -1
// 
// 				}
// 
// 				// now there are 2 + width pixel left in the window
// 
// 				if (width)
// 				{
// 					// width == -1
// 					// step 5
// 					target[4] = target[4] - IDWT_BETA * (target[3] + target[3]);
// 					// step 6
// 					target[3] = target[3] - IDWT_ALPHA * (target[2] + target[4]);	
// 					// two more pixel 3 & 4 done
// 
// 					target += 5;
// 				}
// 				else
// 				{
// 					// width = 0
// 				// setp 4
// 					target[5] = target[5] - IDWT_GAMMA * (target[4] + target[4]);	//mirror previous pixel 
// 					// step 5
// 					target[4] = target[4] - IDWT_BETA * (target[3]	+ target[5]) ;
// 					//step 6
// 					target[3] = target[3] - IDWT_ALPHA * (target[2] + target[4]);		
// 					// step 6
// 					target[5] = target[5] - IDWT_ALPHA * (target[4] + target[4]);			//mirror previous pixel
// 					target += 6;
// 
// 				}
// 			}
// 		}	
// 	}

// 	void	CpuDwtDecoder::HDWT_R53(unsigned char * target, unsigned char  * lowBand, unsigned char  * highBand, int low_w, int high_w, int lines, int wordLen)
// 	{
// 		Sleep(1);
// 
// 		return;
// 	}

	void	CpuDwtDecoder::HDWT_R53_INT(int * target, int  * in_pixel_low, int  * in_pixel_high, int low_w, int high_w, int lines, uint_8 xParity, bool isLowBandNeedTransfer)
	{

		int width  , i;
		for (i = 0; i < lines; ++i)
		{
			width = low_w + high_w;
			if (width < 2)	
			{
				// only one pixel ? do nothing
				target[0] = *(in_pixel_low);
			}
			else
			{
				width -= 2;
				target[0] = *(in_pixel_low++) - ((*in_pixel_high + 1)>>1);

				while (width > 0)
				{

					if (width >= 2)
					{


						target[2] = *(in_pixel_low++) - (((*in_pixel_high) + *(in_pixel_high + 1) + 2)>>2);
						target[1] = (*in_pixel_high++) + ((target[0] + target[2])>>1);
					}
					else
					{
						// width == 1;
						target[2] = *(in_pixel_low++) - (((*in_pixel_high) + 1)>>1);
						target[1] = (*in_pixel_high++) + ((target[0] + target[2])>>1);
					}

					target += 2;
					width -= 2;
				}

				if (!width)
				{
					target[1] = (*in_pixel_high++) + target[0];
					target += 2;
				}
				else
				{
					++target;
				}

			}
		}	
	}

	void	CpuDwtDecoder::HDWT_R53_SHORT(short	 * target, short  * in_pixel_low, short  * in_pixel_high, int low_w, int high_w, int lines, uint_8 xParity, bool isLowBandNeedTransfer)
	{

		int width  , i;
		for (i = 0; i < lines; ++i)
		{
			width = low_w + high_w;
			if (width < 2)	
			{
				// only one pixel ? do nothing
				target[0] = *(in_pixel_low);
			}
			else
			{
				width -= 2;
				target[0] = *(in_pixel_low++) - ((*in_pixel_high + 1)>>1);

				while (width > 0)
				{

					if (width >= 2)
					{


						target[2] = *(in_pixel_low++) - (((*in_pixel_high) + *(in_pixel_high + 1) + 2)>>2);
						target[1] = (*in_pixel_high++) + ((target[0] + target[2])>>1);
					}
					else
					{
						// width == 1;
						target[2] = *(in_pixel_low++) - (((*in_pixel_high) + 1)>>1);
						target[1] = (*in_pixel_high++) + ((target[0] + target[2])>>1);
					}

					target += 2;
					width -= 2;
				}

				if (!width)
				{
					target[1] = (*in_pixel_high++) + target[0];
					target += 2;
				}
				else
				{
					++target;
				}

			}
		}	
	}

// 	template<class T>	void	CpuDwtDecoder::HDWT_R53(T * target, T  * in_pixel_low, T  * in_pixel_high, int low_w, int high_w, int lines, uint_8 xParity, bool isLowBandNeedTransfer)
// 	{
// 
// 		int width  , i;
// 		for (i = 0; i < lines; ++i)
// 		{
// 			width = low_w + high_w;
// 			if (width < 2)	
// 			{
// 				// only one pixel ? do nothing
// 				target[0] = *(in_pixel_low);
// 			}
// 			else
// 			{
// 				width -= 2;
// 				target[0] = *(in_pixel_low++) - ((*in_pixel_high + 1)>>1);
// 
// 				while (width > 0)
// 				{
// 
// 					if (width >= 2)
// 					{
// 
// 
// 						target[2] = *(in_pixel_low++) - (((*in_pixel_high) + *(in_pixel_high + 1) + 2)>>2);
// 						target[1] = (*in_pixel_high++) + ((target[0] + target[2])>>1);
// 					}
// 					else
// 					{
// 						// width == 1;
// 						target[2] = *(in_pixel_low++) - (((*in_pixel_high) + 1)>>1);
// 						target[1] = (*in_pixel_high++) + ((target[0] + target[2])>>1);
// 					}
// 
// 					target += 2;
// 					width -= 2;
// 				}
// 
// 				if (!width)
// 				{
// 					target[1] = (*in_pixel_high++) + target[0];
// 					target += 2;
// 				}
// 				else
// 				{
// 					++target;
// 				}
// 
// 			}
// 		}	
// 	}


	void	CpuDwtDecoder::VDWT_R97_LINES(unsigned char  * taget, unsigned char 	*HResult_L, unsigned char  * HResult_H, unsigned char  ** overlaylines, int loopX, int decodeLine,uint_8 yParity, int threadId,  bool isStart, bool isEnd)
	{

		BMI_ASSERT(decodeLine >= 6);	
#if SSE_OPTIMIZED
		int pitch = loopX * 16;
#else
		int pitch = loopX * 4;
#endif

		unsigned char  * overlayBuf = mSubThreadDwtOverLayMemory + threadId * mSubThreadDwtOverLayMemoryUnitSize;
		unsigned char  * overlayTemp = overlayBuf + pitch;

		unsigned char  * decodingLines[6] = {NULL};
		unsigned char  * inputLine[6];


		if (!yParity)		// L H L H L H ......
		{
			if (!isStart)
			{

				decodingLines[2] = taget;
				decodingLines[3] = decodingLines[2] + pitch;
				decodingLines[4] = decodingLines[3] + pitch;
				decodingLines[5] = decodingLines[4] + pitch;

				inputLine[0] = overlaylines[1];
				inputLine[1] = overlaylines[2];
				inputLine[2] = HResult_L;
				inputLine[3] = HResult_H;
				inputLine[4] = inputLine[2] + pitch;
				inputLine[5] = inputLine[3] + pitch;

				decodeLine -= 4;

			}
			else
			{
				decodingLines[0] = taget;
				decodingLines[1] = decodingLines[0] + pitch;
				decodingLines[2] = decodingLines[1] + pitch;
				decodingLines[3] = decodingLines[2] + pitch;
				decodingLines[4] = decodingLines[3] + pitch;
				decodingLines[5] = decodingLines[4] + pitch;

				inputLine[0] = HResult_L;
				inputLine[1] = HResult_H;
				inputLine[2] = inputLine[0] + pitch;
				inputLine[3] = inputLine[1] + pitch;
				inputLine[4] = inputLine[2] + pitch;
				inputLine[5] = inputLine[3] + pitch;

				decodeLine -= 6;
			}

			// setp 3
			if (isStart)
			{
				vdwt_analyst_line(decodingLines[0], inputLine[0], inputLine[1], inputLine[1], loopX, FILL128(IDWT_DELTA_128), 100);
			}
			vdwt_analyst_line(decodingLines[2], inputLine[2], inputLine[1], inputLine[3], loopX, FILL128(IDWT_DELTA_128), 101);
			vdwt_analyst_line(decodingLines[4], inputLine[4], inputLine[3], inputLine[5], loopX, FILL128(IDWT_DELTA_128), 102);

			//step 4
			if (isStart)
			{
				vdwt_analyst_line(decodingLines[1], inputLine[1], decodingLines[0], decodingLines[2], loopX, FILL128(IDWT_GAMMA_128), 103);
			}
			vdwt_analyst_line(decodingLines[3], inputLine[3], decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128), 104);

			// step 5
			if (isStart)
			{
				vdwt_analyst_line(decodingLines[0], decodingLines[0], decodingLines[1], decodingLines[1], loopX, FILL128(IDWT_BETA_128), 105);
				vdwt_analyst_line(decodingLines[2], decodingLines[2], decodingLines[1], decodingLines[3], loopX, FILL128(IDWT_BETA_128), 106);
			}
			else
			{
				vdwt_analyst_line(overlayBuf, inputLine[0], overlaylines[0], inputLine[1], loopX, FILL128(IDWT_DELTA_128), 107);
				vdwt_analyst_line(overlayBuf, inputLine[1], overlayBuf, decodingLines[2], loopX, FILL128(IDWT_GAMMA_128), 108);
				vdwt_analyst_line(decodingLines[2], decodingLines[2], overlayBuf, decodingLines[3], loopX, FILL128(IDWT_BETA_128), 109);
			}

			// step 6
			if (isStart)
			{
				vdwt_analyst_line(decodingLines[1], decodingLines[1], decodingLines[0],decodingLines[2],loopX, FILL128(IDWT_ALPHA_128), 110);
			}


			unsigned char * input_pre = inputLine[5];
			unsigned char * input_l = inputLine[4] + pitch;
			unsigned char * input_h = inputLine[5] + pitch;

			while(decodeLine >= 2 )
			{	

				decodingLines[0] = decodingLines[2];
				decodingLines[1] = decodingLines[3];
				decodingLines[2] = decodingLines[4];
				decodingLines[3] = decodingLines[5];
				decodingLines[4] += 2 * pitch;
				decodingLines[5] += 2 * pitch;

				//step 3
				vdwt_analyst_line(decodingLines[4], input_l, input_pre, input_h, loopX, FILL128(IDWT_DELTA_128),111 );
				//step4
				vdwt_analyst_line(decodingLines[3], input_pre, decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128),112 );
				//step 5
				vdwt_analyst_line(decodingLines[2], decodingLines[2], decodingLines[1], decodingLines[3], loopX, FILL128(IDWT_BETA_128),113 );
				//step 6
				vdwt_analyst_line(decodingLines[1], decodingLines[1], decodingLines[0], decodingLines[2], loopX, FILL128(IDWT_ALPHA_128),114 );

				decodeLine -= 2;
				input_pre = input_h;
				input_l += pitch;
				input_h += pitch;
			}

			// decodeLine == 0 or 1

			if (decodeLine)	
			{
				BMI_ASSERT(isEnd);
				decodingLines[0] =  decodingLines[5] + pitch;
				//step 3
				vdwt_analyst_line(decodingLines[0], input_l, input_pre, input_pre, loopX, FILL128(IDWT_DELTA_128),115 );
				//step4
				vdwt_analyst_line(decodingLines[5], input_pre, decodingLines[0], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128),116 );
				//step 5
				vdwt_analyst_line(decodingLines[4], decodingLines[4], decodingLines[3], decodingLines[5], loopX, FILL128(IDWT_BETA_128),117 );
				//step 6
				vdwt_analyst_line(decodingLines[3], decodingLines[3], decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_ALPHA_128),118 );

				//step 5
				vdwt_analyst_line(decodingLines[0], decodingLines[0], decodingLines[5], decodingLines[5], loopX, FILL128(IDWT_BETA_128),119 );
				//step 6
				vdwt_analyst_line(decodingLines[5], decodingLines[5], decodingLines[4], decodingLines[0], loopX, FILL128(IDWT_ALPHA_128),120 );

			}
			else	
			{
				if (isEnd)
				{
					//step4
					vdwt_analyst_line(decodingLines[5], input_pre, decodingLines[4], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128),121 );
					//step 5
					vdwt_analyst_line(decodingLines[4], decodingLines[4], decodingLines[3], decodingLines[5], loopX, FILL128(IDWT_BETA_128),122 );
					//step 6
					vdwt_analyst_line(decodingLines[3], decodingLines[3], decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_ALPHA_128),123 );

					//step 6
					vdwt_analyst_line(decodingLines[5], decodingLines[5], decodingLines[4], decodingLines[4], loopX, FILL128(IDWT_ALPHA_128),124 );
				}
				else
				{
					decodingLines[0] = overlayBuf;
					//step 3
					vdwt_analyst_line(decodingLines[0], overlaylines[3], input_pre, overlaylines[4], loopX, FILL128(IDWT_DELTA_128),125 );
					//step4
					vdwt_analyst_line(decodingLines[5], input_pre, decodingLines[0], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128),126 );
					//step 5
					vdwt_analyst_line(decodingLines[4], decodingLines[4], decodingLines[3], decodingLines[5], loopX, FILL128(IDWT_BETA_128),127 );
					//step 6
					vdwt_analyst_line(decodingLines[3], decodingLines[3], decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_ALPHA_128),128 );


					//step 3
					vdwt_analyst_line(overlayTemp, overlaylines[5], overlaylines[4], overlaylines[6], loopX, FILL128(IDWT_DELTA_128),129 );
					//step4
					vdwt_analyst_line(overlayTemp, overlaylines[4], decodingLines[0], overlayTemp, loopX, FILL128(IDWT_GAMMA_128),130 );
					//step 5
					vdwt_analyst_line(overlayTemp, decodingLines[0], overlayTemp, decodingLines[5], loopX, FILL128(IDWT_BETA_128),131 );
					//step 6
					vdwt_analyst_line(decodingLines[5], decodingLines[5],  decodingLines[4], overlayTemp, loopX, FILL128(IDWT_ALPHA_128),132 );


				}
			}
		}
		else				// H L H L H L ......
		{
			unsigned char  * tmp;

			decodingLines[0] = overlayBuf;
			decodingLines[1] = taget;
			decodingLines[2] = decodingLines[1] + pitch;
			decodingLines[3] = decodingLines[2] + pitch;
			decodingLines[4] = decodingLines[3] + pitch;
			decodingLines[5] = decodingLines[4] + pitch;
			decodeLine -= 5;

			if (!isStart)
			{
				inputLine[0] = overlaylines[3];
			}
			inputLine[1] = HResult_H;
			inputLine[2] = HResult_L;
			inputLine[3] = inputLine[1] + pitch;
			inputLine[4] = inputLine[2] + pitch;
			inputLine[5] = inputLine[3] + pitch;

			// setp 3
			vdwt_analyst_line(decodingLines[2], inputLine[2], inputLine[1], inputLine[3], loopX, FILL128(IDWT_DELTA_128), 150);
			vdwt_analyst_line(decodingLines[4], inputLine[4], inputLine[3], inputLine[5], loopX, FILL128(IDWT_DELTA_128), 151);
			if (!isStart)
			{
				vdwt_analyst_line(decodingLines[0], inputLine[0], overlaylines[2], inputLine[1], loopX, FILL128(IDWT_DELTA_128), 152);
			}

			// step 4
			vdwt_analyst_line(decodingLines[3], inputLine[3], decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128), 153);
			tmp = isStart ? decodingLines[2] : decodingLines[0];
			vdwt_analyst_line(decodingLines[1], inputLine[1], tmp, decodingLines[2], loopX, FILL128(IDWT_GAMMA_128), 155);

			//step 5
			vdwt_analyst_line(decodingLines[2], decodingLines[2], decodingLines[1], decodingLines[3], loopX, FILL128(IDWT_BETA_128), 156);
			if (!isStart)
			{
				//step 3
				vdwt_analyst_line(overlayTemp, overlaylines[1], overlaylines[0], overlaylines[2], loopX, FILL128(IDWT_DELTA_128), 157);
				// step 4
				vdwt_analyst_line(overlayTemp, overlaylines[2], decodingLines[0], overlayTemp, loopX, FILL128(IDWT_GAMMA_128), 158);
				//step 5
				vdwt_analyst_line(decodingLines[0], decodingLines[0], overlayTemp, decodingLines[1], loopX, FILL128(IDWT_BETA_128), 159);
			}

			// step6
			tmp = isStart ? decodingLines[2] : decodingLines[0];
			vdwt_analyst_line(decodingLines[1], decodingLines[1], tmp, decodingLines[2], loopX, FILL128(IDWT_ALPHA_128), 160);

			unsigned char * input_pre = inputLine[5];
			unsigned char * input_l = inputLine[4] + pitch;
			unsigned char * input_h = inputLine[5] + pitch;

			while(decodeLine >= 2 )
			{	

				decodingLines[0] = decodingLines[2];
				decodingLines[1] = decodingLines[3];
				decodingLines[2] = decodingLines[4];
				decodingLines[3] = decodingLines[5];
				decodingLines[4] += 2 * pitch;
				decodingLines[5] += 2 * pitch;


				// step 3
				vdwt_analyst_line(decodingLines[4], input_l, input_pre, input_h, loopX, FILL128(IDWT_DELTA_128), 161);
				//step 4
				vdwt_analyst_line(decodingLines[3], input_pre, decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128), 162);
				//step 5
				vdwt_analyst_line(decodingLines[2], decodingLines[2], decodingLines[1], decodingLines[3], loopX, FILL128(IDWT_BETA_128), 163);
				// step 6
				vdwt_analyst_line(decodingLines[1], decodingLines[1], decodingLines[0], decodingLines[2], loopX, FILL128(IDWT_ALPHA_128), 164);

				decodeLine -= 2;
				input_pre = input_h;
				input_l += pitch;
				input_h += pitch;
			}

			// decodeLine == 0 !


			if (isEnd)
			{
				BMI_ASSERT(!decodeLine);
				//step 4
				vdwt_analyst_line(decodingLines[5], input_pre, decodingLines[4], decodingLines[4], loopX, FILL128(IDWT_GAMMA_128), 165);
				//step 5
				vdwt_analyst_line(decodingLines[4], decodingLines[4], decodingLines[3], decodingLines[5], loopX, FILL128(IDWT_BETA_128), 166);
				// step 6
				vdwt_analyst_line(decodingLines[3], decodingLines[3], decodingLines[2], decodingLines[4], loopX,FILL128(IDWT_ALPHA_128), 167);

				//step 6
				vdwt_analyst_line(decodingLines[5], decodingLines[5], decodingLines[4], decodingLines[4], loopX, FILL128(IDWT_ALPHA_128), 168);
			}
			else
			{
				BMI_ASSERT(decodeLine);
				decodingLines[0] = decodingLines[5] + pitch;
				//step 3
				vdwt_analyst_line(decodingLines[0], input_l, input_pre, overlaylines[4], loopX, FILL128(IDWT_DELTA_128), 169);
				vdwt_analyst_line(overlayBuf, overlaylines[4], input_pre, overlaylines[4], loopX, FILL128(IDWT_DELTA_128), 170);
				//step 4
				vdwt_analyst_line(decodingLines[5], input_pre, decodingLines[4], decodingLines[0], loopX, FILL128(IDWT_GAMMA_128), 171);
				vdwt_analyst_line(overlayBuf, overlaylines[4],decodingLines[0], overlayBuf, loopX, FILL128(IDWT_GAMMA_128), 172);
				//step 5
				vdwt_analyst_line(decodingLines[4], decodingLines[4], decodingLines[3], decodingLines[5], loopX, FILL128(IDWT_BETA_128), 173);
				vdwt_analyst_line(decodingLines[0], decodingLines[0], decodingLines[5], overlayBuf, loopX, FILL128(IDWT_BETA_128), 174);
				// step 6
				vdwt_analyst_line(decodingLines[3], decodingLines[3], decodingLines[2], decodingLines[4], loopX, FILL128(IDWT_ALPHA_128), 175);
				vdwt_analyst_line(decodingLines[5], decodingLines[5], decodingLines[4], decodingLines[0], loopX, FILL128(IDWT_ALPHA_128), 176);

			}
		}

	}

	void	CpuDwtDecoder::VDWT_R97(unsigned char * target, unsigned char * H_result_up, unsigned char * H_result_down, int low_h, int high_h, int width, int linesOffset, int linesDecode, uint_8 yParity, int threadId)

	{

		int lineLength = ALIGN_TO_SSE_DWORDS(width) * sizeof(float);
#if SSE_OPTIMIZED
		int loopX = ALIGN_TO_SSE_DWORDS(width) >> 2;
#else
		int loopX = width;
#endif
		bool	isStart = (linesOffset == 0);
		BMI_ASSERT(isStart || linesOffset >= 3);
		bool	isEnd	= (linesOffset + linesDecode >= low_h);
		BMI_ASSERT(isEnd || (low_h - linesOffset) >= 3);

		int outputline = linesDecode * 2;
		if (isEnd && low_h != high_h)
		{
			yParity ? ++outputline : --outputline;
		}

		unsigned char * vdwt_result = target + linesOffset * 2 * lineLength;
		unsigned char * hresult_l  = H_result_up + lineLength * linesOffset;
		unsigned char * hresult_h  = H_result_down + lineLength * linesOffset;

		if (!yParity)		// L H L H L H ......
		{
			if (isStart && isEnd && outputline < 6)
			{
				// total line less than 6
				if (outputline == 1)
				{
					// do nothing, simply copy
					memcpy(vdwt_result, H_result_up, width * 4);
				}
				else	// 2,3,4,5
				{
					bool odd = (low_h != high_h);
					unsigned char * result = vdwt_result;
					unsigned char * in_l = hresult_l;
					unsigned char * in_h = hresult_h;
					// step 3
					vdwt_analyst_line(result, in_l,in_h, in_h, loopX, FILL128(IDWT_DELTA_128), 0 );

					result += lineLength * 2;
					in_l += lineLength;

					for (int i = 2; i <= outputline - 1 ; i += 2)
					{
						vdwt_analyst_line(result, in_l,in_h, in_h + lineLength , loopX, FILL128(IDWT_DELTA_128), 1 );
						result += lineLength * 2;
						in_l += lineLength;
						in_h += lineLength;

					}
					if (odd)
					{
						vdwt_analyst_line(result, in_l,in_h, in_h, loopX, FILL128(IDWT_DELTA_128),2);
					}

					// step 4
					result = vdwt_result + lineLength;
					in_h = hresult_h;

					for (int i = 1; i < outputline - 1; i += 2)
					{
						vdwt_analyst_line(result, in_h,result - lineLength , result + lineLength, loopX, FILL128(IDWT_GAMMA_128), 3 );
						result += lineLength * 2;
						in_h += lineLength;
					}
					if (!odd)
					{
						vdwt_analyst_line(result, in_h, result - lineLength,result - lineLength, loopX, FILL128(IDWT_GAMMA_128), 4 );
					}

					// step 5
					result = vdwt_result;
					vdwt_analyst_line(result, result,  result + lineLength,  result + lineLength, loopX, FILL128(IDWT_BETA_128), 5 );
					result += lineLength * 2;

					for (int i = 2; i <= outputline - 1; i+=2)
					{
						vdwt_analyst_line(result, result,result - lineLength, result + lineLength, loopX, FILL128(IDWT_BETA_128),6 );
						result += lineLength * 2;
					}
					if (odd)
					{
						vdwt_analyst_line(result, result, result - lineLength, result - lineLength, loopX, FILL128(IDWT_BETA_128), 7 );
					}

					// step 6
					result = vdwt_result + lineLength;
					for (int i = 1; i < outputline - 1; i+=2)
					{
						vdwt_analyst_line(result, result, result - lineLength, result + lineLength, loopX, FILL128(IDWT_ALPHA_128),8 );

						result += lineLength * 2;
					}
					if (!odd)
					{
						vdwt_analyst_line(result, result,result - lineLength, result - lineLength, loopX, FILL128(IDWT_ALPHA_128),9 );
					}
				}

			}
			else if (outputline < 6)
			{
				// must be isEnd and the outputline == 5; l h l h l

				BMI_ASSERT(0);  // shouldn't been get into here

			}
			else
			{

				unsigned char * overlayLine[7] = {NULL};

				if (!isStart)
				{
					overlayLine[1] = hresult_l - lineLength;
					overlayLine[2] = hresult_h - lineLength;
					BMI_ASSERT(linesOffset>= 3);
					overlayLine[0] = overlayLine[2] - lineLength;
				}

				if (!isEnd)
				{
					BMI_ASSERT(low_h - linesOffset - linesDecode >=3);

					overlayLine[3] = hresult_l + linesDecode * lineLength;
					overlayLine[4] = hresult_h + linesDecode * lineLength;
					overlayLine[5] = overlayLine[3] + lineLength;
					overlayLine[6] = overlayLine[4] + lineLength;
				}

				VDWT_R97_LINES(vdwt_result, hresult_l, hresult_h, overlayLine, loopX, outputline, yParity, threadId, isStart, isEnd);
			}

		}
		else		// H L H L H L ......
		{
			if (isStart && isEnd && outputline < 6)
			{
				// total line less than 6
				if (outputline == 1)
				{
					// do nothing, simply copy
					memcpy(vdwt_result, H_result_down, width * 4);
				}
				else	// 2,3,4,5
				{
					bool odd = (low_h != high_h);
					unsigned char * result = vdwt_result + lineLength;
					unsigned char * in_h = hresult_h;
					unsigned char * in_l = hresult_l;
					// step 3
					for (int i = 1; i < outputline; i += 2)
					{
						vdwt_analyst_line(result, in_l,in_h, in_h + lineLength , loopX, FILL128(IDWT_DELTA_128), 200 );
						result += lineLength * 2;
						in_l += lineLength;
						in_h += lineLength;
					}
					if (!odd)
					{
						vdwt_analyst_line(result, in_l,in_h, in_h, loopX, FILL128(IDWT_DELTA_128),202);
					}

					// step 4
					result = vdwt_result;
					in_h = hresult_h;
					vdwt_analyst_line(result, in_h,result + lineLength , result + lineLength, loopX, FILL128(IDWT_GAMMA_128), 203 );

					for (int i = 2; i <= outputline - 1; i += 2)
					{
						vdwt_analyst_line(result, in_h,result - lineLength , result + lineLength, loopX, FILL128(IDWT_GAMMA_128), 204 );
						result += lineLength * 2;
						in_h += lineLength;
					}
					if (odd)
					{
						vdwt_analyst_line(result, in_h, result - lineLength,result - lineLength, loopX, FILL128(IDWT_GAMMA_128), 205 );
					}

					// step 5
					result = vdwt_result+lineLength;

					for (int i = 1; i < outputline; i+=2)
					{
						vdwt_analyst_line(result, result,result - lineLength, result + lineLength, loopX, FILL128(IDWT_BETA_128),206 );
						result += lineLength * 2;
					}
					if (!odd)
					{
						vdwt_analyst_line(result, result,result - lineLength, result - lineLength, loopX, FILL128(IDWT_BETA_128),207);
					}


					// step 6
					result = vdwt_result;
					vdwt_analyst_line(result, result,result + lineLength , result + lineLength, loopX, FILL128(IDWT_ALPHA_128), 208 );
					for (int i = 2; i <= outputline - 1; i += 2)
					{
						vdwt_analyst_line(result, result, result - lineLength , result + lineLength, loopX, FILL128(IDWT_ALPHA_128), 209 );
						result += lineLength * 2;
						in_h += lineLength;
					}
					if (odd)
					{
						vdwt_analyst_line(result, result, result - lineLength,result - lineLength, loopX, FILL128(IDWT_ALPHA_128), 210 );
					}
				}
			}
			else if (outputline < 6)
			{
				// must be isEnd and the outputline == 5; l h l h l
				BMI_ASSERT(0);  // shouldn't been get into here
			}
			else
			{

				unsigned char * overlayLine[7] = {NULL}; // previous HLHL  follow LHLH
				if (!isStart)
				{
					overlayLine[3] = hresult_l - lineLength;
					overlayLine[2] = hresult_h - lineLength;
					overlayLine[1] = overlayLine[3] - lineLength;
					overlayLine[0] = overlayLine[2] - lineLength;
				}

				if (!isEnd)
				{
					BMI_ASSERT(low_h - linesOffset - linesDecode >= 3);
					overlayLine[4] = hresult_h + linesDecode * lineLength;
					overlayLine[5] = hresult_l + linesDecode * lineLength;
					overlayLine[6] = overlayLine[4] + lineLength;
				}

				VDWT_R97_LINES(vdwt_result, hresult_l, hresult_h, overlayLine, loopX, outputline, yParity, threadId, isStart, isEnd);
			}

		}
		return;
	}

	template<class T>
	void	CpuDwtDecoder::VDWT_R53_LINES(T * target, T	*HResult_L, T * HResult_H, T ** overlaylines, int width, int decodeLine,uint_8 yParity, int threadId,  bool isStart, bool isEnd)
	{

		T * preLine;
		T * decodingLine[3];
		T * inputLine[2];

		if (!isStart)
		{
			preLine = overlaylines[0];
		}
		else
		{
			preLine = HResult_H;
		}
		decodingLine[0] = target;
		decodingLine[1] = target + width;
 		decodingLine[2] = target + 2 * width;

		inputLine[0] = HResult_L;
		inputLine[1] = HResult_H;
		decodeLine -= 2;

		for (int w = 0; w < width; ++w)
		{
			decodingLine[0][w] = inputLine[0][w] - ((preLine[w] + inputLine[1][w] + 2) >> 2);
		}


		while(decodeLine >= 2)
		{
			decodeLine -= 2;
			preLine = inputLine[1];
			inputLine[0] += width;
			inputLine[1] += width;

			for (int w = 0; w < width; ++w)
			{
				decodingLine[2][w] = inputLine[0][w] - ((preLine[w] + inputLine[1][w] + 2) >> 2);
				decodingLine[1][w] = preLine[w] + ((decodingLine[2][w] + decodingLine[0][w]) >> 1);
			}

			decodingLine[0] = decodingLine[2];
			decodingLine[1] = decodingLine[2] + width;
			decodingLine[2] = decodingLine[1] + width;
		}

		if (decodeLine)
		{
			BMI_ASSERT(isEnd);

			for (int w = 0; w < width; ++w)
			{
				decodingLine[2][w] = inputLine[0][w] - ((preLine[w] + 1) >> 1);
				decodingLine[1][w] = preLine[w] + ((decodingLine[2][w] + decodingLine[0][w]) >> 1);
			}

		}
		else
		{
			if (isEnd)
			{
				for (int w = 0; w < width; ++w)
				{
					decodingLine[1][w] = preLine[w] + decodingLine[0][w];
				}
			}
			else
			{
				for (int w = 0; w < width; ++w)
				{
					T tmp = overlaylines[1][w] - ((inputLine[1][w] + overlaylines[2][w] + 2) >> 2);
					decodingLine[1][w] = inputLine[1][w] + ((decodingLine[0][w] + tmp) >> 1);
				}

			}
		}
	}

	template<class T>
	void	CpuDwtDecoder::VDWT_R53(T * target, T * H_result, int low_h, int high_h, int width, int linesOffset, int linesDecode, uint_8 yParity, int threadId)
	{

		bool	isStart = (linesOffset == 0);
		BMI_ASSERT(isStart || linesOffset >= 2);
		bool	isEnd	= (linesOffset + linesDecode >= low_h);
		BMI_ASSERT(isEnd || (low_h - linesOffset - linesDecode) >= 2);

		int outputline = linesDecode * 2;
		if (isEnd && low_h != high_h)
		{
			--outputline;
		}

		T * vdwt_result = target + width * linesOffset * 2;
		T * hresult_l  = H_result + width * linesOffset;
		T * hresult_h  = H_result + width * (linesOffset + low_h);


		T * overlayLines[3] = {NULL};

		if (!isStart)
		{
			overlayLines[0] = hresult_h - width;
		}
		if (!isEnd)
		{
			overlayLines[1] = hresult_l + linesDecode * width;
			overlayLines[2] = hresult_h + linesDecode * width;
		}


		VDWT_R53_LINES<T>(vdwt_result, hresult_l, hresult_h, overlayLines, width, outputline, yParity, threadId, isStart, isEnd);

		return;
// 
// 		T * in_pixel_low = H_result + linesOffset * width;
// 		T * in_pixel_high = in_pixel_low + low_h * width;
// 
// 		bool oddHigh = (high_h < low_h);
// 
// 		T * preLine = linesOffset ? (in_pixel_high - width) : in_pixel_high ;
// 		T * followLine = reinterpret_cast<T *>(mSubThreadDwtOverLayMemory + threadId * mSubThreadDwtOverLayMemoryUnitSize);
// 
// 		int targetOffset = linesOffset<<1;
// 		target += targetOffset * width;
// 
// 
// 		BMI_ASSERT(linesDecode >= 2 );	// todo
// 
// 		if (linesOffset + linesDecode  >= low_h)
// 		{
// 			linesDecode = low_h - linesOffset;
// 			if (oddHigh)
// 			{
// 				followLine = in_pixel_high + width * (linesDecode - 1);
// 			}
// 			else
// 			{
// 				followLine = target + width * (linesDecode * 2 - 2);
// 			}
// 		}
// 		else
// 		{
// 			oddHigh = false;
// 
// 			T * H1 = in_pixel_high + width * (linesDecode - 1);
// 			T * H2 = H1 + width;
// 			T * L = in_pixel_low + width * (linesDecode - 1);
// 
// 			for (int w = 0; w < width; ++w, ++L, ++H1, ++H2)
// 			{
// 				followLine[w] = *L - ((*H1 + *H2 + 2)>>2);
// 			}
// 		}
// 
// 
// 		// core data transfer
// 		T * targetTmp = target;
// 		T * low_tmp = in_pixel_low;
// 		T * high_tmp = in_pixel_high;
// 
// 		// first line
// 		for (int w = 0; w < width; ++w, ++targetTmp, ++low_tmp, ++high_tmp, ++preLine)
// 		{
// 			*targetTmp = *low_tmp - ((*preLine + *high_tmp + 2) >>2);
// 		}	
// 		targetTmp += width;
// 		for (int l = 0; l < linesDecode - 2; ++l)
// 		{
// 			for (int w = 0; w < width; ++w, ++targetTmp, ++low_tmp, ++high_tmp)
// 			{
// 				*targetTmp = *low_tmp - ((*(high_tmp-width) + *high_tmp + 2) >>2);
// 
// 			}
// 			targetTmp += width;
// 
// 		}
// 		// last line
// 		if (oddHigh)
// 		{
// 			high_tmp -= width;
// 			for (int w = 0; w < width; ++w, ++targetTmp, ++low_tmp, ++high_tmp)
// 			{
// 				*targetTmp = *low_tmp - ((*high_tmp + 1) >>1);
// 			}	
// 		}
// 		else
// 		{
// 			for (int w = 0; w < width; ++w, ++targetTmp, ++low_tmp, ++high_tmp)
// 			{
// 				*targetTmp = *low_tmp - ((*(high_tmp-width) + *high_tmp + 2) >>2);
// 
// 			}
// 		}
// 
// 
// 		high_tmp = in_pixel_high;
// 		targetTmp = target + width;
// 
// 		for (int l = 0; l < linesDecode - 1; ++l)
// 		{
// 			for (int w = 0; w < width; ++w, ++targetTmp, ++high_tmp)
// 			{
// 				*targetTmp = *high_tmp + ((*(targetTmp -width) + *(targetTmp + width)) >> 1);
// 			}
// 			targetTmp += width;
// 
// 		}
// 
// 		// last line:
// 		if (!oddHigh)
// 		{
// 			for (int w = 0; w < width; ++w, ++targetTmp, ++followLine, ++high_tmp)
// 			{
// 				*targetTmp = *high_tmp + ((*(targetTmp -width) + *followLine) >> 1);
// 			}
// 		}
// 
// 
// 		return;

	}

	void	CpuDwtDecoder::MCT_R97(unsigned char  * result, unsigned char * comp0, unsigned char * comp1, unsigned char * comp2, int resultPitch, int width, int height, short bitDepth, OutputFormat outFormat , short checkOutComp,int wordLen)
	{

		if (wordLen == UINT_32_BITS)
		{
#if INTEL_ASSEMBLY_32
			RMCT_R97_FLOAT_ASM(
#else
			RICT_97_FLOAT(
#endif
				result,
				reinterpret_cast<float *>(comp0),
				reinterpret_cast<float *>(comp1),
				reinterpret_cast<float *>(comp2),
				resultPitch, width, height, bitDepth, 
				outFormat,
				checkOutComp);

		}
		else
		{
			BMI_ASSERT(0);
// 			MCT_R97_SHORT16(result,
// 				reinterpret_cast<int_16 *>(comp0),
// 				reinterpret_cast<int_16 *>(comp1),
// 				reinterpret_cast<int_16 *>(comp2),
// 				resultPitch, width, height, bitDepth, checkOutComp);

		}


	}


	void	CpuDwtDecoder::RICT_97_FLOAT(unsigned char  * outResult, float * comp0, float * comp1, float * comp2, int resultPitch, int width, int height, short bitDepth, OutputFormat outFormat, short checkOutComp, int sourceSkip/* = 0*/)
	{

		const	static float	C2_C0_RATIO	= (float)(1.402);
		const	static float	C1_C1_RATIO	= (float)(-0.34413);
		const	static float	C2_C1_RATIO	= (float)(-0.71414);
		const	static float	C1_C2_RATIO	= (float)(1.772);

		int nextRowSkip = resultPitch - width;
		float tmpf;
		int tmpi;
		int_32  maxV =( 0x01 << bitDepth) - 1;

		switch (outFormat)
		{
		case IMAGE_FOTMAT_32BITS_ARGB:
			{
				ARGB_32BITS * result = (ARGB_32BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h, result += nextRowSkip, comp0 += sourceSkip, comp1 += sourceSkip, comp2 += sourceSkip)
				{
					for(int w = 0; w < width; ++w, ++result, ++comp0, ++comp1, ++comp2)
					{
						result->mAlpha = 0;

						switch(checkOutComp)
						{
						case 0:
							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 1:
							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 2:
							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						default:

							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping

							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mGreen = (uint_8)(LIFT(tmpi, bitDepth, outDepth));	


							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mBlue= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping

							break;
						}
					}
				}	
			}
			break;
		case IMAGE_FOTMAT_64BITS_ARGB:
			{
				ARGB_64BITS * result = (ARGB_64BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h, result += nextRowSkip ,comp0 += sourceSkip, comp1 += sourceSkip, comp2 += sourceSkip)
				{
					for(int w = 0; w < width; ++w, ++result, ++comp0, ++comp1, ++comp2)
					{
						result->mAlpha = 0;

						switch(checkOutComp)
						{
						case 0:
							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 1:
							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 2:
							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						default:

							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping

							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mGreen = (uint_16)(LIFT(tmpi, bitDepth, outDepth));	


							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mBlue= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping

							break;
						}
					}
				}	
			}
			break;
		case IMAGE_FOTMAT_48BITS_RGB:
			{
				RGB_48BITS * result = (RGB_48BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h, result += nextRowSkip,comp0 += sourceSkip, comp1 += sourceSkip, comp2 += sourceSkip)
				{
					for(int w = 0; w < width; ++w, ++result, ++comp0, ++comp1, ++comp2)
					{
						switch(checkOutComp)
						{
						case 0:
							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 1:
							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 2:
							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						default:

							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping

							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mGreen = (uint_16)(LIFT(tmpi, bitDepth, outDepth));	


							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mBlue= (uint_16)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;
						}
					}
				}	

			}
			break;

		case IMAGE_FOTMAT_24BITS_RGB:
			{
				RGB_24BITS * result = (RGB_24BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h, result += nextRowSkip,comp0 += sourceSkip, comp1 += sourceSkip, comp2 += sourceSkip)
				{
					for(int w = 0; w < width; ++w, ++result, ++comp0, ++comp1, ++comp2)
					{
						switch(checkOutComp)
						{
						case 0:
							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 1:
							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						case 2:
							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = result->mGreen = result->mBlue
								= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;

						default:

							tmpf = *comp0 + C2_C0_RATIO * (*comp2);		// r
							tmpf += 1 << (bitDepth  - 1);								// perform level shift
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mRed = (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping

							tmpf = *comp0 + C1_C1_RATIO * (*comp1) + C2_C1_RATIO * (*comp2);		// g
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mGreen = (uint_8)(LIFT(tmpi, bitDepth, outDepth));	


							tmpf = *comp0 + C1_C2_RATIO * (*comp1);		// b
							tmpf += 1 << (bitDepth  - 1);
							tmpi = (int)(SET_BOUNDARY(0, tmpf, maxV));
							result->mBlue= (uint_8)(LIFT(tmpi, bitDepth, outDepth));						// clipping
							break;
						}
					}
				}	

			}
			break;
		case IMAGE_FOTMAT_48BITS_ARGB:
		case IMAGE_FOTMAT_36BITS_RGB:
			BMI_ASSERT(0);
#pragma BMI_PRAGMA_TODO_MSG("To support more formated output in MCT_97_Float")
			break;

		case IMAGE_FOTMAT_NONE:
		default:
			BMI_ASSERT(0);
			return;


		}

		return;
	}


// 	void	CpuDwtDecoder::RMCT_97_INT32(void * outResult, int * comp0, int * comp1, int * comp2, int resultPitch, int width, int height, short bitDepth,  OutputFormat outFormat,short checkOutComp)
// 	{
// 
// 		const	static BMI_INT_64	C2_C0_RATIO	= DOUBLE_TO_FIX(1.402);
// 		const	static BMI_INT_64	C1_C1_RATIO	= DOUBLE_TO_FIX(-0.34413);
// 		const	static BMI_INT_64	C2_C1_RATIO	= DOUBLE_TO_FIX(-0.71414);
// 		const	static BMI_INT_64	C1_C2_RATIO	= DOUBLE_TO_FIX(1.772);
// 
// 		int nextRowSkip = resultPitch - width;
// 		int_32 tmp, flag;
// 		int_32 maxV =( 0x01 << bitDepth) - 1;
// 
// 		if (outFormat == IMAGE_FOTMAT_32BITS_ARGB)
// 		{
// 			ARGB_32BITS * result = (ARGB_32BITS *)outResult;
// 
// 			for (int h = 0; h < height; ++h, result += nextRowSkip)
// 			{
// 				for(int w = 0; w < width; ++w, ++result, ++comp0, ++comp1, ++comp2)
// 				{
// 					result->mAlpha = 0;
// 					switch(checkOutComp)
// 					{
// 					case 0:
// 						tmp = *comp0 + FIX_MUL(C2_C0_RATIO , *comp2);		// r
// 						flag = (tmp & 0x80000000) ? -1 : 0;
// 						tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// rounding and convert to integer values
// 						tmp >>= 13;
// 						tmp += 1 << (bitDepth  - 1);								// perform level shift
// 						result->mRed = result->mGreen = result->mBlue
// 							= (SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);						// clipping
// 						break;
// 
// 					case 1:
// 						tmp = *comp0 + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);		// g
// 						flag = (tmp & 0x80000000) ? -1 : 0;
// 						tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// convert fix to int
// 						tmp >>= 13;
// 						tmp += 1 << (bitDepth  - 1);
// 						result->mRed = result->mGreen = result->mBlue
// 							= (SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);
// 						break;
// 
// 					case 2:
// 						tmp = *comp0 + FIX_MUL(C1_C2_RATIO , *comp1);		// b
// 						flag = (tmp & 0x80000000) ? -1 : 0;
// 						tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// convert fix to int
// 						tmp >>= 13;
// 						tmp += 1 << (bitDepth  - 1);
// 						result->mRed = result->mGreen = result->mBlue
// 							= (SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);
// 						break;
// 
// 					default:
// 
// 						tmp = *comp0 + FIX_MUL(C2_C0_RATIO , *comp2);		// r
// 						flag = (tmp & 0x80000000) ? -1 : 0;
// 						tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// rounding and convert to integer values
// 						tmp >>= 13;
// 						tmp += 1 << (bitDepth  - 1);								// perform level shift
// 						result->mRed = (SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);						// clipping
// 
// 						tmp = *comp0 + FIX_MUL(C1_C1_RATIO , *comp1) + FIX_MUL(C2_C1_RATIO , *comp2);		// g
// 						flag = (tmp & 0x80000000) ? -1 : 0;
// 						tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// convert fix to int
// 						tmp >>= 13;
// 						tmp += 1 << (bitDepth  - 1);
// 						result->mGreen = (SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);
// 
// 						tmp = *comp0 + FIX_MUL(C1_C2_RATIO , *comp1);		// b
// 						flag = (tmp & 0x80000000) ? -1 : 0;
// 						tmp = flag * ((flag * tmp + 0x1000) & 0xfffe000);			// convert fix to int
// 						tmp >>= 13;
// 						tmp += 1 << (bitDepth  - 1);
// 						result->mBlue = (SET_BOUNDARY(0, tmp, maxV)) >> (bitDepth - 8);
// 						break;
// 					}
// 				}
// 			}	
// 
// 		}
// 
// 		return;
// 	}

// 	void	CpuDwtDecoder::MCT_R97_SHORT16(int * result, int_16 * comp0, int_16 * comp1, int_16 * comp2, int resultPitch, int width, int height, short bitDepth, short checkOutComp)
// 	{
// 		BMI_ASSERT(0);
// 	}



	template<class T>	void	CpuDwtDecoder::RRct_53(void * outResultBuf, T* redBuf, T* greenBuf, T* blueBuf, int bufPitch, int width, int height, int bitDepth, OutputFormat outFormat,int checkOutComp)
	{

		int nextRowSkip = bufPitch - width;
		int_32 tmp;
		int_32 dc_offset = 0x01 <<(bitDepth - 1);
		int_32 dc_max = (dc_offset << 1) - 1;

		switch (outFormat)
		{
		case IMAGE_FOTMAT_32BITS_ARGB:
			{
				ARGB_32BITS * resultBuf = (ARGB_32BITS *)outResultBuf;
				int outDepth = sizeof(resultBuf->mRed) *8;

				for (int h = 0; h < height; ++h, resultBuf += nextRowSkip)
				{
					for(int w = 0; w < width; ++w, ++resultBuf, ++redBuf, ++greenBuf, ++blueBuf)
					{

						tmp = *redBuf - ((*greenBuf + *blueBuf) >> 2) + dc_offset;
						resultBuf->mAlpha = 0;

						switch(checkOutComp)
						{
						case 0:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth, outDepth);;		// r
							break;

						case 1:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, tmp, dc_max)) , bitDepth, outDepth);		// g
							break;

						case 2:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)) ,bitDepth, outDepth);		//  b

							break;

						default:
							resultBuf->mRed		= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth, outDepth);;		// r
							resultBuf->mGreen	= LIFT((SET_BOUNDARY(0, tmp, dc_max)) , bitDepth, outDepth);		// + g
							resultBuf->mBlue	= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)) ,bitDepth, outDepth);		// + b

							break;

						}
					}
				}
			}
			break;

		case IMAGE_FOTMAT_64BITS_ARGB:
			{
				ARGB_64BITS * resultBuf = (ARGB_64BITS *)outResultBuf;
				int outDepth = sizeof(resultBuf->mRed) *8;

				for (int h = 0; h < height; ++h, resultBuf += nextRowSkip)
				{
					for(int w = 0; w < width; ++w, ++resultBuf, ++redBuf, ++greenBuf, ++blueBuf)
					{

						tmp = *redBuf - ((*greenBuf + *blueBuf) >> 2) + dc_offset;
						resultBuf->mAlpha = 0;

						switch(checkOutComp)
						{
						case 0:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth, outDepth);;		// r
							break;

						case 1:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, tmp, dc_max)) , bitDepth, outDepth);		// g
							break;

						case 2:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)) ,bitDepth, outDepth);		//  b

							break;

						default:
							resultBuf->mRed		= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth, outDepth);;		// r
							resultBuf->mGreen	= LIFT((SET_BOUNDARY(0, tmp, dc_max)) , bitDepth, outDepth);		// + g
							resultBuf->mBlue	= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)) ,bitDepth, outDepth);		// + b

							break;

						}
					}
				}
			}
			break;

		case IMAGE_FOTMAT_48BITS_RGB:
			{
				RGB_48BITS * resultBuf = (RGB_48BITS *)outResultBuf;
				int outDepth = sizeof(resultBuf->mRed) *8;

				for (int h = 0; h < height; ++h, resultBuf += nextRowSkip)
				{
					for(int w = 0; w < width; ++w, ++resultBuf, ++redBuf, ++greenBuf, ++blueBuf)
					{

						tmp = *redBuf - ((*greenBuf + *blueBuf) >> 2) + dc_offset;

						switch(checkOutComp)
						{
						case 0:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth,outDepth);		// r
							break;

						case 1:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, tmp, dc_max)), bitDepth,outDepth);		// g
							break;

						case 2:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)),bitDepth, outDepth);;		// b
							break;

						default:
							resultBuf->mRed		= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth, outDepth);;		// r
							resultBuf->mGreen	= LIFT((SET_BOUNDARY(0, tmp, dc_max)) , bitDepth, outDepth);		// + g
							resultBuf->mBlue	= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)) ,bitDepth, outDepth);		// + b
							break;

						}
					}
				}
			}
			break;

		case IMAGE_FOTMAT_24BITS_RGB:
			{
				RGB_24BITS * resultBuf = (RGB_24BITS *)outResultBuf;
				int outDepth = sizeof(resultBuf->mRed) *8;

				for (int h = 0; h < height; ++h, resultBuf += nextRowSkip)
				{
					for(int w = 0; w < width; ++w, ++resultBuf, ++redBuf, ++greenBuf, ++blueBuf)
					{

						tmp = *redBuf - ((*greenBuf + *blueBuf) >> 2) + dc_offset;

						switch(checkOutComp)
						{
						case 0:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth,outDepth);		// r
							break;

						case 1:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, tmp, dc_max)), bitDepth,outDepth);		// g
							break;

						case 2:
							resultBuf->mBlue = resultBuf->mGreen = resultBuf->mRed
								= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)),bitDepth, outDepth);;		// b
							break;

						default:
							resultBuf->mRed		= LIFT((SET_BOUNDARY(0, (*blueBuf + tmp), dc_max)), bitDepth, outDepth);;		// r
							resultBuf->mGreen	= LIFT((SET_BOUNDARY(0, tmp, dc_max)) , bitDepth, outDepth);		// + g
							resultBuf->mBlue	= LIFT((SET_BOUNDARY(0, (*greenBuf + tmp), dc_max)) ,bitDepth, outDepth);		// + b
							break;

						}
					}
				}
			}
			break;

		case IMAGE_FOTMAT_36BITS_RGB:
		case IMAGE_FOTMAT_48BITS_ARGB:
			BMI_ASSERT(0);
#pragma BMI_PRAGMA_TODO_MSG("To support more formated output in MCT_97_Float")
			break;

		case IMAGE_FOTMAT_NONE:
		default:
			BMI_ASSERT(0);
			return;

		}


	}


	void	CpuDwtDecoder::MergeComponents(DwtJob & job, int tileId, int checkOutComp)
	{

		BMI_ASSERT(job.mJobConfig->mOutputFormat < IMAGE_FOTMAT_NONE && job.mResultbuf.GetBuf());

		unsigned char * result = reinterpret_cast<unsigned char *>(job.mResultbuf.GetBuf());

		int pitch = job.mPicture->mProperties.mOutputSize.x;
		int xoff = job.mPicture->mTileinfo[tileId].mOff.x;
		int yoff = job.mPicture->mTileinfo[tileId].mOff.y;
		int dwtLevel = job.mPicture->mProperties.mDwtLevel;
		int width = job.mPicture->mTileinfo[tileId].mSize.x;
		int height = job.mPicture->mTileinfo[tileId].mSize.y;

		while (dwtLevel-- > job.mDwtDecodingTopLevel )
		{
			xoff = (xoff + 1) >> 1;
			yoff = (yoff + 1) >> 1;
			width = (width + 1) >> 1;
			height = (height + 1) >> 1;
		}

		int pizelSizeInByte;
		int needComponents;
		switch (job.mJobConfig->mOutputFormat)
		{
		case	IMAGE_FOTMAT_48BITS_3COMPONENTS:
		case IMAGE_FOTMAT_48BITS_RGB:
			pizelSizeInByte = sizeof(THREE_COMPONENTS_48BITS);
			needComponents = 3;
			break;
		case	IMAGE_FOTMAT_32BITS_4COMPONENTS:
		case  IMAGE_FOTMAT_32BITS_ARGB:
			pizelSizeInByte = sizeof(FOUR_COMPONENT_32BITS);
			needComponents = 4;
			break;
		case	IMAGE_FOTMAT_24BITS_3COMPONENTS:
		case IMAGE_FOTMAT_24BITS_RGB:
			pizelSizeInByte = sizeof(THREE_COMPONENTS_24BITS);
			needComponents = 3;
			break;
		case IMAGE_FOTMAT_64BITS_4COMPONENTS:
		case IMAGE_FOTMAT_64BITS_ARGB:
			pizelSizeInByte = sizeof(FOUR_COMPONENTS_64BITS);
			needComponents = 4;
			break;

// 		case	IMAGE_FOTMAT_48BITS_ARGB:
// 		case	IMAGE_FOTMAT_36BITS_RGB:
// 		case	IMAGE_FOTMAT_48BITS_RGB:
// 		case	IMAGE_FOTMAT_32BITS_ARGB:
// 		case	IMAGE_FOTMAT_64BITS_ARGB:
// 		case	IMAGE_FOTMAT_24BITS_RGB:
		default:
		BMI_ASSERT(0);
			break;
		}

		result = result + (xoff + yoff * pitch) * pizelSizeInByte;

		unsigned char * comp0 = NULL, *comp1 = NULL, *comp2 = NULL, *comp3 = NULL;

// 		job.mJobConfig->mOutputFormat
		if (checkOutComp != INVALID)
		{
			comp0 = job.mDwtBuf[tileId].GetImgBuf(checkOutComp);
			checkOutComp = 0;
		}
		else
		{
			int c = 0, n = job.mPicture->mProperties.mCalculatedComponentNum;
			if (n--)
			{
				while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
				comp0 = job.mDwtBuf[tileId].GetImgBuf(c - 1); 
				if (n--)
				{
					while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
					comp1 = job.mDwtBuf[tileId].GetImgBuf(c - 1); 
					if (n--)
					{
						while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
						comp2 = job.mDwtBuf[tileId].GetImgBuf(c - 1); 
						if (n--)
						{
							while((job.mJobConfig->mIgnoreComponents >> (c++)) & 0x01);
							comp3 = job.mDwtBuf[tileId].GetImgBuf(c - 1); 		
						}
					}
				}
			}

		}

		if (job.mPicture->mProperties.mMethod == Ckernels_W9X7)
		{
			if (job.mWordLength == 4)
			{
#if INTEL_ASSEMBLY_32 || INTEL_ASSEMBLY_64
				MergeConponentsFloat_WIN_SSE(
#else
				MergeComponents<float>(
#endif
					result, 
					reinterpret_cast<float *>(comp0), 
					reinterpret_cast<float *>(comp1), 
					reinterpret_cast<float *>(comp2), 
					reinterpret_cast<float *>(comp3), 
					pitch, width, height,
					job.mPicture->mProperties.mBitDepth, 
					job.mJobConfig->mOutputFormat,
					checkOutComp);
			}
			else
			{
				BMI_ASSERT(0);
			}

		}
		else	// W5X3
		{
			if (job.mWordLength == 4)
			{
#if INTEL_ASSEMBLY_32 || INTEL_ASSEMBLY_64
				MergeConponentsInt32_WIN_SSE(
#else
				MergeComponents<int_32>(
#endif
					result, 
					reinterpret_cast<int_32 *>(comp0), 
					reinterpret_cast<int_32 *>(comp1), 
					reinterpret_cast<int_32 *>(comp2), 
					reinterpret_cast<int_32 *>(comp3), 
					pitch, width, height,
					job.mPicture->mProperties.mBitDepth, 
					job.mJobConfig->mOutputFormat,
					checkOutComp);
			}
			else
			{
				BMI_ASSERT(0);
#pragma	BMI_PRAGMA_TODO_MSG("merge 3 components using short interger")
			}
		}

	}



	template <class T>
	void	CpuDwtDecoder::MergeComponents(unsigned char * outResult, T * comp0, T * comp1, T * comp2, T * comp3, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp)
	{
		int lift = 1<<(bitDepth - 1);
		int pixMax = (1<<bitDepth) - 1;
		int tmp;

		switch(outFormat)
		{
		case IMAGE_FOTMAT_32BITS_4COMPONENTS:
			{
				ARGB_32BITS * result = (ARGB_32BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h)
				{
					for (int w = 0; w < width; ++w,  ++result)
					{

						switch(checkOutComp)
						{
						case 0:
							BMI_ASSERT(comp0);
							tmp = static_cast<int>(*comp0) + lift;
							tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
							result->mAlpha =  (outFormat == IMAGE_FOTMAT_32BITS_4COMPONENTS) ? tmp : 0;
							result->mRed = result->mGreen = result->mBlue = tmp;
							comp0++;
							break;
						case 1:
						case 2:
							BMI_ASSERT(0);	// shouldn't run to here
							break;
						default:
							if (comp0)
							{
								tmp = static_cast<int>(*comp0) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp0 = tmp;
								comp0++;
							}
							else
							{
								result->mComp0 = 0;
							}

							if (comp1)
							{
								tmp = static_cast<int>(*comp1) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp1 = tmp;
								comp1++;
							}
							else
							{
								result->mComp1 = 0;
							}

							if (comp2)
							{
								tmp = static_cast<int>(*comp2) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp2 = tmp;
								comp2++;
							}
							else
							{
								result->mComp2 = 0;
							}

							if (comp3)
							{
								tmp = static_cast<int>(*comp3) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp3 = tmp;
								comp3++;
							}
							else
							{
								result->mComp3 = 0;
							}

							break;
						}

					}
					result += resultPitch - width;
				}
			}
			break;

		case IMAGE_FOTMAT_64BITS_4COMPONENTS:
			{
				ARGB_64BITS * result = (ARGB_64BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h)
				{
					for (int w = 0; w < width; ++w,  ++result)
					{

						switch(checkOutComp)
						{
						case 0:
							BMI_ASSERT(comp0);
							tmp = static_cast<int>(*comp0) + lift;
							tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
							result->mAlpha =  (outFormat == IMAGE_FOTMAT_64BITS_4COMPONENTS) ? tmp : 0;
							result->mRed = result->mGreen = result->mBlue = tmp;
							comp0++;
							break;
						case 1:
						case 2:
							BMI_ASSERT(0);	// shouldn't run to here
							break;
						default:
							if (comp0)
							{
								tmp = static_cast<int>(*comp0) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp0 = tmp;
								comp0++;
							}
							else
							{
								result->mComp0 = 0;
							}

							if (comp1)
							{
								tmp = static_cast<int>(*comp1) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp1 = tmp;
								comp1++;
							}
							else
							{
								result->mComp1 = 0;
							}

							if (comp2)
							{
								tmp = static_cast<int>(*comp2) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp2 = tmp;
								comp2++;
							}
							else
							{
								result->mComp2 = 0;
							}

							if (comp3)
							{
								tmp = static_cast<int>(*comp3) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp3 = tmp;
								comp3++;
							}
							else
							{
								result->mComp3 = 0;
							}

							break;
						}

					}
					result += resultPitch - width;
				}
			}
			break;

		case IMAGE_FOTMAT_48BITS_3COMPONENTS:
			{
				RGB_48BITS * result = (RGB_48BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h)
				{
					for (int w = 0; w < width; ++w, ++result)
					{

						switch(checkOutComp)
						{
						case 0:
							BMI_ASSERT(comp0);
							tmp = static_cast<int>(*comp0) + lift;
							tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
							result->mRed = result->mGreen = result->mBlue = tmp;
							comp0++;
							break;
						case 1:
						case 2:
							BMI_ASSERT(0);	// shouldn't run to here
							break;
						default:
							if (comp0)
							{
								tmp = static_cast<int>(*comp0) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp0 = tmp;
								comp0++;
							}
							else
							{
								result->mComp0 = 0;
							}

							if (comp1)
							{
								tmp = static_cast<int>(*comp1) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp1 = tmp;
								comp1++;
							}
							else
							{
								result->mComp1 = 0;
							}

							if (comp2)
							{
								tmp = static_cast<int>(*comp2) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp2 = tmp;
								comp2++;
							}
							else
							{
								result->mComp2 = 0;
							}
						}

						break;
					}
					result += resultPitch - width;
				}
			}
			break;

		case	IMAGE_FOTMAT_24BITS_3COMPONENTS:
			{
				RGB_24BITS * result = (RGB_24BITS *)outResult;
				int outDepth = sizeof(result->mRed) * 8;

				for (int h = 0; h < height; ++h)
				{
					for (int w = 0; w < width; ++w, ++result)
					{

						switch(checkOutComp)
						{
						case 0:
							BMI_ASSERT(comp0);
							tmp = static_cast<int>(*comp0) + lift;
							tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
							result->mRed = result->mGreen = result->mBlue = tmp;
							comp0++;
							break;
						case 1:
						case 2:
							BMI_ASSERT(0);	// shouldn't run to here
							break;
						default:
							if (comp0)
							{
								tmp = static_cast<int>(*comp0) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp0 = tmp;
								comp0++;
							}
							else
							{
								result->mComp0 = 0;
							}

							if (comp1)
							{
								tmp = static_cast<int>(*comp1) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp1 = tmp;
								comp1++;
							}
							else
							{
								result->mComp1 = 0;
							}

							if (comp2)
							{
								tmp = static_cast<int>(*comp2) + lift;
								tmp = LIFT((SET_BOUNDARY(0, tmp, pixMax)) , bitDepth, outDepth);
								result->mComp2 = tmp;
								comp2++;
							}
							else
							{
								result->mComp2 = 0;
							}
							break;
						}

					}
					result += resultPitch - width;
				}
			}
			break;

		default:
			BMI_ASSERT(0);
			return;
		}

	}

#if INTEL_ASSEMBLY_32 || INTEL_ASSEMBLY_64
	void	CpuDwtDecoder::MergeConponentsFloat_WIN_SSE(unsigned char  * outResult, float * comp0, float * comp1, float * comp2, float * comp3, int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp)
	{


		BMI_ASSERT(checkOutComp == 0 || checkOutComp == INVALID);
		BMI_ASSERT(comp0);
		// 		BMI_ASSERT(!((unsigned int )(comp0) & 0x0f) && !((unsigned int )(comp1) & 0x0f) && !((unsigned int )(comp2) & 0x0f) && !((unsigned int )(comp3) & 0x0f));


		int sourcePitch = ALIGN_TO_SSE_DWORDS(width);

		int tailWidth, loopWidth;
		int outSkip = resultPitch - width;

		int outBitDepth;
		int outChannel;
		switch(outFormat)
		{
		case IMAGE_FOTMAT_24BITS_3COMPONENTS:
		case IMAGE_FOTMAT_24BITS_RGB:
			outBitDepth = 8;
			outChannel = 3;
			break;
		case IMAGE_FOTMAT_32BITS_4COMPONENTS:
		case IMAGE_FOTMAT_32BITS_ARGB:
			outBitDepth = 8;
			outChannel = 4;
			break;
		case IMAGE_FOTMAT_48BITS_3COMPONENTS:
		case  IMAGE_FOTMAT_48BITS_RGB:
			outBitDepth = 16;
			outChannel = 3;
			break;
		case IMAGE_FOTMAT_64BITS_4COMPONENTS:
		case IMAGE_FOTMAT_64BITS_ARGB:
			outBitDepth = 16;
			outChannel = 4;
			break;
		default:
			BMI_ASSERT(0);
			break;
		}
		int resultSkip = resultPitch - width * outBitDepth * outChannel / 8;

		BMI_ASSERT(outBitDepth <= 16);
		// foolowing clip is only good for less than 16 bits : _mm_max_epi16 and _mm_min_epi16

		float fConvert = (bitDepth > outBitDepth) ? (1.0f / static_cast<float>(1 << (bitDepth - outBitDepth))) : (static_cast<float>(1 << (outBitDepth - bitDepth)));
		__m128	convert_to_out = _mm_set_ps1(fConvert);
		int iLift =  1 << (outBitDepth - 1);
		__m128  liftf = _mm_set_ps1((float)iLift);
		int iMax = (1<<outBitDepth) - 1;
		__m128	maxVf =  _mm_set_ps1((float)iMax);
		__m128	Zerof =  _mm_set_ps1(0.0f);
		__m128 mf;
		__m128i mi;
		unsigned char * out = outResult;

		if (width == sourcePitch)
		{
			loopWidth = width;
			tailWidth = 0;
		}
		else
		{
			loopWidth = sourcePitch - 4;
			tailWidth = width - loopWidth;
		}
		float * l0 = comp0;
		float * l1 = comp1;
		float * l2 = comp2;
		float * l3 = comp3;

		int sourchSkip = sourcePitch - width + tailWidth;


		for (int h = 0; h < height; ++h)
		{
			__m128 * c0 = reinterpret_cast<__m128 * >(l0);
			__m128 * c1 = reinterpret_cast<__m128 * >(l1);
			__m128 * c2 = reinterpret_cast<__m128 * >(l2);
			__m128 * c3 = reinterpret_cast<__m128 * >(l3);

			for (int w = 0; w < loopWidth; w+=4)
			{

				mf = _mm_mul_ps(*c0, convert_to_out);
				mf = _mm_add_ps(mf, liftf);
				mf = _mm_max_ps(mf, Zerof);
				mf = _mm_min_ps(mf, maxVf);
				mi = _mm_cvtps_epi32(mf);
				++c0;
				if (outBitDepth == 8 && outChannel == 4)
				{
					*(__m128i *)(out) = mi;
				}
				else if (outBitDepth == 8 && outChannel == 3)
				{
					out[2] = mi.m128i_i8[0];
					out[5] = mi.m128i_i8[4];
					out[8] = mi.m128i_i8[8];
					out[11] = mi.m128i_i8[12];
					out[1] = 
						out[0] = 
						out[4] = 
						out[3] = 
						out[7] = 
						out[6] = 
						out[10] =
						out[9] = 0;
				}
				else if (outBitDepth == 16)
				{
					((unsigned short *)out)[outChannel   - 1] = mi.m128i_i16[0];
					((unsigned short *)out)[outChannel*2 - 1] = mi.m128i_i16[2];
					((unsigned short *)out)[outChannel*3 - 1] = mi.m128i_i16[4];
					((unsigned short *)out)[outChannel*4 - 1] = mi.m128i_i16[6];
				}


				if (l1)
				{
					mf = _mm_mul_ps(*c1, convert_to_out);
					mf = _mm_add_ps(mf, liftf);
					mf = _mm_max_ps(mf, Zerof);
					mf = _mm_min_ps(mf, maxVf);
					mi = _mm_cvtps_epi32(mf);
					++c1;
					if (outBitDepth == 8 && outChannel == 4)
					{
						mi = _mm_slli_epi32(mi, 8);
						*(__m128i *)(out) = _mm_or_si128(*(__m128i *)(out), mi);
					}
					else if (outBitDepth == 8 && outChannel == 3)
					{
						out[1] = mi.m128i_i8[0];
						out[4] = mi.m128i_i8[4];
						out[7] = mi.m128i_i8[8];
						out[10] = mi.m128i_i8[12];
					}
					else if (outBitDepth == 16)
					{
						((unsigned short *)out)[outChannel   - 2] = mi.m128i_i16[0];
						((unsigned short *)out)[outChannel*2 - 2] = mi.m128i_i16[2];
						((unsigned short *)out)[outChannel*3 - 2] = mi.m128i_i16[4];
						((unsigned short *)out)[outChannel*4 - 2] = mi.m128i_i16[6];
					}
				}
				else if (outBitDepth == 16)
				{
					((unsigned short *)out)[outChannel   - 2] = 
						((unsigned short *)out)[outChannel*2 - 2] = 
						((unsigned short *)out)[outChannel*3 - 2] = 
						((unsigned short *)out)[outChannel*4 - 2] = 0;
				}

				if (l2)
				{
					mf = _mm_mul_ps(*c2, convert_to_out);
					mf = _mm_add_ps(mf, liftf);
					mf = _mm_max_ps(mf, Zerof);
					mf = _mm_min_ps(mf, maxVf);
					mi = _mm_cvtps_epi32(mf);
					++c2;
					if (outBitDepth == 8 && outChannel == 4)
					{
						mi = _mm_slli_epi32(mi, 16);
						*(__m128i *)(out) = _mm_or_si128(*(__m128i *)(out), mi);
					}
					else if (outBitDepth == 8 && outChannel == 3)
					{
						out[0] = mi.m128i_i8[0];
						out[3] = mi.m128i_i8[4];
						out[6] = mi.m128i_i8[8];
						out[9] = mi.m128i_i8[12];
					}
					else if (outBitDepth == 16)
					{
						((unsigned short *)out)[outChannel   - 3] = mi.m128i_i16[0];
						((unsigned short *)out)[outChannel*2 - 3] = mi.m128i_i16[2];
						((unsigned short *)out)[outChannel*3 - 3] = mi.m128i_i16[4];
						((unsigned short *)out)[outChannel*4 - 3] = mi.m128i_i16[6];
					}
				}
				else if (outBitDepth == 16)
				{
					((unsigned short *)out)[outChannel   - 3] =
						((unsigned short *)out)[outChannel*2 - 3] =
						((unsigned short *)out)[outChannel*3 - 3] =
						((unsigned short *)out)[outChannel*4 - 3] = 0;
				}

				if (outChannel == 4)
				{
					if (l3)
					{
						mf = _mm_mul_ps(*c3, convert_to_out);
						mf = _mm_add_ps(mf, liftf);
						mf = _mm_max_ps(mf, Zerof);
						mf = _mm_min_ps(mf, maxVf);
						mi = _mm_cvtps_epi32(mf);
						++c3;
						if (outBitDepth == 8)
						{
							mi = _mm_slli_epi32(mi, 24);
							*(__m128i *)(out) = _mm_or_si128(*(__m128i *)(out), mi);
						}
						else if (outBitDepth == 16)
						{
							((unsigned short *)out)[outChannel   - 4] = mi.m128i_i16[0];
							((unsigned short *)out)[outChannel*2 - 4] = mi.m128i_i16[2];
							((unsigned short *)out)[outChannel*3 - 4] = mi.m128i_i16[4];
							((unsigned short *)out)[outChannel*4 - 4] = mi.m128i_i16[6];
						}

					}	
					else if (outBitDepth == 16)
					{
						((unsigned short *)out)[outChannel   - 4] =
							((unsigned short *)out)[outChannel*2 - 4] =
							((unsigned short *)out)[outChannel*3 - 4] =
							((unsigned short *)out)[outChannel*4 - 4] = 0;
					}

				}
				out += 4 *  outBitDepth * outChannel / 8;
			}

			l0 += loopWidth;
			if (l1)
			{
				l1 += loopWidth;
			}
			if (l2)
			{
				l2 += loopWidth;
			}
			if (l3)
			{
				l3 += loopWidth;
			}

			if (tailWidth)
			{

				int iv;
				for (int i = 0; i < tailWidth; ++i)
				{
					if (outChannel == 4)
					{
						if (l3)
						{
							iv = (int)(l3[i] * fConvert);
							iv += iLift;
							iv = SET_BOUNDARY(0, iv, iMax);
							if (outBitDepth == 8)
							{
								*out++ = (unsigned int)iv;
							}
							else if (outBitDepth == 16)
							{
								*((unsigned short *)out) = (unsigned short)iv;
								out++;
								out++;
							}
						}
						else
						{
							*out++ = 0;
							if (outBitDepth == 16)
							{
								*out++ = 0;
							}
						}
					}

					if (l2)
					{
						iv = (int)(l2[i] * fConvert);
						iv += iLift;
						iv = SET_BOUNDARY(0, iv, iMax);
						if (outBitDepth == 8)
						{
							*out++ = (unsigned int)iv;
						}
						else if (outBitDepth == 16)
						{
							*((unsigned short *)out) = (unsigned short)iv;
							out++;
							out++;
						}
					}
					else
					{
						*out++ = 0;
						if (outBitDepth == 16)
						{
							*out++ = 0;
						}
					}

					if (l1)
					{
						iv = (int)(l1[i] * fConvert);
						iv += iLift;
						iv = SET_BOUNDARY(0, iv, iMax);
						if (outBitDepth == 8)
						{
							*out++ = (unsigned int)iv;
						}
						else if (outBitDepth == 16)
						{
							*((unsigned short *)out) = (unsigned short)iv;
							out++;
							out++;
						}
					}
					else
					{
						*out++ = 0;
						if (outBitDepth == 16)
						{
							*out++ = 0;
						}
					}

					iv = (int)(l0[i] * fConvert);
					iv += iLift;
					iv = SET_BOUNDARY(0, iv, iMax);
					if (outBitDepth == 8)
					{
						*out++ = (unsigned int)iv;
					}
					else
					{
						*((unsigned short *)out) = (unsigned short)iv;
						out++;
						out++;
					}

				}


			}
			l0 += sourchSkip;
			if (l1)
			{
				l1 += sourchSkip;
			}
			if (l2)
			{
				l2 += sourchSkip;
			}
			if (l3)
			{
				l3 += sourchSkip;
			}		
			out += outSkip * outChannel * outBitDepth / 8;
		}

		return;

	}

	void 	CpuDwtDecoder::MergeConponentsInt32_WIN_SSE(unsigned char  * outResult, int_32 * comp0, int_32 * comp1, int_32 * comp2,int_32 * comp3,  int resultPitch, int width, int height, short bitDepth,OutputFormat outFormat,  short checkOutComp)
	{
		// 		MergeComponents<int_32>(outResult, comp0, comp1, comp2, comp3, resultPitch, width, height, bitDepth, outFormat, checkOutComp);

		return ;

#pragma BMI_PRAGMA_TODO_MSG("implement assembly for 53 components merge")

	}

#endif // _WINDOWS

#if !ENABLE_ASSEMBLY_OPTIMIZATION
	void vdwt_analyst_line_func(unsigned char * dst, unsigned char * src, unsigned char * left, unsigned char * right, int loopX, FLOAT_REG coef)
	{

		//							    S
		//				       L		|		 R
		//						\		|		/
		//							    D	
		float * S = (float *) src, * D = (float *)dst, * L = (float *)left, * R = (float *)right;
#if SSE_OPTIMIZED
		for (int i = 0; i < loopX * 4; ++i, ++S, ++D, ++L, ++R)
#else
		for (int i = 0; i < loopX; ++i, ++S, ++D, ++L, ++R)
#endif
		{
			*D = *S - coef *(*L + *R);
		}
	}
#endif

#if ENABLE_ASSEMBLY_OPTIMIZATION
	void	CpuDwtDecoder::HDWT_R97_FLOAT_ASM(int threadId, float * result, float  * band_l, float  * band_h, int low_w, int high_w,  int lines, float lowgain, float highgain, uint_8 xParity,  bool isLowBandNeedTransfer)
	{

		int resultSkip = ALIGN_TO_SSE_DWORDS(low_w+high_w);
		int lowSkip = ALIGN_TO_SSE_DWORDS(low_w);
		int highSkip = ALIGN_TO_SSE_DWORDS(high_w);

		float * tempBuf = reinterpret_cast<float *>(mSubThreadDwtOverLayMemory + threadId * mSubThreadDwtOverLayMemoryUnitSize);

		for (int i = 0; i < lines; ++i, result += resultSkip, band_l += lowSkip, band_h += highSkip)
		{
			HDWT_R97_FLOAT_ASM_1Line(result, band_l, band_h, tempBuf, low_w, high_w, lowgain, highgain, xParity,  isLowBandNeedTransfer);
		}
	}
#endif


};