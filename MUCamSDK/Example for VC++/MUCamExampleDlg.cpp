// MUCamExampleDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MUCamExample.h"
#include "MUCamExampleDlg.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMUCamExampleDlg dialog
inline float GetStep(float f)
{

  float param = 1000;
  for(int i = 0; i < 8; i++)
  {
    int test = (int)(f/param);
    if( test > 0)
      break;
    param /= 10;
  }
  return param/10;
}
CMUCamExampleDlg::CMUCamExampleDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMUCamExampleDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMUCamExampleDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	
	m_iCurCameraIndx = -1;
	m_iCameraCount = 0;
	m_iBinningCount = 0;
	m_pWidth = NULL;
	m_pHeight = NULL;
	m_iGainCount = 0;
	m_pGain = NULL;
	m_iMaxOffset = 0;
	m_iMinOffset = 0;
	m_fMaxExposure = 0;
	m_fMinExposure = 0;
	
	m_pBits = NULL;
	m_iWidth = 0;
	m_iHeight = 0;
	m_hTempDC = NULL;
	m_hOldBitmap = NULL;
	m_pBuffer = NULL;
	m_bDragging = false;
	m_iFailedTimes = 0;
	
	InitializeCriticalSection(&m_crtSec);
}

CMUCamExampleDlg::~CMUCamExampleDlg()
{
  DeleteCriticalSection(&m_crtSec);
  if(m_pWidth)delete[]m_pWidth;
  if(m_pHeight)delete[]m_pHeight;
  if(m_pGain)delete[] m_pGain; 
  if(m_hTempDC)
  {
    if(m_hOldBitmap)
    {
      ::DeleteObject(::SelectObject(m_hTempDC, m_hOldBitmap));
    }
    ::DeleteDC(m_hTempDC);
    m_hOldBitmap = NULL;
    m_hTempDC = NULL;
    m_pBits = 0;
  }
  if(m_pBuffer)
    delete[] m_pBuffer;
}

void CMUCamExampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMUCamExampleDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Control(pDX, IDC_COMBO_DEVICES, m_cmbxDevices);
	DDX_Control(pDX, IDC_COMBO_BINNING, m_cmbxBinning);
	DDX_Control(pDX, IDC_EDIT_EXPOSURE, m_cedtExposure);
	DDX_Control(pDX, IDC_EDIT_OFFSET, m_cedtOffset);
	DDX_Control(pDX, IDC_SLIDER_OFFSET, m_sldOffset);
	DDX_Control(pDX, IDC_EDIT_REDGAIN, m_cedtRedGain);
	DDX_Control(pDX, IDC_SLIDER_REDGAIN, m_sldRedGain);
	DDX_Control(pDX, IDC_EDIT_GREENGAIN, m_cedtGreenGain);
	DDX_Control(pDX, IDC_SLIDER_GREENGAIN, m_sldGreenGain);
	DDX_Control(pDX, IDC_EDIT_BLUEGAIN, m_cedtBlueGain);
	DDX_Control(pDX, IDC_SLIDER_BLUEGAIN, m_sldBlueGain);
	DDX_Control(pDX, IDC_CHECK_COOLER, m_cbtCooler);
	DDX_Control(pDX, IDC_CHECK_FLIP, m_cbtFlip);
	DDX_Control(pDX, IDC_CHECK_MIRROR, m_cbtMirror);
	DDX_Control(pDX, IDC_STATIC_SHOW, m_csShow);
	DDX_Control(pDX, IDC_RADIO_8Bits, m_cbt8Bits);
	DDX_Control(pDX, IDC_RADIO_16BITS, m_cbt16Bits);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CMUCamExampleDlg, CDialog)
	//{{AFX_MSG_MAP(CMUCamExampleDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_CBN_SELCHANGE(IDC_COMBO_DEVICES, OnCbnSelchangeComboDevices)
	ON_CBN_SELCHANGE(IDC_COMBO_BINNING, OnCbnSelchangeComboBinning)
	ON_BN_CLICKED(IDC_BUTTON_ROI, OnBnClickedButtonRoi)
	ON_BN_CLICKED(IDC_CHECK_FLIP, OnBnClickedCheckFlip)
	ON_BN_CLICKED(IDC_CHECK_MIRROR, OnBnClickedCheckMirror)
	ON_BN_CLICKED(IDC_CHECK_COOLER, OnBnClickedCheckCooler)
	ON_WM_HSCROLL()
	ON_EN_KILLFOCUS(IDC_EDIT_EXPOSURE, OnEnKillfocusEditExposure)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_EXPOSURE, OnDeltaposSpinExposure)
	ON_WM_TIMER()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_BN_CLICKED(IDC_RADIO_8Bits, OnBnClickedRadiobits)
	ON_BN_CLICKED(IDC_RADIO_16BITS, OnBnClickedRadiobits)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMUCamExampleDlg message handlers

