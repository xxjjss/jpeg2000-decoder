/***************************************************************************************
 *
 * Copyright (C) 2004 Broad Motion, Inc.
 *
 * All rights reserved.
 *
 * http://www.broadmotion.com
 *
 **************************************************************************************/

#include <string.h>
#include "dec_stru.h"
#include "debug.h"
#include "codec_base.h"

/**************************************************************************************/
/*                                         ceil_ratio                                 */
/**************************************************************************************/
// int ceil_ratio(int num, int den)
// {
//   return (num + den - 1 ) / den;
// }

/**************************************************************************************/
/*                                        floor_ratio                                 */
/**************************************************************************************/
// int floor_ratio(int num, int den)
// {
//   return num / den;
// }

/**************************************************************************************/
/*                                         count_bits                                 */
/**************************************************************************************/
int count_bits(unsigned int v)
{
  int n=0;
  while(v) {
    ++n;   v >>= 1;
  }
  return n;
}

/**************************************************************************************/
/*                                     int_log2                                       */
/**************************************************************************************/
int int_log2(unsigned int v)
{
  // calc (int)log(2, v) = count_bits(v >> 1)
//  int n = 0;
//  while(v >>= 1)  ++n;
//  return n;
  return count_bits(v >> 1);
}

/**************************************************************************************/
/*                                     ostream_init                                   */
/**************************************************************************************/
int ostream_init(stru_stream *stream, unsigned char *buffer, int buf_size)
{
  stream->start = buffer;
  stream->cur = buffer;
  stream->end = buffer + buf_size;
  stream->bits = 0;

  return 0;
}

/**************************************************************************************/
/*                                    ostream_tell                                    */
/**************************************************************************************/
int ostream_tell(stru_stream *stream)
{
  return (int)(stream->cur - stream->start);
}

/**************************************************************************************/
/*                                    ostream_seek                                    */
/**************************************************************************************/
int ostream_seek(stru_stream *stream, int offset)
{
  stream->cur = stream->start + offset;
  stream->bits = 0;

  return 0;
}

int ostream_skip(stru_stream *stream, int offset)
{
	stream->cur += offset;
	stream->bits = 0;

	return 0;
}

/**************************************************************************************/
/*                                  ostream_byte_align                                */
/**************************************************************************************/
int ostream_byte_align(stru_stream *stream)
{
  if(stream->bits) {
    if(!(char)~(*stream->cur))    ++stream->cur;
    stream->bits = 0;
    ++stream->cur;
  }

  return 0;
}

/**************************************************************************************/
/*                                  ostream_get_bit                                   */
/**************************************************************************************/
int ostream_get_bit(stru_stream *stream)
{
  int value;

  if(stream->bits == 8)   stream->bits = !(char)~(*stream->cur ++);
  value = (*stream->cur >> (8 - (++stream->bits))) & 1;

#if defined(WIN32)
  if(stream->cur >= stream->end)    
  {
	  throw -1;
  }
#endif
  return value;
}

/**************************************************************************************/
/*                                     ostream_get_int                                */
/**************************************************************************************/
int ostream_get_int(stru_stream *stream, int bits)
{
  int value = 0;

  while(bits --) {
    if(stream->bits == 8)   stream->bits = !(char)~(*stream->cur ++);
    value = (value << 1) | ((*stream->cur >> (8 - (++stream->bits))) & 1);
  }

#if defined(WIN32)
  if(stream->cur >= stream->end)    
  {
	  throw -1;
  }
#endif
  return value;
}

/**************************************************************************************/
/*                                 ostream_restore_bytes                              */
/**************************************************************************************/
unsigned char * ostream_restore_bytes(stru_stream *stream, unsigned int len)
{
  unsigned char *buffer;

  if(stream->bits) {
    if(!(char)~(*stream->cur))    ++stream->cur;
    stream->bits = 0;
    ++stream->cur;
  }

  buffer = stream->cur;
  stream->cur += len;

#if defined(WIN32)
  if(stream->cur >= stream->end)    
  {
	  throw BMI::PARSE_ERROR;
  }
#endif
  return buffer;
}

