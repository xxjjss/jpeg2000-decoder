
#include "gpufunc.h"
#include <cuda_runtime.h>
#include "bmi_dwt_gpu_kernel.h"


int		GPU_NUMBER	=	182;

	inline void	__bmi_cuda_call(cudaError err, const char *file, const int line)
	{
		if(err != cudaSuccess) {

			printf(
				"ERROR: CUDA Runtime error in file <%s>, line %i:  %s\n",
				file, line, cudaGetErrorString(err) );
#if !_DEBUG
			exit(-1);
#else
			assert(0);
#endif
		}
	}

	inline	void	__bmi_cuda_call_ign(cudaError err, const char *file, const int line)
	{
		if(err != cudaSuccess) {
			// reset error state
			err = cudaGetLastError();
// 			bmi_eprintf((
// 				"WARNING:  CUDA Runtime error in file <%s>, line %i:  %s\n",
// 				file, line, cudaGetErrorString(err) )
// 				);
		}
	}

////////////////////////////////////////////////////////////////////////////////
/////////		Internal functions			///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define BMI_CEILINT(x,y) ((x/y) + ((x%y) ? 1 : 0))

int bmi_idwt_calc_row_grid(dim3 *grid, dim3 *block, int length, int rows)
{
	// set block size to tile width
	block->x = IDWT_TILE_WIDTH;
	block->y = IDWT_TILE_WIDTH;
	// set the grid dimension based on the block dimension
	grid->x = BMI_CEILINT(BMI_CEILDIV2(length), block->x);
	grid->y = BMI_CEILINT(rows, block->y);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int	bmi_get_gpu_info(GpuInfo *gpu)
{
	// locals
	int count;
	int dev;
	cudaDeviceProp prop;
	// 	char format_s[] = "  %-20s  %s\n";
	// 	char format_d[] = "  %-20s  %d\n";

	// get the number of devices.  for now, we'll only support one.
	bmi_cuda_call_ign(cudaGetDeviceCount(&count));
	if (count == 0) { //***UUU No ! this is not the way cudaGetDeviceCount() works
//		PRIN(("Warning:  CUDA not supported by GPUs in this system\n\n"));
		gpu->device = -1;
		return -1;
	}
	if (count > 1) {
//		bmi_eprintf(("Warning:  CUDA supported on %d devices in this system.  Only device 0 will be used.\n\n"));
		count = 1;
	}

	// get the info for devices
	assert(count == 1);
	for (dev = 0; dev < count; dev++)
	{
		if(cudaGetDeviceProperties(&prop, dev) == cudaSuccess)
		{
			break;
		}
	}
	
	if(prop.major == 9999)//***UUUU  No cuda enabled devices available
	{
		gpu->device = -1;
		return -1;
	}
	

	assert(dev != count);
 	gpu->major = prop.major;
 	gpu->minor = prop.minor;
 	strcpy(gpu->name, prop.name);
	gpu->device = dev;	
	gpu->multiProcessorCount = prop.multiProcessorCount;
	GPU_NUMBER = (gpu->multiProcessorCount<<3) ; // * 8 */
	/*
	Individual GPU program launches are limited to a run time of less than 5 seconds on 
	the device. Exceeding this time limit usually causes a launch failure reported through 
	the CUDA driver or the CUDA runtime, but in some cases hangs the entire machine, 
	requiring a hard reset. Microsoft Windows has a "watchdog" timer that causes programs 
	using the primary graphics adapter to time out if they run longer than the maximum 
	allowed time. For this reason it is recommended that CUDA is run on a G80 that is NOT 
	attached to a display and does not have the Windows desktop extended onto it. In this 
	case, the system must contain at least one NVIDIA GPU that serves as the primary graphics 
	adapter.

	SHIT!!! if we reserve one processor, 10-msec slower!
	*/
	gpu->clockRate = prop.clockRate;
	gpu->memory = (int)(prop.totalGlobalMem);
	// set device
	gpu->active = 1;
	bmi_cuda_call_ign(cudaSetDevice(gpu->device));

	// check for any miscellaneous errors hanging around
	bmi_cuda_call_ign(cudaThreadSynchronize());

	return 0;
}

int	bmi_del_gpu_info(GpuInfo *gpu) 
{
	// just reset the thread state
	bmi_cuda_call(cudaThreadExit());
	gpu->active = 0;
	return 0;
}


void malloc_gpu_buf(CudaBuf * buf)
{

	assert(buf->mBuf == NULL);

	switch(buf->mType)
	{
	case GPU_BUF:
		if (buf->mWidth != 0 && buf->mHeight != 0)
		{
			buf->mSize = buf->mWidth * buf->mHeight;
		}
		buf->mPitch = buf->mWidth;
		bmi_cuda_call(cudaMalloc(&buf->mBuf, buf->mSize));
		break;

	case GPU_ALLIGN_BUF:
		bmi_cuda_call(cudaMallocPitch(&buf->mBuf, (size_t *)&buf->mPitch, (size_t)buf->mWidth, (size_t)buf->mHeight));
		buf->mSize = buf->mPitch * buf->mHeight;
		break;

	case HOST_ALLLIGN_BUF:
	case HOST_BUF:
		buf->mSize = buf->mWidth * buf->mHeight;
		buf->mPitch = buf->mWidth;
		bmi_cuda_call(cudaMallocHost(&buf->mBuf, buf->mSize));
		break;
	default:
		assert(0);
		break;
	}

}

void delete_gpu_buf(CudaBuf * buf)
{

	switch(buf->mType)
	{
	case GPU_BUF:
	case GPU_ALLIGN_BUF:
		bmi_cuda_call(cudaFree(buf->mBuf));
		break;

	case HOST_ALLLIGN_BUF:
	case HOST_BUF:
		bmi_cuda_call(cudaFreeHost(buf->mBuf));
		break;

	default:
		assert(0);
		break;
	}
}

StreamId bmi_gpu_stream_new()
{
	StreamId ret = 0;
	bmi_cuda_call(cudaStreamCreate(&ret));
	return ret;
}

void bmi_gpu_stream_delete(StreamId  * P_gpu_str)
{
	// clean up stream context
	if (P_gpu_str)
	{
		bmi_cuda_call(cudaStreamDestroy(* P_gpu_str));
	}

}


void bmi_sync_job(StreamId * P_asyncId)
{
	if (P_asyncId)
	{
		bmi_cuda_call(cudaStreamSynchronize(*P_asyncId));
	}
	else	// sync all unfinished job
	{
		bmi_cuda_call(cudaThreadSynchronize());
	}
}

int memory_copy_linear(void * dest_buf, void * src_buf, int copy_size_in_byte, StreamId async_id, int direction)
{
	int dir;
	if		(direction == CUDA_COPY_HOST_TO_DEVICE)		dir = cudaMemcpyHostToDevice;
	else if (direction == CUDA_COPY_HOST_TO_HOST)		dir = cudaMemcpyHostToHost;
	else if (direction == CUDA_COPY_DEVICE_TO_DEVICE)	dir = cudaMemcpyDeviceToDevice;
	else if (direction == CUDA_COPY_DEVICE_TO_HOST)		dir = cudaMemcpyDeviceToHost;
	else	assert(0);

	if (async_id > 0)
	{
		bmi_cuda_call(cudaMemcpyAsync( dest_buf,  src_buf, copy_size_in_byte, (cudaMemcpyKind)dir, async_id ) );
	}
	else
	{
		bmi_cuda_call(cudaMemcpy( dest_buf,  src_buf, copy_size_in_byte, (cudaMemcpyKind)dir ) );
	}
	return 0;

}

int memory_copy_2D(void * dest_buf, size_t dest_pitch, void * src_buf,   size_t src_pitch, size_t width, size_t height, StreamId *  P_asyncId, int direction  )
{

	int dir;
	if		(direction == CUDA_COPY_HOST_TO_DEVICE)		dir = cudaMemcpyHostToDevice;
	else if (direction == CUDA_COPY_HOST_TO_HOST)		dir = cudaMemcpyHostToHost;
	else if (direction == CUDA_COPY_DEVICE_TO_DEVICE)	dir = cudaMemcpyDeviceToDevice;
	else if (direction == CUDA_COPY_DEVICE_TO_HOST)		dir = cudaMemcpyDeviceToHost;
	else	assert(0);

	if (P_asyncId)
	{
		bmi_cuda_call(cudaMemcpy2DAsync( dest_buf,  dest_pitch, src_buf,  src_pitch,  width,  height, (cudaMemcpyKind)dir, * P_asyncId ) );
	}
	else
	{
 		bmi_cuda_call(cudaMemcpy2D( dest_buf,  dest_pitch, src_buf,  src_pitch,  width,  height, (cudaMemcpyKind)dir));
	}
	return 0;
}

// 
// int memory_copy_device_to_host_2D (void * host_buf, void * device_buf,  size_t host_pitch, size_t device_pitch, size_t width, size_t height, StreamId  async_id)
// {
// 	if (async_id > 0)
// 	{
// 		bmi_cuda_call(cudaMemcpy2DAsync( host_buf,  host_pitch, device_buf,  device_pitch,  width,  height, cudaMemcpyDeviceToHost, async_id ) );
// 	}
// 	else
// 	{
//  		bmi_cuda_call(cudaMemcpy2D(host_buf,  host_pitch, device_buf,  device_pitch,  width,  height, cudaMemcpyDeviceToHost));
// 	}
// 	return 0;
// }
// 
// int memory_copy_device_to_device_2D( void * dest_buf, void * src_buf,  size_t dest_pitch, size_t src_pitch, size_t width, size_t height, StreamId  async_id)
// {
// 		if (async_id > 0)
// 	{
// 		bmi_cuda_call(cudaMemcpy2DAsync( dest_buf,  dest_pitch, src_buf,  src_pitch,  width,  height, cudaMemcpyDeviceToDevice, async_id ) );
// 	}
// 	else
// 	{
//  		bmi_cuda_call(cudaMemcpy2D(dest_buf,  dest_pitch, src_buf,  src_pitch,  width,  height, cudaMemcpyDeviceToDevice));
// 	}
// 	return 0;
// }
// 
// 
// int memory_copy_host_to_device_2D( void * host_buf, void * device_buf,  size_t host_pitch, size_t device_pitch, size_t width, size_t height, StreamId  async_id)
// {
// 	if (async_id > 0)
// 	{
// 		bmi_cuda_call(cudaMemcpy2DAsync( device_buf,  device_pitch, host_buf,  host_pitch,  width,  height, cudaMemcpyHostToDevice, async_id ) );
// 	}
// 	else
// 	{
//  		bmi_cuda_call(cudaMemcpy2D(device_buf,  device_pitch, host_buf,  host_pitch,  width,  height, cudaMemcpyHostToDevice));
// 	}
// 	return 0;
// }

// int	 bmi_i97_dequantize_component(CudaBuf * deviceBuf, TileInfo_c * tileInfo, int compId, short wordShift, StreamId asyncId)
// {
// 		// 	dim3 block_size;
// 
// 	int blocknum = tileInfo->mSize.y;
// 
// 	int threadnum = GPU_NUMBER < tileInfo->mSize.x ? GPU_NUMBER :  tileInfo->mSize.x;
// 
// 	if (asyncId <= 0)
// 	{
// 		bmi_i97_dequantize_transfer<<<blocknum, threadnum, 0 >>>(
// 				(int *)deviceBuf->mBuf,
// 				tileInfo->mOff.x,
// 				tileInfo->mOff.x,
// 				BytesToPixels(deviceBuf->mPitch, wordShift),
// 				tileInfo->mSize.x,
// 				tileInfo->mSize.y,
// 				subbandInfo->mAbsStep,
// 				threadnum,
// 				subbandInfo->mQquanShift
// 				);	
// 	}
// 	else
// 	{
// 		bmi_i97_dequantize_transfer<<<blocknum, threadnum, 0, asyncId >>>(
// 				(int *)deviceBuf->mBuf,
// 				subbandInfo->mOff.x + tile_off_x,
// 				subbandInfo->mOff.y + tile_off_y,
// 				BytesToPixels(deviceBuf->mPitch, wordShift),
// 				subbandInfo->mSize.x,
// 				subbandInfo->mSize.y,
// 				subbandInfo->mAbsStep,
// 				threadnum,
// 				subbandInfo->mQquanShift
// 				);	
// 	}
// 	
// 
// 	return 0;
// }

#if !GPU_W9X7_FLOAT
int	 bmi_i97_dequantize_subband(CudaBuf * deviceBuf, int tile_off_x, int tile_off_y, SubbandInfo_c * subbandInfo,short wordShift,  StreamId asyncId)
{
	// 	dim3 block_size;

	int blocknum = subbandInfo->mSize.y;

	int threadnum = GPU_NUMBER < subbandInfo->mSize.x ? GPU_NUMBER :  subbandInfo->mSize.x;

	if (asyncId <= 0) 
	{
		bmi_i97_dequantize_transfer<<<blocknum, threadnum, 0 >>>( //***UUU use <<<blocknum, threadnum, 0 , (asyncId <= 0 ? 0 : asyncId) >>>
				(int *)deviceBuf->mBuf,
				subbandInfo->mOff.x + tile_off_x,
				subbandInfo->mOff.y + tile_off_y,
				BytesToPixels(deviceBuf->mPitch, wordShift),
				subbandInfo->mSize.x,
				subbandInfo->mSize.y,
				subbandInfo->miAbsStep,
				threadnum,
				subbandInfo->mQquanShift
				);	
	}
	else //****UUU This repitition is not needed, one for asyncId = 0 and asyncId > 0
	{
		bmi_i97_dequantize_transfer<<<blocknum, threadnum, 0, asyncId >>>(
				(int *)deviceBuf->mBuf,
				subbandInfo->mOff.x + tile_off_x,
				subbandInfo->mOff.y + tile_off_y,
				BytesToPixels(deviceBuf->mPitch, wordShift),
				subbandInfo->mSize.x,
				subbandInfo->mSize.y,
				subbandInfo->miAbsStep,
				threadnum,
				subbandInfo->mQquanShift
				);	
	}
	

	return 0;
}
#endif

int	 bmi_dc_shift(CudaBuf * compBuf, CudaBuf * compBufTemp, int src_x_off, int src_y_off, int dst_x_off, int dst_y_off, int widthInPixel, int heightInpixel, int pixelWordLength, int bit_depth, int isFloat, StreamId asyncId)
{
	// 	dim3 block_size;

	int blocknum = heightInpixel;

	int threadNum = widthInPixel;
	while(threadNum > MAX_THREADS)
	{
		threadNum = (threadNum + 1) /2;
	}

		bmi_dc_shift_kernel<<<blocknum, threadNum, 0,(asyncId <= 0 ? 0 : asyncId) >>>( //***UUU use <<<blocknum, threadnum, 0 , (asyncId <= 0 ? 0 : asyncId) >>>
				(int *)compBuf->mBuf,
				(int *)compBufTemp->mBuf,
				src_x_off,
				src_y_off,
				dst_x_off,
				dst_y_off,
				(compBuf->mPitch / pixelWordLength),
				(compBuf->mPitch / pixelWordLength),//use the same pitch
				widthInPixel,
				threadNum,
				isFloat, 
				bit_depth
				);	
	
	

	return 0;
}


void bmi_idwt_r53_gpu_transform(TileInfo_c * tile,CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, int toplevel)
{

// 	printf("doing DWT for a component , use async %d\n", asyncId);
	// kernel launch variables
	dim3 block_size;
	dim3 grid_size;
	int tile_off_y = tile->mOff.y;
	int tile_off_x = tile->mOff.x;
	int dwtLevel = (comp->mNumOfSubband - 1) /3 ;
	if (toplevel != -1)
	{
		dwtLevel = (dwtLevel > toplevel ? toplevel : dwtLevel);
	}


	unsigned int  texture_orig_size = orig->mPitch * tile_off_y;
	// now it's the y offset

	unsigned char * texture_orig_buf = (unsigned char *)orig->mBuf + texture_orig_size /*+ PixelsToBytes(tile_off_x, wordShift)*/;
	texture_orig_size = 	orig->mSize - texture_orig_size;
	// now we got the real size
		
	unsigned int  texture_temp_size = temp->mPitch * tile_off_x;
	// now it's the offset

	unsigned char * texture_temp_buf = (unsigned char *)temp->mBuf + texture_temp_size /*+ PixelsToBytes(tile_off_y, wordShift)*/;
	texture_temp_size = 	temp->mSize - texture_temp_size;
	// now we got the real size

	// iterate over all the levels.  each call of the kernel processes a row
	// and transposes the data.  each level requires two passes.


	const struct textureReference* texRefPtr; //***UUU
	cudaGetTextureReference(&texRefPtr, "tc_tex_int"); //***UUUU
	size_t a=0; //****UUUU
	cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc(32, 0, 0, 0,cudaChannelFormatKindSigned); //***UUU
	
	for (int level = 0; level < dwtLevel; level++)
	{
		// shorthand variables
		// - pitch info
		int d_pitch  = BytesToPixels(orig->mPitch, wordShift);
		int t_pitch  = BytesToPixels(temp->mPitch, wordShift);
		// - row info
		int r_par    = tile->mXParity[level] ; /*comp->mSubbandInfo[level * 3 + 2].mOff.x;*/
		int r_len    = comp->mSubbandInfo[level * 3 + 2].mSize.x + comp->mSubbandInfo[level * 3 + 3].mSize.x;
		int r_len_lo = comp->mSubbandInfo[level * 3 + 2].mSize.x;
		// - column info
		int c_par    = tile->mYParity[level] ; /*comp->mSubbandInfo[level * 3 + 1].mOff.y; */
		int c_len    = comp->mSubbandInfo[level * 3 + 1].mSize.y + comp->mSubbandInfo[level * 3 + 3].mSize.y;
		int c_len_lo = comp->mSubbandInfo[level * 3 + 1].mSize.y;

		// launch the kernel on the rows
		

		bmi_idwt_calc_row_grid(&grid_size, &block_size, r_len, c_len);
		bmi_cuda_call(cudaBindTexture(&a, texRefPtr/*tc_tex_int*/, (void *)texture_orig_buf, &channelDesc, texture_orig_size));
		//assert(a == 0); //***UUU remove &a, texRefPtr and &channelDesc
		if (wordShift == 2 )	// INT_32_BITS
		{
			if (asyncId <= 0)
			{
				bmi_idwt_r53_1d_gpu_row_int32<<<grid_size, block_size, 0>>>
					((int *)texture_temp_buf + tile_off_y, tile_off_x, t_pitch, d_pitch, r_len, r_len_lo, r_par, c_len);

			}
			else
			{
				bmi_idwt_r53_1d_gpu_row_int32<<<grid_size, block_size, 0>>>
					((int *)texture_temp_buf + tile_off_y, tile_off_x, t_pitch, d_pitch, r_len, r_len_lo, r_par, c_len);

			}
		}
		else
		{
			assert(0); // not support yet
// 			assert(wordShift == 1 );	// SHORT_16_BITS
// 
// 			if (asyncId <= 0)
// 			{
// 				bmi_idwt_r53_1d_gpu_row_short16<<<grid_size, block_size, 0>>>
// 					((short *)temp->mBuf, 0,t_pitch, d_pitch, r_len, r_len_lo, r_par, c_len, true);
// 			}
// 			else
// 			{
// 				bmi_idwt_r53_1d_gpu_row_short16<<<grid_size, block_size, 0, asyncId>>>
// 					((short *)temp->mBuf, 0,t_pitch, d_pitch, r_len, r_len_lo, r_par, c_len, true);
// 			}

		}

		// launch the kernel on the columns
		bmi_idwt_calc_row_grid(&grid_size, &block_size, c_len, r_len);
		bmi_cuda_call(cudaBindTexture(0, tc_tex_int, (void *)texture_temp_buf, texture_temp_size));
		if (wordShift == 2 )	// INT_32_BITS
		{
			if (asyncId <= 0)
			{
				bmi_idwt_r53_1d_gpu_row_int32<<<grid_size, block_size, 0>>>
					((int *)texture_orig_buf + tile_off_x,tile_off_y,  d_pitch, t_pitch, c_len, c_len_lo, c_par, r_len);

			}
			else
			{
				bmi_idwt_r53_1d_gpu_row_int32<<<grid_size, block_size, 0, asyncId>>>
					((int *)texture_orig_buf + tile_off_x,tile_off_y,d_pitch, t_pitch, c_len, c_len_lo, c_par, r_len);
			}

		}
		else
		{
			assert(0); // not support yet
// 			assert(wordShift == 1 );	// SHORT_16_BITS
// 			if (asyncId <= 0)
// 			{
// 				bmi_idwt_r53_1d_gpu_row_short16<<<grid_size, block_size, 0>>>
// 				((short *)orig->mBuf, 0,d_pitch, t_pitch, c_len, c_len_lo, c_par, r_len, false);
// 			}
// 			else
// 			{
// 				bmi_idwt_r53_1d_gpu_row_short16<<<grid_size, block_size, 0, asyncId>>>
// 				((short *)orig->mBuf,0,d_pitch, t_pitch, c_len, c_len_lo, c_par, r_len, false);
// 			}
		}
	}
}


void 	SetGridSize(dim3 *grid_size, dim3 * block_size, int lines, int rows)
{

	int SM = GPU_NUMBER / 8;
	grid_size->z = 1;
	if (lines <= SM)
	{
		grid_size->x = lines;
		grid_size->y = 1;
	}
	else
	{
		grid_size->x = SM;
		grid_size->y =(lines + SM - 1) / SM;
	}
	// lineId = grid_size.y * SM + grid_size.x

	int lineLength = rows;
	while (((lineLength + 2) * 4 ) > 16000)	// count't store a line in one SM shared memory
		// 2 : we need 2 random data at the front /end of the line
	{
		++grid_size->z;
		lineLength = (rows + grid_size->z - 1) / grid_size->z;
	}
	// grid_z used for calculate line_offset

	block_size->z = 1;
	if (lineLength <= 64)	// 64 = 32 * 2 :32  threads number of each warp
	{
		block_size->x = (lineLength + 1) / 2;
		block_size->y = 1;
	}
	else
	{
		block_size->x = 32;		// threads number of each warp
		lineLength = (lineLength + 1 ) / 2;	// each thread handle 2 pixels
		// w_num * 32 * loop >= lineLength	: w_num: warp number, less than 24,  loop : each thread loop number 
		// w_num * loop >= lineLength / 32
		int loop = 1;

		lineLength = (lineLength + 31) / 32;
// 		int w_num;
		do 
		{
			block_size->y = (lineLength + loop - 1) / loop;
			++loop;
		} while (block_size->y > 24);
// 		block_size->y = w_num;
	}

	// threadId = y * 32 + x
}

void bmi_idwt_r53_gpu_transform_resolution(CudaBuf * orig, CudaBuf * temp,				// input suda buffer and temp cuda buffer used for DWT
										   int x_off, int y_off,						// x-offset and y-offset in input buffer
										   int lowW, int highW, int lowH, int highH,		// the width and height for the 4 subbands in this resotion
										   int xParity, int yParity,					// the parity for vertial and horizontal
										   int	isLLBandOrig,								// Is this dwt the first DWT? 
										   short wordlength, StreamId asyncId)			// wordlength of input buffer and the asyncId
{
#pragma BMI_PRAGMA_TODO_MSG(("Do DWT for W5X3 by resolution"))
	assert(0);

}


void bmi_idwt_i97_gpu_transform_resolution(CudaBuf * orig, CudaBuf * temp,				// input suda buffer and temp cuda buffer used for DWT
										   int x_off, int y_off,						// x-offset and y-offset in input buffer
										   int lowW, int highW, int lowH, int highH,		// the width and height for the 4 subbands in this resotion
										   int xParity, int yParity,					// the parity for vertial and horizontal
										   float coef0, float coef1, float coef2, float coef3,	// the 4 coefiient for dwt calculation, for original data will be NORM * qqStep * logain/hiGain;
																								// for data from last level will be logain / highgain
										   int	isLLBandFloat,								// Is this dwt the first DWT? if yes will do the RENORM transfer to int
										   short wordlength, StreamId asyncId)			// wordlength of input buffer and the asyncId
{
#if 1
	{

		// buffer pointers
		// kernel launch variables
		dim3 block_size;
		dim3 grid_size;


		unsigned int  texture_orig_size = orig->mPitch * y_off;
		// now it's the y offset

		unsigned char * texture_orig_buf = (unsigned char *)orig->mBuf + texture_orig_size /*+ PixelsToBytes(tile_off_x, wordShift)*/;
		texture_orig_size = 	orig->mSize - texture_orig_size;
		// now we got the real size

		unsigned int  texture_orig_sizeH = orig->mPitch * (y_off + lowH);
		// now it's the y offset

		unsigned char * texture_orig_bufH = (unsigned char *)orig->mBuf + texture_orig_sizeH /*+ PixelsToBytes(tile_off_x, wordShift)*/;
		texture_orig_sizeH = 	orig->mSize - texture_orig_sizeH;

		unsigned int  texture_temp_size = temp->mPitch * x_off;
		// now it's the offset
		unsigned char * texture_temp_buf = (unsigned char *)temp->mBuf + texture_temp_size /*+ PixelsToBytes(tile_off_y, wordShift)*/;
		texture_temp_size = 	temp->mSize - texture_temp_size;
		// now we got the real size



			// - pitch info
			int d_pitch  = orig->mPitch / 4;
			int t_pitch  = temp->mPitch / 4;
			// - row info

			int r_len    = lowW + highW;
			int r_len_lo = lowW;
			// - column info
			int c_len    = highH + lowH ;
			int c_len_lo = lowH;

			// launch the kernel on the rows

			if (asyncId <= 0)
			{
			bmi_idwt_calc_row_grid(&grid_size, &block_size, r_len, lowH);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_buf, texture_orig_size));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_temp_buf + y_off, x_off, t_pitch, d_pitch, r_len, r_len_lo, xParity, lowH, coef0, coef1, isLLBandFloat,0);
			bmi_idwt_calc_row_grid(&grid_size, &block_size, r_len, highH);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_bufH, texture_orig_sizeH));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_temp_buf + y_off + lowH, x_off, t_pitch, d_pitch, r_len, r_len_lo, xParity, highH, coef2, coef3,0,0);
			}
			else
			{
			bmi_idwt_calc_row_grid(&grid_size, &block_size, r_len, lowH);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_buf, texture_orig_size));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0, asyncId>>>
					((float *)texture_temp_buf + y_off, x_off, t_pitch, d_pitch, r_len, r_len_lo, xParity, lowH, coef0, coef1, isLLBandFloat, 0);
			bmi_idwt_calc_row_grid(&grid_size, &block_size, r_len, highH);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_bufH, texture_orig_sizeH));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_temp_buf + y_off + lowH, x_off, t_pitch, d_pitch, r_len, r_len_lo, xParity, highH, coef2, coef3,0, 0);
			}


			// launch the kernel on the columns
			bmi_idwt_calc_row_grid(&grid_size, &block_size, c_len, r_len);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_temp_buf, texture_temp_size));
			//			bmi_cuda_call(cudaBindTexture(0, tc_tex_int, temp->mBuf, temp->mSize));

			if (asyncId <= 0)
			{
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_orig_buf + x_off,y_off,  d_pitch, t_pitch, c_len, c_len_lo, yParity, r_len, IDWT_LO_GAIN, IDWT_HI_GAIN, 1, 1);
			}
			else
			{
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0, asyncId>>>
					((float *)texture_orig_buf + x_off, y_off,  d_pitch, t_pitch, c_len, c_len_lo, yParity, r_len, IDWT_LO_GAIN, IDWT_HI_GAIN,1, 1);
			}

		return;
	}
