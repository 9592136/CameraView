using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Motic.MUCam32.Net
{

    public enum MUCam_Type
    {
        MUCAM_TYPE_UNKNOWN,
        MUCAM_TYPE_MC1001,
        MUCAM_TYPE_MC2001,
        MUCAM_TYPE_MC3001,
        MUCAM_TYPE_MC2001B,
        MUCAM_TYPE_MC1002,
        MUCAM_TYPE_MC2002,
        MUCAM_TYPE_MA205,
        MUCAM_TYPE_MA285,
        MUCAM_TYPE_MA252,
        MUCAM_TYPE_SWIFT_MC1002,
        MUCAM_TYPE_MD3300,
        MUCAM_TYPE_MC5001,
        MUCAM_TYPE_MC3111,
        MUCAM_TYPE_MC3222,
        MUCAM_TYPE_MC3022,
        MUCAM_TYPE_SWIFT_MC3001,
        MUCAM_TYPE_SWIFT_MC3222,
        MUCAM_TYPE_SWIFT_MC3111,
        MUCAM_TYPE_MA282,
        MUCAM_TYPE_MC352PLUS,
        MUCAM_TYPE_MC3521,
        MUCAM_TYPE_MC10M,
        MUCAM_TYPE_MC580,
        MUCAM_TYPE_MCHDMI,
        MUCAM_TYPE_VISION_3001,
        MUCAM_TYPE_VISION_3002,
        MUCAM_TYPE_MC3001F,
        MUCAM_TYPE_IMX224,//MoticamS1
        MUCAM_TYPE_IMX178,//MoticamS6
        MUCAM_TYPE_IMX123,//MoticamS3
        MUCAM_TYPE_IMX226,//MoticamS12
        MUCAM_TYPE_MCX3,
        MUCAM_TYPE_IMX250,//MoticamProS5 Plus
        MUCAM_TYPE_IMX264,//MoticamProS5 Lite
        MUCAM_TYPE_IMX183,//MoticamS20
        MUCAM_TYPE_IMX253,//MoticamProS12
		MUCAM_TYPE_IMX250POL,//MoticamProS5 POL
		MUCAM_TYPE_IMX250M,//MoticamProS5 Plus M
		MUCAM_TYPE_IMX264M,//MoticamProS5 Lite M
		MUCAM_TYPE_IMX178M,//MoticamS6 M
		MUCAM_TYPE_IMX253M,//MoticamProS12 M
        MUCAM_TYPE_IMX546,//MoticamProS8 Lite
		MUCAM_TYPE_IMX546M//MoticamProS8 Lite M    
    }
    public enum MUCam_Format
    {

        MUCAM_FORMAT_BAYER_GR_BG,

        MUCAM_FORMAT_BAYER_BG_GR,

        MUCAM_FORMAT_BAYER_GB_RG,

        MUCAM_FORMAT_BAYER_RG_GB,

        MUCAM_FORMAT_COLOR_RGB,

        MUCAM_FORMAT_COLOR_BGR,

        MUCAM_FORMAT_MONOCHROME
    }

    public enum MUCam_Binning_Type
    {
        MUCAM_BINNING_NORMAL,

        MUCAM_BINNING_SAMPLING,
        MUCAM_BINNING_FAST_DISPLAY
    }

    public enum MUCam_Trigger_Type
    {
        MUCAM_TRIGGER_FREE,

        MUCAM_TRIGGER_SOFTWARE,

        MUCAM_TRIGGER_HARDWARE_RISE,

        MUCAM_TRIGGER_HARDWARE_FALL
    }

}
