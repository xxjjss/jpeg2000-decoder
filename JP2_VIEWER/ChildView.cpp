// ChildView.cpp : implementation of the CChildView class
//

#include "stdafx.h"
#include "jp2_viewer.h"
#include "ChildView.h"
#include "MainFrm.h"

#include "process.h"
#include "time.h"
#include "debug.h"
#include "debugtools.h"
#include	"testclock.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define USE_TCP_PROTOCOL
#define USE_KKD
#define HDR_SIZ               30*1024
#define MET_SIZ               320*1024
#define DAT_SIZ               450*1024

bool  ENABLE_GPU_DWT = true;
int pixelBitLength = 24;

LONG64	bitsRead;
int outSizeX, outSizeY;

// DWORD WINAPI thread_ethernet(void *par);
DWORD WINAPI thread_local(void* par);
// DWORD WINAPI thread_save(void* par);
DWORD WINAPI thread_draw(void* par);

/////////////////////////////////////////////////////////////////////////////
// CChildView

CChildView::CChildView():
total_decode_time(0)
{

//////////////////////////////////////////////////////////////////////////
// merge jp2 file into stream
//////////////////////////////////////////////////////////////////////////
// 	{
// 		FILE * jp2;
// 		FILE * idx;
// 		FILE * stream;
// 
// 		const int MAX_FILE_LENGTH = 5 * 1024 * 1024;	// 5M
// 		const int FRAME_TIME = 30;	// m-seconds
// 		const char * PATH = "d:\\test\\jp2 video\\";
// 		const char * FOLDER = "TheBreakup2";
// 		const char * FILENAME = "dci_jp2_%d.jp2";
// 	
// 		// borrow the string company
// 		sprintf_s(company, FIXED_STRING_LENGTH, "%s%s.idx", PATH, FOLDER);	// use the folder name as the stream file name
// 		idx = fopen(company, "wt");
// 		assert(idx);
// 		sprintf_s(company, FIXED_STRING_LENGTH, "%s%s.jp2", PATH, FOLDER);	// use the folder name as the stream file name
// 		stream = fopen(company, "wb");
// 		assert(stream);
// 
// 		int fileIndex = 0;
// 		sprintf_s(company, FIXED_STRING_LENGTH, "%s%s\\%s", PATH, FOLDER, FILENAME);	// use the folder name as the stream file name
// 		unsigned char * fileBuf = new unsigned char[MAX_FILE_LENGTH];
// 		while (1)
// 		{
// 
// 	
// 			// borrow string ip
// 			sprintf_s(ip, FIXED_STRING_LENGTH, company, fileIndex++);
// 			jp2 = fopen(ip, "rb");
// 			if (!jp2)
// 			{
// 				break;
// 			}
// 
// 			fseek(jp2,0, SEEK_END);
// 			int size = ftell(jp2);	// file size
// 			fseek(jp2,0,SEEK_SET);
// 
// 			fread(fileBuf, 1, size, jp2);
// 
// 			fclose(jp2);
// 
// 			fprintf(idx, "%d\t%d\n", size,FRAME_TIME);
// 			fwrite(fileBuf, 1, size, stream);
// 			fflush(stream);
// 			fflush(idx);
// 
// 		}
// 
// 		delete[] fileBuf;
// 		fclose(idx);
// 		fclose(stream);
// 	}

	//////////////////////////////////////////////////////////////////////////
  int i;

  opt = new COptions(this);
  dlg = new CFileDialog(TRUE, NULL, NULL, 0, "BMI JP2 Index Files (*.idx)|*.idx||");

  QueryPerformanceFrequency ((LARGE_INTEGER *) &tickPerMS);
  tickPerMS.QuadPart /= 1000;
  tstart.QuadPart = 0;
  tcur_p.QuadPart = 0;
  tcur.QuadPart = 0;

  total_frame_p = 0;
  total_frame = 0;

  bethernet = blocal = bsave = bpaint = boption = broi = status[0] = 0;
  methernet = mlocal = msave = mdraw = NULL;
  buf_idx = -1;

  for(i = 0; i < BUF_CNT/* + 1*/; i ++) {
    buf[i] = new unsigned char[EACH_FRAME_BUF_SIZE];
  }

  memset(reinterpret_cast<void *>(decode_time), 0, sizeof(decode_time));
 
  bi = (BITMAPINFO*) new unsigned char[sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD)];
  for(i = 0; i < 256; i ++) {
    bi->bmiColors[i].rgbBlue = i;
    bi->bmiColors[i].rgbGreen = i;
    bi->bmiColors[i].rgbRed = i;
  }

  init_covert_table();

  BMI::EnginConfig enginConfig;
  enginConfig.mUsingGpu = ENABLE_GPU_DWT;

	enginConfig.mThreadNum = -1;
  enginConfig.mInputQueueLen = 1;
  enginConfig.mOutputQueueLen = 1;
	decoder = BMI::Decoder::Get();

	decoder->Init(&enginConfig);	
	ENABLE_GPU_DWT = enginConfig.mUsingGpu;

	BMI::DecodeJobConfig	config;
	decoder->GetJobConfig(config);
	config.mCheckOutComponent = -1;
	config.mDwtDecodeTopLevel = -1;
	config.mIgnoreComponents = ENABLE_GPU_DWT ? 0 : 0xf8;	
	config.mOutputFormat = IMAGE_FOTMAT_24BITS_RGB;
	pixelBitLength = 24;

	decoder->SetJobConfig(config);


