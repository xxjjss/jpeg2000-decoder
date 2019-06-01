
#ifndef		_GPU_LIB_H
#define		_GPU_LIB_H

#include <stdio.h>
#include <stdlib.h>
#include "cudainterfacedatatype.h"

#ifdef __cplusplus
extern "C" {
#endif
	
	const	float	IDWT_ALPHA	=	-1.58613434205992f;
	const	float	IDWT_BETA	=	-0.05298011857296f;
	const	float	IDWT_GAMMA	=	 0.88291107553093f;
	const	float	IDWT_DELTA	=	 0.44350685204397f;

	const	float	IDWT_LO_GAIN =	 1.23017410558578f;
	const	float	IDWT_HI_GAIN =	 1.62578613134411f;		// (2.0 / IDWT_LO_GAIN)
	const	float	IDWT_RENORM	 =	 8192.0f;			// 1 << 13
	const	float	IDWT_NORM	 =	 0.0001220703125f;	// (1.0 / IDWT_RENORM)


	int GpuInit(GpuInfo * gpfInfo);
	int GpuRemove(GpuInfo * gpfInfo);

	void	MallocGpuBuf(CudaBuf * buf);
	void	FreeGpuBuf(CudaBuf * buf);

	StreamId	CreateANewStream();
	void DestroyStream(StreamId * async_id);

	void GpuSync(StreamId * P_asyncId);

	void	Gpu2DCopy(CudaBuf * stripeBuf, CudaBuf * deviceBuf,int src_x_off, int src_y_off, int dest_x_off, int dest_y_off, int width, int height, StreamId * P_asyncId, CudaCopyDirection direction, int src_no_pitch, int dest_no_pitch);
	void	Gpu2DCopyWithDiffPitch(CudaBuf * srcBuf, CudaBuf * destBuf, int src_x_off, int src_y_off, int dest_x_off, int dest_y_off, int src_pitch, int dest_pitch, int width, int height, StreamId *async_id, CudaCopyDirection direction, int src_no_pitch, int dest_no_pitch );
	void	GpuLinearMemCopy(CudaBuf * srcBuf, CudaBuf * destBuf, int copy_size_in_byte, StreamId async_id, CudaCopyDirection direction );

#if !GPU_W9X7_FLOAT
	int BMI_i97_dequantize_subband(CudaBuf * buf, int tile_off_x, int tile_off_y, SubbandInfo_c * subbandInfo, short wordShift, StreamId asyncId);
	int BMI_i97_dequantize_component(CudaBuf * buf, TileInfo_c * tileInfo,int topDeqLevel,  int compId, short wordShift, StreamId asyncId);
#endif 

	int BMI_comp_idwt(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId,short wordShift, EncodeMathod mathod, int toplevel);
	int BMI_resolution_idwt(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId,short wordShift, EncodeMathod mathod, int resolutionLevel);

	void	BMI_tile_MCT(TileInfo_c * tileInfo, short bitDepth, short wordShift,  int dwtlevel, int decodeLevel, OutputFormat outFormat, StreamId asyncId);
	void	BMI_tile_ICT(TileInfo_c * tileInfo, short bitDepth, short wordShift,  int dwtlevel, int decodeLevel,OutputFormat outFormat, StreamId asyncId);
	void	BMI_tile_Merge(TileInfo_c * tileInfo, int componentNum, int dwtLevel, int decodeLevel, short bitDepth, short wordShift, int compBufPitchBytes, OutputFormat outFormat, EncodeMathod method,StreamId asyncId);
	void	BMI_tile_One_Component(TileInfo_c * tile, int component_id,  EncodeMathod method, int hasMCT, int bitDepth, int wordShift,int dwtLevel,  int decodeLevel, StreamId asyncId);
// 	void	BMI_tile_One_Component_orig(TileInfo_c * tile, int component_id,  EncodeMathod method, int bitDepth, int wordShift,int dwtLevel,  int decodeLevel, StreamId asyncId);

	void	BMI_Comp_DC_SHIFT(CudaBuf * compBuf, CudaBuf * compBufTemp, EncodeMathod methos, int src_x_off, int src_y_off,  
							int dst_x_off, int dst_y_off, int width, int height, int bitDepth, int pixelWordLength, StreamId asyncId);
#if GPU_T1_TESTING
int BMI_gpu_decode_codeblock();
#endif


#ifdef __cplusplus
	}	// extern "C"
#endif

#endif	//_GPU_LIB_H