BOOL CMUCamExampleDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	InitCameras();
	SetTimer(0x00110, 100, NULL);
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMUCamExampleDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		if(m_hTempDC)
		{
			CDC* pDC = GetDC();
			RECT rc;
			m_csShow.GetWindowRect(&rc);
			ScreenToClient(&rc);
			::BitBlt(pDC->GetSafeHdc(), rc.left + 1, rc.top + 1, rc.right - rc.left - 1, rc.bottom - rc.top - 1, m_hTempDC, 0, 0, SRCCOPY);      
			//draw ROI Rect
			if(m_retROI.bottom > m_retROI.top && m_retROI.right > m_retROI.left)
			{
				pDC->MoveTo(m_retROI.left + rc.left, m_retROI.top + rc.top);
				pDC->LineTo(m_retROI.left + rc.left, m_retROI.bottom + rc.top);
				pDC->LineTo(m_retROI.right + rc.left, m_retROI.bottom + rc.top);
				pDC->LineTo(m_retROI.right + rc.left, m_retROI.top + rc.top);
				pDC->LineTo(m_retROI.left + rc.left, m_retROI.top + rc.top);
			}
			ReleaseDC(pDC);
		}
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMUCamExampleDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CMUCamExampleDlg::OnCbnSelchangeComboDevices()
{
  //Get Current Selected Camera's index
  int indx = m_cmbxDevices.GetCurSel();
  if(indx == m_iCurCameraIndx)
    return;
  if(indx < 0 && indx >= m_iCameraCount)
    return;
  Lock();
  //close opened camera 
  if(m_iCurCameraIndx != -1)
    CloseCamera(m_hCameras[m_iCurCameraIndx]);
  //open selected camera
  m_iCurCameraIndx = indx;
  if(OpenCamera(m_hCameras[m_iCurCameraIndx]))
  {
	//new Camera is open, change the buffer size  
    UpdateBufferSize(m_pWidth[m_iBinningIndx], m_pHeight[m_iBinningIndx]);
  }
  UnLock();
}

void CMUCamExampleDlg::OnCbnSelchangeComboBinning()
{
  int bin = m_cmbxBinning.GetCurSel();
  if(bin != m_iBinningIndx)
  {    
    Lock();
    if(MUCam_setBinningIndex(m_hCameras[m_iCurCameraIndx], bin))
    {
	  //The exposure time range may be changed.
	  MUCam_getExposureRange(m_hCameras[m_iCurCameraIndx], &m_fMinExposure, &m_fMaxExposure);   
      m_iBinningIndx = bin;
    }
    UpdateBufferSize(m_pWidth[m_iBinningIndx], m_pHeight[m_iBinningIndx]);
    UnLock();
  }
}

void CMUCamExampleDlg::OnBnClickedCheckFlip()
{
  bool bFlip = (m_cbtFlip.GetCheck() == TRUE);
  if(m_iCurCameraIndx >= 0 && m_iCurCameraIndx < m_iCameraCount)
  {
    MUCam_setFlip(m_hCameras[m_iCurCameraIndx],bFlip);
  }
}

