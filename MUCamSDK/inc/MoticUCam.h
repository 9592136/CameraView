#ifndef __MOTIC_UCAM_H__
#define __MOTIC_UCAM_H__
#ifndef _WIN32
#pragma GCC visibility push(default)
#endif
/**
 * @file
 *
 * @brief
 *
 * @if Chinese
 * Motic Universal Camera API声明文件。
 * @endif
 *
 * @if English
 * Motic Universal Camera API declaration file.
 * @endif
 */

/**
 * @mainpage Motic Universal Camera API
 *
 * @if Chinese
 * Motic Universal Camera API （以下简称MUCam）是Motic为其各种USB 2.0摄像头提供的编程接口。MUCam之所以称为Universal，
 * 是因为其综合了不同摄像头的硬件特性，并使其接口在Windows平台、Mac OS X平台和Linux平台下保持一致。MUCam提供了一个平面的动态
 * 链接库（包括32位和64位版本）供其它应用程序调用。
 *
 * @endif
 *
 * @if English
 * Motic Universal Camera API (MUCam) is a programming interface for various Motic USB 2.0 cameras. MUCam is "universal"
 * as it covers different camera features and can be used on Windows, MacOS X and Linux. This SDK is a C DLL (including 32-bit
 * and 64-bit version).
 *
 * @endif
 *	
 */

/**
 * @brief
 *
 * @if Chinese
 * 摄像头对象句柄。
 *
 * 该句柄实际为void *类型，不要与int或long类型转换，特别是在64位平台上。
 * @endif
 *
 * @if English
 * The camera object handle type.
 *
 * The handle is the "void *" type, never convert it to "int" or "long",
 * especially in 64-bit operating system.
 * @endif
 */
typedef void * MUCam_Handle;

/**
 * @brief
 *
 * @if Chinese
 * 摄像头对象类型枚举定义。
 *
 * 可以利用该参数确定在应用程序中显示的摄像头名称。
 * @endif
 *
 * @if English
 * The camera identifier.
 *
 * It could be used to determine camera name in an application.
 * @endif
 */
