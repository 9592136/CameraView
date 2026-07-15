// MUCamExample.h : main header file for the MUCAMEXAMPLE application
//

#if !defined(AFX_MUCAMEXAMPLE_H__43BDB10B_4E59_4A2D_B798_78ECCFC67407__INCLUDED_)
#define AFX_MUCAMEXAMPLE_H__43BDB10B_4E59_4A2D_B798_78ECCFC67407__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CMUCamExampleApp:
// See MUCamExample.cpp for the implementation of this class
//

class CMUCamExampleApp : public CWinApp
{
public:
	CMUCamExampleApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMUCamExampleApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CMUCamExampleApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MUCAMEXAMPLE_H__43BDB10B_4E59_4A2D_B798_78ECCFC67407__INCLUDED_)