//   FILE* fp = ::fopen("server.txt", "rt");
// #ifdef  USE_TCP_PROTOCOL
//   fscanf(fp, "%s\n%s\n%d", company, ip, &port);
// #else
//   fscanf(fp, "%s\n%s\n%d%s", company, ip, &port, ip);
// #endif
//   fclose(fp);
}

CChildView::~CChildView()
{
}


BEGIN_MESSAGE_MAP(CChildView,CWnd )
	//{{AFX_MSG_MAP(CChildView)
	ON_WM_PAINT()
	ON_COMMAND(ID_VIEW_OPTIONS, OnViewOptions)
	ON_COMMAND(ID_VIEW_ETHERNET, OnViewEthernet)
	ON_COMMAND(ID_VIEW_LOCAL, OnViewLocal)
	ON_COMMAND(ID_VIEW_SAVE, OnViewSave)
	ON_WM_DESTROY()
	ON_UPDATE_COMMAND_UI(ID_VIEW_OPTIONS, OnUpdateViewOptions)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ETHERNET, OnUpdateViewEthernet)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOCAL, OnUpdateViewLocal)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SAVE, OnUpdateViewSave)
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CChildView message handlers

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS,
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);

  return TRUE;
}

void CChildView::OnPaint()
{
	CPaintDC dc(this); // device context for painting
  CDC* my_dc = GetDC();

	// TODO: Add your message handler code here
  if(bpaint) {
	  try
	  {
    RECT crect;
    GetClientRect(&crect);
    crect.left = (crect.right + crect.left - width)/2;
    crect.top = (crect.bottom + crect.top - height)/2;
    if(crect.left < 0)  crect.left = 0;
    if(crect.top < 0)   crect.top = 0;

    update_status();

    BITMAPINFOHEADER *bih = &bi->bmiHeader;

    bih->biSize					  =	sizeof(BITMAPINFOHEADER);
    bih->biWidth					= width;
    bih->biHeight				  = -height;
    bih->biPlanes				  = 1;
    bih->biCompression		=	BI_RGB;
    bih->biSizeImage			= 0;
    bih->biXPelsPerMeter	= 0;
    bih->biYPelsPerMeter	= 0;
    bih->biClrImportant	  = 0;

	bih->biBitCount       = pixelBitLength;
      bih->biClrUsed				= 0;

      ::SetDIBitsToDevice(	my_dc->m_hDC,
                            crect.left, crect.top, bih->biWidth,	-bih->biHeight,
                            0, 0, 0,						-bih->biHeight,
                            buf[BUF_CNT],
                            bi,
                            DIB_RGB_COLORS);
	  }
	  catch (...) 
	  {       
		  PRINT(("On paint error\n"));
	  }
  }

  ReleaseDC(my_dc);
	// Do not call CWnd::OnPaint() for painting messages
}

