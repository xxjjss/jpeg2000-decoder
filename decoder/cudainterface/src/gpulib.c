
#include "gpulib.h"
#include "gpufunc.h"
#include "cudainterfacedatatype.h"
#include "platform.h"
#include "debug.h"
#include "gpu_setting.h"

#define GPU_FAST_EMU	0


int GpuInit(GpuInfo * gpfInfo)
{
	BMI_ASSERT(gpfInfo);
	return 	bmi_get_gpu_info(gpfInfo);
}

int GpuRemove(GpuInfo * gpfInfo)
{
	BMI_ASSERT(gpfInfo);
	return 	bmi_del_gpu_info(gpfInfo);
}


void	MallocGpuBuf(CudaBuf * buf)
{

	malloc_gpu_buf(buf);
}

void	FreeGpuBuf(CudaBuf * buf)
{
	delete_gpu_buf(buf);

}

StreamId	CreateANewStream()
{
	return	bmi_gpu_stream_new();
}

void DestroyStream(StreamId  *Pasync_id)
{
	bmi_gpu_stream_delete(Pasync_id);
}

void GpuSync(StreamId * P_asyncId)
{
	bmi_sync_job(P_asyncId);
}

void	Gpu2DCopy(CudaBuf * srcBuf, CudaBuf * destBuf, int src_x_off, int src_y_off, int dest_x_off, int dest_y_off,int width, int height, StreamId * P_asyncId, CudaCopyDirection direction, int src_no_pitch, int dest_no_pitch )
{
	unsigned char * src = (unsigned char *)srcBuf->mBuf; 
	unsigned char * dest = (unsigned char *)destBuf->mBuf;
	src += src_x_off + src_y_off * srcBuf->mPitch;
	dest += dest_x_off + dest_y_off * destBuf->mPitch;


	memory_copy_2D(dest, (dest_no_pitch ? width : destBuf->mPitch), src, (src_no_pitch ? width : srcBuf->mPitch), width, height, P_asyncId, direction);
}

void	Gpu2DCopyWithDiffPitch(CudaBuf * srcBuf, CudaBuf * destBuf, int src_x_off, int src_y_off, int dest_x_off, int dest_y_off, int src_pitch, int dest_pitch, int width, int height, StreamId * P_asyncId, CudaCopyDirection direction, int src_no_pitch, int dest_no_pitch )
{
	unsigned char * src = (unsigned char *)srcBuf->mBuf; 
	unsigned char * dest = (unsigned char *)destBuf->mBuf;
	src += src_x_off + src_y_off * src_pitch;
	dest += dest_x_off + dest_y_off * dest_pitch;


	memory_copy_2D(dest, (dest_no_pitch ? width : dest_pitch), src, (src_no_pitch ? width : src_pitch), width, height, P_asyncId, direction);
}

void	GpuLinearMemCopy(CudaBuf * srcBuf, CudaBuf * destBuf, int copy_size_in_byte, StreamId async_id, CudaCopyDirection direction )
{

	memory_copy_linear(destBuf->mBuf, srcBuf->mBuf, copy_size_in_byte, async_id, direction);
}

#if !GPU_W9X7_FLOAT
int BMI_i97_dequantize_subband(CudaBuf * deviceBuf, int tile_off_x, int tile_off_y, SubbandInfo_c * subbandInfo, short wordShift, StreamId asyncId)
{
	return bmi_i97_dequantize_subband( deviceBuf, tile_off_x, tile_off_y, subbandInfo, wordShift, asyncId);
}

