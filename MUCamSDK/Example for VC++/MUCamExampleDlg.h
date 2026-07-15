// MUCamExampleDlg.h : header file
//

#if !defined(AFX_MUCAMEXAMPLEDLG_H__0D818758_C7BB_4A81_8E3E_CC7479A8DC26__INCLUDED_)
#define AFX_MUCAMEXAMPLEDLG_H__0D818758_C7BB_4A81_8E3E_CC7479A8DC26__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "MoticUCam.h"
/////////////////////////////////////////////////////////////////////////////
// CMUCamExampleDlg dialog
#define  MAX_DEVICES 30
class CMUCamExampleDlg : public CDialog
{
// Construction
public:
	CMUCamExampleDlg(CWnd* pParent = NULL);	// standard constructor
	~CMUCamExampleDlg();

// Dialog Data
	//{{AFX_DATA(CMUCamExampleDlg)
	enum { IDD = IDD_MUCAMEXAMPLE_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMUCamExampleDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CMUCamExampleDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
		
protected:
  // devices list
  CComboBox m_cmbxDevices;
  // current binning list
  CComboBox m_cmbxBinning;
  // Exposure value
  CEdit m_cedtExposure;
  // Offset Value
  CEdit m_cedtOffset;
  // Offset value
  CSliderCtrl m_sldOffset;
  //Gain Value
  CEdit m_cedtRedGain;
  CSliderCtrl m_sldRedGain;
  CEdit m_cedtGreenGain;
  CSliderCtrl m_sldGreenGain;
  CEdit m_cedtBlueGain;
  CSliderCtrl m_sldBlueGain;
  
  CButton m_cbtCooler;
  CButton m_cbtFlip;
  CButton m_cbtMirror;
  CButton m_cbt8Bits;
  CButton m_cbt16Bits;
public:
  //Select a Camera
  afx_msg void OnCbnSelchangeComboDevices();
  //Select a binning
  afx_msg void OnCbnSelchangeComboBinning();
  //Set Flip
  afx_msg void OnBnClickedCheckFlip();
  //Set mirror
  afx_msg void OnBnClickedCheckMirror();
  //set cooler
  afx_msg void OnBnClickedCheckCooler();
  afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
  //ROI
  afx_msg void OnBnClickedButtonRoi();
private: 
  MUCam_Handle m_hCameras[MAX_DEVICES];
  int m_iCameraCount;
  int m_iCurCameraIndx;
  int m_iBinningCount;
  int *m_pWidth;
  int *m_pHeight;
  int m_iBitDepth;
  int m_iColorChannel;
  int m_iBinningIndx;

  int m_iMinOffset;
  int m_iMaxOffset;
  int m_iGainCount;
  float *m_pGain;
  float m_fMinExposure;
  float m_fMaxExposure;
  RECT  m_retROI;
  CRITICAL_SECTION m_crtSec;

  BYTE*   m_pBits;
  int     m_iWidth;
  int     m_iHeight;
  HDC     m_hTempDC;
  HBITMAP m_hOldBitmap;
  BYTE*   m_pBuffer;
  bool    m_bDragging;
  int     m_iFailedTimes;
  float   m_fHz;
private:  
  void InitCameras();
  bool OpenCamera(MUCam_Handle hCamera);
  void CloseCamera(MUCam_Handle hCamera);
  afx_msg void OnEnKillfocusEditExposure();
  afx_msg void OnDeltaposSpinExposure(NMHDR *pNMHDR, LRESULT *pResult);
  void Lock();
  void UnLock();  
  void UpdateBufferSize(int width, int height);
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  void ConvertToBGR24(BYTE* pSour, BYTE* pDest);
protected:
  CStatic m_csShow;
public:
  afx_msg BOOL OnEraseBkgnd(CDC* pDC);
public:
  afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
public:
  afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
public:
  afx_msg void OnMouseMove(UINT nFlags, CPoint point);
public:
  afx_msg void OnBnClickedRadiobits();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MUCAMEXAMPLEDLG_H__0D818758_C7BB_4A81_8E3E_CC7479A8DC26__INCLUDED_)