typedef enum
{
  /**
   * @brief 未知类型的摄像头。
   *
   * Unknown camera type.
   */
  MUCAM_TYPE_UNKNOWN,
  /**
   * @brief MC1001类型的摄像头。
   *
   * MC1001在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1280*1024 15帧/秒，640*512 40帧/秒。
   *
   * MC1001 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 15(1280*1024) or 40(640*512).
   */
  MUCAM_TYPE_MC1001,
  /**
   * @brief MC2001类型的摄像头。
   *
   * MC2001在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1600*1200 10帧/秒，800*600 30帧/秒。
   *
   * MC2001 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 10(1600*1200) or 30(800*600).
   */
  MUCAM_TYPE_MC2001,
  /**
   * @brief MC3001类型的摄像头。
   *
   * MC3001在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：2048*1536 6帧/秒，1024*768 21帧/秒。
   *
   * MC3001 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 6(2048*1536) or 21(1024*768).
   */
  MUCAM_TYPE_MC3001,
  /**
   * @brief MC2001B类型的摄像头。
   *
   * MC2001B在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1280*1024 10帧/秒，640*512 30帧/秒。
   *
   * MC2001B camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 10(1280*1024) or 30(640*512).
   */
  MUCAM_TYPE_MC2001B,
  /**
   * @brief MC1002类型的摄像头。
   *
   * MC1002在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1280*1024 14帧/秒，640*512 52帧/秒。
   *
   * MC1002 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 14(1280*1024) or 52(640*512).
   */
  MUCAM_TYPE_MC1002,
  /**
   * @brief MC2002类型的摄像头。
   *
   * MC2002在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1600*1200 5帧/秒，800*600 20帧/秒。
   *
   * MC2002 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 5(1600*1200) or 20(800*600).
   */
  MUCAM_TYPE_MC2002,
  /**
   * @brief MA205类型的摄像头。
   *
   * MA205在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1360*1024 10帧/秒。
   *
   * MA205 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 10(1360*1024).
   */
  MUCAM_TYPE_MA205,
  /**
   * @brief MA285类型的摄像头。
   *
   * MA285在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1360*1024 15帧/秒。
   *
   * MA285 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 15(1360*1024).
   */
  MUCAM_TYPE_MA285,
  /**
   * @brief MA252类型的摄像头。
   *
   * MA252在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：2048*1536 11帧/秒。
   *
   * MA252 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 11(2048*1536).
   */
  MUCAM_TYPE_MA252,
  /**
   * @brief Swift MC1002类型的摄像头。
   *
   * Swift MC1002在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1280*1024 14帧/秒，640*512 52帧/秒。
   *
   * Swift MC1002 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 14(1280*1024) or 52(640*512).
   */
  MUCAM_TYPE_SWIFT_MC1002,
  /**
   * @brief MD3300类型的摄像头。
   *
   * Undocumented.
   *
   * @deprecated Since MA252 added.
   */
  MUCAM_TYPE_MD3300,
  /**
   * @brief MC5001类型的摄像头。
   *
   * MC5001在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：2592*1944 7帧/秒，1296*972 20帧/秒。
   *
   * MC5001 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 7(2592*1944) or 20(1296*972).
   */
  MUCAM_TYPE_MC5001,

  /**
   * @brief MC3111类型的摄像头。
   *
   * MC3111在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1280*1024 13帧/秒，640*480 40帧/秒。
   *
   * MC3111 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 13(1280*1024) or 40(640*480).
   */
  MUCAM_TYPE_MC3111,

  /**
   * @brief MC3222类型的摄像头。
   *
   * MC3222在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1600*1200 10帧/秒，800*600 30帧/秒。  
   *
   * MC3222 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 10(1600*1200) or 30(800*600).
   */
  MUCAM_TYPE_MC3222,

  /**
   * @brief MC3022类型的摄像头。
   *
   * MC3022在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1600*1200 7帧/秒，800*600 20帧/秒。
   *
   * 该摄像头使用软件的方法使得视场大小与MC2002相同
   *
   * MC3022 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 7(1600*1200) or 20(800*600).
   */
  MUCAM_TYPE_MC3022,

  /**
   * @brief Swift MC3001类型的摄像头。
   *
   * Swift MC3001在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：2048*1536 6帧/秒，1024*768 21帧/秒。
   *
   * Swift MC3001 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 6(2048*1536) or 21(1024*768).
   */
  MUCAM_TYPE_SWIFT_MC3001,

  /**
   * @brief Swift MC3222类型的摄像头
   *
   * Swift MC3222在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1600*1200 10帧/秒，800*600 30帧/秒。
   *
   * Swift MC3222 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 10(1600*1200) or 30(800*600).
   */
  MUCAM_TYPE_SWIFT_MC3222,

  /**
   * @brief Swift MC3111类型的摄像头。
   *
   * Swift MC3111在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：1280*1024 13帧/秒，640*480 40帧/秒。
   *
   * Swift MC3111 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 13(1280*1024) or 40(640*480).
   */
  MUCAM_TYPE_SWIFT_MC3111,

  /**
   * @brief MA282类型的摄像头
   *
   * MA282在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：2560*1920 6帧/秒。
   *
   * MA282 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 6(2560*1920).
   */
  MUCAM_TYPE_MA282,

  /**
   * @brief MC352+ Camrea
   *  
   * Undocumented. Not available at present.
   */
  MUCAM_TYPE_MC352PLUS,

  /**
   * @brief MC3521
   *
   * MC3521在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：800*600 30帧/秒，400*300 80帧/秒。
   *
   * MC3521 camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 30(800*600) or 80(400*300).
   *
   */
  MUCAM_TYPE_MC3521,

  /**
   * @brief MC10M类型的摄像头。
   *
   * MC10M在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：3664*2748 2.2帧/秒，1832*1374 8.5帧/秒, 916*686 30帧/秒。
   *
   * MC10M camera type. On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 2.2(3664*2748) or 8.5(1832*1374) or 30(916*686).
   */
  MUCAM_TYPE_MC10M,

   /**
   * @brief Moticam580摄像头,该摄像头支持亮度，对比度，饱和度，色调，锐化参数调整
   *
   * Moticam580在Windows/Linux平台上输出BGR格式的图像，在MacOS X平台上输出RGB格式的图像。
   * 标称帧速率为：30帧/秒,支持1280*960,800*600,640*480三种分辨率
   *
   * Moticam580 camera. This camera supports the image property adjustment,such as brightness, contrast, saturation, hue and sharpness.
   * On Windows/Linux it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 30， has three resolutions: 1280*960,800*600,640*480.
   */
  MUCAM_TYPE_MC580,

  MUCAM_TYPE_MCHDMI,

  //MC3001
  MUCAM_TYPE_VISION_3001,
  //MC3001
  MUCAM_TYPE_VISION_3002,
  //MC3001F
  MUCAM_TYPE_MC3001F,//For Focu purpose

  /**
   * @brief MoticamS1类型的摄像头。
   *
   * MoticamS1 设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为120(1280*960)或240(640*480)
   *
   * MoticamS1 camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
   * The normal FPS is 120(1280*960) or 240(640*480).
   */
    MUCAM_TYPE_IMX224,//MoticamS1

    /**
     * @brief MoticamS6类型的摄像头。
     *
     * MoticamS6设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为30(3072*2048)或50(1536*1024)
     *
     * MoticamS6 camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 30(3072*2048) or 50(1536*1024)
     */
    MUCAM_TYPE_IMX178,//MoticamS6

    /**
     * @brief MoticamS3类型的摄像头。
     *
     * MoticamS3设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为60(2048*1536)或60(1920*1080)
     *
     * MoticamS3 camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 60(2048*1536) or 60(1920*1080)
     */
    MUCAM_TYPE_IMX123,//MoticamS3

      /**
     * @brief MoticamS12类型的摄像头。
     *
     * MoticamS12设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为25(4000*3000) 或 50(2048*1080)
     *
     * MoticamS12 camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 25(4000*3000) or 50(2048*1080)
     */
    MUCAM_TYPE_IMX226,//MoticamS12

    MUCAM_TYPE_MCX3,

    /**
     * @brief MoticamProS5 Plus类型的摄像头。
     *
     * MoticamProS5 Plus设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为68 (2448* 2048)或175(1224*1024)
     *
     * MoticamProS5 Plus camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 68 (2448* 2048) or 175(1224*1024)
     */
    MUCAM_TYPE_IMX250,//MoticamProS5 Plus

    /**
     * @brief MoticamProS5 Lite类型的摄像头。
     *
     * MoticamProS5 Lite设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为35(2448* 2048)或88(1224*1024)
     *
     * MoticamProS5 Lite camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 35(2448* 2048) or 88(1224*1024)
     */
    MUCAM_TYPE_IMX264,//MoticamProS5 Lite

    /**
     * @brief MoticamS20类型的摄像头。
     *
     * MoticamS20 设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为18(5472*3648) 或 20(2736*1824)
     *
     * MoticamS20 camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 18(5472*3648) or 20(2736*1824)
     */
    MUCAM_TYPE_IMX183,//MoticamS20

    /**
     * @brief MoticamProS12类型的摄像头。
     *
     * MoticamProS12设备类型. 在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式. 标称帧率为29(4000* 3000) 或 91(2000*1500)
     *
     * MoticamProS12 camera type. On Windows it exports BGR format images. On MacOS X it exports RGB format images.
     * The normal FPS is 29(4000* 3000) or 91(2000*1500)
     */
    MUCAM_TYPE_IMX253,//MoticamProS12

    /**
    * @brief MoticamProS5 POL类型的摄像头。
    *
    *  MoticamProS5 POL设备类型. 
    *
    * MoticamProS5 POL camera type.des
    */
    MUCAM_TYPE_IMX250POL,//MoticamProS5

    /**
     * @brief MoticamProS5 Plus类型的黑白摄像头。
     *
     * MoticamProS5 Plus设备类型.黑白格式.标称帧率为68(2448 * 2048)或175(1224 * 1024)
     *
     * MoticamProS5 Plus M camera type.Monochrome image format.The maximum FPS is 68 (2448 * 2048) or 175(1224 * 1024)
     */   
    MUCAM_TYPE_IMX250M,
    
    /**
    * @brief MoticamProS5 Lite类型的摄像头。
    *
    * MoticamProS5 Lite黑白设备类型.黑白格式.标称帧率为35(2448 * 2048)或88(1224 * 1024)
    *
    * MoticamProS5 Lite M camera type.Monochrome image format.The maximum FPS is 35(2448 * 2048) or 88(1224 * 1024)
    */
    MUCAM_TYPE_IMX264M,	

    /**
    * @brief MoticamS6 黑白类型的摄像头。
    *
    * MoticamS6 黑白设备类型.黑白格式.标称帧率为30(3072 * 2048)或50(1536 * 1024)
    *
    * MoticamS6 M camera type.Monochrome image format. The maximum FPS is 30(3072 * 2048) or 50(1536 * 1024)
    */
    MUCAM_TYPE_IMX178M,
    
    /**
    * @brief MoticamProS12 黑白类型的摄像头。
    *
    * MoticamProS12设备类型.黑白格式.标称帧率为29(4000 * 3000) 或 91(2000 * 1500)
    *
    * MoticamProS12 M camera type.Monochrome image format. The maximum FPS is 29(4000 * 3000) or 91(2000 * 1500)
    */
    MUCAM_TYPE_IMX253M,	
    
    /**
    * @brief MoticamProS8 Lite类型的摄像头。
    *
    * MoticamProS8 Lite设备类型.在WINDOWS与LINUX系统使用BGR 格式的图像，在MAC OSX中使用RGB格式.标称帧率为29(4000 * 3000) 或 91(2000 * 1500)
    *
    * MoticamProS8 Lite On Windows it exports BGR format images.On MacOS X it exports RGB format images.
    * The normal FPS is 29(4000 * 3000) or 91(2000 * 1500)
    */
    MUCAM_TYPE_IMX546,	
    
    /**
    * @brief MoticamProS8 Lite M 黑白类型的摄像头。
    *
    * MoticamProS8 Lite M设备类型.黑白格式.标称帧率为29(4000 * 3000) 或 91(2000 * 1500)
    *
    * MoticamProS8 Lite M camera type.Monochrome image format. The maximum FPS is 29(4000 * 3000) or 91(2000 * 1500)
    */
    MUCAM_TYPE_IMX546M

} MUCam_Type;