void CChildView::OnViewOptions()
{
	// TODO: Add your command handler code here
  boption = !boption;

  opt->Create(IDD_OPTIONS, this);

}

void CChildView::OnViewEthernet()
{
	// TODO: Add your command handler code here
	DWORD threadID;

  bethernet = !bethernet;

  if(bethernet) {
// 	  methernet = CreateThread(NULL, 0, thread_ethernet, this, 0, &threadID);
	  mdraw = CreateThread(NULL, 0, thread_draw, this, 0, &threadID);
    SetThreadPriority(methernet, THREAD_PRIORITY_HIGHEST);
  } else {
    WaitForSingleObject(methernet, 60*1000);
	  WaitForSingleObject(mdraw, 60*1000);
    if(bsave)    OnViewSave();
  }
}

void CChildView::OnViewLocal()
{
	// TODO: Add your command handler code here
	DWORD threadID;

  if(!blocal && dlg->DoModal() != IDOK) {
    return;
  }

  blocal = !blocal;

  if(blocal) {
	  mlocal = CreateThread(NULL, 0, thread_local, this, 0, &threadID);
	  mdraw = CreateThread(NULL, 0, thread_draw, this, 0, &threadID);
  } else {
	  WaitForSingleObject(mlocal, 60*1000);
	  WaitForSingleObject(mdraw, 60*1000);
    if(bsave)    OnViewSave();
  }
}

void CChildView::OnViewSave()
{
	// TODO: Add your command handler code here
// 	DWORD threadID;
// 
//   char str[FIXED_STRING_LENGTH];
//   sprintf_s(str,FIXED_STRING_LENGTH, "No such folder: %s", company);
//   if(!CreateDirectory(company, NULL)) {
//     if(GetLastError() != ERROR_ALREADY_EXISTS) {
//       AfxMessageBox(str);
//       return;
//     }
//   }
// 
//   bsave = !bsave;
// 
//   if(bsave) {
//     msave = CreateThread(NULL, 0, thread_save, this, 0, &threadID);
//   } else {
// 	  WaitForSingleObject(msave, 60*1000);
//   }
}

void CChildView::init_covert_table()
{
   long int crv,cbu,cgu,cgv;
   int i,ind;

   crv = 104597; cbu = 132201;  /* fra matrise i global.h */
   cgu = 25675;  cgv = 53279;

   for (i = 0; i < 256; i++) {
      crv_tab[i] = (i-128) * crv;
      cbu_tab[i] = (i-128) * cbu;
      cgu_tab[i] = (i-128) * cgu;
      cgv_tab[i] = (i-128) * cgv;
      tab_76309[i] = 76309*(i-16);
   }

   for (i=0; i<384; i++)
	  clp[i] =0;
   ind=384;
   for (i=0;i<256; i++)
	   clp[ind++]=i;
   ind=640;
   for (i=0;i<384;i++)
	   clp[ind++]=255;
}

void CChildView::yuv420_bmp(int width, int height)
{
  unsigned char *src = (unsigned char*)output[0];
  unsigned char *dst_ori = buf[BUF_CNT];

	unsigned char *src0;
	unsigned char *src1;
	unsigned char *src2;
	int y1,y2,u,v;
	unsigned char *py1,*py2;
	int i,j, c1, c2, c3, c4;
	unsigned char *d1, *d2;

	//Initialization
	src0=src;
	src1=(unsigned char*)output[1];
	src2=(unsigned char*)output[2];

	py1=src0;
	py2=py1+width;
	d1=dst_ori;
	d2=d1+3*width;

 	for (j = 0; j < height; j += 2) {
		for (i = 0; i < width; i += 2) {

			u = *src1++;
			v = *src2++;

			c1 = crv_tab[v];
			c2 = cgu_tab[u];
			c3 = cgv_tab[v];
			c4 = cbu_tab[u];

			//up-left
			y1 = tab_76309[*py1++];
			*d1++ = clp[384+((y1 + c4)>>16)];
			*d1++ = clp[384+((y1 - c2 - c3)>>16)];
			*d1++ = clp[384+((y1 + c1)>>16)];

			//down-left
			y2 = tab_76309[*py2++];
			*d2++ = clp[384+((y2 + c4)>>16)];
			*d2++ = clp[384+((y2 - c2 - c3)>>16)];
			*d2++ = clp[384+((y2 + c1)>>16)];

			//up-right
			y1 = tab_76309[*py1++];
			*d1++ = clp[384+((y1 + c4)>>16)];
			*d1++ = clp[384+((y1 - c2 - c3)>>16)];
			*d1++ = clp[384+((y1 + c1)>>16)];

			//down-right
			y2 = tab_76309[*py2++];
			*d2++ = clp[384+((y2 + c4)>>16)];
			*d2++ = clp[384+((y2 - c2 - c3)>>16)];
			*d2++ = clp[384+((y2 + c1)>>16)];
		}
		d1 += 3*width;
		d2 += 3*width;
		py1+=   width;
		py2+=   width;
	}
}