/**************************************************************************************/
/*                 ostream_read_bytes, read bytes in big endian                       */
/**************************************************************************************/
int ostream_read_bytes(stru_stream *stream, unsigned int bytes)
{

#if ENABLE_ASSEMBLY_OPTIMIZATION
	unsigned char ** stream_add = &(stream->cur);
	if (bytes > (unsigned int)(stream->end - stream->cur))
	{
		throw BMI::PARSE_ERROR_INPUTFILE_ERROR;
	}

#if INTEL_ASSEMBLY_32
  // return value in EAX
  __asm
  {
	MOV EDI, stream_add
	MOV ESI, [EDI]
	MOV EBX, [ESI]
	MOV EAX, bytes
	CMP EAX, 4
	JG finished
	MOV ECX, 4
	SUB ECX, EAX
	MOV AH, BL
	MOV AL, BH
	SHL EAX, 16
	SHR EBX, 16
	MOV AH, BL
	MOV AL, BH
	TEST ECX, ECX
	JZ finished
shift_a_byte:
	SHR EAX, 8
	LOOPNZ shift_a_byte
finished:
	ADD ESI, bytes
	MOV [EDI], ESI
  }

#elif 0// AT_T_ASSEMBLY

  int ret;
  __asm__  (
		"movl (%%ebx), %%esi;"
		"cmpl $4, %%eax;"
		"jg finished;"
		"movl $4, %%ecx;"
		"subl %%eax, %%ecx;"
		"movb %%bl, %%ah;"
		"movb %%bh, %%al;"
		"shll $16, %%eax;"
		"shrl $16, %%ebx;"
		"movb %%bl, %%ah;"
		"movb %%bh, %%al;"
		"testl %%ecx, %%ecx;"
		"jz finished;"
"shift_a_byte:;"
		"shrl $8, %%eax;"
		"loopnz shift_a_byte;"
"finished:;"
		"addl %%eax, %%esi;"
		"movl %%esi, (%%ebx);"
	: "=a" (ret)                   //output in eax
	: "b" (stream_add), "a" (bytes)           //inputs		stream_add load into edi  bytes load into edx
//	: "memory"             // memory modified
  );
  return ret;

#else	// INTEL_ASSEMBLY_64

	int value = 0;
	switch(bytes) {
	case 4  :   value |= (*(stream->cur ++)) << 24;
	case 3  :   value |= (*(stream->cur ++)) << 16;
	case 2  :   value |= (*(stream->cur ++)) << 8;
	case 1  :   value |= (*(stream->cur ++)) << 0;
	case 0  :	break;
	default :   
		if (bytes > (unsigned int)(stream->end - stream->cur))
		{
			throw -1;
		}
		else	// skip
		{
			stream->cur += bytes;
			stream->bits = 0;
		}
		break;
	}
	return value;

#endif


#else

   int value = 0;
  switch(bytes) {
    case 4  :   value |= (*(stream->cur ++)) << 24;
    case 3  :   value |= (*(stream->cur ++)) << 16;
    case 2  :   value |= (*(stream->cur ++)) << 8;
    case 1  :   value |= (*(stream->cur ++)) << 0;
	case 0  :	break;
    default :   
		if (bytes > (unsigned int)(stream->end - stream->cur))
		{
			throw -1;
		}
		else	// skip
		{
			stream->cur += bytes;
			stream->bits = 0;
		}
		break;
  }

#if _WINDOWS
  if(stream->cur > stream->end)    
  {
	  throw -1;
  }
#endif
  return value;

#endif	//ENABLE_ASSEMBLY_OPTIMIZATION

}

/**************************************************************************************/
/*                               bmi_DAT_copy_my                                      */
/**************************************************************************************/
unsigned int bmi_DAT_copy_my(void *src, void *dst, unsigned short byteCnt)
{
  memcpy(dst, src, byteCnt);

  return DAT_XFRID_WAITNONE;
}

/**************************************************************************************/
/*                               bmi_DAT_copy2d_my                                    */
/**************************************************************************************/
unsigned int bmi_DAT_copy2d_my( unsigned int type, void *src, void *dst,
                                unsigned short lineLen, unsigned short lineCnt,
                                unsigned short linePitch)
{
  unsigned char *psrc = (unsigned char *)src;
  unsigned char *pdst = (unsigned char *)dst;

  if(!lineCnt || !lineLen || !linePitch)    return 0;

  do {
    memcpy(pdst, psrc, lineLen);
    psrc += (type == DAT_1D2D) ? lineLen : linePitch;
    pdst += (type == DAT_2D1D) ? lineLen : linePitch;
  } while(--lineCnt);

  return DAT_XFRID_WAITNONE;
}

void bmi_CACHE_wbL2_my(void *blockPtr, unsigned int byteCnt, int wait) {}
void bmi_CACHE_invL2_my(void *blockPtr, unsigned int byteCnt, int wait) {}
void bmi_CACHE_wbInvL2_my(void *blockPtr, unsigned int byteCnt, int wait) {}
void bmi_CACHE_wbInvAllL2_my(int wait) {}