/**
 * @brief 摄像头输出图像格式枚举定义。
 *
 * The camera exported image format definition.
 */
typedef enum
{
  /**
   * @brief GR开头的Bayer图像格式。
   *
   * The bayer image format in GRBG pattern.
   */
  MUCAM_FORMAT_BAYER_GR_BG,
  /**
   * @brief BG开头的Bayer图像格式。
   *
   * The bayer image format in BGGR pattern.
   */
  MUCAM_FORMAT_BAYER_BG_GR,
  /**
   * @brief GB开头的Bayer图像格式。
   *
   * The bayer image format in GBRG pattern.
   */
  MUCAM_FORMAT_BAYER_GB_RG,
  /**
   * @brief RG开头的Bayer图像格式。
   *
   * The bayer image format in RGGB pattern.
   */
  MUCAM_FORMAT_BAYER_RG_GB,
  /**
   * @brief RGB彩色图像格式。
   *
   * The RGB color image format.
   */
  MUCAM_FORMAT_COLOR_RGB,
  /**
   * @brief BGR彩色图像格式。
   *
   * The BGR color image format.
   */
  MUCAM_FORMAT_COLOR_BGR,
  /**
   * @brief 单色图像格式。
   *
   * The monochrome image format.
   */
  MUCAM_FORMAT_MONOCHROME
} MUCam_Format;

/**
 * @brief 摄像头Binning模式。
 *
 * The camera binning type.
 */
typedef enum
{
  /**
   * @brief 正常模式。
   *
   * Normal binning type.
   */
  MUCAM_BINNING_NORMAL,
  /**
   * @brief 子采样模式。
   *
   * 由软件完成的子采样Binning模式。
   *
   * The sampling type completed by software.
   */
  MUCAM_BINNING_SAMPLING,
  /**
   * @brief 快速显示模式。
   *
   * 针对高分辨率摄像头的快速显示模式。
   *
   * The fast display type for high-resolution camera.
   */
  MUCAM_BINNING_FAST_DISPLAY
} MUCam_Binning_Type;

/**
 * @brief 数据采集的触发模式
 */
typedef enum
{
  /**
   * @brief 自由模式（默认）。
   *
   * The trigger free mode. The SDk will automatically grab frames as soon as possible. It's the default mode when a camera opened.
   */
  MUCAM_TRIGGER_FREE,
  /**
   * @brief 软件触发模式。
   *
   * The software trigger mode. Everytime invoking MUCam_getFrame function will cause camera to exposure and transfer a frame data.
   */
  MUCAM_TRIGGER_SOFTWARE,
  /**
   * @brief 硬件触发模式，在脉冲上升沿触发。
   *
   * The external hardware tigger singal on TTL rise side.
   */
  MUCAM_TRIGGER_HARDWARE_RISE,
  /**
   * @brief 硬件触发模式，在脉冲下降沿触发。
   *
   * The external hardware tigger signal on TTL fall side.
   */
  MUCAM_TRIGGER_HARDWARE_FALL
} MUCam_Trigger_Type;