void CChildView::yuv422_bmp(int width, int height)
{
  unsigned char *src = (unsigned char*)output[0];
  unsigned char *dst_ori = buf[BUF_CNT];

	unsigned char *src0;
	unsigned char *src1;
	unsigned char *src2;
	int y1,u,v;
	unsigned char *py1;
	int i,j, c1, c2, c3, c4;
	unsigned char *d1, *d2;

	//Initialization
	src0=src;
	src1=(unsigned char*)output[1];
	src2=(unsigned char*)output[2];

	py1=src0;
	d1=dst_ori;
	d2=d1+3*width;

 	for (j = 0; j < height; j ++) {
		for (i = 0; i < width; i += 2) {

			u = *src1++;
			v = *src2++;

			c1 = crv_tab[v];
			c2 = cgu_tab[u];
			c3 = cgv_tab[v];
			c4 = cbu_tab[u];

			//up-left
			y1 = tab_76309[*py1++];
			*d1++ = clp[384+((y1 + c4)>>16)];
			*d1++ = clp[384+((y1 - c2 - c3)>>16)];
			*d1++ = clp[384+((y1 + c1)>>16)];

			//up-right
			y1 = tab_76309[*py1++];
			*d1++ = clp[384+((y1 + c4)>>16)];
			*d1++ = clp[384+((y1 - c2 - c3)>>16)];
			*d1++ = clp[384+((y1 + c1)>>16)];
		}
	}
}

void CChildView::rgb420_bmp(int width, int height)
{
	int i,j;
	unsigned char *pr1,*pr2, *pg1,*pg2, *pb1,*pb2;
	unsigned char *d1, *d2;

	//Initialization
	unsigned char *src0=(unsigned char*)output[0];
	unsigned char *src1=(unsigned char*)output[1];
	unsigned char *src2=(unsigned char*)output[2];
  unsigned char *dst_ori = buf[BUF_CNT];

	pr1=src0;     pr2=pr1+width;
	pg1=src1;	    pg2=pg1+width / 2;
	pb1=src2;   	pb2=pb1+width / 2;
	d1=dst_ori; 	d2=d1+3*width;

 	for (j = 0; j < height; j += 2) {
		for (i = 0; i < width; i += 2) {
			//up-left
            *d1++ = *pb1;
			*d1++ = *pg1;
			*d1++ = *pr1++;

			//down-left
            *d2++ = (*pb1 + *pb2) / 2;
			*d2++ = (*pg1 + *pg2) / 2;
			*d2++ = *pr2++;

			//up-right
			*d1++ = (*pb1 + pb1[1]) / 2;
			*d1++ = (*pg1 + pg1[1]) / 2;
			*d1++ = *pr1++;

			//down-right
            *d2++ = (*pb2 + pb2[1]) / 2;
			*d2++ = (*pg2 + pg2[1]) / 2;
			*d2++ = *pr2++;

            pg1++;
            pg2++;
            pb1++;
            pb2++;
		}
		pr1+=   width;
		pr2+=   width;
		d1 += 3*width;
		d2 += 3*width;
	}
}

