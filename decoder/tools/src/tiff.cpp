
#include"tiff.h"
#include "codec.h"

namespace BMI
{
	
	sIFDItem::~sIFDItem()
	{ 
		if(mValueOutSide)
		{
			SAFE_FREE(mOutSideValue);
		}
	}
	
	
	P_IFDItem IFD::addEntry(unsigned short tag)
	{
		mNumIFDEntries++;
		BMI_ASSERT((tag > mLastTag) & (mNumIFDEntries < MAX_ENTRIES) & (mNumIFDEntries > 0));
		mLastTag = tag;	
		P_IFDItem newItem =  &mItems[mNumIFDEntries-1];
		newItem->mIFDEntry.mTag = tag;	
		return newItem;
	}

	int IFD::initialize(const ImageParams &params)
	{
		mInitialized = true;

		int bytesPerSample = 0;
		switch(params.mSamplesPerPixel)
		{
		case 1:
			bytesPerSample = params.mBitsPerSample[0];
			break;
		case 3:
			bytesPerSample = params.mBitsPerSample[0] + params.mBitsPerSample[1] + params.mBitsPerSample[2];
			break;		
		case 4:
			bytesPerSample = params.mBitsPerSample[0] + params.mBitsPerSample[1] + params.mBitsPerSample[2] + params.mBitsPerSample[3];
			break;
		default:
			BMI_ASSERT(0);
		}

		BMI_ASSERT((bytesPerSample % 8) == 0); // Still the bit count. assuming per pixel info is a full byte ,which is not true for some cases, but we may not need them
		bytesPerSample /= 8; 

		int pixel_data_byte_cnt = params.mWidth * params.mHeight * bytesPerSample;

		mImageWidth = addEntry(0x100);
		setValue(params.mWidth, mImageWidth);

		mImageLength = addEntry(0x101);
		setValue(params.mHeight, mImageLength);

		mBitsPerSample = addEntry(0x102);
		setValue(params.mBitsPerSample, params.mSamplesPerPixel, mBitsPerSample);

		mCompression = addEntry(0x103);
		setValue(params.mCompression, mCompression);

		mPhotoMetry = addEntry(0x106);
		setValue(params.mPhotMetry, mPhotoMetry);

		mStripOffsets = addEntry(0x111);
		setValue((int)HEADER_SIZE, mStripOffsets);

		mSamplesPerPixel = addEntry(0x115);
		setValue(params.mSamplesPerPixel, mSamplesPerPixel);

		mRowsPerStrip = addEntry(0x116);
		setValue(params.mHeight, mRowsPerStrip);

		mStripByteCount = addEntry(0x117);
		setValue(pixel_data_byte_cnt, mStripByteCount);

		int value_start_offset = HEADER_SIZE + pixel_data_byte_cnt + SIZE_NUM_IFD_ENTRIES + mNumIFDEntries * sizeof(IFDEntry) + SIZE_LAST_4_BYTES;
		setOffsets(value_start_offset);

		return 0;
	}

	short IFD::getNumEntries()
	{
		return mNumIFDEntries;
	}

	int IFD::setValue(const int value, IFDItem * item)
	{
		BMI_ASSERT(mInitialized);
		item->mIFDEntry.mCount = 1;
		item->mIFDEntry.mType = 4;
		
		item->mIFDEntry.mIntValue = value;	
		return 0;
	}

	int IFD::setValue(const short value, IFDItem * item)
	{
		BMI_ASSERT(mInitialized);
		item->mIFDEntry.mCount = 1;
		item->mIFDEntry.mType = 3;
		
		item->mIFDEntry.mShortValue[0] = value;	
		return 0;
	}

	int IFD::setValue(const unsigned char value, IFDItem * item)
	{
		BMI_ASSERT(mInitialized);
		item->mIFDEntry.mCount = 1;
		item->mIFDEntry.mType = 1;
		
		item->mIFDEntry.mUcharValue[0] = value;
		return 0;
	}


	int IFD::setValue(const int* valArray, int count, IFDItem * item)
	{
		BMI_ASSERT(mInitialized & (count > 0));
		item->mIFDEntry.mCount = count;
		item->mIFDEntry.mType = 4;

		if(count == 1)
		{
			item->mIFDEntry.mIntValue = *valArray;
		}
		else
		{
			item->mValueOutSide = true;
			item->mOutSideValue = malloc(count * sizeof(int));
			int * values = (int *)item->mOutSideValue;
			for(int i = 0; i < count; i++)
			{
				values[i] = valArray[i];
			}
		}
		
		return 0;
	}

