// videpClipGen.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

char * fileName = "frame";
char * jp2Dir = "D:\\test\\jp2 video\\red frames\\jp2_frames";
char * idxDir = "D:\\test\\jp2 video";
int jp2Num = 640;

int _tmain(int argc, _TCHAR* argv[])
{

	char jp2Name[255];
	char outName[255];

	FILE * fidxOut = NULL, *fvideoOut = NULL;

	sprintf_s(outName, 255, "%s\\%s.idx", idxDir, fileName );
	fopen_s(&fidxOut, outName, "wb+");

	sprintf_s(outName, 255, "%s\\%s.jp2", idxDir, fileName );
	fopen_s(&fvideoOut, outName, "wb+");

	if (fidxOut && fvideoOut)
	{
		void * ptr = NULL;
		int ptrLen = 0;

		for (int i = 1; i <= jp2Num; ++i)
		{
			sprintf_s(jp2Name, 255, "%s\\%s_%05d.jp2", jp2Dir, fileName, i);
			FILE * fin = NULL;
			fopen_s(&fin, jp2Name, "rb");
			if (fin)
			{
				int flen;
				fseek(fin, 0L, SEEK_END);
				flen = ftell(fin);
				fprintf(fidxOut, "%d\t30\n", flen);
			
				fseek(fin, 0L, SEEK_SET);

				if (ptrLen < flen)
				{
					if (ptr)
					{
						free(ptr);
					}
					ptr = malloc(flen);
					assert(ptr);
					ptrLen = flen;
				}

				fread_s(ptr, ptrLen, flen, 1, fin);

				fwrite(ptr, flen, 1, fvideoOut);

				fflush(fvideoOut);
				fflush(fidxOut);

				fclose(fin);
			}
		}
		fclose(fidxOut);
		fclose(fvideoOut);
	}


	return 0;
}