void CChildView::rgb_bmp(int width, int height)
{
	unsigned char *src0 = (unsigned char*)output[0];
	unsigned char *src1 = (unsigned char*)output[1];
	unsigned char *src2 = (unsigned char*)output[2];
  unsigned char *dst = buf[BUF_CNT];

  int i, j;

 	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++) {
      *dst++ = *src2++;
      *dst++ = *src1++;
      *dst++ = *src0++;
    }
  }
}

DWORD WINAPI thread_local(void *par)
{
  CChildView* cv = (CChildView *)par;
  char str[FIXED_STRING_LENGTH], str2[FIXED_STRING_LENGTH];
  long sn = 0;
  int index = 0;
  int msec;

  bitsRead = 0;

  sprintf_s(str2,FIXED_STRING_LENGTH, "%s", cv->dlg->GetPathName());
  str2[strlen(str2) - 4] = 0;
  sprintf_s(str,FIXED_STRING_LENGTH, "%s.jp2", str2);
  FILE* fp_vid;
  fopen_s(&fp_vid,str, "rb");
  if (!fp_vid)
  {
	  exit(0);
  }
  sprintf_s(str, FIXED_STRING_LENGTH,"%s.idx", str2);
  FILE* fp_idx;
  fopen_s(&fp_idx, str, "rt");
  if (!fp_idx)
  {
	  fclose(fp_vid);
	  exit(0);
  }

  while(cv->blocal) {
// #if READ_FILE_IN_MEMORY

    if(feof(fp_vid) || feof(fp_idx)) {
      fseek(fp_vid, 0, SEEK_SET);
      fseek(fp_idx, 0, SEEK_SET);
    }
    fscanf_s(fp_idx, "%d\t%d\n", &cv->buf_filled[index], &msec);
    fread(cv->buf[index], 1, cv->buf_filled[index], fp_vid);
	bitsRead += cv->buf_filled[index];
      Sleep(msec);
//    	  Sleep(10);

    InterlockedExchange(&cv->buf_idx, sn);
    index = (++sn) % BUF_CNT;
  }

  fclose(fp_vid);
  fclose(fp_idx);

  return 0;

}


