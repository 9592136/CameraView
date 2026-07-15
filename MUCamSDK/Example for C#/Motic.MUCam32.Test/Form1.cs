using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using Motic.MUCam32.Net;
using System.Runtime.InteropServices;

namespace Motic.MUCam32.Test
{
    public partial class Form1 : Form
    {
        private IntPtr[] m_hCameras;
        private int m_iCameraCount;
        private int m_iCurCameraIndx;
        private int m_iBinningCount;
        private int[] m_pWidth;
        private int[] m_pHeight;
        private int m_iBinningIndx;
        private int m_iColorChannel;
        private float m_fMinExposure;
        private float m_fMaxExposure;
        private int m_iMinOffset;
        private int m_iMaxOffset;
        private float[] m_pGain;
        private int m_iGainCount;
        private IntPtr m_bufferPtr;
        private Bitmap m_preImage;
        private Boolean m_bRunning;
        private Object thisLock = new Object();

        public Form1()
        {
            InitializeComponent();
            m_hCameras = new IntPtr[32];
            m_iCameraCount = 0;
            m_iCurCameraIndx = -1;
            m_iBinningCount = 0;
            m_iColorChannel = 0;
            m_bRunning = false;
        }
        private void Form1_Load(object sender, EventArgs e)
        {
            InitCameras();
            if (cmbDevice.Items.Count > 0)
            {
                cmbDevice.SelectedIndex = 0;
                Open();
            }
        }
        private void cmbDevice_SelectedIndexChanged(object sender, EventArgs e)
        {
            Open();           
        }
        private void Open()
        {
            int indx = cmbDevice.SelectedIndex;
            if (indx == m_iCurCameraIndx)
                return;
            if (indx < 0 && indx >= m_iCameraCount)
                return;

            //close opened camera 
            if (m_iCurCameraIndx != -1)
                CloseCamera(m_hCameras[m_iCurCameraIndx]);
            //open selected camera
            m_iCurCameraIndx = indx;
            if (OpenCamera(m_hCameras[m_iCurCameraIndx]))
            {
                //new Camera is open, change the buffer size  
                UpdateBufferSize(m_pWidth[m_iBinningIndx], m_pHeight[m_iBinningIndx]);
                m_bRunning = true;
                videoTimer.Start();
            } 
        }
        private void UpdateBufferSize(int width, int height)
        {
            if (m_bufferPtr != IntPtr.Zero)
            {
                Marshal.Release(m_bufferPtr);
            }
            m_bufferPtr = Marshal.AllocHGlobal(width*height*m_iColorChannel);
        }
        