	int IFD::setValue(const short* valArray, int count, IFDItem * item)
	{
		BMI_ASSERT(mInitialized & (count > 0));

		item->mIFDEntry.mCount = count;
		item->mIFDEntry.mType = 3;

		if(count <= 2)
		{
			for(int i = 0; i < count; i++)
			{
				item->mIFDEntry.mShortValue[i] = valArray[i];
			}
		}
		else
		{
			item->mValueOutSide = true;
			item->mOutSideValue = malloc(count * sizeof(short));
			short * values = (short *)item->mOutSideValue;
			for(int i = 0; i < count; i++)
			{
				values[i] = valArray[i];
			}
		}
		
		return 0;
	}

	int IFD::setValue(const unsigned char* valArray, int count, IFDItem * item)
	{
		BMI_ASSERT(mInitialized & (count > 0));

		item->mIFDEntry.mCount = count;
		item->mIFDEntry.mType = 1;

		if(count <= 4)
		{
			for(int i = 0; i < count; i++)
				item->mIFDEntry.mUcharValue[i] = valArray[i];
		}
		else
		{
			item->mValueOutSide = true;
			item->mOutSideValue = malloc(count * sizeof(unsigned char));
			unsigned char * values = (unsigned char *)item->mOutSideValue;
			for(int i = 0; i < count; i++)
			{
				values[i] = valArray[i];
			}
		}
		
		return 0;
	}


	int IFD::setOffsets(int startOffset)
	{
		BMI_ASSERT(mInitialized);

		for(int i = 0; i < mNumIFDEntries; i++)
		{
			if(mItems[i].mValueOutSide)
			{
				mItems[i].mIFDEntry.mOffset = startOffset;

				int valueSize = 0;
				switch(mItems[i].mIFDEntry.mType)
				{
				case 1:
					valueSize = 1;
					break;
				case 3:
					valueSize = 2;
					break;			
				case 4:
					valueSize = 4;
					break;
				default:
					BMI_ASSERT(0);
				}
				BMI_ASSERT(mItems[i].mIFDEntry.mCount > 0);		
				valueSize *= mItems[i].mIFDEntry.mCount;
				startOffset += valueSize;
			}
		}

		return 0;
	}

	int IFD::writeHeader(FILE * fs )
	{
		short header1[2] = {0x4949, 42};
		fwrite(header1, 2, 2, fs);

		int ifd_offset = HEADER_SIZE + mStripByteCount->mIFDEntry.mIntValue; //***UUU here we assume the out side values are after the directory
		fwrite(&ifd_offset, 1, 4, fs);
		return 0;
	}

	int IFD::writeEntries(FILE * fs )
	{
		BMI_ASSERT(mInitialized);
		fwrite(&mNumIFDEntries, 2, 1, fs); 

		for(int i = 0; i < mNumIFDEntries; i++) //***UUU to do: since little endian, we should be able to write mIFDEntry as a single mem block
		{
			fwrite(&mItems[i].mIFDEntry.mTag, 2, 1, fs); 
			fwrite(&mItems[i].mIFDEntry.mType, 2, 1, fs); 
			fwrite(&mItems[i].mIFDEntry.mCount, 4, 1, fs); 

			int valueSize = 0;

			if(mItems[i].mValueOutSide)
			{
				fwrite(&mItems[i].mIFDEntry.mOffset, 4, 1, fs); 
			}
			else //***UUU check this part
			{
				switch(mItems[i].mIFDEntry.mType)
				{
				case 1:
					valueSize = 1;
					break;
				case 3:
					valueSize = 2;
					break;			
				case 4:
					valueSize = 4;
					break;
				default:
					BMI_ASSERT(0);
				}
				BMI_ASSERT(mItems[i].mIFDEntry.mCount > 0);		
				valueSize *= mItems[i].mIFDEntry.mCount;
				BMI_ASSERT(valueSize <= 4);
				fwrite(mItems[i].mIFDEntry.mUcharValue, valueSize, 1, fs); 
				int zero = 0;
				if(valueSize < 4)
				{
					fwrite(&zero, 1, 4 - valueSize,  fs); 
				}

			}
		}
		int zero = 0;
		fwrite(&zero,4,1,fs);
		//***UUU write the offset values in a different function.
		// this supports writing the values at a different location than just after the IFD
		return 0;
	}


	int IFD::writeOutSideValues(FILE * fs )
	{
		BMI_ASSERT(mInitialized);

		for(int i = 0; i < mNumIFDEntries; i++)
		{
			if(mItems[i].mValueOutSide)
			{
				int valueSize = 0;
				switch(mItems[i].mIFDEntry.mType)
				{
				case 1:
					valueSize = 1;
					break;
				case 3:
					valueSize = 2;
					break;			
				case 4:
					valueSize = 4;
					break;
				default:
					BMI_ASSERT(0);
				}
				BMI_ASSERT(mItems[i].mIFDEntry.mCount > 0);		
				valueSize *= mItems[i].mIFDEntry.mCount;
				fwrite(mItems[i].mOutSideValue, valueSize, 1, fs); 
			}
		}
		return 0;
	}

};