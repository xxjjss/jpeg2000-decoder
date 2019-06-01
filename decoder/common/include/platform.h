
#pragma once

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

typedef		char	int_8;
typedef		unsigned char	uint_8;


#if defined WIN64
#define		_WIN64_		1
#define		_LINUX		0
#define		_WIN32_		0
#define		_MAC_OS		0
#elif (defined WIN32) 
#define		_WIN32_		1
#define		_LINUX		0
#define		_WIN64_		0
#define		_MAC_OS		0
#elif defined MAC_OS
#define		_MAC_OS		1
#define		_LINUX		0
#define		_WIN32_		0
#define		_WIN64_		0
#elif defined LINUX
#define		_LINUX		1
#define		_MAC_OS		0
#define		_WIN32_		0
#define		_WIN64_		0
#endif


#if _MAC_OS
	#ifndef		MAC_32
	#define		MAC_32
	#endif
#elif _WIN32_ || _WIN64_
	#define _WINDOWS	1
	#include <windows.h>
#elif _LINUX
	#ifndef		LINUX_32
	#define		LINUX_32
	#endif
#endif


#define BMI_EXPORT

#if _WINDOWS
#  undef  BMI_J2K_CODEC_EXPORTS
#  if defined BMI_EXPORT
#    define BMI_J2K_CODEC_EXPORTS __declspec(dllexport)
#  elif defined BMI_IMPORTS
#    define BMI_J2K_CODEC_EXPORTS __declspec(dllimport)
#  else
#    define BMI_J2K_CODEC_EXPORTS
#  endif	// BMI_EXPORT
#else		// _WINDOWS
#define BMI_J2K_CODEC_EXPORTS
#endif




#if _MAC_OS
void mac_sprintf_s( char * buf, int buf_len, const char * fmt, ...);
#define  FOPEN(fs, fname, mode)		fs = fopen(fname,mode)
#define  FREAD_S(buf, buf_size, ele_size, ele_num, fs) 		fread(buf, ele_size, ele_num, fs)
#define SPRINTF_S(x)	mac_sprintf_s x

#elif _LINUX
void linux_sprintf_s( char * buf, int buf_len, const char * fmt, ...);
#define  FOPEN(fs, fname, mode)		fs = fopen(fname,mode)
#define  FREAD_S(buf, buf_size, ele_size, ele_num, fs) 		fread(buf, ele_size, ele_num, fs)
#define SPRINTF_S(x)	linux_sprintf_s x
#else
#define  FOPEN(fs, fname, mode)							fopen_s(&fs, fname, mode)
#define  FREAD_S(buf, buf_size, ele_size, ele_num, fs) 		fread_s(buf, buf_size, ele_size, ele_num, fs)
#define SPRINTF_S(x)	sprintf_s x
#endif
