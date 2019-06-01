#pragma once

// 
// 
// #include <stdlib.h>
// #include "decoder.h"
// 
// using namespace BMI;
// 
// #ifndef _MAC_OS
// #define _MAC_OS		0
// #endif
// #define NUMBER_BUFFER			3
// 
// #ifndef SET_BOUNDARY
// #define SET_BOUNDARY(min_v, v, max_v)		((v) < (min_v) ? (min_v) : ((v) > (max_v) ? (max_v) : (v)))
// #endif
// 
// #ifndef		_ASSERT_ENABLE
// #if _DEBUG
// #define _ASSERT_ENABLE		1
// #else
// #define _ASSERT_ENABLE		0
// #endif // DEBUG
// #if _ASSERT_ENABLE
// #include "assert.h"
// #define		BMI_ASSERT(x)		 BMI_assert(x);
// #else
// #define		BMI_ASSERT(x)
// #endif
// #endif
// 
// 
// 
// struct		expand_buf
// {
// 	expand_buf():
// buf(NULL),
// buf_size(0)
// {}
// 
// void * buf;
// unsigned int buf_size;
// int width;
// int height;
// };
// 
// typedef expand_buf	ExpendBufInfo;
// 
// //////////////////////////////////////////////////////////////////////////
// // The following interface for external using
// // we need to create the singleton class within the dll
// //////////////////////////////////////////////////////////////////////////
// 
// 
// // void CudaStartNewFrame();
// 
// 
//  void CudaInitTile(int tile_id_x, int tile_id_y, int num_components, int filter_type, int word_shift, int x_offset, int y_offset);
// 
//  void CudaInitComponent(int tile_id_x, int tile_id_y, int component_id, int off_x, int off_y, int width, int height, int res_level, int blck_size_x, int blck_size_y, int bitDepth);
// 
//  void SetSubbandDequanStep(int tile_id_x, int componentId,int subband_Seq, int val);
// 
// ExpendBufInfo	CudaGetFinalData(int tile_id_x, int tile_id_y);
// 
//  void BMI_assert(bool good);
// 
// //  void CudaInit( int tile_x = 0, int tile_y = 0);
// 
// /* void CudaQuit();*/