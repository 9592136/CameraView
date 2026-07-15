# CameraView - MUCam 工业相机预览

这是一个按 `MUCam API.pdf` 和 `MUCamSDK` 真实头文件实现的 Windows 工业相机预览程序。程序启动后会自动加载 Motic MUCam SDK DLL，查找第一台相机，打开相机并循环抓帧显示。

## 已实现

- 优先动态加载 `MUCam32Ex.dll`，兼容旧版 `MUCam32.dll`
- 已新增 `ICameraDriver` 和 `MUCamCameraDriver`，主窗口通过相机驱动接口枚举、打开、取帧和设置曝光
- 已新增 `CameraDeviceListFormatter`，统一相机设备下拉框的占位文本、设备显示文本和选择索引校验
- 已新增 `FrameBuffer`，采集线程通过统一缓冲发布最新帧，UI 读取稳定快照
- 已新增 `WindowLayout`、`WindowControlLayout` 和 `WindowControlDefinitions`，分别封装窗口区域计算、工具栏/右侧面板控件摆放和控件创建定义
- 右侧功能面板使用可收缩卡片归类 Camera、Image、Fluorescence、Processing、Measurement、Project；Camera 卡可调自动曝光、曝光、增益和白平衡，窗口变宽时侧栏会自适应加宽
- 已新增 `MeasurementActionApplier` 和 `MeasurementInteractionActions`，封装测量交互动作到标定/测量对象的应用规则，以及点击/完成多边形后的状态、列表和预览刷新结果
- 已新增 `MeasurementInteractionState`，封装标定、长度、角度、矩形面积和多边形面积工具的点位采集状态
- 已新增 `MeasurementHitTester`，封装测量端点/顶点拖拽编辑的命中检测
- 已新增 `MeasurementEditSession`，封装测量点拖拽编辑会话和点位更新规则
- 已新增 `MeasurementOverlayModelBuilder`，统一把测量集合、标定和待完成测量状态组装为叠加绘制模型
- 已新增 `MeasurementDisplayActions`，统一测量显示单位、结果列表文本、选中测量映射和叠加绘制模型入口
- 已新增 `MeasurementToolAvailability` 和 `MeasurementToolStartActions`，统一标定和测量工具启动前的预览帧可用性检查、输入校验、交互状态切换与提示文本
- 已新增 `ImageViewport`，封装图像绘制和图像/屏幕坐标换算；滚轮缩放、拖拽平移和视口平移会话状态由 `ViewportInteractionActions` 统一封装
- 已新增 `OverlayRenderer`，封装长度、角度、面积测量叠加和待完成测量点绘制
- 支持枚举已连接相机，在界面下拉框中选择要打开的设备
- 按 SDK 示例流程调用 `MUCamEx_findCamera`、`MUCamEx_openCamera`、`MUCamEx_getBinningList`、`MUCamEx_setBinningIndex`、`MUCamEx_getFrame`
- 默认设置 binning 0、自由触发；旧版 DLL 支持时额外设置 8-bit
- 支持 BGR、RGB、单色显示
- Bayer 图像会优先调用 `MUCamEx_getBayerFormat`、`MUCamEx_getBayer`、`MUCamEx_bayer2RGB` 转 RGB；如果 DLL 不导出相关函数，则以灰度预览
- 支持打开、停止、曝光时间设置
- 实时预览采用复用后台缓冲和当前显示帧缓存，降低图像刷新闪烁并减少大图重复拷贝/重复伪彩计算
- 状态栏右侧显示 SDK 加载诊断和真实预览遥测，包括 DLL、接口类型、可选能力、设备、相机类型、分辨率、FPS 和帧时间戳；预览遥测文本由 `CameraTelemetryFormatter` 统一生成，相机断开时会保留断开提示，便于现场验证
- 相机控制区的设备刷新、设备选择、曝光输入校验、曝光夹取和状态结果由 `CameraPanelActions` 统一封装，设备列表显示由 `CameraDeviceListFormatter` 生成，枚举、选择、打开和曝光设置提示由 `CameraControlStatusFormatter` 生成
- 支持鼠标滚轮按光标位置实时缩放图像，缩放输入由 `ViewportInteractionActions` 校验和转发，状态栏会显示当前缩放倍率
- 支持右键或鼠标中键拖拽平移放大后的图像，平移开始、拖动和结束状态由 `ViewportInteractionActions` 维护
- 支持工具栏 `Fit` 按钮一键恢复完整图像视图，便于放大观察后快速回到全图
- 已开始按显微观察与测量软件设计拆分基础模块：`ImageFrame`、`CameraDevice`、`CameraPanelActions`、`CameraDeviceListFormatter`、`CameraControlStatusFormatter`、`CameraTelemetryFormatter`、`FrameBuffer`、`DiagnosticReportActions`、`ExportActions`、`ProjectActions`、`MeasurementActionApplier`、`MeasurementDisplayActions`、`MeasurementInteractionActions`、`MeasurementInteractionState`、`MeasurementHitTester`、`MeasurementEditSession`、`MeasurementListActions`、`MeasurementListSelection`、`MeasurementOverlayModelBuilder`、`MeasurementToolAvailability`、`MeasurementToolStartActions`、`MeasurementCollection`、`MeasurementFormatter`、`MeasurementNameFormatter`、`MeasurementCsvExporter`、`DiagnosticReportBuilder`、`ProjectSessionMapper`、`ProjectSessionRestorer`、`FileDialog`、`TextInputParser`、`DyeProfileFormParser`、`DyeProfileFormPresenter`、`DyeLibraryActions`、`DyeLibrary`、`FluorescenceDisplayActions`、`FluorescenceChannelFactory`、`FluorescenceChannelFormPresenter`、`FluorescenceChannelListActions`、`FluorescenceChannelSettings`、`FluorescenceChannelUpdater`、`FluorescenceFormatter`、`ProcessingParameterRules`、`ProcessingBuildActions`、`ProcessingBuildInputActions`、`ProcessingQueueActions`、`ProcessingStartActions`、`ProcessingProgressActions`、`ProcessingWorkerActions`、`ProcessingJobExecutor`、`ProcessingProgressThrottle`、`ProcessingPanelActions`、`ProcessingRetryActions`、`ProcessingResultActions`、`ProcessingStatusFormatter`、`PreviewDisplayActions`、`PreviewFrameComposer`、`ProcessingJobState`、`ProcessingResultFrames`、`ProcessingRetryState`、`StitchTileListActions`、`StitchTilePlacementPlanner`、`EdfStackListActions`、`ViewportInteractionActions`、`ControlIds`、`WindowLayout`、`WindowControlLayout`、`WindowControlDefinitions`、`ImageViewport`、`OverlayRenderer`、`ViewTransform`
- 已加入图像坐标转换、两点标定、长度测量、角度测量、矩形面积测量和多边形面积测量核心模块
- 已加入右侧测量面板，可进行两点标定、长度/角度/矩形面积/多边形面积测量、叠加显示和结果列表查看
- 已新增 `MeasurementFormatter`，统一测量结果列表、状态栏和叠加绘制中的测量文本格式
- 已新增 `MeasurementNameFormatter`，统一长度、角度、矩形面积和多边形面积测量对象的默认命名规则
- 已新增 `MeasurementListActions` 和 `MeasurementListSelection`，统一测量结果列表删除、重命名、选中索引和删除后的下一项选择规则
- 已新增 `TextInputParser`，统一曝光时间、标定长度、染料参数、通道范围、拼接搜索范围和 EDF 半径等输入解析规则
- 已新增 `FileDialog`，统一 CSV、BMP、诊断报告和项目文件的保存/打开对话框
- 支持通过 `Open Image` 打开未压缩 8 位灰度/调色板 BMP、24 位 BMP 或 32 位 BGRA BMP，作为当前帧进行测量、伪彩、融合、拼接和 EDF 离线验证；打开离线图像时会取消并忽略旧后台处理结果，状态栏和遥测区会显示载入图像尺寸
- 支持测量结果重命名，并可在预览图上拖拽端点/顶点编辑测量位置
- 支持删除选中的测量结果，并通过 `ExportActions` 和 `MeasurementCsvExporter` 将长度、角度、面积测量表导出为 CSV
- 支持通过 `ProjectActions` 保存和打开项目文件，恢复标定比例、长度/角度/面积测量列表、荧光染料资料、荧光通道配置以及拼接/EDF 处理参数；会话与项目文档互转由 `ProjectSessionMapper` 维护，打开项目后的运行态应用和旧处理队列清理由 `ProjectSessionRestorer` 统一维护
- 支持通过 `ImageExporter` 读取/导出 BMP；导出时可带长度、角度、矩形面积、多边形面积测量叠加，导出成功状态会显示图像尺寸和当前显示模式，并保存 UTF-8 BOM 诊断报告
- 支持通过 `DiagnosticReportActions` 收集现场诊断状态，并由 `DiagnosticReportBuilder` 生成诊断报告文本，记录 SDK、设备列表和选中设备、当前帧来源、视口缩放、当前预览显示模式、帧信息、标定、测量、处理队列以及拼接/EDF 结果类型、当前显示来源和尺寸信息
- 支持实时图像伪彩显示，伪彩下拉显示、选择状态、预览模式标签和状态栏文本由 `PreviewDisplayActions` 统一封装，伪彩映射由 `PseudoColorMapper` 提供，并通过 `PreviewFrameComposer` 与融合/处理结果统一生成当前预览帧
- 支持默认荧光染料资料、自定义染料新增/更新/删除、当前帧添加为荧光通道、多通道融合预览、通道可见性/黑白范围调节和融合效果导出；染料输入解析由 `DyeProfileFormParser` 统一维护，染料资料表单显示文本由 `DyeProfileFormPresenter` 统一维护，染料资料保存/删除动作、状态文本和删除后选择项由 `DyeLibraryActions` 统一维护，染料库的同名更新、删除后选择项和默认空通道染料由 `DyeLibrary` 统一维护，染料下拉文本、当前染料选择、通道列表文本和通道选中索引由 `FluorescenceDisplayActions` 统一维护，按染料和当前帧创建默认通道由 `FluorescenceChannelFactory` 统一维护，通道设置表单显示文本由 `FluorescenceChannelFormPresenter` 统一维护，添加/清空通道后的融合预览和列表选中规则由 `FluorescenceChannelListActions` 统一维护，通道可见性、黑白范围和列表索引规则由 `FluorescenceChannelSettings` 统一维护，通道设置应用和错误状态由 `FluorescenceChannelUpdater` 统一维护，荧光通道默认名称、染料和通道列表文本由 `FluorescenceFormatter` 统一生成
- 支持当前帧加入拼接队列、调节拼接搜索范围、相邻图自动平移配准、多图全局位姿优化并在后台生成拼接预览，支持当前帧加入 EDF 队列、调节 EDF 清晰度半径，并在后台生成景深扩展预览和焦点图；拼接搜索百分比、EDF 半径、配准搜索半径和拼接优化半径由 `ProcessingParameterRules` 统一维护，当前帧加入拼接队列、无图像拒绝和入队状态文本由 `StitchTileListActions` 统一维护，拼接 tile 放置和配准失败回退由 `StitchTilePlacementPlanner` 统一维护，当前帧加入 EDF 堆栈、无图像拒绝和帧数状态文本由 `EdfStackListActions` 统一维护，拼接/EDF 入队编排、成功入队后的旧处理结果清理和预览刷新请求由 `ProcessingQueueActions` 统一维护，拼接/EDF 构建前输入校验和启动请求组装由 `ProcessingBuildActions` 统一维护，拼接搜索和 EDF 半径文本输入由 `ProcessingBuildInputActions` 统一解析并转成构建动作，后台作业启动前运行状态检查、旧 worker 回收回调、重试快照记录和启动作业编号由 `ProcessingStartActions` 统一维护，后台 worker 线程创建、进度回调接线和结果发布回调由 `ProcessingWorkerActions` 统一维护
- 已新增 `ProcessingJobState`，统一管理后台拼接/EDF 作业编号、取消令牌、运行状态和待发布结果
- 已新增 `ProcessingResultFrames`，统一管理拼接结果、EDF 合成图、EDF 焦点图、当前处理结果显示状态和显示来源标签，并可在 `EDF Image` 与 `Focus Map` 之间切换
- 已新增 `ProcessingRetryState`，统一管理拼接/EDF 后台任务的重试快照和有效性判断
- 支持通过状态栏查看拼接/EDF 后台处理进度，可用 `Retry` 重试上一后台作业，并可用 `Clear Processing` 清空队列、请求取消正在运行的后台作业；拼接/EDF 作业启动准备由 `ProcessingStartActions` 统一封装，后台 worker 线程创建由 `ProcessingWorkerActions` 统一封装，作业执行由 `ProcessingJobExecutor` 统一封装，后台完成结果的旧作业忽略、失败状态和成功发布由 `ProcessingResultActions` 统一维护，显示 EDF 合成图、焦点图和清空处理队列动作由 `ProcessingPanelActions` 统一维护，重试请求判定和重试提示由 `ProcessingRetryActions` 统一维护，后台处理状态文本由 `ProcessingStatusFormatter` 统一生成，取消检查和进度上报判定由 `ProcessingProgressActions` 统一维护，进度刷新节流由 `ProcessingProgressThrottle` 统一维护
- 已加入核心逻辑自动验证目标 `CameraViewDomainTests`
- 不依赖 OpenCV、Qt 或厂家 `.lib` 文件

