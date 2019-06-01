#ifndef		_GPU_FUNC_H
#define		_GPU_FUNC_H

#include <stdio.h>
#include <assert.h>

#if defined MAC_OS || defined LINUX
#include "../../common/include/platform.h"
#include "../include/cudainterfacedatatype.h"
#include "../../codec/include/codec_macro.h"
#include "../include/gpu_setting.h"
#else
#include "..\..\common\include\platform.h"
#include "..\include\cudainterfacedatatype.h"
#include "..\..\codec\include\codec_macro.h"
#include "..\include\gpu_setting.h"
#endif

#define		PixelsToBytes(x,type)		((x)<<(type))
#define		BytesToPixels(x,type)		((x)>>(type))


#ifdef __cplusplus
extern "C" {
#endif


	//////////////////////////////////////////////////////////////////////
	// defines
	//////////////////////////////////////////////////////////////////////
#define bmi_cuda_call(err)		__bmi_cuda_call(err, __FILE__, __LINE__)
#define bmi_cuda_call_ign(err)	__bmi_cuda_call_ign(err, __FILE__, __LINE__)

int	bmi_get_gpu_info(GpuInfo *gpu);
int	bmi_del_gpu_info(GpuInfo *gpu);

void malloc_gpu_buf(CudaBuf * buf);
void delete_gpu_buf(CudaBuf * buf);

StreamId bmi_gpu_stream_new();
void bmi_gpu_stream_delete(StreamId  * gpu_str);

void bmi_sync_job(StreamId * P_asyncId);

int memory_copy_2D(void * dest_buf, size_t dest_pitch, void * src_buf,   size_t src_pitch, size_t width, size_t height, StreamId * async_id, int direction);
int memory_copy_linear(void * destBuf, void * srcBuf, int copy_size_in_byte, StreamId async_id, int direction);

// int memory_copy_host_to_device_2D( void * host_buf, void * device_buf,  size_t host_pitch, size_t device_pitch, size_t width, size_t height, StreamId  async_id);
// int memory_copy_device_to_host_2D( void * host_buf, void * device_buf,  size_t host_pitch, size_t device_pitch, size_t width, size_t height, StreamId  async_id);
// int memory_copy_device_to_device_2D( void * dest_buf, void * src_buf,  size_t dest_pitch, size_t src_pitch, size_t src_width, size_t src_height, StreamId  async_id);
// int	 bmi_i97_dequantize_component(CudaBuf * deviceBuf, TileInfo_c * tileInfo, int compId, short wordShift, StreamId asyncId);
#if !GPU_W9X7_FLOAT
int	 bmi_i97_dequantize_subband(CudaBuf * deviceBuf, int tile_off_x, int tile_off_y, SubbandInfo_c * subbandInfo, short wordShift, StreamId asyncId);
void bmi_idwt_i97_gpu_transform(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, int toplevel);
#endif

void bmi_idwt_r53_gpu_transform(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, int toplevel);
void bmi_idwt_i97_gpu_transform_float(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, int toplevel);

int bmi_MCT_gpu(TileInfo_c * tile, int bitDepth, int wordShift,  int dwtLevel, int decodeLevel,   int elementPerPixel, int bitsPerElement,StreamId asyncId);
int bmi_ICT_gpu(TileInfo_c * tile, int bitDepth, int wordShift, int dwtLevel, int decodeLevel,  int elementPerPixel, int bitsPerElement, StreamId asyncId);
// int bmi_merge_gpu(TileInfo_c * tileInfo, int decodeLevel, short bitDepth, short wordShift, int red_comp, int green_comp,  int blue_comp, EncodeMathod method, StreamId asyncId);
int bmi_merge_gpu(TileInfo_c * tile, int componentNum, int dwtLevel, int decodeLevel, short bitDepth, short wordShift, int inPitchBytes, int elementPerPixel, int bitsPerElement, EncodeMathod method, StreamId asyncId);
int bmi_get_one_component(TileInfo_c * tile, int component_id,  EncodeMathod method, int hasMCT, int bitDepth, int wordShift,int decodeLevel,  StreamId asyncId);
int bmi_get_one_component_orig(TileInfo_c * tile, int component_id, EncodeMathod method, int bitDepth, int wordShift, int decodeLevel, StreamId asyncId);
int	 bmi_dc_shift(CudaBuf * compBuf, CudaBuf * compBufTemp, int src_x_off, int src_y_off, int dst_x_off, int dst_y_off, int widthInPixel, int heightInpixel, int pixelWordLength, int bit_depth, int isFloat, StreamId asyncId);


// void bmi_idwt_i97_gpu_transform_resolution(CudaBuf * orig, CudaBuf * temp,				// input suda buffer and temp cuda buffer used for DWT
// 										   int x_off, int y_off,						// x-offset and y-offset in input buffer
// 										   int lowW, int highW, int lowH, int highH,	// the width and height for the 4 subbands in this resotion
// 										   int xParity, int yParity,					// the parity for vertial and horizontal
// 										   float coef0, float coef1, float coef2, float coef3,	// the 4 coefiient for dwt calculation, for original data will be NORM * qqStep * logain/hiGain;
// 										   bool isFinal,								// Is this dwt the top DWT? if yes will do the RENORM
// 										   short wordlength, StreamId asyncId);			// wordlength of input buffer and the asyncId
void bmi_idwt_i97_gpu_transform_resolution(CudaBuf * orig, CudaBuf * temp, 
										   int x_off, int y_off,
										   int lowW, int highW, int lowH, int highH,
										   int xParity, int yParity,
										   float coef0, float coef1, float coef2, float coef3, 
										   int	isLLBandFloat,
										   short wordlength, StreamId asyncId);
void bmi_idwt_r53_gpu_transform_resolution(CudaBuf * orig, CudaBuf * temp, 
										   int x_off, int y_off,
										   int lowW, int highW, int lowH, int highH,
										   int xParity, int yParity,
										   int	isLLBandOrig,
										   short wordlength, StreamId asyncId);

#if GPU_T1_TESTING
int gpu_decode_coderblock();
#endif

#ifdef __cplusplus
	}
#endif
#endif	// _GPU_FUNC_H