#else
	{


#define BYTES2PIXELS(x)		((x)/4)
#define PIXELS2BYTES(x)		((x)*4)

#define BLOCK_DIM 16

		int  lines = lowH + highH;
		int	 rows = lowW + highW;

		// assume each thread handle one pixels at a time
		// If each block we set 32N threads, calculate for 64N pixels (N is the number of warp, up to 24, 768 threads)
		// like below :  R is random, lower case is loaded form orig and upper case means calcualted data; ' means will be ignore
		// Each block we can calculate at least thredNum * 2 - 8 = 64N - 8 pixels :
		// shared memory we need 2 more pixels for random, size will be (64N + 2) * 4
		// considering for 2K image, 
		// In horizontal DWT, 1080 lines for 2048 pixels, we can set 1080 blocks, each block has 18 * 32 = 576 threadsneed, need 8K+ shard memory, each SM one block
		// each thread calculate 4 pixels, good for 576 * 4 - 8 = 2296 pixels
		// In horizontal DWT, 2048 rows for 1080 pixels, we can set 2048 blocks, each block has 10 * 32 = 320 threadsneed, need 4K+ shard memory, each SM 2 block
		// each thread calculate 4 pixels, good for 320 * 4 - 8 = 1272 pixels

		// 97 no parity:	NORMAL case : throw away 8 pixels, 
		// 	R	h'	l'	h'|	l	h	l	h |	l'	h'	l'	h'	l'	R
		// 			L'	  |	L		L	  |	L'		L'		L'
		// 		H'		H'|		H		H |		H'		H'
		// 			L'	  |	L		L	  |	L'		L'		L'
		// 		H'		H'|		H		H |		H'		H'
		// 97 no parity:odd	AT THE END OF LINE : throw away 7 pixels, 
		// 	R	h'	l'	h'|	l	h	l |	h' 	l'	h''	l'	R
		// 			L'	  |	L		L |	  	L		L''		
		// 		H'		H'|		H	  |	H' 		H'
		// 			L'	  |	L		L |	  	L'		L'
		// 		H'		H'|		H	  |	H' 		H'
		// 97 parity:	AT THE BEGINING OF LINE : throw away 9 pixels, 
		// 	R	h'	l'	h'	l'|	h	l	h |	l'	h'	l'	h'	l'	R
		// 			L'	  	L'|		L	  |	L'		L'		L'
		// 		H'		H'	  |	H		H |		H'		H'
		// 			L'	  	L'|		L	  |	L'		L'		L'
		// 		H'		H'	  |	H		H |		H'		H'

		dim3 block_size;
		dim3 grid_size;


		// 	unsigned int  texture_orig_size = orig->mPitch * y_off;
		// 	// now it's the y offset
		// 
		// 	unsigned char * texture_orig_buf = (unsigned char *)orig->mBuf + texture_orig_size;
		// 	texture_orig_size = 	orig->mSize - texture_orig_size;
		// 	// now we got the real size of the original buffer
		// 		
		// 	unsigned int  texture_temp_size = temp->mPitch * x_off;
		// 	// now it's the offset
		// 	unsigned char * texture_temp_buf = (unsigned char *)temp->mBuf + texture_temp_size;

		float * bufL, * bufH, *dwtResult;
		int calLength;

		// 	texture_temp_size = 	temp->mSize - texture_temp_size;
		// 	// now we got the real size of the temp buffer

		float * tempDBG = (float *)malloc(2048*2048*4 + 256);

		float  * tempBuf  = tempDBG;
		tempBuf = (float *)((int)tempBuf & 0xfffffff0);



		// 		bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_buf, texture_orig_size));
		calLength = rows + (xParity ? 9 : (lowW == highW ? 8 : 7));
		SetGridSize(&grid_size, & block_size, lowH, calLength);
		bufL = (float *)((unsigned char *)orig->mBuf + y_off * orig->mPitch + PIXELS2BYTES(x_off));
		bufH = (float *)((unsigned char *)orig->mBuf + y_off * orig->mPitch + PIXELS2BYTES(x_off + lowW));
		dwtResult = (float *)((unsigned char *)temp->mBuf + x_off * temp->mPitch + PIXELS2BYTES(y_off));
		memory_copy_2D(tempBuf, (lowW+highW)*4,bufL, orig->mPitch, (lowW+highW)*4,lowH+highH, NULL,CUDA_COPY_DEVICE_TO_HOST  );
		gou_dwt97_1d_transfer<<<grid_size, block_size, (lowW+highW+11) * sizeof(float)/*, asyncId*/>>>(
			bufL,				//	float * lowBand,
			bufH,				// float * highBand
			dwtResult,			// float * dwtResult
			orig->mPitch,		// int inPitch
			temp->mPitch,		// int outPitch
			coef0 * IDWT_LO_GAIN, coef1 * IDWT_LO_GAIN,		// float lowCoef, float highCoef
			isLLBandFloat, 0,	//	int isLowBandFloat, int isHighBandFloat,
			lowH,				// int height
			lowW, highW,		// int low_w, int high_w 
			xParity,			// int parity
			grid_size.x,		// int yStep
			block_size.x * block_size.y,		// int x_step
			grid_size.z			// int lineParts
			);		

		memory_copy_2D(tempBuf, lowH*4,dwtResult, temp->mPitch, lowH*4, lowW+highW, NULL,CUDA_COPY_DEVICE_TO_HOST  );



		SetGridSize(&grid_size, & block_size, highH, calLength);
		dwtResult = (float *)((unsigned char *)temp->mBuf + x_off * temp->mPitch + PIXELS2BYTES(y_off + lowH));
		bufL = (float *)((unsigned char *)orig->mBuf + (y_off + lowH) * orig->mPitch + PIXELS2BYTES(x_off));
		bufH = (float *)((unsigned char *)orig->mBuf + (y_off + lowH) * orig->mPitch + PIXELS2BYTES(x_off + lowW));
		gou_dwt97_1d_transfer<<<grid_size, block_size, (lowW+highW+11) * sizeof(float)/*, asyncId*/>>>(
			bufL,				//	float * lowBand,
			bufH,				// float * highBand
			dwtResult,			// float * dwtResult
			orig->mPitch,		// int inPitch
			temp->mPitch,		// int outPitch
			coef2 * IDWT_HI_GAIN, coef3 * IDWT_HI_GAIN,		// float lowCoef, float highCoef
			0, 0,				//	int isLowBandFloat, int isHighBandFloat,
			highH,				// int height
			lowW, highW,		// int low_w, int high_w 
			xParity,			// int parity
			grid_size.x,		// int yStep
			block_size.x * block_size.y,		// int x_step
			grid_size.z			// int lineParts
			);		

		memory_copy_2D(tempBuf+lowH, highH*4,dwtResult, temp->mPitch, highH*4, lowW+highW, NULL,CUDA_COPY_DEVICE_TO_HOST  );

		calLength = lowH + highH + (yParity ? 9 : (lowH == highH ? 8 : 7));
		SetGridSize(&grid_size, & block_size, lowW + highW, calLength);
		dwtResult = (float *)((unsigned char *)orig->mBuf + y_off * orig->mPitch + PIXELS2BYTES(x_off));
		bufL  = (float *)((unsigned char *)temp->mBuf + x_off * temp->mPitch + PIXELS2BYTES(y_off));
		bufH  = (float *)((unsigned char *)temp->mBuf + x_off * temp->mPitch + PIXELS2BYTES(y_off + lowH));
		gou_dwt97_1d_transfer<<<grid_size, block_size, (lowH+highH+11) * sizeof(float)/*, asyncId*/>>>(
			bufL,				//	float * lowBand,
			bufH,				// float * highBand
			dwtResult,			// float * dwtResult
			temp->mPitch,		// int inPitch
			orig->mPitch,		// int outPitch
//			IDWT_LO_GAIN, IDWT_HI_GAIN,			// float lowCoef, float highCoef
			1.0, 1.0,
			1, 1,				//	int isLowBandFloat, int isHighBandFloat,
			lowW + highW,				// int height
			lowH, highH,		// int low_w, int high_w 
			yParity,			// int parity
			grid_size.x,		// int yStep
			block_size.x * block_size.y,		// int x_step
			grid_size.z			// int lineParts
			);		
		memory_copy_2D(tempBuf, 4*(lowW+highW),bufL, orig->mPitch, 4*(lowW+highW),lowH+highH, NULL,CUDA_COPY_DEVICE_TO_HOST  );

		free(tempDBG);
		return;
	}