        private bool OpenCamera(IntPtr hCamera)
        {
            if (hCamera == IntPtr.Zero) return false;
            //Open the Camera
            if (MUCam32API.MUCam_openCamera(hCamera))
            {
                //@Binning:
                //get binning count
                int binCount = MUCam32API.MUCam_getBinningCount(hCamera);
                if (binCount <= 0)
                {
                    CloseCamera(hCamera);
                    return false;
                }
                if (binCount != m_iBinningCount)
                {
                    m_iBinningCount = binCount;
                    m_pWidth = new int[m_iBinningCount];
                    m_pHeight = new int[m_iBinningCount];
                }
                //get the binning list
                if (MUCam32API.MUCam_getBinningList(hCamera, m_pWidth, m_pHeight))
                {
                    //get the binning type
                    MUCam_Binning_Type[] binType = new MUCam_Binning_Type[binCount];
                    MUCam32API.MUCam_getBinningType(hCamera, binType);

                    //Update the Binning UI   
                    cmbBinning.ResetText();
                    for (int i = 0; i < m_iBinningCount; i++)
                    {
                        String strBin = "(" + m_pWidth[i] + "X" + m_pHeight[i] + ")";
                        if (binType[i] == MUCam_Binning_Type.MUCAM_BINNING_NORMAL || binType[i] == MUCam_Binning_Type.MUCAM_BINNING_SAMPLING)
                        {
                            cmbBinning.Items.Add("Bin" + i + strBin);
                        }
                        else if (binType[i] == MUCam_Binning_Type.MUCAM_BINNING_FAST_DISPLAY)
                        {
                            cmbBinning.Items.Add("FastDisplay" + strBin);
                        }
                    }
                }
                //by default we select binning 1 to display    
                if (MUCam32API.MUCam_setBinningIndex(hCamera, 1))
                {
                    cmbBinning.SelectedIndex = 1;
                    m_iBinningIndx = 1;
                }

                //Frame Foramt
                m_iColorChannel = 3;//RGB 3 channels
                MUCam_Format fmt = MUCam32API.MUCam_getFrameFormat(hCamera);
                if (fmt == MUCam_Format.MUCAM_FORMAT_MONOCHROME)
                {
                    //Monochrome camera
                    m_iColorChannel = 1;//only 1 channel
                }

                //@Exposure    
                MUCam32API.MUCam_getExposureRange(hCamera, ref m_fMinExposure, ref m_fMaxExposure);
                float fval = 10.0f;//set a default exposure value
                MUCam32API.MUCam_setExposure(hCamera, fval);
                nudExposure.Minimum = (Decimal)(m_fMinExposure * 1000);
                nudExposure.Maximum = (Decimal)(m_fMaxExposure * 1000);
                nudExposure.Value = (Decimal)(fval * 1000);//uSecond


                //@Offset
                MUCam32API.MUCam_getOffsetRange(hCamera, ref m_iMinOffset, ref m_iMaxOffset);
                trackBarOffset.SetRange(m_iMinOffset, m_iMaxOffset);
                trackBarOffset.Value = (m_iMaxOffset + m_iMinOffset) / 2;

                MUCam32API.MUCam_setRGBOffset(hCamera, trackBarOffset.Value, trackBarOffset.Value, trackBarOffset.Value);
                textBoxOffset.Text = "" + trackBarOffset.Value;

                //@Gain
                m_iGainCount = MUCam32API.MUCam_getGainCount(hCamera);

                m_pGain = new float[m_iGainCount];

                MUCam32API.MUCam_getGainList(hCamera, m_pGain);
                trackBarRedGain.SetRange(0, m_iGainCount - 1);

                trackBarGreenGain.SetRange(0, m_iGainCount - 1);

                trackBarBlueGain.SetRange(0, m_iGainCount - 1);

                //by default we set gain value = 1.
                int r = 0, g = 0, b = 0;
                if (MUCam32API.MUCam_setRGBGainValue(hCamera, 1.0f, 1.0f, 1.0f, ref r, ref g, ref b))
                {
                    trackBarRedGain.Value = r;
                    trackBarGreenGain.Value = g;
                    trackBarBlueGain.Value = b;
                    textBoxRedGain.Text = "" + m_pGain[r];
                    textBoxGreenGain.Text = "" + m_pGain[g];
                    textBoxBlueGain.Text = "" + m_pGain[b];
                }               
                return true;
            }
            return false;
        }

        private void CloseCamera(IntPtr hCamera)
        {
            m_bRunning = false;
            lock(thisLock)
            {
                videoTimer.Stop();
            }
            if (hCamera != IntPtr.Zero)
            {
                MUCam32API.MUCam_closeCamera(hCamera);
            }
            //Reset UI
            cmbBinning.ResetText();
            Flip.Checked = false;
            Mirror.Checked = false;
        }

        private void cmbBinning_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (!m_bRunning) return;
            int idx = cmbBinning.SelectedIndex;
            if (idx == m_iBinningIndx) return;
            lock (thisLock)
            {
                if (MUCam32API.MUCam_setBinningIndex(m_hCameras[m_iCurCameraIndx], idx))
                {
                    m_iBinningIndx = idx;
                    UpdateBufferSize(m_pWidth[m_iBinningIndx], m_pHeight[m_iBinningIndx]);
                }
            }
        }

        private void nudExposure_ValueChanged(object sender, EventArgs e)
        {
            if (!m_bRunning) return;
            int val = (int)nudExposure.Value;
            float fval = (val/1000.0f);
            MUCam32API.MUCam_setExposure(m_hCameras[m_iCurCameraIndx], fval);            
        }

        private void trackBarOffset_Scroll(object sender, EventArgs e)
        {
            if (!m_bRunning) return;
            int val = trackBarOffset.Value;
            MUCam32API.MUCam_setRGBOffset(m_hCameras[m_iCurCameraIndx], val, val, val);
        }

        private void trackBarRedGain_Scroll(object sender, EventArgs e)
        {
            OnChangeGain();
        }

        private void trackBarGreenGain_Scroll(object sender, EventArgs e)
        {
            OnChangeGain();
        }