void CMUCamExampleDlg::OnBnClickedCheckMirror()
{
  bool bMirror = (m_cbtMirror.GetCheck() == TRUE);
  if(m_iCurCameraIndx >= 0 && m_iCurCameraIndx < m_iCameraCount)
  {
    MUCam_setMirror(m_hCameras[m_iCurCameraIndx], bMirror);
  }
}

void CMUCamExampleDlg::OnBnClickedCheckCooler()
{
  bool bCooler = (m_cbtCooler.GetCheck() == TRUE);
  if(m_iCurCameraIndx >= 0 && m_iCurCameraIndx < m_iCameraCount)
  {
    if(MUCam_activateCooler(m_hCameras[m_iCurCameraIndx], bCooler) == false)
    {
      //Cooler activate fail
      MessageBox(TEXT("Cooler can not be activated!"));
      m_cbtCooler.SetCheck(bCooler? FALSE:TRUE);
    }    
  }
}

void CMUCamExampleDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
  if(pScrollBar)
  {    
    CString str;
   switch(pScrollBar->GetDlgCtrlID())
   {
   case IDC_SLIDER_OFFSET:
     {
       int offset = m_sldOffset.GetPos();
       MUCam_setRGBOffset(m_hCameras[m_iCurCameraIndx], offset, offset, offset);
       str.Format(TEXT("%d"), offset);
       m_cedtOffset.SetWindowText(str);
     }
     break;
   case IDC_SLIDER_REDGAIN:     
   case IDC_SLIDER_GREENGAIN:     
   case IDC_SLIDER_BLUEGAIN:
     {
       int r = m_sldRedGain.GetPos();
       int g = m_sldGreenGain.GetPos();
       int b = m_sldBlueGain.GetPos();
       if(MUCam_setRGBGainIndex(m_hCameras[m_iCurCameraIndx], r, g, b))
       {
         str.Format(TEXT("%f"), m_pGain[r]);
         m_cedtRedGain.SetWindowText(str);
         str.Format(TEXT("%f"), m_pGain[g]);
         m_cedtGreenGain.SetWindowText(str);
         str.Format(TEXT("%f"), m_pGain[b]);
         m_cedtBlueGain.SetWindowText(str);
       }
     }
     break;
   }
  }

  CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}
void CMUCamExampleDlg::OnBnClickedButtonRoi()
{
  if(m_retROI.left == m_retROI.right || m_retROI.bottom == m_retROI.top)
  {
    Lock();
    if(MUCam_setBinningIndex(m_hCameras[m_iCurCameraIndx], m_iBinningIndx))
	{
		//The exposure time range may be changed.
		MUCam_getExposureRange(m_hCameras[m_iCurCameraIndx], &m_fMinExposure, &m_fMaxExposure);
		UpdateBufferSize(m_pWidth[m_iBinningIndx], m_pHeight[m_iBinningIndx]);
	}
    UnLock();
  }
  else
  {
    int left = m_retROI.left;
    int top = m_iHeight - m_retROI.bottom;
    int right = m_retROI.right;
    int bottom = m_iHeight - m_retROI.top;
    Lock();
    if(MUCam_setROI(m_hCameras[m_iCurCameraIndx], &top, &left, &bottom, &right))
    {
	  //The exposure time range may be changed.
	  MUCam_getExposureRange(m_hCameras[m_iCurCameraIndx], &m_fMinExposure, &m_fMaxExposure);
      memset(&m_retROI, 0 , sizeof(RECT));     
      UpdateBufferSize(right - left + 1, bottom - top + 1);
    }
    
    UnLock();
  }
}