DWORD WINAPI thread_draw(void *par)
{
#if defined(THREAD_SAVE)
  LARGE_INTEGER ts, tp, tc;
	QueryPerformanceCounter ((LARGE_INTEGER *) &ts);
  tp = ts;
   FILE* fp_vid = fopen("video.jp2", "wb");
   FILE* fp_idx = fopen("video.idx", "wt");
#endif
  CChildView* cv = (CChildView *)par;
  long i = -1, j = -1, sn = -1;
//   int rc = 0;

  int handler, lastHandler = -1;



  cv->tcur_p = cv->tstart;
  cv->decode_time[4] = 99999;	// min decode time
  cv->decode_time[5] = 0;		// max decode time
  QueryPerformanceCounter ((LARGE_INTEGER *) &cv->tstart);

  double t = 0;
//   FILE * fp;
//   int bufsize;
//   void *  filebuf;
//   fopen_s(&fp, /*"d:\\test\\in53.jpc"*/"d:\\test\\m97.jp2", "rb");
//   if (fp)
//   {
// 	  fseek(fp, 0, SEEK_END);
// 	  bufsize = ftell(fp);
// 	  fseek(fp, 0, SEEK_SET);
// 	  filebuf = malloc(bufsize);
// 	  fread_s(filebuf, bufsize, 1, bufsize, fp);
// 	  fclose(fp);
//   }
//   CHECK_MEMORY_CREATE(5);
//   CHECK_MEMORY_SET(0, filebuf, bufsize);

  while(cv->bethernet || cv->blocal) {
    
	  InterlockedExchange(&sn, cv->buf_idx);
	 
	  i = sn % BUF_CNT;



    try {
      if(j != i) 
	  {
			j = i;

			if (lastHandler != -1)
			{
				cv->decoder->Release(lastHandler); 
				lastHandler = -1;
			}
			double t;
// 			dbg_error_printf("decode frame %d\n", sn);
			handler = cv->decoder->Decode(/*filebuf*/ cv->buf[i], /*bufsize*/cv->buf_filled[i], t);
			// 		   BMI_ASSERT(CHECK_MEMORY_CMP(0, filebuf, bufsize));

			BMI::Status status = cv->decoder->CheckStatus(handler);


		   if (status != BMI::READY)
		   {
			   PRINT(("Decode error!!\n"));
			   lastHandler = handler;
		   }
		   else
		   {

			   BMI::CodeStreamProperties properties;
			   cv->decoder->CheckProperties(handler, properties);

			   outSizeX =
			   cv->width = properties.mOutputSize.x;
			   outSizeY=
			   cv->height = properties.mOutputSize.y;

			   int picSize;
			   const void * buf = NULL;
			   
			   RESETCLOCK(0);
			   STARTCLOCK(0);

			   buf =  cv->decoder->GetResult(handler, picSize);
			   lastHandler = handler;

			   STOPCLOCK(0);

			   cv->buf[BUF_CNT] = static_cast<unsigned char *>(const_cast<void *>(buf));

			   if (t > 0.000001)
			   {
  				  cv->decode_time[0] = static_cast<int>(t * 10);
				  cv->total_decode_time += cv->decode_time[0];
				  cv->decode_time[4] = cv->decode_time[4] > cv->decode_time[0] ? cv->decode_time[0] : cv->decode_time[4];	// min
				  cv->decode_time[5] = cv->decode_time[5] > cv->decode_time[0] ? cv->decode_time[5] : cv->decode_time[0];	// max
				  ++cv->total_frame;

				  cv->decode_time[1] =  static_cast<int>(GETCLOCKTIME(0) * 10);
			   }
			  QueryPerformanceCounter ((LARGE_INTEGER *) &cv->tcur);
// 			  RESETCLOCK(0);

	#if _DEBUG
			  DEBUG_PRINT(("[%d] frame on picture %d \n", cv->total_frame, sn));
	#endif
			  cv->flash_frame();	  
		   }



      } 
	else 
	  {       
        Sleep(3);
      }
    } catch (...) 
	{       // dec error
		PRINT(("decode error\n"));
      Sleep(10);
    }
  }

  return 0;

}

void CChildView::update_status()
{
  int msec = static_cast<int>((tcur.QuadPart - tcur_p.QuadPart) / tickPerMS.QuadPart);

  if(msec > 1000) {    // refresh rate: maximum one second (30 frames)
    int total_msec = static_cast<int>((tcur.QuadPart - tstart.QuadPart) / tickPerMS.QuadPart);
    double fps = total_frame * 1000.0 / total_msec;

    total_frame_p = total_frame;
    tcur_p = tcur;
	if (decode_time[0])
	{
		sprintf_s(status, FIXED_STRING_LENGTH, "%02d:%02d:%02d  Frame:%6d  Fps:%2.1f    Transfer Rate : %3.2fMB  Video Size %d x %d"/*Dec:%3.1f[min:%3.1f ave:%3.1f max:%3.1f] + download:%2.1f)"*/, //(t1: %2.1f GPU: %3.1f<up:%2.1f dwt:%2.1f down:%2.1f>)",
			(total_msec / 1000) /3600 , 
			((total_msec / 1000)%3600 )/60,
			(total_msec / 1000)%60,
			total_frame, 
			fps,	
			(double)(bitsRead / total_msec) / 125.0,
			outSizeX, 
			outSizeY
// 			(decode_time[0] / 10.0f),	// dec
// 			(decode_time[4] / 10.0f), //min
// 			(total_decode_time / total_frame) / 10.0f, // ave
// 			decode_time[5] / 10.0f,		// max
// 			(decode_time[1] / 10.0f)	// download
			);		
// 			(decode_time[0] - decode_time[1] - decode_time[2] - decode_time[3]) / 10.0f,
// 			(decode_time[1] + decode_time[2] + decode_time[3]) / 10.0f,
// 			(decode_time[1] / 10.0f) ,
// 			(decode_time[2] / 10.0f) ,
// 			(decode_time[3] / 10.0f));
	}
	else
	{
		sprintf_s(status, FIXED_STRING_LENGTH, "Second:%6d.      Frame:%6d.      Fps:%5.1f.      Ratio: %2d : 1.",
            total_msec / 1000, total_frame, fps, back_c.ratio);
	}
    ((CMainFrame*)AfxGetApp()->m_pMainWnd)->update_status();
  }
}

