
#include "platform.h"
#include "debug.h"

const int FIXED_STRING_LENGTH	= 256;

int main(int argc, char * argv[])
{
	if (argc !=4)
	{
		PRINT_SCREEN(("Usage :\n"));
		PRINT_SCREEN(("v2f video_filename input_path output_path\n"));
	}
	else
	{
		char str[FIXED_STRING_LENGTH], str_jp2[FIXED_STRING_LENGTH];

		sprintf_s(str,FIXED_STRING_LENGTH, "%s", argv[2]);

		int len = strlen(str);
		if (str[len - 1] != '\\') 
		{
			str[len] = '\\'; 
			len++;
		}


		sprintf_s(str+len,FIXED_STRING_LENGTH-len, "%s.idx\0", argv[1]);
		len = strlen(str);

		FILE* fp_vid;
		fopen_s(&fp_vid,str, "rt");
		if (!fp_vid)
		{
			exit(0);
		}

		str[len-4] = 0;
		sprintf_s(str+len-4, FIXED_STRING_LENGTH-len+4,"%s\0",".jp2");
		FILE* fp_jp2;
		fopen_s(&fp_jp2, str, "rb");
		if (!fp_jp2)
		{
			fclose(fp_vid);
			exit(0);
		}

		sprintf_s(str,FIXED_STRING_LENGTH, "%s", argv[3]);
		len = strlen(str);
		if (str[len - 1] != '\\') 
		{
			str[len] = '\\'; 
			len++;
		}

		sprintf_s(str+len,FIXED_STRING_LENGTH-len, "%s\0", argv[1]);
		len = strlen(str);

		FILE * fp_f;
		int	fileSN = 0;
		int curPos = 0, curLen = 0, t = 0;
		unsigned char * buf = NULL;
		int bufLen = 0;

		while (1)
		{    
			if ((!feof(fp_vid)) && (!feof(fp_jp2)))
			{

				fscanf_s(fp_vid, "%d\t%d\n", &curLen, &t);

				if (bufLen < curLen)
				{
					SAFE_FREE(buf);
					buf = reinterpret_cast<unsigned char *>(malloc(curLen));
					bufLen = curLen;
				}

				fread(buf, 1,curLen, fp_jp2);

				sprintf_s(str_jp2, FIXED_STRING_LENGTH, "%s%05d.jp2", str, ++fileSN);

				fopen_s(&fp_f, str_jp2, "wb");
				if (!fp_f)
				{
					break;
				}

				PRINT_SCREEN(("Creating jp2 file %s\n", str_jp2));
				fwrite(buf, curLen, 1, fp_f);
				fclose(fp_f);
			}
			else
			{
				break;
			}

	
		}

		SAFE_FREE(buf);
		fclose(fp_jp2);
		fclose(fp_vid);
	}

}