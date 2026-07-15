using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using System.Runtime.InteropServices;

namespace Motic.MUCam32.Net
{
    public class MUCam32API
    {
        const string DLL_NAME = "MUCam32.dll";
       
        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_findCamera")]
        public static extern IntPtr MUCam_findCamera();

         [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_releaseCamera")]
        public static extern void MUCam_releaseCamera(IntPtr camera); 
 
         [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getType")]
         public static extern MUCam_Type MUCam_getType(IntPtr camera);

         [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_openCamera")]
         public static extern bool MUCam_openCamera(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_closeCamera")]
         public static extern void MUCam_closeCamera(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getFrameFormat")]
        public static extern MUCam_Format MUCam_getFrameFormat(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getFrame")]
        public static extern bool MUCam_getFrame(IntPtr camera, IntPtr buf, ref int ts);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setBitCount")]
        public static extern bool MUCam_setBitCount(IntPtr camera, int bit);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getBinningCount")]
        public static extern int MUCam_getBinningCount(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getBinningList")]
        public static extern bool MUCam_getBinningList(IntPtr camera, int[] w, int[] h);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getBinningType")]
        public static extern bool MUCam_getBinningType(IntPtr camera, MUCam_Binning_Type[] tl);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setBinningIndex")]
        public static extern bool MUCam_setBinningIndex(IntPtr camera, int idx);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setROI")]
        public static extern bool MUCam_setROI(IntPtr camera, ref int top, ref int left, ref int bottom, ref int right);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setFlip")]
        public static extern bool MUCam_setFlip(IntPtr camera, bool b);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setMirror")]
        public static extern bool MUCam_setMirror(IntPtr camera, bool b);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getGainCount")]
        public static extern int MUCam_getGainCount(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getGainList")]
        public static extern bool MUCam_getGainList(IntPtr camera, float[] g);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setRGBGainIndex")]
        public static extern bool MUCam_setRGBGainIndex(IntPtr camera, int r, int g, int b);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setRGBGainValue")]
        public static extern bool MUCam_setRGBGainValue(IntPtr camera, float r, float g, float b, ref int ri, ref int gi, ref int bi);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getOffsetRange")]
        public static extern bool MUCam_getOffsetRange(IntPtr camera, ref int min, ref int max);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setRGBOffset")]
        public static extern bool MUCam_setRGBOffset(IntPtr camera, int r, int g, int b);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getExposureRange")]
        public static extern bool MUCam_getExposureRange(IntPtr camera, ref float min, ref float max);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setExposure")]
        public static extern bool MUCam_setExposure(IntPtr camera, float t);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_isConnected")]
        public static extern bool MUCam_isConnected(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_isCoolerAvailable")]
        public static extern bool MUCam_isCoolerAvailable(IntPtr camera);
        
        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_findCamera")]
        public static extern bool MUCam_activateCooler(IntPtr camera, bool act);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getFrequencyCount")]
        public static extern int MUCam_getFrequencyCount(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setFrequencyIndex")]
        public static extern bool MUCam_setFrequencyIndex(IntPtr camera, int level);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getFrequencyIndex")]
        public static extern int MUCam_getFrequencyIndex(IntPtr camera);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_setTriggerType")]
        public static extern bool MUCam_setTriggerType(IntPtr camera, MUCam_Trigger_Type type);

        [DllImport(DLL_NAME, CharSet = CharSet.Auto, EntryPoint = "MUCam_getTemperature")]
        public static extern bool MUCam_getTemperature(IntPtr camera, ref float st, ref float at, ref float rh);
        
        
    }
}