void CMUCamExampleDlg::InitCameras()
{
  m_cmbxDevices.ResetContent();
  m_iCameraCount = 0; 

  //Find Cameras
  MUCam_Handle hCamera = MUCam_findCamera();
  CString strName = TEXT("");
  while(hCamera)
  {
	  m_hCameras[m_iCameraCount++] = hCamera;
	  switch(MUCam_getType(hCamera))
	  {     
	  case MUCAM_TYPE_MC1001:
		  strName = TEXT("MC1001");
		  break;
	  case  MUCAM_TYPE_MC2001:
		  strName = TEXT("MC2001");
		  break;
	  case MUCAM_TYPE_MC3001:
		  strName = TEXT("MC3001");
		  break;
	  case MUCAM_TYPE_MC2001B:
		  strName = TEXT("MC2001B");
		  break;
	  case MUCAM_TYPE_MC1002:
		  strName = TEXT("MC1002");
		  break;
	  case MUCAM_TYPE_MC2002:
		  strName = TEXT("MC2002");
		  break;
	  case MUCAM_TYPE_MA205:
		  strName = TEXT("MA205");
		  break;
	  case MUCAM_TYPE_MA285:
		  strName = TEXT("MA285");
		  break;
	  case MUCAM_TYPE_MA252:
		  strName = TEXT("MA252");
		  break;	 
	  case MUCAM_TYPE_MC5001:
		  strName = TEXT("MC5001");
		  break;
	  case MUCAM_TYPE_MC3111:
		  strName = TEXT("MC3111");
		  break;
	  case MUCAM_TYPE_MC3222:
		  strName = TEXT("MC3222");
		  break;	  
	  case MUCAM_TYPE_MA282:
		  strName = TEXT("MA282");
		  break;
    case MUCAM_TYPE_MC10M:
      strName = TEXT("MC10M");
      break;
    case MUCAM_TYPE_IMX224://MoticamS1
       strName = TEXT("MoticamS1");
      break;
    case MUCAM_TYPE_IMX178://MoticamS6
      strName = TEXT("MoticamS6");
       break;
    case MUCAM_TYPE_IMX123://MoticamS3
      strName = TEXT("MoticamS3");
       break;
    case MUCAM_TYPE_IMX226://MoticamS12
      strName = TEXT("MoticamS12");
       break;   
    case MUCAM_TYPE_IMX250://MoticamProS5
      strName = TEXT("MoticamProS5 Plus");
       break;
    case MUCAM_TYPE_IMX264://MoticamS5      
      strName = TEXT("MoticamProS5 Lite");
       break;
    case MUCAM_TYPE_IMX183:
      strName = TEXT("MoticamS20");
      break;
    case MUCAM_TYPE_IMX253:
      strName = TEXT("MoticamProS12");
      break;
	  case MUCAM_TYPE_IMX250M:
	    strName = TEXT("MoticamProS5 Plus M");
	    break;
    case MUCAM_TYPE_IMX264M:
	    strName = TEXT("MoticamProS5 Lite M");
	    break;
    case MUCAM_TYPE_IMX178M:
	    strName = TEXT("MoticamS6 M");
	    break;
    case MUCAM_TYPE_IMX253M:
	    strName = TEXT("MoticamProS12 M");
	    break;
    case MUCAM_TYPE_IMX546:
	    strName = TEXT("MoticamProS8 Lite");
	    break;
    
    case MUCAM_TYPE_IMX546M:
	    strName = TEXT("MoticamProS8 Lite M");
	    break;
    default:
	    strName = TEXT("Unknow Device");
	    break;
	  }
	  //Add the name of camera to the device list
	  m_cmbxDevices.AddString(strName);
	  
	  //find the next camera
	  hCamera = MUCam_findCamera();
  }
}
bool CMUCamExampleDlg::OpenCamera(MUCam_Handle hCamera)
{
  if(hCamera == NULL)
    return false;
  //Open the Camera
  if(MUCam_openCamera(hCamera))
  {
    //@Binning:
    //get binning count
    int binCount = MUCam_getBinningCount(hCamera);
    if(binCount <= 0)
    {
      CloseCamera(hCamera);
      return false;
    }
    if(binCount != m_iBinningCount || m_pWidth == NULL || m_pHeight == NULL)
    {
      m_iBinningCount = binCount;
      if(m_pWidth)delete[] m_pWidth;
      if(m_pHeight)delete[] m_pHeight;
      m_pWidth = new int[m_iBinningCount];
      m_pHeight = new int[m_iBinningCount];
    }	
    //get the binning list
    if(MUCam_getBinningList(hCamera, m_pWidth, m_pHeight))
    {
		//get the binning type
		MUCam_Binning_Type* binType = new MUCam_Binning_Type[binCount];
		MUCam_getBinningType(hCamera, binType);
		CString strType = TEXT("");
      //Update the Binning UI
      CString strBin = TEXT("");
      m_cmbxBinning.ResetContent();
      for(int i = 0; i < m_iBinningCount; i++)
      {		
		  if(binType[i] == MUCAM_BINNING_NORMAL || binType[i] == MUCAM_BINNING_SAMPLING)
		  {
			  strType.Format(TEXT("Bin%d"), i);
		  }
		  else if(binType[i] == MUCAM_BINNING_FAST_DISPLAY)
		  {
			  strType = TEXT("FastDisplay");
		  }
        strBin.Format(TEXT("(%dX%d)"), m_pWidth[i], m_pHeight[i]);
        m_cmbxBinning.AddString(strType + strBin);
      }
    }
    //by default we select binning 0 to display    
    if(MUCam_setBinningIndex(hCamera, 0))
    {
      m_cmbxBinning.SetCurSel(0);
      m_iBinningIndx = 0;
    }
    m_cmbxBinning.EnableWindow(TRUE);

    //@bitCout
    //by default we set the image color depth 8bits:
    m_iBitDepth = 8;
    if(MUCam_setBitCount(hCamera, 8))
    {
      m_iBitDepth = 8;    
    }
    m_cbt8Bits.SetCheck(TRUE);
    m_cbt16Bits.SetCheck(FALSE);
    //Frame Foramt
    m_iColorChannel = 3;//RGB 3 channels
    MUCam_Format fmt = MUCam_getFrameFormat(hCamera);
    if(fmt == MUCAM_FORMAT_MONOCHROME)
    {
      //Monochrome camera
      m_iColorChannel = 1;//only 1 channel
    }    

    //@Exposure    
    MUCam_getExposureRange(hCamera, &m_fMinExposure, &m_fMaxExposure);
    float fval = 10.0f;//set a default exposure value
    MUCam_setExposure(hCamera, fval);
    CString strExposure;
    strExposure.Format(TEXT("%f"), fval);
    m_cedtExposure.SetWindowText(strExposure);

    //@Offset    
    MUCam_getOffsetRange(hCamera, &m_iMinOffset, &m_iMaxOffset);
    m_sldOffset.SetRange(m_iMinOffset, m_iMaxOffset);
    m_sldOffset.SetTic(1);
    m_sldOffset.SetPos(m_iMaxOffset);
    int val = (m_iMaxOffset + m_iMinOffset) / 2;
    MUCam_setRGBOffset(hCamera, val, val, val);
    m_sldOffset.SetPos(val);
    CString strOffset;
    strOffset.Format(TEXT("%d"), val);
    m_cedtOffset.SetWindowText(strOffset);

    //@Gain
    int gainCount = MUCam_getGainCount(hCamera);
    if(gainCount != m_iGainCount)
    {
      m_iGainCount = gainCount;
      if(m_pGain)delete[] m_pGain;
      if(m_iGainCount > 0)
      {
        m_pGain = new float[m_iGainCount];
      }
    }
    MUCam_getGainList(hCamera, m_pGain);
    m_sldRedGain.SetRange(0, gainCount - 1);
    m_sldRedGain.SetTic(1);
    m_sldGreenGain.SetRange(0, gainCount - 1);
    m_sldGreenGain.SetTic(1);
    m_sldBlueGain.SetRange(0, gainCount - 1);
    m_sldBlueGain.SetTic(1);
    //by default we set gain value = 1.
    int r, g, b;
    if(MUCam_setRGBGainValue(hCamera, 1.0f, 1.0f, 1.0f, &r, &g, &b))
    {
      m_sldRedGain.SetPos(r);
      m_sldGreenGain.SetPos(g);
      m_sldBlueGain.SetPos(b);
      CString strGain;
      strGain.Format(TEXT("%f"), m_pGain[r]);
      m_cedtRedGain.SetWindowText(strGain);
      strGain.Format(TEXT("%f"), m_pGain[g]);
      m_cedtGreenGain.SetWindowText(strGain);
      strGain.Format(TEXT("%f"), m_pGain[b]);
      m_cedtBlueGain.SetWindowText(strGain);
    }

    //Cooler
    if(MUCam_isCoolerAvailable(hCamera))
    {
      m_cbtCooler.EnableWindow(TRUE);
    }
    else
    {
      m_cbtCooler.EnableWindow(FALSE);
    }  
    if(MUCam_activateCooler(hCamera, true))
    {
      m_cbtCooler.SetCheck(TRUE);
    }
    else
    {
      m_cbtCooler.SetCheck(FALSE);
    }
    return true;
  }
  return false;
}
void CMUCamExampleDlg::CloseCamera(MUCam_Handle hCamera)
{
  if(hCamera)
  {
    MUCam_closeCamera(hCamera);
  }

  //Reset UI
  m_cmbxBinning.ResetContent();
  m_cmbxBinning.EnableWindow(FALSE);
  m_cbtFlip.SetCheck(FALSE);
  m_cbtMirror.SetCheck(FALSE);
  m_cbtCooler.SetCheck(FALSE);
  m_cbtCooler.EnableWindow(FALSE);  
}
void CMUCamExampleDlg::OnEnKillfocusEditExposure()
{
  CString strExposur;
  m_cedtExposure.GetWindowText(strExposur);
  float fv = _ttof(strExposur.GetBuffer(0));
  if(fv < m_fMinExposure)
  {
    fv = m_fMinExposure;
    strExposur.Format(TEXT("%f"), m_fMinExposure);
    m_cedtExposure.SetWindowText(strExposur);
  }
  else if(fv > m_fMaxExposure)
  {
    fv = m_fMaxExposure;
    strExposur.Format(TEXT("%f"), m_fMaxExposure);
    m_cedtExposure.SetWindowText(strExposur);
  }
  MUCam_setExposure(m_hCameras[m_iCurCameraIndx], fv);
}