#endif
}

#if !GPU_W9X7_FLOAT

void bmi_idwt_i97_gpu_transform(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, int toplevel)
{
	// buffer pointers
	// kernel launch variables
	dim3 block_size;
	dim3 grid_size;
	int tile_off_y = tile->mOff.y;
	int tile_off_x = tile->mOff.x;


	int dwtLevel = (comp->mNumOfSubband - 1) / 3;

	if (toplevel != -1)
	{
		dwtLevel = (dwtLevel > toplevel ? toplevel : dwtLevel);
	}
	// iterate over all the levels.  each call of the kernel processes a row
	// and transposes the data.  each level requires two passes.

	unsigned int  texture_orig_size = orig->mPitch * tile_off_y;
	// now it's the y offset

	unsigned char * texture_orig_buf = (unsigned char *)orig->mBuf + texture_orig_size /*+ PixelsToBytes(tile_off_x, wordShift)*/;
	texture_orig_size = 	orig->mSize - texture_orig_size;
	// now we got the real size
		
	unsigned int  texture_temp_size = temp->mPitch * tile_off_x;
	// now it's the offset

	unsigned char * texture_temp_buf = (unsigned char *)temp->mBuf + texture_temp_size /*+ PixelsToBytes(tile_off_y, wordShift)*/;
	texture_temp_size = 	temp->mSize - texture_temp_size;
	// now we got the real size


	for (int level = 0; level < dwtLevel; level++) {

		// - pitch info
		int d_pitch  = BytesToPixels(orig->mPitch, wordShift);
		int t_pitch  = BytesToPixels(temp->mPitch, wordShift);
		// - row info
		int r_par    = (int)tile->mXParity[level]; /*comp->mSubbandInfo[level * 3 + 2].mOff.x; */

		int r_len    = comp->mSubbandInfo[level * 3 + 2].mSize.x + comp->mSubbandInfo[level * 3 + 3].mSize.x;
		int r_len_lo = comp->mSubbandInfo[level * 3 + 2].mSize.x;
		// - column info
		int c_par    = (int)tile->mYParity[level]; /*comp->mSubbandInfo[level * 3 + 1].mOff.y; */
		int c_len    = comp->mSubbandInfo[level * 3 + 1].mSize.y + comp->mSubbandInfo[level * 3 + 3].mSize.y;
		int c_len_lo = comp->mSubbandInfo[level * 3 + 1].mSize.y;

		if (wordShift == 2 )	// INT_32_BITS
		{
			// launch the kernel on the rows
			bmi_idwt_calc_row_grid(&grid_size, &block_size, r_len, c_len);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_int, (void *)texture_orig_buf, texture_orig_size));

			if (asyncId <= 0)
			{
				bmi_idwt_i97_1d_gpu_row<<<grid_size, block_size, 0>>>
					((int *)texture_temp_buf + tile_off_y, tile_off_x, t_pitch, d_pitch, r_len, r_len_lo, r_par, c_len);
			}
			else
			{
				bmi_idwt_i97_1d_gpu_row<<<grid_size, block_size, 0, asyncId>>>
					((int *)texture_temp_buf + tile_off_y, tile_off_x, t_pitch, d_pitch, r_len, r_len_lo, r_par, c_len);
			}


			// launch the kernel on the columns
			bmi_idwt_calc_row_grid(&grid_size, &block_size, c_len, r_len);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_int, (void *)texture_temp_buf, texture_temp_size));