## 显微观察与测量软件实现阶段

当前项目正在从相机预览程序演进为显微观察和测量软件。架构设计和 UML 建模资料已放在 `docs/`，用户已确认默认方案，代码已进入第一阶段基础模块拆分。

设计入口：

- `docs/design_index.md`：设计文档总索引和推荐阅读顺序
- `docs/design_confirmation.md`：已确认的默认方案
- `docs/architecture_uml.md`：架构设计和 UML 说明
- `docs/requirements_traceability.md`：需求、模块和 UML 追踪矩阵
- `docs/uml/README.md`：PlantUML 图源索引
- `docs/uml_gallery.md`：已渲染 PNG 图集索引
- `docs/implementation_progress.md`：设计确认后的代码实现进度
- `docs/camera_field_verification.md`：连接真实相机后的现场预览验证清单

UML 源码级检查：

```powershell
.\tools\render_uml.ps1 -Mode check
```

如已安装 PlantUML，可用同一脚本渲染 PNG 或 SVG，具体见 `docs/uml_rendering.md`。

当前 PNG 渲染结果位于 `docs\uml\rendered\`。

## 运行前准备

1. 安装 Visual Studio 2022，勾选“使用 C++ 的桌面开发”。
2. 打开 `CameraView.vcxproj`。
3. 选择与相机驱动一致的平台：32 位驱动选 `Win32`，64 位驱动选 `x64`。
4. 生成工程会自动把 `MUCamSDK\bin\x86` 或 `MUCamSDK\bin\x64` 下的 `MUCam32Ex.dll`、`MUCam32.dll` 复制到输出目录。
5. 连接相机，运行程序。

## 构建方式

Visual Studio:

```text
打开 CameraView.vcxproj -> 选择 Win32/Release -> 生成
```

CMake:

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## 注意事项

- 程序用动态加载方式调用 DLL，因此不需要链接 `MUCam32Ex.lib`。
- 当前代码已用 Visual Studio C++ x64 工具链和 CMake Release 编译通过；连接真实相机时按 `docs/camera_field_verification.md` 记录预览稳定性、FPS 和交互结果。
- 如果运行时提示未找到相机，请先运行厂家自带的 `MUCamSDK\bin\x86\MUCamExample.exe` 或 `MUCamSDK\bin\x64\MUCamExample.exe` 检查驱动和相机连接。
