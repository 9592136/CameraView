#pragma once

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

  MUCAM_TYPE_IMX224,//MoticamS1

  MUCAM_TYPE_IMX178,//MoticamS6

  MUCAM_TYPE_IMX123,//MoticamS3

  MUCAM_TYPE_IMX226,//MoticamS12

  MUCAM_TYPE_MCX3,

  MUCAM_TYPE_IMX250,//MoticamProS5
  MUCAM_TYPE_IMX264,//MoticamS5

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
* @see MUCamEx_openCamera, MUCamEx_releaseCamera
*/
MUCam_Handle MUCamEx_findCamera();

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
* @see MUCamEx_findCamera
*/
void MUCamEx_releaseCamera(MUCam_Handle camera);

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
MUCam_Type MUCamEx_getType(MUCam_Handle camera);

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
* @see MUCamEx_closeCamera
*/
bool MUCamEx_openCamera(MUCam_Handle camera);

/**
* @brief 关闭摄像头。
*
* 关闭已经打开的摄像头对象。关闭未被打开的摄像头没有作用。关闭后的摄像头可以被重新打开。摄像头对象句柄
* 必须被释放才能完全的释放所有占用的资源。
*
* To close the opened camera. The closed camera can be re-opened. The MUCamEx_releaseCamera function should
* be invoked to release all resources asscociated with a camera.
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @see MUCamEx_releaseCamera
*/
void MUCamEx_closeCamera(MUCam_Handle camera);

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
MUCam_Format MUCamEx_getFrameFormat(MUCam_Handle camera);

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
* @see MUCamEx_getFrameFormat
*/
bool MUCamEx_getFrame(MUCam_Handle camera, unsigned char *buf, unsigned long *ts);



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
int MUCamEx_getBinningCount(MUCam_Handle camera);

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
* @see MUCamEx_getBinningCount
*/
bool MUCamEx_getBinningList(MUCam_Handle camera, int *w, int *h);

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
* @see MUCamEx_getBinningCount MUCam_Binning_Type
*/
bool MUCamEx_getBinningType(MUCam_Handle camera, MUCam_Binning_Type *tl);

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
* @see MUCamEx_getBinningCount
*/
bool MUCamEx_setBinningIndex(MUCam_Handle camera, int idx);


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
int MUCamEx_getGainCount(MUCam_Handle camera);

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
* @see MUCamEx_getGainCount
*/
bool MUCamEx_getGainList(MUCam_Handle camera, float *g);

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
* @see MUCamEx_getGainCount
*/
bool MUCamEx_setRGBGainIndex(MUCam_Handle camera, int r, int g, int b);

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
*       MUCamEx_setRGBGainValue(camera, redGain, greenGain, blueGain, &currentRedGainIndex, &currentGreenGainIndex, &currentBlueGainIndex);
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
* @see MUCamEx_getGainCount, MUCamEx_setRGBGainIndex
*/
bool MUCamEx_setRGBGainValue(MUCam_Handle camera, float r, float g, float b, int *ri, int *gi, int *bi);


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
* @see MUCamEx_setBinningIndex, MUCamEx_setROI
*/
bool MUCamEx_getExposureRange(MUCam_Handle camera, float *min, float *max);

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
* @see MUCamEx_getExposureRange, MUCamEx_setBinningIndex, MUCamEx_setROI
*/
bool MUCamEx_setExposure(MUCam_Handle camera, float t);

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
bool MUCamEx_getOffsetRange(MUCam_Handle camera, int *min, int *max);

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
* @see MUCamEx_getOffsetRange
*/
bool MUCamEx_setRGBOffset(MUCam_Handle camera, int r, int g, int b);

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
bool MUCamEx_isConnected(MUCam_Handle camera);


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
int MUCamEx_getFrequencyCount(MUCam_Handle camera);

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
* @see MUCamEx_getFrequencyCount
*/
bool MUCamEx_setFrequencyIndex(MUCam_Handle camera, int level);

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
int MUCamEx_getFrequencyIndex(MUCam_Handle camera);

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
* @see MUCamEx_Trigger_Type
*/
bool MUCamEx_setTriggerType(MUCam_Handle camera, MUCam_Trigger_Type type);

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
bool MUCamEx_setFlip(MUCam_Handle camera, bool b);

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
bool MUCamEx_setMirror(MUCam_Handle camera, bool b);