#ifdef __cplusplus
extern "C"{
#endif
/**
 * @brief 查找连接到计算机一个MUCam支持的摄像头设备。
 *
 * 该函数会查找已经连接到计算机的未被使用的一个MUCam摄像头设备，如果成功找到则创建摄像头对象句柄并返回。
 * 如果找不到则返回0。创建的摄像头对象并未初始化打开，使用完毕后必须使用MUCam_releaseCamera释放。查找的
 * 顺序是不确定的。多次调用该函数，直到返回0，可以查找所有连接的摄像头。
 *
 * To find a camera supported by MUCam. This function will search for a unused camera. If find a camera then
 * return its handle, otherwise return 0. The returned camera object is uninitialized. It should be released
 * by MUCam_releaseCamera function after use. The search order is not fixed. Continuously invoke this function
 * until it returns 0 to find all the connected cameras.
 *
 * @return 摄像头对象句柄，失败则返回0。
 *         The camera handle, 0 if failed.
 *
 * @see MUCam_openCamera, MUCam_releaseCamera
 */
MUCam_Handle MUCam_findCamera();

/**
 * @brief 释放一个摄像头对象。
 *
 * 释放一个摄像头对象句柄，该摄像头不需要先被关闭。
 *
 * To release a camera object. Closing it before releasing is not necessary.
 *
 * @param camera 摄像头句柄，函数返回后该句柄不再有效。
 *               The camera handle, not valid after return.
 *
 * @see MUCam_findCamera
 */
void MUCam_releaseCamera(MUCam_Handle camera);

/**
 * @brief 取得摄像头类型。
 *
 * 可以不用打开摄像头就调用该函数。
 *
 * To get the camera identifier. The function can be invoked without opening the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 摄像头类型枚举常量。
 *         The camera identifier.
 *
 * @see MUCam_Type
 */
MUCam_Type MUCam_getType(MUCam_Handle camera);

/**
 * @brief 打开摄像头。
 *
 * 新创建的摄像头对象打开后即可进行各种操作。打开已经被打开的摄像头没有作用，会返回true。摄像头打开后
 * 的各个参数状态是不确定的，应用程序必须根据需要（如从用户配置文件中取得前一次的设置）立即设置各个参
 * 数，然后再开始捕捉图像。
 *
 * To open the camera. The camera should be opened before use it. Invoking this function twice has not any effect.
 * The camera status after openning is uncertain, the application should set all the camera status parameters
 * before capturing an image.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 是否成功打开摄像头。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_closeCamera
 */
bool MUCam_openCamera(MUCam_Handle camera);

/**
 * @brief 关闭摄像头。
 *
 * 关闭已经打开的摄像头对象。关闭未被打开的摄像头没有作用。关闭后的摄像头可以被重新打开。摄像头对象句柄
 * 必须被释放才能完全的释放所有占用的资源。
 *
 * To close the opened camera. The closed camera can be re-opened. The MUCam_releaseCamera function should
 * be invoked to release all resources asscociated with a camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 *
 * @see MUCam_releaseCamera
 */
void MUCam_closeCamera(MUCam_Handle camera);

/**
 * @brief 取得摄像头输出的图像格式。
 *
 * To get the image format exported by the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 图像格式类型。
 *         The image format.
 */
MUCam_Format MUCam_getFrameFormat(MUCam_Handle camera);

/**
 * @brief 读取一帧图像。
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param buf 缓冲区指针，必须足够容纳将取得的图像数据。
 *            The frame buffer, must be big enough for containing the image data.
 * @param ts 存储该帧数据时间戳（毫秒）的指针，可以为0，表示不关心该参数。
 *            The pointer to the buffer that will receive time stamp(ms) of the frame. Might be 0, in which case it is not used.
 * @return 是否成功读取图像。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getFrameFormat
 */
bool MUCam_getFrame(MUCam_Handle camera, unsigned char *buf, unsigned long *ts);

/**
 * @brief 设置图像数据的位深。
 *
 * 设置位深后，再读取的图像就将按该位深存储。数据高低字节顺序的处理由MUCam自动完成，应用程序获得的图像数据
 * 将在位深定义的范围内。
 *
 * @param camera 摄像头句柄。
 * @param bit 数据位深。例如：
 *            @li @c 8  表示单通道8比特位深，图像数据为0~255，用1个字节存储。
 *            @li @c 10 表示单通道10比特位深，图像数据为0~1023，用2个字节存储。
 *            @li @c 16 表示单通道16比特位深，图像数据为0~4095，用2个字节存储。
 * @return 是否成功设置该参数。
 *
 * @attention 图像设备并不能支持所有的位深格式，目前MUCam的摄像头仅支持8位位深格式。
 * @deprecated 目前MUCam所支持的摄像头都只能支持8位位深，所以该函数目前没有作用，在Windows平台的动态链接库中也没有输出该函数。
 *             Not available at present.
 */
bool MUCam_setBitCount(MUCam_Handle camera, int bit);

/**
 * @brief 取得摄像头Binning状态的个数。
 *
 * To get the count of binning supported by the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 摄像头Binning状态的个数。
 *         The count of binning.
 */
int MUCam_getBinningCount(MUCam_Handle camera);

/**
 * @brief 取得摄像头各个Binning的图像尺寸列表。
 *
 * 图像尺寸总是按从大到小的顺序排列，即最大分辨率的尺寸为w[0]和h[0]。
 *
 * To get the image size of each binning. The image size will be descent, i.e. the full resolution size is w[0] and h[0].
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param w 整数数组，用于保存不同Binning下的图像宽度（像素）。其长度必须大于或等于Binning状态个数。
 *          The integer array that will receive the image width(pixel) of each binning. Must be big enough for containing all the data.
 * @param h 整数数组，用于保存不同Binning下的图像高度（像素）。其长度必须大于或等于Binning状态个数。
 *          The integer array that will receive the image height(pixel) of each binning. Must be big enough for containing all the data.
 * @return 是否成功取得该列表。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getBinningCount
 */
bool MUCam_getBinningList(MUCam_Handle camera, int *w, int *h);

/**
 * @brief 取得摄像头各个Binning的类型。
 *
 * To get the type of each binning.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param tl Binning类型数组，其长度必须大于或等于Binning状态个数。
 *           The MUCam_Binning_Type array that will receive the type of each binning. Must be big enough for containing all the data.
 * @return 是否成功取得该列表。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getBinningCount MUCam_Binning_Type
 */
bool MUCam_getBinningType(MUCam_Handle camera, MUCam_Binning_Type *tl);

/**
 * @brief 设置摄像头的Binning索引。
 *
 * To set the index of the selected binning.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param idx Binning索引，从0开始。
 *            The 0-based index of binning.
 * @return 是否成功设置该参数。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getBinningCount
 */
bool MUCam_setBinningIndex(MUCam_Handle camera, int idx);

/**
 * @brief 设置摄像头ROI。
 *
 * ROI的坐标是按当前输出图像（MUCam_getFrame）的尺寸定义的，不论当前的Binning状态是什么、是否已经处于ROI状态和
 * 是否有设置垂直和水平的翻转。当前图像的左上角为（0，0），右下角为（W - 1，H - 1）。函数成功返回后，输入参数
 * 的ROI会被转换到全分辨率下的坐标位置，并可能会根据硬件的要求被调整邻近合适的位置。
 *
 * 考虑到Windows平台绘制位图时要求图像的宽度按4字节对齐，函数对ROI的宽度进行了修正，保证返回的ROI宽度总是按4字
 * 节对齐的。应用程序可以不必再考虑对齐问题。
 *
 * To set the ROI(region of interesting) of the camera. The input coordinates are based on the size of currently
 * exported image, without consideration of the current binning, ROI and flip and mirror settings. The left-top
 * corner of currently exported image is (0,0), the right-bottom is (W-1, H-1). If the function returns successfully,
 * the input coordinate parameters will be transformed to be based on camera sensor matrix, and its value might be adjusted.
 *
 * The ROI image width will be adjusted to align by 4 byte for rendering optimization on Windows.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param top ROI的顶边坐标，以像素为单位，从0开始，函数成功返回后为实际设置的坐标。
 *            The 0-based top coordinate(pixel). After successful return, it will be the coordinate on sensor matrix.
 * @param left ROI的左边坐标，以像素为单位，从0开始，函数成功返回后为实际设置的坐标。
 *            The 0-based left coordinate(pixel). After successful return, it will be the coordinate on sensor matrix.
 * @param bottom ROI的底边坐标，以像素为单位，从0开始，函数成功返回后为实际设置的坐标。bottom - top + 1即为ROI的高度（像素）。
 *            The 0-based bottom coordinate(pixel). After successful return, it will be the coordinate on sensor matrix.
 *            The bottom-top+1 equals the width(pixel) of ROI.
 * @param right ROI的右边坐标，以像素为单位，从0开始，函数成功返回后为实际设置的坐标。right - left + 1即为ROI的宽度（像素）。
 *            The 0-based right coordinate(pixel). After successful return, it will be the coordinate on sensor matrix.
 *            The right-left+1 equals the height(pixel) of ROI.
 * @return 是否成功设置摄像头ROI。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getFrame
 */
bool MUCam_setROI(MUCam_Handle camera, int *top, int *left, int *bottom, int *right);

/**
 * @brief 设置摄像头垂直翻转状态。
 *
 * To flip the camera image.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param b 是否垂直翻转摄像头图像。
 *          The flip setting.
 * @return 是否成功设置该参数。
 *         Returns true if successful; false if failed.
 */
bool MUCam_setFlip(MUCam_Handle camera, bool b);

/**
 * @brief 设置摄像头水平翻转状态。
 *
 * To mirror the camera image.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param b 是否水平翻转摄像头图像。
 *          The mirror setting.
 * @return 是否成功设置该参数。
 *         Returns true if successful; false if failed.
 */
bool MUCam_setMirror(MUCam_Handle camera, bool b);

/**
 * @brief 取得摄像头所支持的硬件增益值个数。
 *
 * 摄像头硬件不能支持连续的增益值，应用程序如果要使用硬件增益，则只能从有限值中选择。
 *
 * To get the count of the gain supported by camera. The camera gain is a series of discrete values, the application only can select one of them.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 支持的增益值个数。
 *         The count of the gain supported by camera.
 */
int MUCam_getGainCount(MUCam_Handle camera);

/**
 * @brief 取得摄像头所支持的增益值列表。
 *
 * 如果应用程序不直接设置硬件增益值，则可以忽略本函数，直接设置增益的索引即可。
 *
 * To get the gain value list.
 *
 * @param camera 摄像头句柄。
 *               The camera setting.
 * @param g 浮点数组，必须足够容纳所有的增益值。
 *          The float array, must be big enough for containing data.
 * @return 是否成功取得该列表。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getGainCount
 */
bool MUCam_getGainList(MUCam_Handle camera, float *g);

/**
 * @brief 设置RGB各颜色通道的增益值索引。
 *
 * To set the gain index of RGB channel.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param r 红色通道增益索引，从0开始。如果超过许可范围，则不改变当前值，函数返回false。
 *          The 0-based gain index of red channel.
 * @param g 绿色通道增益索引，从0开始。如果超过许可范围，则不改变当前值，函数返回false。
 *          The 0-based gain index of green channel.
 * @param b 蓝色通道增益索引，从0开始。如果超过许可范围，则不改变当前值，函数返回false。
 *          The 0-based gain index of blue channel.
 * @return 是否成功设置该参数。
 *          Returns true if successful; false if failed.
 *
 * @attention 某些摄像头不支持RGB各个通道使用不同的增益值，所以只有在输入参数都相同的情况下才会成功执行。
 *            Some camears don't support different gain values in RGB channels. The function will return true only when the parameters r,g,b are the same.
 *
 * @see MUCam_getGainCount
 */
bool MUCam_setRGBGainIndex(MUCam_Handle camera, int r, int g, int b);

/**
 * @brief 设置RGB各颜色通道的增益值。
 *
 * 当应用程序通过计算得到（如做白色平衡时）各个颜色通道的增益值后，则可以用本函数直接设置期望的增益值。函数会在允许的增益值列表中
 * 选择最接近的值进行设置，并返回实际选择的增益值索引。由于硬件增益不能保证最终得到的图像颜色值被精确缩放到指定倍数，所以可以进行
 * 多次设置来得到理想的效果。一般3次测量设置即可满足要求。可以参考如下代码：
 *
 * @code
 *
 * void doWhiteBalance()
 * {
 *   ROI = getROI();
 *   for (1 to 3)
 *   {
 *     Image = grabImage();
 *     CalculateWhiteBalance(Image, ROI, &redGain, &greenGain, &blueGain);
 *     if (redGain != 1 && greenGain != 1 && blueGain != 1)
 *     {
 *       redGain   *= gainValue[currentRedGainIndex];
 *       greenGain *= gainValue[currentGreenGainIndex];
 *       blueGain  *= gainValue[currentBlueGainIndex];
 *
 *       MUCam_setRGBGainValue(camera, redGain, greenGain, blueGain, &currentRedGainIndex, &currentGreenGainIndex, &currentBlueGainIndex);
 *     }
 *     else
 *     {
 *       break;
 *     }
 *   }
 * }
 *
 * @endcode
 *
 * 图像的白色平衡取决于光源的状态，当光源变化（如在转换物镜时调整亮度）时必须要重新计算白色平衡的增益参数。而改变曝光时间是不会改变
 * 图像的白色平衡的。所以应用程序可以用调整曝光时间来避免多次计算白色平衡。
 *
 * To set the gain value of RGB channel. When the application calculated the expected gain value(eg. do white-balance processing)
 * it could invoke the function to set the gain value directly. The function will find the closest gain value in gain value list, and
 * return the index actual set. Because the camera hardware gain is not an exact multiply calculation, the application could invoke the
 * function several times to get better result. The above-mentioned sample code shows the precedure.
 *
 * The white-balance of image is influenced by the light source. The application should re-calculate white-balance gain value after changing
 * the light source. But, the exposure time does not affect the white-balance status. It is suggested that the application adjusts exposure
 * time instead of changing the light source.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param r 期望的红色通道增益值。
 *          The expected gain value of red channel.
 * @param g 期望的绿色通道增益值。
 *          The expected gain value of green channel.
 * @param b 期望的蓝色通道增益值。
 *          The expected gain value of blue channel.
 * @param ri 整数指针，取得实际选择的红色通道增益值索引。可以为0，表示不想取得该参数。
 *           The pointer to the buffer that will receieve the gain index of red channel. Might be 0, in which case it is not used.
 * @param gi 整数指针，取得实际选择的绿色通道增益值索引。可以为0，表示不想取得该参数。
 *           The pointer to the buffer that will receieve the gain index of green channel. Might be 0, in which case it is not used.
 * @param bi 整数指针，取得实际选择的蓝色通道增益值索引。可以为0，表示不想取得该参数。
 *           The pointer to the buffer that will receieve the gain index of blue channel. Might be 0, in which case it is not used.
 * @return 是否成功设置该参数。
 *         Returns true if successful; false if failed.
 *
 * @attention 某些摄像头不支持RGB各个通道使用不同的增益值，所以只有在输入参数都相同的情况下才会成功执行。
 *            Some camears don't support different gain values in RGB channels. The function will return true only when the parameters r,g,b are the same.
 *
 * @see MUCam_getGainCount, MUCam_setRGBGainIndex
 */
bool MUCam_setRGBGainValue(MUCam_Handle camera, float r, float g, float b, int *ri, int *gi, int *bi);

/**
 * @brief 取得摄像头偏移值的闭区间范围。
 *
 * 偏移值（Offset）均为整数值。
 *
 * To get the offset range of the camera.
 *
 * @attention 偏移值并非以颜色灰度等级为单位，只是表达了硬件可以达到的偏移效果。
 *            The offset value is not based on gray level. It just means the offset effect that can be achieved by the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param min 整数指针，保存偏移范围最小值。
 *            The pointer to the buffer that will receieve the minimum offset value.
 * @param max 整数指针，保存偏移范围最大值。
 *            The pointer to the buffer that will receieve the maximum offset value.
 * @return 是否成功取得参数范围。
 *         Returns true if successful; false if failed.
 */
bool MUCam_getOffsetRange(MUCam_Handle camera, int *min, int *max);

/**
 * @brief 设置RGB各颜色通道的偏移值。
 *
 * To set the offset value of RGB channel.
 *
 * @attention 偏移值并非以颜色灰度等级为单位，只是表达了硬件可以达到的偏移效果。偏移为100，并不能使实际得到的象素颜色增加100。
 *            The offset value is not based on gray level, eg. the offse 100 does not mean the actual pixel gray level can be increased 100.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param r 红色通道偏移，如果超过许可范围，则不改变当前值，函数返回false。
 *          The offset value of red channel.
 * @param g 绿色通道偏移，如果超过许可范围，则不改变当前值，函数返回false。
 *          The offset value of green channel.
 * @param b 蓝色通道偏移，如果超过许可范围，则不改变当前值，函数返回false。
 *          The offset value of blue channel.
 * @return 是否成功设置该参数。
 *         Returns true if successful; false if failed.
 *
 * @attention 某些摄像头不支持RGB各个通道使用不同的偏移值，所以只有在输入参数都相同的情况下才会成功执行。
 *            Some camears don't support different offset values in RGB channels. The function will return true only when the parameters r, g, b are the same.
 *
 * @see MUCam_getOffsetRange
 */
bool MUCam_setRGBOffset(MUCam_Handle camera, int r, int g, int b);

/**
 * @brief 取得摄像头当前状态下支持的曝光参数闭区间范围。
 *
 * 设置Binning和ROI都有可能会改变当前的曝光范围，应用程序在做上述操作后，要及时检查，以保证用户界面的对应。
 *
 * To get the current exposure time range of the camera. The setting of binning and ROI will change the exposure time range, the application
 * should have a check after finishing these settings.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param min 浮点数指针，用于保存当前的曝光最小值（多数摄像头是以毫秒为单位，MC1002/2002是无单位的数值）。
 *            The pointer to the buffer that will receieve the minimum exposure value. For MC1002/2002, it's a value without unit; for other
 *            camera, it's based on millisecond.
 * @param max 浮点数指针，用于保存当前的曝光最大值（多数摄像头是以毫秒为单位，MC1002/2002是无单位的数值）。
 *            The pointer to the buffer that will receieve the maximum exposure value. For MC1002/2002, it's a value without unit; for other
 *            camera, it's based on millisecond.
 * @return 是否成功取得参数范围。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_setBinningIndex, MUCam_setROI
 */
bool MUCam_getExposureRange(MUCam_Handle camera, float *min, float *max);

/**
 * @brief 设置摄像头曝光参数。
 *
 * To set the exposure time of the camera.
 *
 * @attention 曝光参数必须在设置过Binning或者ROI之后才能被设置，即打开摄像头后要先设置图像尺寸后再设置曝光参数。
 *            The exposure time shuld be set after setting binning or ROI.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param t 曝光参数（多数摄像头是以毫秒为单位，MC1002/2002是无单位的数值）。如果该值超过当前许可的范围，则不改变当前值，函数返回false。
 *          The exposure time.
 * @return 是否成功设置该参数。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getExposureRange, MUCam_setBinningIndex, MUCam_setROI
 */
bool MUCam_setExposure(MUCam_Handle camera, float t);

/**
 * @brief 检查设备是否还连接在计算机上。
 *
 * 如果连续多次读取图像设备出错时，可以利用该函数检查是否该摄像头已经从计算机上拔出。
 *
 * To check whether the camera is still connected. Could invoke this function to check whether the camera has been un-plugged
 * if failed to get image from camera many times.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 该设备是否仍然连接。
 *         Returns true if camera is connected; false if camera has been removed.
 */
bool MUCam_isConnected(MUCam_Handle camera);

/**
 * @brief 获得设备是否支持冷却的功能，只有部分设备支持。
 *
 * To check whether the cooler is available in the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 是否成功，如果失败表示不支持冷功能。
 *         Returns true if the camera has a cooler; false if not.
 */
bool MUCam_isCoolerAvailable(MUCam_Handle camera);

/**
 * @brief 设置设备冷却的功能,只有部分设备支持。
 *
 * To activate or inactivate the cooler in the camera.
 *
 * @param camera 摄像头句柄
 *               The camera handle.
 * @param act 是否打开冷却功能，true打开冷却，false关闭冷却。
 *            True for activating the cooler, false for inactivating.
 * @return 是否成功设置。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_isCoolerAvailable
 */
bool MUCam_activateCooler(MUCam_Handle camera, bool act);

/**
 * @brief 获得摄像头可调整的工作频率级别的个数。
 *
 * To get the working frequency count supported by the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @return 支持可调整的频率级别个数，如果返回n则可调整的频率级别有0 1 ... n-1。其中0为最高工作频率，n-1为最低工作频率。
 *         The count of working frequency. The available working frequency is from 0 to n-1, 0 means the fastest.
 */
int MUCam_getFrequencyCount(MUCam_Handle camera);

/**
 * @brief 设置摄像头的工作频率级别。
 *
 * To set the working frequency index of the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param level 工作频率的级别索引。
 *               The index of working frequency.
 * @return 是否成功设置。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getFrequencyCount
 */
bool MUCam_setFrequencyIndex(MUCam_Handle camera, int level);

/**
 * @brief 获取摄像头当前工作频率的级别索引。
 *
 * To get current working frequency index of the camera.
 *
 * @param camera 摄像头句柄
 *               The camera handle.
 * @return 当前工作频率的级别索引。
 *         Current working frequency index.
 */
int MUCam_getFrequencyIndex(MUCam_Handle camera);

/**
 * @brief 设置软件的触发模式。
 *
 * To set the trigger mode of the camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param type 触发模式类型。
 *             The type of trigger mode.
 *
 * @return 是否设置成功。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_Trigger_Type
 */
bool MUCam_setTriggerType(MUCam_Handle camera, MUCam_Trigger_Type type);

/**
 * @brief 查寻温度湿度值
 *
 * To get temperature and humidity values. Only some cameras support the function
 *
 * @param camrea 摄像头句柄
 *               The camera handle
 *
 * @param st 传感器表面温度(摄氏温度)
 *           Sensor surface temperature (degrees Celsius)
 *
 * @param at 环境温度(摄氏温度)
 *           Ambient temperature (degrees Celsius)
 *
 * @param rh 相对湿度
 *           Relative humidity
 *
 * @return 是否成功获取
 *         True if successful; False if failed
 */
bool MUCam_getTemperature(MUCam_Handle camera, float *st, float *at, float *rh);

/**
 * @brief 查寻当前的设备是否是USB3.0设备。
 *
 * To check whether the camera is a usb3.0 camera.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param flag 返回当前设备的工作模式，0表示工作在USB2.0模式，1表示工作在USB3.0模式。
 *               receive current camera working mode, return 0 means working on USB2.0 mode, 
 *               1 means working on USB3.0 mode.
 *
 * @return 是否为USB3.0设备。
 *         Returns true if it is USB3.0 camera; false USB2.0 Camera.
 *
 */
bool MUCam_isUSB3Cam(MUCam_Handle camera, int*flag);

 /**
 * @brief 获取当前Bayer图像格式。
 *
 * To get the format of Bayer data (Raw data) .
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 *
 * @return 返回Bayer图像的格式.该值减去7与MUCam_Format中的Bayer类型对应相同
 *         Return Bayer format, the return value minus 7 will equate to the Bayer type defined in MUCam_Format
 *
 *@see  MUCam_getBayer,MUCam_bayer2RGB
*
 */
int MUCam_getBayerFormat(MUCam_Handle camera);

  /**
 * @brief 直接获取原始的Bayer图像数据,Bayer的格式由MUCam_getBayerFormat获取，可通过MUCam_bayer2RGB转为RGB图像
 *
 * To grab a frame of bayer image data, the format of image can be obtained from MUCam_getBayerFormat，
 * and the bayer image can be converted to RGB image by calling MUCam_bayer2RGB
 *
 *@param buf 缓冲区指针，必须足够容纳将取得的图像数据。
 *            The frame buffer, must be big enough for containing the image data.
 * @param ts 存储该帧数据时间戳（毫秒）的指针，可以为0，表示不关心该参数。
 *            The pointer to the buffer that will receive time stamp(ms) of the frame. Might be 0, in which case it is not used.
 * @return 是否成功读取图像。
 *         Returns true if successful; false if failed.
 *
 * @see MUCam_getBayerFormat, MUCam_bayer2RGB
*
 */
bool MUCam_getBayer(MUCam_Handle camera, unsigned char *buf, unsigned long *ts);


 /**
 * @brief .将Bayer数据转化成RGB数据
 *
 * Convert the bayer image data to RGB image.
 *
 * @param camera 摄像头句柄。
 *               The camera handle.
 * @param bayer  Bayer图像格式的缓冲区指针，该Bayer数据可由MUCam_getBayer获取
				The bayer image buffer, read from MUCam_getBayer.
 * @param fmt   Bayer图像的格式，与MUCam_getBayerFormat获取的值相同
                The format of bayer, read from MUCam_getBayerFormat
 * @param w     图像的宽度
                The width of image
 * @param h   图像的高度
				The Height of image
 * @param bitcount 图像的位深,默认为8
                 The color depth for the image, the default value is 8
 * @param rgb 用于接收转化后的RGB图像的续冲区，必须足够容纳图像数据
				 The RGB image buffer, must be large enough to hold the image data

 *  @return 是否成功
 *         True if successful; False if failed
 *
 *@see  MUCam_getBayerFormat, MUCam_getBayer, MUCam_setBitCount
*
 */
bool MUCam_bayer2RGB(MUCam_Handle camera, unsigned char* bayer, int fmt, int w, int h, int bitcount, unsigned char* rgb);


/**
* @brief .设置自动曝光，目前只有MoticamProS8 Lite或MoticamProS8 Lite M有该功能
*
* set Auto Exposure，Only some cameras support the function(MoticamProS8 Lite,MoticamProS8 Lite M only currently)
*
* @param camera 摄像头句柄。
*               The camera handle.
* @param target 自动曝光的目标亮度，取值范围0~255, 0表示手动曝光，1~255表示硬件自动曝光
*               Target brightness, range: 0~255, 0 means manual exposure, 1~255 for auto exposure
* @param exposure 当前的曝光时间 设为手动曝光时(target=0)有效， 自动曝光时(target=1~255)无效
*                 Current exposure value, Only available when target=0  
*  @return 成功返回true; 失败返回false. 失败表示不支持该功能
*         True if successful; False if the camera does not support this function
*
*@see  MUCam_setAERect
*/
bool MUCam_setAutoExposure(MUCam_Handle camera, int target, float* exposure);

/**
* @brief 设置自动曝光的参考区域，只有MoticamProS8 Lite或MoticamProS8 Lite M有该功能
*
* set Auto Exposure，Only some cameras support the function(MoticamProS8 Lite,MoticamProS8 Lite M only currently)
*
* @param camera 设置自动曝光的参考区域,目前只有MoticamProS8 Lite或MoticamProS8 Lite M有该功能
*               Set the reference area for the auto exposure,MoticamProS8 Lite,MoticamProS8 Lite M only currently
* @param x 自动曝光区域的左边坐标，以像素为单位，从0开始。
*          The 0-based left coordinate(pixel).
* @param y 自动曝光区域的顶边坐标，以像素为单位，从0开始。
*          The 0-based top coordinate(pixel). 
* @param w 自动曝光区域的宽度，以像素为单位。
*          The width (pixel) of reference area. 
* @param h 自动曝光区域的高度，以像素为单位。
*          The height(pixel) of reference area. 
*  @return 成功返回true; 失败返回false. 失败表示不支持该功能
*         True if successful; False if the camera does not support this function
*
*@see  MUCam_setAutoExposure
*/
bool MUCam_setAERect(MUCam_Handle camera, int x, int y, int w, int h);

/**
* @brief 设置开启白平衡，目前只有MoticamProS8 Lite或MoticamProS8 Lite M有该功能
*
* Set the white balance status，Only some cameras support the function(MoticamProS8 Lite,MoticamProS8 Lite M only currently)
*
* @param camera 摄像头句柄。
*               The camera handle.
* @param status 白平衡状态，0关闭硬件白平衡，1使用当前图像计算白平衡参数并开启，2开启白平衡,使用最近一次记录的白平衡参数
*               The white balance status, 0 close white balance, 1 calcuate and apply the white balance base on current image, 2 open white balance by using the last white balance paramters
*  @return 成功返回true; 失败返回false. 失败表示不支持该功能
*         True if successful; False if the camera does not support this function
*
*@see  MUCam_setAWBRect
*/
bool MUCam_setWhiteBalance(MUCam_Handle camera, int status);

/**
* @brief 设置白平衡的参考区域，只有MoticamProS8 Lite或MoticamProS8 Lite M有该功能
*
* set Auto Exposure，Only some cameras support the function(MoticamProS8 Lite,MoticamProS8 Lite M only currently)
*
* @param camera 设置自动曝光的参考区域,目前只有MoticamProS8 Lite或MoticamProS8 Lite M有该功能
*               Set the reference area for the white balance,MoticamProS8 Lite,MoticamProS8 Lite M only currently
* @param x 自动曝光区域的左边坐标，以像素为单位，从0开始。
*          The 0-based left coordinate(pixel).
* @param y 自动曝光区域的顶边坐标，以像素为单位，从0开始。
*          The 0-based top coordinate(pixel).
* @param w 自动曝光区域的宽度，以像素为单位。
*          The width (pixel) of reference area.
* @param h 自动曝光区域的高度，以像素为单位。
*          The height(pixel) of reference area.
*  @return 成功返回true; 失败返回false. 失败表示不支持该功能
*         True if successful; False if the camera does not support this function
*
*@see  MUCam_setWhiteBalance
*/
bool MUCam_setAWBRect(MUCam_Handle camera, int x, int y, int w, int h);



#ifdef __cplusplus
}
#endif
#ifndef _WIN32
#pragma GCC visibility pop
#endif
#endif // __MOTIC_UCAM_H__