void CMUCamExampleDlg::OnDeltaposSpinExposure(NMHDR *pNMHDR, LRESULT *pResult)
{
  LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
  CString strExposur;
  m_cedtExposure.GetWindowText(strExposur);
  float fv = _ttof(strExposur.GetBuffer(0));
  //here we need to calculate the value of exposure to adjust.
  float step = GetStep(fv);
  if(pNMUpDown->iDelta > 0)
  {//decrease
    fv -= step;
    if(fv < m_fMinExposure)fv = m_fMinExposure;
  }
  else
  {//increase
    fv += step;
    if(fv > m_fMaxExposure)fv = m_fMaxExposure;
  }
  MUCam_setExposure(m_hCameras[m_iCurCameraIndx], fv);
  strExposur.Format(TEXT("%f"), fv);
  m_cedtExposure.SetWindowText(strExposur);

  *pResult = 0;
}

void CMUCamExampleDlg::Lock()
{
  EnterCriticalSection(&m_crtSec);
}
void CMUCamExampleDlg::UnLock()
{
  LeaveCriticalSection(&m_crtSec);
}
void CMUCamExampleDlg::UpdateBufferSize(int width, int height)
{
  if(m_hTempDC)
  {
    if(m_hOldBitmap)
    {
      ::DeleteObject(::SelectObject(m_hTempDC, m_hOldBitmap));
    }    
    m_hOldBitmap = NULL;
    m_pBits = 0;
  }
  m_iWidth = width;
  m_iHeight = height;

  //prepare the bitmap to display the frames.
  BITMAPINFOHEADER		bih;
  ZeroMemory(&bih, sizeof(BITMAPINFOHEADER));
  bih.biSize			= sizeof(BITMAPINFOHEADER);
  bih.biBitCount		= 24;
  bih.biWidth			= width;
  bih.biHeight			= height;
  bih.biCompression	    = BI_RGB;
  bih.biPlanes			= 1;
  bih.biSizeImage		= 0;
  CDC* pDC = GetDC();
  HDC	hdc					= pDC->GetSafeHdc();
  HBITMAP hBitmap	= ::CreateDIBSection(hdc, (BITMAPINFO*)&bih, DIB_RGB_COLORS, (void **)&m_pBits, NULL, 0);
  if(m_hTempDC == NULL)
  {
    m_hTempDC	= ::CreateCompatibleDC(hdc);
  }
  m_hOldBitmap = (HBITMAP)::SelectObject(m_hTempDC, hBitmap);
  ReleaseDC(pDC);

  //if the frame format is not 24bits, to realloc the frame buffer. see the OnTimer function
  if(m_iBitDepth*m_iColorChannel != 24)
  {
    if(m_pBuffer)delete[] m_pBuffer;
    m_pBuffer = new unsigned char[width*height*m_iBitDepth*m_iColorChannel/8];
  }
  InvalidateRect(NULL);

}
void CMUCamExampleDlg::OnTimer(UINT_PTR nIDEvent)
{
  if(nIDEvent == 0x00110)
  {
    Lock();
    if(m_iCurCameraIndx >= 0 && m_iCurCameraIndx < m_iCameraCount)
    {
      unsigned char* bits = NULL;
	  // if the frame format is not 24bits, use the m_pBuffer
	  // or else use  the m_pBits, the display frame buffer directly.  
      if(m_iBitDepth*m_iColorChannel != 24)
      {
        bits = m_pBuffer;
      }
      else
      {
        bits = m_pBits;
      }
      if(MUCam_getFrame(m_hCameras[m_iCurCameraIndx], bits, 0))
      {
		m_iFailedTimes = 0;
        if(bits == m_pBuffer)
        {
			// the frame format cannot display with 24bits, conversion is necessary.
          ConvertToBGR24(m_pBuffer, m_pBits);
        }

		// Update the display Area to redraw the new frame.
        RECT rc;
        m_csShow.GetWindowRect(&rc);
        ScreenToClient(&rc);
        InvalidateRect(&rc, FALSE);
      }
	  else
	  {
	    if(m_iFailedTimes++ > 5)
		{
		  m_iFailedTimes = 0;
		  if(MUCam_isConnected(m_hCameras[m_iCurCameraIndx]) == false)
		  {
		    //the camera is unconnected 
		    // KillTimer(0x00110);
		  }
			  
		}
	  }
    }
    UnLock();
  }
  CDialog::OnTimer(nIDEvent);
}