void CChildView::flash_frame()
{
  if(bethernet || blocal) {
    bpaint = 1;
    Invalidate(binv);
    SendMessage(WM_PAINT,0,0);
  }
}

void CChildView::OnDestroy()
{
	CWnd ::OnDestroy();

	// TODO: Add your message handler code here
  if(bethernet)  OnViewEthernet();
  if(blocal)     OnViewLocal();
  if(bsave)      OnViewSave();

  for(int i = 0; i < BUF_CNT/* + 1*/; i ++) {
    delete [] buf[i];
  }

//   delete [] dec_param.output[0];
//   delete [] dec_param.mempool[0];
//   delete [] dec_param.mempool[1];
  delete [] bi;

  delete opt;
  delete dlg;



}

void CChildView::OnUpdateViewOptions(CCmdUI* pCmdUI)
{
	// TODO: Add your command update UI handler code here
  if(!boption)    pCmdUI->Enable(TRUE);
  else            pCmdUI->Enable(FALSE);

}

void CChildView::OnUpdateViewEthernet(CCmdUI* pCmdUI)
{
	// TODO: Add your command update UI handler code here
  if(bethernet)   pCmdUI->SetText("Stop Ethernet");
  else            pCmdUI->SetText("Start Ethernet");

  if(!blocal)     pCmdUI->Enable(TRUE);
  else            pCmdUI->Enable(FALSE);

  pCmdUI->Enable(FALSE);

}

void CChildView::OnUpdateViewLocal(CCmdUI* pCmdUI)
{
	// TODO: Add your command update UI handler code here
  if(blocal)      pCmdUI->SetText("Stop Local");
  else            pCmdUI->SetText("Start Local");

  if(!bethernet)  pCmdUI->Enable(TRUE);
  else            pCmdUI->Enable(FALSE);
}

void CChildView::OnUpdateViewSave(CCmdUI* pCmdUI)
{
	// TODO: Add your command update UI handler code here
  if(bsave)       pCmdUI->SetText("Stop Save Video");
  else            pCmdUI->SetText("Start Save Video");

  if(bethernet)   pCmdUI->Enable(TRUE);
  else            pCmdUI->Enable(FALSE);
  pCmdUI->Enable(FALSE);

}

void CChildView::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
  broi = 1;
	mouse_rect.x0 = static_cast<short>(point.x);
	mouse_rect.y0 = static_cast<short>(point.y);

	CWnd ::OnLButtonDown(nFlags, point);
}

void CChildView::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
  if(broi) {
	  mouse_rect.x1 = static_cast<short>(point.x);
	  mouse_rect.y1 = static_cast<short>(point.y);
  }

  CWnd ::OnMouseMove(nFlags, point);
}

void CChildView::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
  if(broi) {
    broi = !broi;
	  mouse_rect.x1 = static_cast<short>(point.x);
	  mouse_rect.y1 = static_cast<short>(point.y);

    if(mouse_rect.x1 != mouse_rect.x0 && mouse_rect.y1 != mouse_rect.y0) {
      back_c.roi_rect.x0 = (mouse_rect.x0 < mouse_rect.x1) ? mouse_rect.x0 : mouse_rect.x1;
      back_c.roi_rect.y0 = (mouse_rect.y0 < mouse_rect.y1) ? mouse_rect.y0 : mouse_rect.y1;
      back_c.roi_rect.x1 = (mouse_rect.x0 > mouse_rect.x1) ? mouse_rect.x0 : mouse_rect.x1;
      back_c.roi_rect.y1 = (mouse_rect.y0 > mouse_rect.y1) ? mouse_rect.y0 : mouse_rect.y1;
    }
  }

	CWnd ::OnLButtonUp(nFlags, point);
}