//			bmi_cuda_call(cudaBindTexture(0, tc_tex_int, temp->mBuf, temp->mSize));

			if (asyncId <= 0)
			{
				bmi_idwt_i97_1d_gpu_row<<<grid_size, block_size, 0>>>
					((int *)texture_orig_buf + tile_off_x,tile_off_y,  d_pitch, t_pitch, c_len, c_len_lo, c_par, r_len);
			}
			else
			{
				bmi_idwt_i97_1d_gpu_row<<<grid_size, block_size, 0, asyncId>>>
					((int *)texture_orig_buf + tile_off_x,tile_off_y,d_pitch, t_pitch, c_len, c_len_lo, c_par, r_len);
			}

		}
		else
		{
			assert(0); // todo : add 97 16 bits support
		}
	}

}
#else


void bmi_idwt_i97_gpu_transform_float(TileInfo_c * tile, CompInfo_c	* comp, CudaBuf * orig, CudaBuf * temp, StreamId asyncId, short wordShift, int toplevel)
{
	// buffer pointers
	// kernel launch variables
	dim3 block_size;
	dim3 grid_size;


	int dwtLevel = (comp->mNumOfSubband - 1) / 3;

	if (toplevel != -1)
	{
		dwtLevel = (dwtLevel > toplevel ? toplevel : dwtLevel);
	}
	// iterate over all the levels.  each call of the kernel processes a row
	// and transposes the data.  each level requires two passes.


	unsigned char * texture_orig_buf = (unsigned char *)orig->mBuf + orig->mPitch * tile->mOff.y ;
	unsigned int  texture_orig_size = orig->mSize - orig->mPitch * tile->mOff.y;
		
	unsigned char * texture_temp_buf = (unsigned char *)temp->mBuf + temp->mPitch * tile->mOff.x ;
	unsigned int  texture_temp_size = temp->mSize - temp->mPitch * tile->mOff.x;


	for (int level = 0; level < dwtLevel; level++) {

		// - pitch info
		int in_pitch  = BytesToPixels(orig->mPitch, wordShift);
		int out_pitch  = BytesToPixels(temp->mPitch, wordShift);

		int x_par    = tile->mXParity[level] ;
		int y_par    = tile->mYParity[level];

		
		int width    = comp->mSubbandInfo[level * 3 + 2].mSize.x + comp->mSubbandInfo[level * 3 + 3].mSize.x;
		int lowW = comp->mSubbandInfo[level * 3 + 2].mSize.x;

		int height   = comp->mSubbandInfo[level * 3 + 1].mSize.y + comp->mSubbandInfo[level * 3 + 3].mSize.y;
		int lowH     = comp->mSubbandInfo[level * 3 + 1].mSize.y;
		int highH     = comp->mSubbandInfo[level * 3 + 3].mSize.y;

		if (wordShift == 2 )	// INT_32_BITS
		{
			// launch the kernel on the rows

			if (asyncId <= 0)
			{
				// LL and LH
				bmi_idwt_calc_row_grid(&grid_size, &block_size, width, lowH);
				bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_buf, texture_orig_size));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_temp_buf + tile->mOff.y, tile->mOff.x,
					out_pitch, in_pitch, 
					width, lowW, x_par,
					lowH, 
					level ? IDWT_LO_GAIN : comp->mSubbandInfo[0].mfAbsStepNorm * IDWT_LO_GAIN,
					comp->mSubbandInfo[level * 3 + 1].mfAbsStepNorm * IDWT_HI_GAIN,
					level ? 1 : 0,
					0);

				// HL and LL
				bmi_idwt_calc_row_grid(&grid_size, &block_size, width, highH);
				bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)(texture_orig_buf + lowH * orig->mPitch), (texture_orig_size - lowH * orig->mPitch)));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_temp_buf + tile->mOff.y + lowH, tile->mOff.x,
					out_pitch, in_pitch, 
					width, lowW, x_par,
					highH, 
					comp->mSubbandInfo[level * 3 + 2].mfAbsStepNorm * IDWT_LO_GAIN,
					comp->mSubbandInfo[level * 3 + 3].mfAbsStepNorm * IDWT_HI_GAIN,
					0,
					0);
			}
			else
			{
				bmi_idwt_calc_row_grid(&grid_size, &block_size, width, lowH);
				bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_orig_buf, texture_orig_size));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0, asyncId>>>
					((float *)texture_temp_buf + tile->mOff.y, tile->mOff.x,
					out_pitch, in_pitch, 
					width, lowW, x_par,
					lowH, 
					level ? IDWT_LO_GAIN : comp->mSubbandInfo[0].mfAbsStepNorm * IDWT_LO_GAIN,
					comp->mSubbandInfo[level * 3 + 1].mfAbsStepNorm * IDWT_HI_GAIN,
					level ? 1 : 0,
					0);
				bmi_idwt_calc_row_grid(&grid_size, &block_size, width, highH);
				bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)(texture_orig_buf + lowH * orig->mPitch), (texture_orig_size - lowH * orig->mPitch)));
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0, asyncId>>>
					((float *)texture_temp_buf + tile->mOff.y + lowH, tile->mOff.x,
					out_pitch, in_pitch, 
					width, lowW, x_par,
					highH, 
					comp->mSubbandInfo[level * 3 + 2].mfAbsStepNorm * IDWT_LO_GAIN,
					comp->mSubbandInfo[level * 3 + 3].mfAbsStepNorm * IDWT_HI_GAIN,
					0,
					0);			
			}


			// launch the kernel on the columns
			bmi_idwt_calc_row_grid(&grid_size, &block_size, height, width);
			bmi_cuda_call(cudaBindTexture(0, tc_tex_float, (void *)texture_temp_buf, texture_temp_size));
			if (asyncId <= 0)
			{
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0>>>
					((float *)texture_orig_buf + tile->mOff.x, tile->mOff.y,
					in_pitch, out_pitch, 
					height, lowH, y_par,
					width, 
					IDWT_LO_GAIN,
					IDWT_HI_GAIN,
					1,
					1);
			}
			else
			{
				bmi_idwt_i97_1d_gpu_row_float<<<grid_size, block_size, 0, asyncId>>>
					((float *)texture_orig_buf + tile->mOff.x, tile->mOff.y,
					 in_pitch, out_pitch,
					height, lowH, y_par,
					width, 
					IDWT_LO_GAIN,
					IDWT_HI_GAIN,
					1,
					1);
			}

		}
		else
		{
			assert(0); // todo : add 97 16 bits support
		}
	}
	
}

