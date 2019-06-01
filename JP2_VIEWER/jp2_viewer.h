// jp2_viewer.h : main header file for the JP2_VIEWER application
//

#if !defined(AFX_JP2_VIEWER_H__C9E89950_76F9_43AF_BC7F_69EB00D87E35__INCLUDED_)
#define AFX_JP2_VIEWER_H__C9E89950_76F9_43AF_BC7F_69EB00D87E35__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CJp2_viewerApp:
// See jp2_viewer.cpp for the implementation of this class
//

class CJp2_viewerApp : public CWinApp
{
public:
	CJp2_viewerApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CJp2_viewerApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

public:
	//{{AFX_MSG(CJp2_viewerApp)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_JP2_VIEWER_H__C9E89950_76F9_43AF_BC7F_69EB00D87E35__INCLUDED_)