        private void trackBarBlueGain_Scroll(object sender, EventArgs e)
        {
            OnChangeGain();
        }
        private void OnChangeGain()
        {
            if(!m_bRunning)return;
            int r = trackBarRedGain.Value;
            int g = trackBarGreenGain.Value;
            int b = trackBarBlueGain.Value;
            if (MUCam32API.MUCam_setRGBGainIndex(m_hCameras[m_iCurCameraIndx], r, g, b))
            {
                textBoxRedGain.Text = "" + m_pGain[r];
                textBoxGreenGain.Text = "" + m_pGain[g];
                textBoxBlueGain.Text = "" + m_pGain[b];
            }                    
        }

        private void Flip_CheckedChanged(object sender, EventArgs e)
        {
            if (m_iCurCameraIndx >= 0)
            {
                MUCam32API.MUCam_setFlip(m_hCameras[m_iCurCameraIndx], Flip.Checked);
            }
        }

        private void Mirror_CheckedChanged(object sender, EventArgs e)
        {
            if (m_iCurCameraIndx >= 0)
            {
                MUCam32API.MUCam_setMirror(m_hCameras[m_iCurCameraIndx], Mirror.Checked);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        private void InitCameras()
        {
            cmbDevice.ResetText();
            m_iCameraCount = 0;
            String strName;
            //Find Cameras
            IntPtr hCamera = MUCam32API.MUCam_findCamera();
            while (hCamera != IntPtr.Zero)
            {
                //MUCam32API.MUCam_releaseCamera(hCamera);
                m_hCameras[m_iCameraCount++] = hCamera;
                //int type =;
                switch ( MUCam32API.MUCam_getType(hCamera))
                {
                    case MUCam_Type.MUCAM_TYPE_MC1001:
                        strName = "MC1001";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC2001:
                        strName = "MC2001";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC3001:
                        strName = "MC3001";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC2001B:
                        strName = "MC2001B";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC1002:
                        strName = "MC1002";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC2002:
                        strName = "MC2002";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MA205:
                        strName = "MA205";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MA285:
                        strName = "MA285";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MA252:
                        strName = "MA252";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC5001:
                        strName = "MC5001";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC3111:
                        strName = "MC3111";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC3222:
                        strName = "MC3222";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MA282:
                        strName = "MA282";
                        break;
                    case MUCam_Type.MUCAM_TYPE_MC10M:
                        strName = "MC10M";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX224://MoticamS1
                        strName = "MoticamS1";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX178://MoticamS6
                        strName = "MoticamS6";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX123://MoticamS3
                        strName = "MoticamS3";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX226://MoticamS12
                        strName = "MoticamS12";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX250://MoticamProS5
                        strName = "MoticamProS5 Plus";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX264://MoticamS5      
                        strName = "MoticamProS5 Lite";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX183://MoticamS5      
                        strName = "MoticamS20";
                        break;
                    case MUCam_Type.MUCAM_TYPE_IMX253://MoticamS5      
                        strName = "MoticamProS12";
                        break;
                    default:
                        strName = "Unknow Device";
                        break;
                }
                cmbDevice.Items.Add(strName);

                //find the next camera
                hCamera = MUCam32API.MUCam_findCamera();
            }
        }

        private void videoTimer_Tick(object sender, EventArgs e)
        {            
            lock (thisLock)
            {
                if (m_bRunning) Render();
            }
        }

        private void Render()
        {
            Bitmap img = GetFrame();
            if (img != null)
            {
                pictureBox.Image = img;
                if (m_preImage != null)
                {
                    m_preImage.Dispose();
                }
                m_preImage = img;
            }
        }
        private Bitmap GetFrame()
        {
            if (m_bufferPtr == IntPtr.Zero) return null;
            int ts = 0;
            int width = m_pWidth[m_iBinningIndx];
            int height = m_pHeight[m_iBinningIndx];
            if (MUCam32API.MUCam_getFrame(m_hCameras[m_iCurCameraIndx], m_bufferPtr, ref ts))
            {
                Bitmap bmp = new Bitmap(width, height,
                   width * m_iColorChannel,
                   System.Drawing.Imaging.PixelFormat.Format24bppRgb,
                   m_bufferPtr);
                return bmp;
            }
            return null;
        }
        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            videoTimer.Stop();
            if (m_iCurCameraIndx != -1)
                CloseCamera(m_hCameras[m_iCurCameraIndx]);
            if (m_preImage != null)
            {
                m_preImage.Dispose();
            }
            if (m_bufferPtr != IntPtr.Zero)
            {
                Marshal.Release(m_bufferPtr);
            }
        }
    }
}