#endif

int bmi_MCT_gpu(TileInfo_c * tile, int bitDepth, int wordShift,int dwtLevel,  int decodeLevel,int elementPerPixel, int bitsPerElement, StreamId asyncId)
{

	int threadNum;

	int blocknum = tile->mSize.y; //mCompInfo[0].mCompBuf->mHeight;
	int width_pixel = tile->mSize.x;
	int result_x_off = tile->mOff.x;
	int result_y_off = tile->mOff.y;
	if (decodeLevel < 0 || decodeLevel > dwtLevel)
	{
		decodeLevel = -1;
	}
 	if (decodeLevel != -1)
	{
		blocknum = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.y + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.y;		width_pixel = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.x + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.x;
		while (dwtLevel > decodeLevel)
		{
			result_x_off = (result_x_off + 1) / 2;
			result_y_off = (result_y_off + 1) / 2;
			dwtLevel--;
		}

	}

	threadNum = width_pixel;
	while(threadNum > MAX_THREADS)
	{
		threadNum = (threadNum + 1) /2;
	}

	if (asyncId < 0)		// sync job
	{
		asyncId = 0;
	}


	if (wordShift == 2)	
	{

		bmi_mct_tile_int32<<<blocknum, threadNum, 0, asyncId>>>(
			(int *)tile->mCompInfo[0].mCompBuf->mBuf,
			(int *)tile->mCompInfo[1].mCompBuf->mBuf,
			(int *)tile->mCompInfo[2].mCompBuf->mBuf,
			tile->mOff.x,
			tile->mOff.y,
			result_x_off,
			result_y_off,
			(unsigned char *)tile->mResultBuf->mBuf,		
			BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
			tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
			bitDepth, 
			bitsPerElement, 
			bitsPerElement,
			elementPerPixel,
			width_pixel,
			threadNum);

	}
	else
	{
		assert(0); // no done yet
	}


	return 0;
}

