
#pragma once

#include<stdlib.h>
#include <stdio.h>
//#include<assert.h>
#include "debug.h"
#include "imagesaver.h"

//#define DEBUG_PAUSE(x)		dbg_pause((x))
//#define BMI_ASSERT(x)	assert((x))



#define MAX_VAL_SIZE 100
#define MAX_ENTRIES 30
#define HEADER_SIZE 8
#define SIZE_NUM_IFD_ENTRIES 2
#define SIZE_LAST_4_BYTES 4

namespace BMI
{

	typedef struct sImageParams
	{
		sImageParams():
		mWidth(0),
		mHeight(0),
		mCompression(1),
		mPhotMetry(2),
		mSamplesPerPixel(3)
		{
			mBitsPerSample[0] = mBitsPerSample[1] = mBitsPerSample[2] = mBitsPerSample[3] = 8;

		}

		short mWidth;
		short mHeight;
		short mBitsPerSample[4];
		short mCompression;
		short mPhotMetry;
		short mSamplesPerPixel;
	} ImageParams;

	typedef struct sIFDEntry
	{
		sIFDEntry():
		mTag(0),
		mType(0),
		mCount(0),
		mIntValue(0)
		{}
		
		unsigned short mTag; //uchar[2] ? but we may not have to access byte by byte
		unsigned short mType;
		unsigned int mCount;
		union{
			unsigned int mIntValue;
			unsigned int mOffset;
			unsigned short mShortValue[2];
			unsigned char mUcharValue[4];
		};
	} IFDEntry;

	typedef struct sIFDItem
	{
		BMI_J2K_CODEC_EXPORTS	sIFDItem():
		mValueOutSide(false),
		mOutSideValue(NULL)
		{
			//mOutSideValue = (void*) new unsigned char[MAX_VAL_SIZE];
		}

		BMI_J2K_CODEC_EXPORTS	~sIFDItem();

		IFDEntry  mIFDEntry;
		 bool mValueOutSide;
		 void * mOutSideValue;
	} IFDItem, * P_IFDItem;

	//Remember to write 4 zero bytes at the end

	class IFD
	{
	public:
		IFD():
		  mImageWidth(NULL),
		  mImageLength(NULL),
		  mBitsPerSample(NULL),
		  mCompression(NULL),
		  mPhotoMetry(NULL),
		  mStripOffsets(NULL),
		  mSamplesPerPixel(NULL),
		  mRowsPerStrip(NULL),
		  mStripByteCount(NULL),
		  mNumIFDEntries(0),
		  mLastTag(0),
		  mInitialized(false)
		{}
		  
		  ~IFD()
		  {}

		int initialize(const ImageParams &params);
		short getNumEntries();			
		int writeHeader(FILE * fs );
		int writeEntries(FILE * fs );
		int writeOutSideValues(FILE * fs );

	private:

		P_IFDItem addEntry(unsigned short tag); 

		int setValue(const int vale,  P_IFDItem item); 
		int setValue(const short value, IFDItem * item);
		int setValue(const unsigned char value, IFDItem * item);
		int setValue(const int* valArray, int count, P_IFDItem item);
		int setValue(const short* valArray, int count, IFDItem * item);
		int setValue(const unsigned char* valArray, int count, IFDItem * item);

		int setOffsets(int startOffset);

		// IFD entries
		P_IFDItem mImageWidth;
		P_IFDItem mImageLength;
		P_IFDItem mBitsPerSample;
		P_IFDItem mCompression;
		P_IFDItem mPhotoMetry;
		P_IFDItem mStripOffsets;		
		P_IFDItem mSamplesPerPixel;
		P_IFDItem mRowsPerStrip;
		P_IFDItem mStripByteCount;

		IFDItem mItems[MAX_ENTRIES];
		short mNumIFDEntries;
		short mLastTag;
		bool mInitialized;
	};
};