int BMI_i97_dequantize_component(CudaBuf * buf, TileInfo_c * tileInfo,int topDeqLevel, int compId, short wordShift, StreamId asyncId)
{
	int subbandId = 0;
	int subbandNum = tileInfo->mCompInfo[compId].mNumOfSubband;
	if (topDeqLevel != -1)
	{
		subbandNum = topDeqLevel * 3 + 1;
	}
	for (; subbandId < subbandNum; ++subbandId)
	{
		bmi_i97_dequantize_subband( buf, tileInfo->mOff.x, tileInfo->mOff.y, &(tileInfo->mCompInfo[compId].mSubbandInfo[subbandId]),  wordShift, asyncId);
	}
	return 0;
}
#endif
int BMI_resolution_idwt(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId,short wordShift, EncodeMathod mathod, int resolutionLevel)
{
#if GPU_FAST_EMU
	return 0;
#endif 
	int TR_BandId = 3 * resolutionLevel + 1;
	if (mathod == Ckernels_W9X7)
	{
// 		CudaBuf * orig, CudaBuf * temp,				// input suda buffer and temp cuda buffer used for DWT
// 			int x_off, int y_off,						// x-offset and y-offset in input buffer
// 			int lowW, int highW, int lowH, int highH,		// the width and height for the 4 subbands in this resotion
// 			int xParity, int yParity,					// the parity for vertial and horizontal
// 			float coef0, float coef1, float coef2, float coef3,	// the 4 coefiient for dwt calculation, for original data will be NORM * qqStep * logain/hiGain;
// 			// for data from last level will be logain / highgain
// 			int	isLLBandFloat,								// Is this dwt the top DWT? if yes will do the RENORM transfer to int
// 			short wordlength, StreamId asyncId)			// wordlength of input buffer and the asyncId

		DEBUG_PRINT((">>>>> Doing DWT job for  resolution %d id %d\n",resolutionLevel, asyncId));
 		bmi_idwt_i97_gpu_transform_resolution(	orig,
												temp,
												tile->mOff.x,
												tile->mOff.y,
												comp->mSubbandInfo[TR_BandId + 1].mSize.x,
												comp->mSubbandInfo[TR_BandId    ].mSize.x,
												comp->mSubbandInfo[TR_BandId    ].mSize.y,
												comp->mSubbandInfo[TR_BandId + 1].mSize.y,
												tile->mXParity[resolutionLevel],
												tile->mYParity[resolutionLevel],
												resolutionLevel ? IDWT_LO_GAIN : comp->mSubbandInfo[0].mfAbsStepNorm * IDWT_LO_GAIN,
												comp->mSubbandInfo[TR_BandId    ].mfAbsStepNorm * IDWT_HI_GAIN,
												comp->mSubbandInfo[TR_BandId + 1].mfAbsStepNorm * IDWT_HI_GAIN,
												comp->mSubbandInfo[TR_BandId + 2].mfAbsStepNorm * IDWT_HI_GAIN,
												(resolutionLevel ? 1 : 0),
												wordShift,
												asyncId);


	}
	else if (mathod == Ckernels_W5X3)
	{
		bmi_idwt_r53_gpu_transform_resolution(	orig,
												temp,
												tile->mOff.x,
												tile->mOff.y,
												comp->mSubbandInfo[TR_BandId + 1].mSize.x,
												comp->mSubbandInfo[TR_BandId    ].mSize.x,
												comp->mSubbandInfo[TR_BandId    ].mSize.y,
												comp->mSubbandInfo[TR_BandId + 1].mSize.y,
												tile->mXParity[resolutionLevel],
												tile->mYParity[resolutionLevel],
												(resolutionLevel ? 0 : 1),
												wordShift,
												asyncId);

	}
	else
	{
		BMI_ASSERT(0);
	}
	return 0;

}


int BMI_comp_idwt(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, EncodeMathod mathod, int topLevel)
{
#if GPU_FAST_EMU
	return 0;
#endif 
	if (mathod == Ckernels_W9X7)
	{
 		bmi_idwt_i97_gpu_transform_float(tile, comp, orig, temp, asyncId, wordShift, topLevel);
// 		bmi_idwt_i97_gpu_transform(tile, comp, orig, temp, asyncId, wordShift, topLevel);
	}
	else if (mathod == Ckernels_W5X3)
	{
		bmi_idwt_r53_gpu_transform(tile, comp, orig, temp, asyncId, wordShift, topLevel);
	}
	else
	{
		BMI_ASSERT(0);
	}
	return 0;

}

void	BMI_tile_MCT(TileInfo_c * tileInfo, short bitDepth, short wordShift, int dwtLevel,  int decodeLevel,OutputFormat ourFormat,StreamId asyncId)
{
#if GPU_FAST_EMU
	return 0;
#endif 
	int elementPerPixel, bitsPerElement;

	switch (ourFormat)
	{
	case IMAGE_FOTMAT_32BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 8;
		break;

	case IMAGE_FOTMAT_48BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 16;
		break;

	case IMAGE_FOTMAT_24BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 8;
		break;

	case IMAGE_FOTMAT_64BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 16;
		break;

	case IMAGE_FOTMAT_36BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 12;
		break;

	case IMAGE_FOTMAT_48BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 12;
		break;

	case IMAGE_FOTMAT_32BITS_4COMPONENTS:
	case IMAGE_FOTMAT_64BITS_4COMPONENTS:
	case IMAGE_FOTMAT_48BITS_3COMPONENTS:
	case IMAGE_FOTMAT_24BITS_3COMPONENTS:
	case IMAGE_FOTMAT_NONE:
		BMI_ASSERT(0);
		break;

	}
	bmi_MCT_gpu(tileInfo, bitDepth, wordShift,dwtLevel,  decodeLevel, elementPerPixel, bitsPerElement, asyncId);
}

