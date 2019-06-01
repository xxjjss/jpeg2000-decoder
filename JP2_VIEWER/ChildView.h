// ChildView.h : interface of the CChildView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHILDVIEW_H__CB74A4BC_2237_4AA6_9A34_A994A3B28A3D__INCLUDED_)
#define AFX_CHILDVIEW_H__CB74A4BC_2237_4AA6_9A34_A994A3B28A3D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Options.h"

#include "codec_base.h"

#define   RANGE_MIN_RATIO         1
#define   RANGE_MAX_RATIO         100
#define   RANGE_MIN_PROGRESSION   1
#define   RANGE_MAX_PROGRESSION   3

#define  BUF_CNT  30

typedef struct tag_stru_rect {

  // rect coordinate: (x0, y0) and (x1-1, y1-1)
  short x0;                             // x of upper left coner
  short y0;                             // y of upper left coner
  short x1;                             // x+1 of lower right coner
  short y1;                             // y+1 of lower right coner

} stru_rect;

typedef struct tag_stru_back_channel {

	int	      roi;
	int	      rlcp;
	int		    ratio;
	int		    progression;

  stru_rect roi_rect;

} stru_back_channel;

/////////////////////////////////////////////////////////////////////////////
// CChildView window
const int FIXED_STRING_LENGTH	= 400;
const int EACH_FRAME_BUF_SIZE	= 10*1024*1024;

class CChildView : public CWnd
{
// Construction
public:
	CChildView();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildView)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	void flash_frame();
	void update_status();
	void init_covert_table();
	void yuv420_bmp(int width, int height);
	void yuv422_bmp(int width, int height);
	void rgb420_bmp(int width, int height);
	void rgb_bmp(int width, int height);
  inline int ceil_ratio(int num1, int num2) { return (num1 + num2 - 1 ) / num2; }

  int port;
  char ip[FIXED_STRING_LENGTH];
  char company[FIXED_STRING_LENGTH];
  char status[FIXED_STRING_LENGTH];

  unsigned int total_frame, total_frame_p;
  void *output[4];
  int width, height, progression;
  int decode_time[6];
  int total_decode_time;
  LARGE_INTEGER tstart, tcur, tcur_p, tickPerMS;

  UINT32 bethernet, blocal, bsave, bpaint, boption, broi, binv;
  HANDLE methernet, mlocal, msave, mdraw;

  unsigned char* buf[BUF_CNT + 1];
  int buf_filled[BUF_CNT + 1];
  long buf_idx;

  long int crv_tab[FIXED_STRING_LENGTH];
  long int cbu_tab[FIXED_STRING_LENGTH];
  long int cgu_tab[FIXED_STRING_LENGTH];
  long int cgv_tab[FIXED_STRING_LENGTH];
  long int tab_76309[FIXED_STRING_LENGTH];
  unsigned char clp[1024];			//for clip in CCIR601

 	BITMAPINFO* bi;

 stru_rect mouse_rect;
 stru_back_channel back_c;

  COptions *opt;
  CFileDialog *dlg;

  virtual ~CChildView();


  BMI::Decoder * decoder;

	// Generated message map functions
protected:
	//{{AFX_MSG(CChildView)
	afx_msg void OnPaint();
	afx_msg void OnViewOptions();
	afx_msg void OnViewEthernet();
	afx_msg void OnViewLocal();
	afx_msg void OnViewSave();
	afx_msg void OnDestroy();
	afx_msg void OnUpdateViewOptions(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewEthernet(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewLocal(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewSave(CCmdUI* pCmdUI);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHILDVIEW_H__CB74A4BC_2237_4AA6_9A34_A994A3B28A3D__INCLUDED_)