void CMUCamExampleDlg::ConvertToBGR24(BYTE* pSour, BYTE* pDest)
{
  if(pSour == NULL || pDest == NULL)
    return; 
  if(m_iBitDepth > 8)
  {
    unsigned short* pData = (unsigned short*)pSour;
    unsigned char pixel;
    if(m_iColorChannel == 1)
    {
      for(int i = 0; i < m_iHeight;i++)
      {
        for(int j = 0; j < m_iWidth; j++)
        {
          pixel = ((*pData) >> 8);
          *pDest ++ = pixel;
          *pDest ++ = pixel;
          *pDest ++ = pixel;
          pData ++;
        }
      }
    }
    else if(m_iColorChannel == 3)
    {
      for(int i = 0; i < m_iHeight;i++)
      {
        for(int j = 0; j < m_iWidth; j++)
        {
          *pDest ++ = ((*pData ++) >> 8);
          *pDest ++ = ((*pData ++) >> 8);
          *pDest ++ = ((*pData ++) >> 8);
        }
      }
    }

  }
  else
  {
    if(m_iColorChannel == 1)
    {
      for(int i = 0; i < m_iHeight;i++)
      {
        for(int j = 0; j < m_iWidth; j++)
        {
          *pDest ++ = *pSour;
          *pDest ++ = *pSour;
          *pDest ++ = *pSour;
          pSour ++;
        }
      }
    }
  }


}
BOOL CMUCamExampleDlg::OnEraseBkgnd(CDC* pDC)
{  
  return CDialog::OnEraseBkgnd(pDC);
}

void CMUCamExampleDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
  RECT rc;
  m_csShow.GetWindowRect(&rc);
  ScreenToClient(&rc);
  rc.right = rc.right > rc.left + m_iWidth ? rc.left + m_iWidth : rc.right;
  rc.bottom = rc.bottom > rc.top + m_iHeight ? rc.top + m_iHeight : rc.bottom;
  if(point.x >= rc.left && point.x <= rc.right&&point.y >= rc.top && point.y <= rc.bottom)
  {

	//ROI Selected.  
    m_bDragging = true;
    m_retROI.left = point.x - rc.left;
    m_retROI.top = point.y - rc.top;
    m_retROI.right = point.x - rc.left;
    m_retROI.bottom = point.y - rc.bottom;
  }
  CDialog::OnLButtonDown(nFlags, point);
}

void CMUCamExampleDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
  
  m_bDragging = false;
  CDialog::OnLButtonUp(nFlags, point);
}

void CMUCamExampleDlg::OnMouseMove(UINT nFlags, CPoint point)
{
  if(m_bDragging)
  {
    RECT rc;
    m_csShow.GetWindowRect(&rc);
    ScreenToClient(&rc);
    rc.right = rc.right > rc.left + m_iWidth ? rc.left + m_iWidth : rc.right;
    rc.bottom = rc.bottom > rc.top + m_iHeight ? rc.top + m_iHeight : rc.bottom;
    if(point.x >= rc.left && point.x <= rc.right&&point.y >= rc.top && point.y <= rc.bottom)
    {
      if(m_retROI.left > point.x - rc.left)
      {
        m_retROI.right = m_retROI.left;
        m_retROI.left = point.x - rc.left;
      }
      else
      {
        m_retROI.right = point.x - rc.left;
      }
      if(m_retROI.top > point.y - rc.top)
      {
        m_retROI.bottom = m_retROI.top;
        m_retROI.top = point.y - rc.top;
      }
      else
      {
        m_retROI.bottom = point.y - rc.top;
      } 
    }
	//Update the display area to redraw the rectangle of ROI.
    m_csShow.GetWindowRect(&rc);
    ScreenToClient(&rc);
    InvalidateRect(&rc, FALSE);
  }
  CDialog::OnMouseMove(nFlags, point);
}

void CMUCamExampleDlg::OnBnClickedRadiobits()
{
  bool b8bits= m_cbt8Bits.GetCheck() == TRUE;  
  Lock();
  if(b8bits)
  {
	// set the 8 bits count.  
    MUCam_setBitCount(m_hCameras[m_iCurCameraIndx], 8);
    if(m_iBitDepth != 8)
    {
      m_iBitDepth = 8;
      UpdateBufferSize(m_iWidth, m_iHeight);
    }    
  }
  else
  {
	// set the 16 bits count, only some type of cameras support this function,
	//refer to the SDK document.
    if(false == MUCam_setBitCount(m_hCameras[m_iCurCameraIndx], 16))
    {
      MessageBox(TEXT("High bits is not available!"));
      m_cbt8Bits.SetCheck(TRUE);
      m_cbt16Bits.SetCheck(FALSE);      
    }
    else
    {
	  //this camera support the 16 bits count format. 
	 //after change the format success we must change the buffer size.
      if(m_iBitDepth != 16)
      {
        m_iBitDepth = 16;		
        UpdateBufferSize(m_iWidth, m_iHeight);
      }
    }
  }
  UnLock();
    
}