/**
* @brief 读取一帧图像。
*        get one frame of image
*
* @param camera 摄像头句柄。
*               The camera handle.
* @param buf 缓冲区指针，必须足够容纳将取得的图像数据。
*            The frame buffer, must be big enough for containing the image data.
* @param w  读取图像的宽度，该值必须与当前设定的Binning值对应的分辨率的宽度。
*           width of image, should be the same value as the width of current resolution.
* @param h  读取图像的高度，该值必须与当前设定的Binning值对应的分辨率的高度。
*           height of image, should be the same value as the heigh of current resolution.

* @return 是否成功读取图像。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_getFrame(MUCam_Handle camera, unsigned char *buf, int w, int h);

/**
* @brief 指定摄像头曝光到目标亮度。
*        To set the camera auto expousre to the target brightness.
*
* @param camera 摄像头句柄。
*               The camera handle.
* @param target 曝光的目标亮度,取值10-250
*            The target brightness. range 10-250.
* @return 执行完曝光操作后的曝光值。
*         Returns current exposure after auto exposure finished
*
*/
float MUCamEx_AutoExposureOnce(MUCam_Handle camera, long target);

/**
* @brief 拍照JPG图到指定的文件。
*        Capture jpg image file
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @param filename  用于存储拍照图像的文件名
*                  the file name for saving the capted image。

* @return 是否成功拍照。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_CaptureJpg(MUCam_Handle camera, char* filename);

/**
* @brief 拍照BMP图到指定的文件。
*        Capture bmp image file
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @param filename  用于存储拍照图像的文件名
*                  the file name for saving the capted image。

* @return 是否成功拍照。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_CaptureBmp(MUCam_Handle camera, char* filename);

/**
* @brief 拍照PNG图到指定的文件。
*        Capture png image file
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @param filename  用于存储拍照图像的文件名
*                  the file name for saving the capted image。

* @return 是否成功拍照。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_CapturePng(MUCam_Handle camera, char* filename);

/**
* @brief 计算并应用白平衡。该方法最后在纯背景下做,调用该方法后可以通过MUCamEx_GetWhiteBalanceParam得到白平衡参数，下次打开只需要调用MUCamEx_SetWhiteBalanceParam即可启动白平衡功能
*        calculate and apply the white balance，please call this function in the pure backgroud environment.
*        you can get the parameters of white balance, and using the white balance by calling MUCamEx_SetWhiteBalanceParam.
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @return 是否成功。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_CalcWhiteBalance(MUCam_Handle camera);

/**
* @brief 获取当前白平衡参数, 请在调用MUCamEx_CalcWhiteBalance调用并记忆参数，下次打开软件直接调用MUCamEx_SetWhiteBalanceParam来开启白平衡
*        get the parameters for current white balance，please remember the paramters after you call MUCamEx_CalcWhiteBalance, next time you can just invoke MUCamEx_SetWhiteBalanceParam to open the white balance
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @return 是否成功。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_GetWhiteBalanceParam(MUCam_Handle camera, float* r, float*g, float*b);

/**
* @brief 设定当前白平衡参数,如果己有白平衡参数可以直接设置，而不要再调用MUCamEx_CalcWhiteBalance
*        set the parameters for current white balance， usally we are calling this function to open white balance instad of calling MUCamEx_CalcWhiteBalance if we have parameters.
*
* @param camera 摄像头句柄。
*               The camera handle.
*
* @return 是否成功。
*         Returns true if successful; false if failed.
*
*/
bool MUCamEx_SetWhiteBalanceParam(MUCam_Handle camera, float r, float g, float b);