void	BMI_tile_ICT(TileInfo_c * tileInfo, short bitDepth, short wordShift,  int dwtLevel, int decodeLevel,OutputFormat outFormat,StreamId asyncId)
{
#if GPU_FAST_EMU
	return 0;
#endif 
	int elementPerPixel, bitsPerElement;

	switch (outFormat)
	{
	case IMAGE_FOTMAT_32BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 8;
		break;

	case IMAGE_FOTMAT_48BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 16;
		break;

	case IMAGE_FOTMAT_24BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 8;
		break;

	case IMAGE_FOTMAT_64BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 16;
		break;

	case IMAGE_FOTMAT_36BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 12;
		break;

	case IMAGE_FOTMAT_48BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 12;
		break;

	case IMAGE_FOTMAT_NONE:
	case IMAGE_FOTMAT_64BITS_4COMPONENTS:
	case IMAGE_FOTMAT_24BITS_3COMPONENTS:
	case IMAGE_FOTMAT_48BITS_3COMPONENTS:
	case IMAGE_FOTMAT_32BITS_4COMPONENTS:
		// don't need to do MCT
		return;

		BMI_ASSERT(0);
		break;
		
	}
	bmi_ICT_gpu(tileInfo, bitDepth, wordShift, dwtLevel, decodeLevel, elementPerPixel, bitsPerElement, asyncId);
}

void	BMI_tile_Merge(TileInfo_c * tileInfo,int componentNum, int dwtLevel,  int decodeLevel, short bitDepth, short wordShift,int compBufPitchBytes,  OutputFormat outFormat, EncodeMathod method,StreamId asyncId)
{

	int elementPerPixel, bitsPerElement;

	switch (outFormat)
	{
	case IMAGE_FOTMAT_32BITS_ARGB:
	case IMAGE_FOTMAT_32BITS_4COMPONENTS:
		elementPerPixel = 4;
		bitsPerElement	= 8;
		break;

	case IMAGE_FOTMAT_48BITS_RGB:
	case IMAGE_FOTMAT_48BITS_3COMPONENTS:
		elementPerPixel = 3;
		bitsPerElement	= 16;
		break;

	case IMAGE_FOTMAT_24BITS_RGB:
	case IMAGE_FOTMAT_24BITS_3COMPONENTS:
		elementPerPixel = 3;
		bitsPerElement	= 8;
		break;

	case IMAGE_FOTMAT_64BITS_ARGB:
	case IMAGE_FOTMAT_64BITS_4COMPONENTS:
		elementPerPixel = 4;
		bitsPerElement	= 16;
		break;

	case IMAGE_FOTMAT_36BITS_RGB:
		elementPerPixel = 3;
		bitsPerElement	= 12;
		break;

	case IMAGE_FOTMAT_48BITS_ARGB:
		elementPerPixel = 4;
		bitsPerElement	= 12;
		break;

	case IMAGE_FOTMAT_NONE:
		// don't need to do Merge
		return;

		BMI_ASSERT(0);
		break;
	}
	bmi_merge_gpu( tileInfo, componentNum, dwtLevel, decodeLevel, bitDepth, wordShift, compBufPitchBytes ,elementPerPixel, bitsPerElement, method, asyncId);
}

void	BMI_tile_One_Component(TileInfo_c * tile, int component_id,  EncodeMathod method, int hasMCT, int bitDepth, int wordShift, int dwtLevel, int decodeLevel, StreamId asyncId)
{
#if GPU_FAST_EMU
	return 0;
#endif 

	bmi_get_one_component(tile, component_id,  method, hasMCT , bitDepth, wordShift, decodeLevel, asyncId);
}

// void	BMI_tile_One_Component_orig(TileInfo_c * tile, int component_id,  EncodeMathod method, int bitDepth, int wordShift,int dwtLevel,  int decodeLevel, StreamId asyncId)
// {
// #if GPU_FAST_EMU
// 	return 0;
// #endif 
// 	if(tile->mCompInfo[component_id].mDeShifted == 0 )
// 	{
// 		tile->mCompInfo[component_id].mDeShifted = 1;
// 		bmi_get_one_component_orig(tile, component_id,  method, bitDepth, wordShift, decodeLevel, asyncId);
// 	}
// }

void	BMI_Comp_DC_SHIFT(CudaBuf * compBuf, CudaBuf * compBufTemp, EncodeMathod method, int src_x_off, int src_y_off,  
						  int dst_x_off, int dst_y_off, int width, int height, int bitDepth, int pixelWordLength, StreamId asyncId)
{
	bmi_dc_shift(compBuf, compBufTemp, src_x_off, src_y_off,  dst_x_off, dst_y_off, width, height, pixelWordLength, bitDepth, /*isFloat*/ (method == Ckernels_W9X7 ? 1 : 0), asyncId);

}

#if GPU_T1_TESTING
int BMI_gpu_decode_codeblock()
{
	// call gpu_decode_coderblock();
	return 0;
}
#endif
