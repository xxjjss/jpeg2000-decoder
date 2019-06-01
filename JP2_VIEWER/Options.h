#if !defined(AFX_OPTIONS_H__335832B3_C844_45FC_A26F_699BC8643312__INCLUDED_)
#define AFX_OPTIONS_H__335832B3_C844_45FC_A26F_699BC8643312__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Options.h : header file
//

class CChildView;

/////////////////////////////////////////////////////////////////////////////
// COptions dialog

class COptions : public CDialog
{
// Construction
public:
	COptions(CWnd* pParent = NULL);   // standard constructor

  void UpdateView();

// Dialog Data
	//{{AFX_DATA(COptions)
	enum { IDD = IDD_OPTIONS };
	CSliderCtrl	m_ctrl_progression;
	CSliderCtrl	m_ctrl_ratio;
  CStatic m_buddy_ratio;
	BOOL	m_roi;
	BOOL	m_rlcp;
	BOOL	m_lrcp;
	int		m_ratio;
	int		m_progression;
  CChildView  *m_wndView;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COptions)
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnClose();
	afx_msg void OnUpdateData();
	virtual BOOL OnInitDialog();
	afx_msg void OnUpdateData2(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONS_H__335832B3_C844_45FC_A26F_699BC8643312__INCLUDED_)
