// Options.cpp : implementation file
//

#include "stdafx.h"
#include "jp2_viewer.h"
#include "Options.h"

#include "ChildView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptions dialog


COptions::COptions(CWnd* pParent /*=NULL*/)
	: CDialog(COptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptions)
	m_roi = TRUE;
	m_rlcp = TRUE;
	m_lrcp = !m_rlcp;
	m_ratio = RANGE_MAX_RATIO - 24;
	m_progression = RANGE_MAX_PROGRESSION;
  m_wndView = (CChildView *)pParent;
  //}}AFX_DATA_INIT

  UpdateView();

}


void COptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptions)
	DDX_Control(pDX, IDC_PROGRESSION, m_ctrl_progression);
	DDX_Control(pDX, IDC_RATIO, m_ctrl_ratio);
	DDX_Check(pDX, IDC_ROI, m_roi);
	DDX_Check(pDX, IDC_RLCP, m_rlcp);
	DDX_Check(pDX, IDC_LRCP, m_lrcp);
	DDX_Slider(pDX, IDC_RATIO, m_ratio);
	DDX_Slider(pDX, IDC_PROGRESSION, m_progression);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptions, CDialog)
	//{{AFX_MSG_MAP(COptions)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_ROI, OnUpdateData)
	ON_BN_CLICKED(IDC_RLCP, OnUpdateData)
	ON_BN_CLICKED(IDC_LRCP, OnUpdateData)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_RATIO, OnUpdateData2)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_PROGRESSION, OnUpdateData2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptions message handlers

BOOL COptions::OnInitDialog()
{
	CDialog::OnInitDialog();

  // TODO: Add extra initialization here
  m_ctrl_ratio.SetRange(RANGE_MIN_RATIO, RANGE_MAX_RATIO, TRUE);
  m_ctrl_ratio.SetPos(m_ratio);
  m_ctrl_ratio.SetTicFreq(RANGE_MAX_RATIO/10);

  m_ctrl_progression.SetRange(RANGE_MIN_PROGRESSION, RANGE_MAX_PROGRESSION, TRUE);
  m_ctrl_progression.SetPos(m_progression);
  m_ctrl_progression.SetTicFreq(1);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void COptions::OnClose()
{
	// TODO: Add your message handler code here and/or call default
  m_wndView->boption = !m_wndView->boption;

  CDialog::DestroyWindow();
	CDialog::OnClose();
}

void COptions::OnOK()
{
	// TODO: Add extra validation here
	OnClose();
}

void COptions::OnCancel()
{
	// TODO: Add extra cleanup here
	OnClose();
}

void COptions::OnUpdateData()
{
	// TODO: Add your control notification handler code here
  UpdateData(TRUE);
  UpdateView();

}

void COptions::OnUpdateData2(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: Add your control notification handler code here
	OnUpdateData();

	*pResult = 0;
}

void COptions::UpdateView()
{
	m_wndView->back_c.roi = m_roi;
	m_wndView->back_c.rlcp = m_rlcp;
	m_wndView->back_c.ratio = RANGE_MAX_RATIO - m_ratio + 1;
	m_wndView->back_c.progression = m_progression;

  if(!m_roi) {
    m_wndView->back_c.roi_rect.x0 = 0;
    m_wndView->back_c.roi_rect.y0 = 0;
    m_wndView->back_c.roi_rect.x1 = 0;
    m_wndView->back_c.roi_rect.y1 = 0;
  }

}