int bmi_ICT_gpu(TileInfo_c * tile, int bitDepth, int wordShift,int dwtLevel, int decodeLevel, int elementPerPixel, int bitsPerElement,  StreamId asyncId)
{
	int threadNum;

	int blocknum = tile->mSize.y; //mCompInfo[0].mCompBuf->mHeight;
	int width_pixel = tile->mSize.x;
	int result_x_off = tile->mOff.x;
	int result_y_off = tile->mOff.y;
	if (decodeLevel < 0 || decodeLevel > dwtLevel)
	{
		decodeLevel = -1;
	}
 	if (decodeLevel != -1)
	{
		blocknum = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.y + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.y;		
		width_pixel = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.x + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.x;
		while (dwtLevel > decodeLevel)
		{
			result_x_off = (result_x_off + 1) / 2;
			result_y_off = (result_y_off + 1) / 2;
			dwtLevel--;
		}

	}



	threadNum = width_pixel;
	while(threadNum > MAX_THREADS)
	{
		threadNum = (threadNum + 1) /2;
	}

	if (asyncId > 0)		// async job
	{
		if (wordShift == 2)	
		{

#if GPU_W9X7_FLOAT
			bmi_ict_tile_float<<<blocknum, threadNum, 0, asyncId>>>(
				(float *)tile->mCompInfo[0].mCompBuf->mBuf,
				(float *)tile->mCompInfo[1].mCompBuf->mBuf,
				(float *)tile->mCompInfo[2].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,
				result_x_off,
				result_y_off,
				(unsigned char *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
				tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
				bitDepth, 
				bitsPerElement, 
				bitsPerElement,
				elementPerPixel,
				width_pixel,
				threadNum);
#else
			bmi_ict_tile_int32<<<blocknum, threadNum, 0, asyncId>>>(
				(int *)tile->mCompInfo[0].mCompBuf->mBuf,
				(int *)tile->mCompInfo[1].mCompBuf->mBuf,
				(int *)tile->mCompInfo[2].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,
				(int *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
				tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
				bitDepth, 
				width_pixel,
				threadNum);			
#endif
		}
		else
		{
			assert(0); // no done yet
		}

	}
	else		// sync job
	{
		if (wordShift == 2)		// INT_32_BITS
		{
#if GPU_W9X7_FLOAT
			bmi_ict_tile_float<<<blocknum, threadNum, 0>>>(
				(float *)tile->mCompInfo[0].mCompBuf->mBuf,
				(float *)tile->mCompInfo[1].mCompBuf->mBuf,
				(float *)tile->mCompInfo[2].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,
				result_x_off,
				result_y_off,
				(unsigned char *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
				tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
				bitDepth, 
				bitsPerElement, 
				bitsPerElement,
				elementPerPixel,
				width_pixel,
				threadNum);
#else
			bmi_ict_tile_int32<<<blocknum, threadNum, 0>>>(
				(int *)tile->mCompInfo[0].mCompBuf->mBuf,
				(int *)tile->mCompInfo[1].mCompBuf->mBuf,
				(int *)tile->mCompInfo[2].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,
				(int *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
				tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
				bitDepth, 
				width_pixel,
				threadNum);			
#endif
		}
		else
		{
			assert(0); // no done yet
		}

	}
	



	return 0;
}

// int bmi_merge_gpu(TileInfo_c * tile, int decodeLevel, short bitDepth, short wordShift, int red_comp, int green_comp, int blue_comp, EncodeMathod method, StreamId asyncId)
int bmi_merge_gpu(TileInfo_c * tile, int componentNum, int dwtLevel, int decodeLevel, short bitDepth, short wordShift, int inPitchBytes, int elementPerPixel, int bitsPerElement, EncodeMathod method, StreamId asyncId)
{
// 	return 0;

//  	int blocknum = tile->mSize.y;
// 	int width_pixel = tile->mSize.x;
// 
// 	 if (decodeLevel != -1)
// 	{
// 		blocknum = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.y + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.y;
// 		width_pixel = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.x + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.x;
// 	}
// 	int threadNum = width_pixel;
// 	while(threadNum > MAX_THREADS)
// 	{
// 		threadNum = (threadNum + 1) /2;
// 	}




	int threadNum;

	int blocknum = tile->mSize.y; //mCompInfo[0].mCompBuf->mHeight;
	int width_pixel = tile->mSize.x;
	int result_x_off = tile->mOff.x;
	int result_y_off = tile->mOff.y;
	if (decodeLevel < 0 || decodeLevel > dwtLevel)
	{
		decodeLevel = -1;
	}
	if (decodeLevel != -1								// downsize
		&& tile->mCompInfo[0].mNumOfSubband >= decodeLevel * 3)		// not for the thumbnail
	{
		blocknum = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.y + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.y;		
		width_pixel = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.x + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.x;
		while (dwtLevel > decodeLevel)
		{
			result_x_off = (result_x_off + 1) / 2;
			result_y_off = (result_y_off + 1) / 2;
			dwtLevel--;
		}

	}

	threadNum = width_pixel;
	while(threadNum > MAX_THREADS)
	{
		threadNum = (threadNum + 1) /2;
	}

	if (asyncId < 0)	
	{
		asyncId = 0;
	}



	if (wordShift == 2)	
	{
		if (method == Ckernels_W5X3)
		{
			bmi_tile__merge_int32<<<blocknum, threadNum, 0, asyncId>>>(
				(int *)tile->mCompInfo[0].mCompBuf->mBuf,
				(componentNum > 1 ? (int *)tile->mCompInfo[1].mCompBuf->mBuf : NULL),
				(componentNum > 2 ?(int *)tile->mCompInfo[2].mCompBuf->mBuf : NULL),
				(componentNum > 3 ?(int *)tile->mCompInfo[3].mCompBuf->mBuf : NULL),
				tile->mOff.x,
				tile->mOff.y,
				result_x_off,
				result_y_off,
				(unsigned char *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
				tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
				bitDepth, 
				bitsPerElement, 
				bitsPerElement,
				elementPerPixel,
				width_pixel,
				threadNum);
		}
		else
		{

			bmi_tile__merge_float<<<blocknum, threadNum, 0, asyncId>>>(
				(float *)tile->mCompInfo[0].mCompBuf->mBuf,
				(componentNum > 1 ? (float *)tile->mCompInfo[1].mCompBuf->mBuf : NULL),
				(componentNum > 2 ?(float *)tile->mCompInfo[2].mCompBuf->mBuf : NULL),
				(componentNum > 3 ?(float *)tile->mCompInfo[3].mCompBuf->mBuf : NULL),
				tile->mOff.x,
				tile->mOff.y,
				result_x_off,
				result_y_off,
				(unsigned char *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mCompInfo[0].mCompBuf->mPitch, wordShift),
				tile->mResultBuf->mPitch / (elementPerPixel * bitsPerElement / 8), //INT_32_BITS
				bitDepth, 
				bitsPerElement, 
				bitsPerElement,
				elementPerPixel,
				width_pixel,
				threadNum);
		}
	}
	else
	{
		assert(0); // no done yet
	}



	return 0;
}


int bmi_get_one_component(TileInfo_c * tile, int component_id, EncodeMathod method, int hasMCT, int bitDepth, int wordShift, int decodeLevel, StreamId asyncId)
{
	int step = GPU_NUMBER; 
	int blocknum = tile->mSize.y; //mCompInfo[0].mCompBuf->mHeight;
	int width_pixel = tile->mSize.x;
 	if (decodeLevel != -1)
	{
		blocknum = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.y + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.y;
		width_pixel = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.x + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.x;
	}

	if (hasMCT)
	{
		
		if (asyncId > 0)		// async job
		{
			bmi_get_one_component_from_result<<<blocknum, GPU_NUMBER, 0, asyncId>>>(
				component_id,
				tile->mOff.x,
				tile->mOff.y,
				(int *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mResultBuf->mPitch , 2 ), //INT_32_BITS
				width_pixel,
				step);	
		}
		else
		{
			bmi_get_one_component_from_result<<<blocknum, GPU_NUMBER, 0>>>(
				component_id,
				tile->mOff.x,
				tile->mOff.y,
				(int *)tile->mResultBuf->mBuf,		
				BytesToPixels(tile->mResultBuf->mPitch , 2 ), //INT_32_BITS
				width_pixel,
				step);	
		}
	}
	else
	{
		if (method == Ckernels_W5X3)
		{
			if (asyncId > 0)		// async job
			{
				bmi_tile_one_component_int<<<blocknum, GPU_NUMBER, 0, asyncId>>>(
					component_id,
					(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
					tile->mOff.x,
					tile->mOff.y,
					(int *)tile->mResultBuf->mBuf,		
					BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
					BytesToPixels(tile->mResultBuf->mPitch , 2 ), //INT_32_BITS
					bitDepth, 
					width_pixel,
					step);	
			}
			else
			{
				bmi_tile_one_component_int<<<blocknum, GPU_NUMBER, 0>>>(
					component_id,
					(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
					tile->mOff.x,
					tile->mOff.y,
					(int *)tile->mResultBuf->mBuf,		
					BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
					BytesToPixels(tile->mResultBuf->mPitch , 2 ), //INT_32_BITS
					bitDepth, 
					width_pixel,
					step);	
			}
		}
		else	// Ckernels_W9X7
		{
			if (asyncId > 0)		// async job
			{
				bmi_tile_one_component_float<<<blocknum, GPU_NUMBER, 0, asyncId>>>(
					(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
					tile->mOff.x,
					tile->mOff.y,
					(int *)tile->mResultBuf->mBuf,		
					BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
					BytesToPixels(tile->mResultBuf->mPitch , 2 ), //INT_32_BITS
					bitDepth, 
					width_pixel,
					step);		
			}
			else
			{
				bmi_tile_one_component_float<<<blocknum, GPU_NUMBER, 0>>>(
					(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
					tile->mOff.x,
					tile->mOff.y,
					(int *)tile->mResultBuf->mBuf,		
					BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
					BytesToPixels(tile->mResultBuf->mPitch , 2 ), //INT_32_BITS
					bitDepth, 
					width_pixel,
					step);		
			}
		}
	}
	return 0;
}

int bmi_get_one_component_orig(TileInfo_c * tile, int component_id, EncodeMathod method, int bitDepth, int wordShift, int decodeLevel, StreamId asyncId)
{
	int step = GPU_NUMBER; 
	int blocknum = tile->mSize.y; //mCompInfo[0].mCompBuf->mHeight;
	int width_pixel = tile->mSize.x;
 	if (decodeLevel != -1)
	{
		blocknum = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.y + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.y;
		width_pixel = tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 2].mSize.x + tile->mCompInfo[0].mSubbandInfo[decodeLevel * 3 - 1].mSize.x;
	}
	
	if (method == Ckernels_W5X3)
	{
		if (asyncId > 0)		// async job
		{
			bmi_tile_one_component_int_orig<<<blocknum, GPU_NUMBER, 0, asyncId>>>(
				component_id,
				(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,		
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift), //INT_32_BITS
				bitDepth, 
				width_pixel,
				step);	
		}
		else
		{
			bmi_tile_one_component_int_orig<<<blocknum, GPU_NUMBER, 0>>>(
				component_id,
				(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,		
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift), //INT_32_BITS
				bitDepth, 
				width_pixel,
				step);	
		}
	}
	else	// Ckernels_W9X7
	{
		if (asyncId > 0)		// async job
		{
			bmi_tile_one_component_float_orig<<<blocknum, GPU_NUMBER, 0, asyncId>>>(
				(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,	
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift), //INT_32_BITS
				bitDepth, 
				width_pixel,
				step);		
		}
		else
		{
			bmi_tile_one_component_float_orig<<<blocknum, GPU_NUMBER, 0>>>(
				(int *)tile->mCompInfo[component_id].mCompBuf->mBuf,
				tile->mOff.x,
				tile->mOff.y,	
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift),
				BytesToPixels(tile->mCompInfo[component_id].mCompBuf->mPitch, wordShift), //INT_32_BITS
				bitDepth, 
				width_pixel,
				step);		
		}
	}
	
	return 0;
}


#if GPU_T1_TESTING
int gpu_decode_coderblock()
{
	return 0;
}
#endif
