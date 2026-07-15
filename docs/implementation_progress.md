# 显微观察与测量软件实现进度

本文档记录用户确认设计后，代码实现的实际推进情况。

## 2026-06-26 第一阶段基础拆分

已完成：

- 新增 `src/domain/ImageFrame.h`，把预览帧数据从主窗口中抽为可复用图像帧对象。
- 新增 `src/camera/CameraDevice.h`，为设备枚举、选择和后续多设备管理提供统一描述。
- 新增 `src/imaging/ViewTransform.h` 和 `src/imaging/ViewTransform.cpp`，集中处理图像显示区域、缩放比例和视口中心。
- 主窗口已接入 `ImageFrame`、`CameraDevice` 和 `ViewTransform`。
- 保留现有双缓冲绘制，继续用于降低图像刷新闪烁。
- 保留鼠标滚轮按光标位置实时缩放图像的行为。
- `CMakeLists.txt` 已加入新增模块。
- `CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已同步新增模块，便于后续直接用 Visual Studio 打开工程。
- 已用 Visual Studio C++ x64 工具链和 CMake Release 构建通过。

验证结果：

```text
[100%] Built target CameraView
```

本次构建输出：

```text
C:\TempVS\CameraView_cmake_x64\CameraView.exe
```

## 2026-06-26 视图平移、坐标转换和基础测量核心

已完成：

- `ViewTransform` 增加屏幕坐标到图像坐标的转换。
- `ViewTransform` 增加图像坐标到屏幕坐标的转换。
- `ViewTransform` 增加拖拽平移能力。
- 主窗口接入右键拖拽和鼠标中键拖拽平移预览图像。
- 新增 `src/domain/Geometry.h`，定义图像点和像素距离计算。
- 新增 `src/domain/CalibrationProfile.h/.cpp`，支持两点标定和 `microns-per-pixel` 换算。
- 新增 `src/domain/Measurement.h/.cpp`，支持长度测量核心计算。
- 无有效标定时，长度测量会自动回退为像素单位，避免把未标定结果误显示为真实单位。
- 新增 `tests/domain_smoke.cpp`，验证标定、长度换算、缩放锚点和拖拽平移。
- `CMakeLists.txt` 增加 `CameraViewDomainTests` 测试目标。
- `CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已同步新增领域模块。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-26 标定和长度测量交互

已完成：

- 主窗口右侧新增测量面板。
- 测量面板支持输入标定长度，单位支持 `um` 和 `mm`。
- 支持点击 `Calibrate` 后在图像上点两点完成标定。
- 支持点击 `Length` 后在图像上点两点创建长度测量。
- 测量线和端点会叠加绘制在预览图像上。
- 右侧列表会显示长度测量结果。
- `Clear` 可清空当前测量结果。
- 标定变化后，已有长度测量会按新的标定比例刷新显示。
- 自动测试增加毫米标定换算和零距离标定失败校验。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-26 测量结果管理和 CSV 导出

已完成：

- 右侧测量面板新增 `Delete Selected`，可删除当前选中的单条长度测量。
- 右侧测量面板新增 `Export CSV`，可通过保存对话框导出当前测量结果。
- CSV 导出包含测量名称、类型、结果值、单位、像素长度、起点坐标、终点坐标和当前标定比例。
- CSV 使用 UTF-8 BOM，便于 Excel 直接打开。
- CMake 和 Visual Studio 工程已链接 `comdlg32`，用于 Windows 保存文件对话框。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-26 项目保存和重新打开

已完成：

- 新增 `src/storage/ProjectDocument.h`，表达可保存的项目状态。
- 新增 `src/storage/ProjectRepository.h/.cpp`，支持保存和读取轻量 JSON 项目文件。
- 项目文件当前保存标定比例和长度测量列表。
- 右侧测量面板新增 `Open Project` 和 `Save Project`。
- 打开项目后会恢复标定、测量列表和图像叠加。
- 自动测试增加项目保存/读取往返验证，覆盖测量名称转义和坐标恢复。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-26 图片导出

已完成：

- 新增 `src/storage/ImageExporter.h/.cpp`，支持 BMP 图片导出。
- 右侧测量面板新增 `Export Image`。
- 导出的 BMP 使用当前相机帧，并叠加长度测量线和端点。
- 图片导出不会修改原始 `ImageFrame`。
- 自动测试增加 BMP 文件头和测量叠加像素校验。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-26 图像伪彩

已完成：

- 新增 `src/imaging/PseudoColorMapper.h/.cpp`，提供独立伪彩映射。
- 支持 `Original`、`Grayscale`、`Hot`、`Green`、`Cyan`、`Magenta`。
- 右侧测量面板新增 `Pseudo color` 下拉框。
- 预览图像会按当前伪彩设置实时刷新。
- `Export Image` 会导出当前伪彩效果和测量叠加。
- 伪彩只生成显示/导出用派生帧，不修改原始 `ImageFrame`。
- 自动测试增加伪彩映射校验。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-26 荧光通道融合和染料管理

已完成：

- 新增 `src/imaging/Fluorescence.h`，定义荧光染料、RGB 显示颜色和荧光通道数据。
- 新增 `src/imaging/DyeLibrary.h/.cpp`，提供 DAPI、FITC、TRITC、Cy5 默认染料资料，包含激发/发射波长和默认颜色。
- 新增 `src/imaging/ChannelFusionEngine.h/.cpp`，支持多通道加法融合、通道可见性和黑白强度范围映射。
- 右侧面板新增 `Dye` 下拉框、`Add Channel`、`Fusion Preview`、`Clear Channels` 和通道列表。
- 可以把当前相机画面按所选染料保存为荧光通道，并实时切换融合预览。
- 融合预览接入滚轮缩放、平移、测量叠加和图片导出路径；导出图像会跟随当前显示效果。
- 融合和伪彩都只生成显示/导出用派生帧，不修改原始 `ImageFrame`。
- 自动测试新增默认染料库、双通道融合、隐藏通道忽略和源图像不被修改的校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-26 图像拼接和 EDF 首版

已完成：

- 新增 `src/imaging/ImageStitcher.h/.cpp`，支持带偏移的多 tile 平均融合拼接。
- 新增 `src/imaging/EdfProcessor.h/.cpp`，支持根据局部清晰度评分从 Z 序列中选择最清晰像素，生成 EDF 景深扩展图。
- 右侧面板新增 `Add Tile`、`Stitch`、`Add EDF`、`EDF` 和 `Clear Processing`。
- 可以把当前相机画面加入拼接队列或 EDF 队列，并生成处理结果预览。
- 处理结果预览接入缩放、平移、测量叠加和图片导出路径。
- 自动测试新增拼接画布尺寸、重叠区域平均融合和 EDF 清晰像素选择校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-27 角度和矩形面积测量

已完成：

- `src/domain/Measurement.h/.cpp` 新增 `AngleMeasurement` 和 `RectangleAreaMeasurement`。
- 角度测量支持三点创建，结果以 `deg` 显示。
- 矩形面积测量支持两个对角点创建，未标定时显示 `px^2`，已标定时显示 `um^2` 或 `mm^2`。
- 右侧测量面板新增 `Angle` 和 `Area` 按钮。
- 测量结果列表、单条删除、清空、CSV 导出和 BMP 图片导出已覆盖长度、角度、矩形面积三类测量。
- 项目保存文件升级到 version 2，可保存/恢复长度、角度、矩形面积；旧的长度项目仍可读取。
- 自动测试新增角度计算、矩形面积换算和三类测量项目往返保存校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-27 测量重命名和点位编辑

已完成：

- `LengthMeasurement`、`AngleMeasurement`、`RectangleAreaMeasurement` 新增名称和点位修改接口。
- 右侧测量结果列表上方新增名称输入框和 `Rename` 按钮。
- 选中测量结果后，名称会自动填入输入框；修改后点击 `Rename` 可重命名。
- 预览区支持左键拖拽测量端点/顶点，实时更新长度、角度和矩形面积结果。
- 点击命中的测量点时会自动选中右侧列表中的对应测量。
- 自动测试新增测量对象重命名和点位修改校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-27 荧光通道可见性和强度范围调节

已完成：

- 右侧荧光通道列表显示 `[on]/[off]` 状态、图像尺寸和黑白范围。
- 新增 `Visible`、`Black`、`White` 和 `Apply Channel` 控件。
- 选中通道后会自动加载可见性和黑白范围。
- 修改可见性或黑白范围后可即时刷新融合预览。
- 黑白范围输入限制为 `0-255`，并校验白场必须大于黑场。
- 自动测试新增黑白范围映射强度校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-27 多边形面积测量

已完成：

- `src/domain/Measurement.h/.cpp` 新增 `PolygonAreaMeasurement`。
- 多边形面积使用顶点鞋带公式计算像素面积，已标定时换算为 `um^2` 或 `mm^2`。
- 右侧测量面板新增 `Poly Area` 和 `Finish Poly`。
- 点击 `Poly Area` 后可连续点选顶点，点击 `Finish Poly` 后生成多边形面积测量。
- 多边形面积接入测量列表、重命名、删除、拖拽顶点编辑、预览叠加、CSV 导出、BMP 导出和项目保存/打开。
- 项目文件版本升级到 version 3，可保存/恢复 `polygon_area` 顶点列表。
- 自动测试新增多边形面积换算、顶点编辑和项目往返保存校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-27 拼接自动平移配准

已完成：

- 新增 `src/imaging/ImageRegistration.h/.cpp`。
- 支持在给定搜索范围内，用重叠区域灰度差异估计相邻图像的平移偏移。
- `Add Tile` 从第二张图开始会优先自动估计相对上一张 tile 的偏移。
- 自动配准失败时仍回退到原来的横向追加策略。
- 状态栏会提示自动估计出的 tile 偏移。
- 自动测试新增已知偏移图像对的配准校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-27 荧光通道配置项目保存

已完成：

- `ProjectDocument` 新增荧光通道配置列表，保存通道名、显示颜色、可见性和黑白范围。
- 项目文件版本升级到 version 4，新增 `fluorescence_channels` JSON 节点。
- `Save Project` 会把当前荧光通道配置写入项目文件。
- `Open Project` 会恢复荧光通道列表和参数，并关闭融合预览以避免没有图像帧时显示空融合图。
- 通道列表对恢复出的无图像帧通道显示 `no frame`。
- 自动测试新增荧光通道配置项目往返保存校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 自定义荧光染料管理

已完成：

- 右侧荧光染料区域新增名称、Ex、Em、RGB 输入框，以及 `Save Dye`、`Delete Dye`。
- 可新增项目内自定义染料，也可用同名保存更新已有染料参数。
- 可删除当前下拉框选中的染料。
- `Add Channel` 会使用当前染料库中的颜色和染料名创建荧光通道。
- `ProjectDocument` 新增染料资料列表，项目文件升级到 version 5，保存 `dye_profiles`。
- `Open Project` 会恢复项目内染料库；旧项目没有染料资料时回退到默认 DAPI/FITC/TRITC/Cy5。
- 自动测试新增自定义染料资料项目往返保存校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 EDF 焦点图

已完成：

- 新增 `EdfResult`，EDF 处理结果包含景深扩展合成图和焦点图。
- `EdfProcessor::ComposeFocusStack` 会记录每个像素来自哪一层焦平面，并生成灰度焦点图。
- 保留 `FuseFocusStack` 兼容入口，旧调用仍返回合成图。
- 右侧处理区域新增 `Focus Map`，可在生成 EDF 后切换查看焦点图。
- 自动测试新增 EDF 焦点图像素来源编码校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 拼接和 EDF 后台处理

已完成：

- 主窗口新增后台图像处理 worker，用于执行拼接和 EDF 构建。
- `Stitch` 按钮会复制当前 tile 队列并在后台生成拼接图，完成后通过窗口消息发布到预览区。
- `EDF` 按钮会复制当前 EDF 栈并在后台生成合成图和焦点图，完成后通过窗口消息发布到预览区。
- 处理运行中再次启动处理会提示已有作业正在运行。
- `Clear Processing` 会清空队列并让正在运行的旧作业结果失效，避免旧结果回写到界面。
- 窗口关闭时会等待后台处理线程结束，避免线程访问已释放的窗口对象。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 后台处理取消

已完成：

- `ImageStitcher::StitchAverage` 新增可选取消标记，按图块和行检查取消请求。
- `EdfProcessor::ComposeFocusStack` 和 `FuseFocusStack` 新增可选取消标记，按行检查取消请求。
- 后台拼接和 EDF 作业会把取消标记传入算法层。
- `Clear Processing` 会请求取消当前后台作业，同时让旧作业结果失效。
- 关闭窗口时会请求取消后台作业并等待线程结束。
- 自动测试新增拼接和 EDF 取消校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 后台处理进度反馈

已完成：

- `ImageStitcher::StitchAverage` 新增可选进度回调，按图块累加和输出生成阶段报告百分比。
- `EdfProcessor::ComposeFocusStack` 和 `FuseFocusStack` 新增可选进度回调，按行报告百分比。
- 后台拼接和 EDF 作业会把进度回调接到状态栏，并按 10% 粒度更新，避免过度刷新。
- 取消作业后不再继续报告进度。
- 自动测试新增拼接和 EDF 进度从 0 到 100 且不倒退的校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 EDF 参数调节

已完成：

- 新增 `EdfOptions`，当前包含 `focus_radius` 清晰度评分半径。
- `EdfProcessor::ComposeFocusStack` 和 `FuseFocusStack` 支持按半径累计邻域差异，默认半径仍为 1。
- 右侧处理区域新增 `EDF radius` 输入框，允许 1 到 16。
- 后台 EDF 作业会读取当前半径，并用该参数生成合成图和焦点图。
- 自动测试新增不同半径导致焦平面选择变化的校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 EDF 参数项目保存

已完成：

- `ProjectDocument` 新增 `ProjectProcessingSettings`，当前保存 `edf_focus_radius`。
- 项目文件升级到 version 6，新增 `processing_settings.edf_focus_radius`。
- `Save Project` 会保存当前 EDF 清晰度半径。
- `Open Project` 会恢复 EDF 半径并更新右侧 `EDF radius` 输入框。
- 旧项目没有处理参数时继续使用默认半径 1。
- 自动测试新增 EDF 半径项目往返保存校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 拼接搜索参数和项目保存

已完成：

- 右侧处理区域新增 `Stitch search %` 输入框，默认 50，允许 5 到 100。
- `Add Tile` 从第二张图开始会使用该百分比计算自动平移配准的搜索范围。
- `ProjectProcessingSettings` 新增 `stitch_search_percent`。
- 项目文件升级到 version 7，新增 `processing_settings.stitch_search_percent`。
- `Save Project` 会保存当前拼接搜索百分比。
- `Open Project` 会恢复拼接搜索百分比并更新右侧输入框。
- 自动测试新增拼接搜索百分比项目往返保存校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 后台处理重试

已完成：

- 右侧处理区域新增 `Retry` 按钮。
- 拼接和 EDF 后台作业提交时会保存上一份输入快照。
- `Retry` 会使用上一份拼接 tile 或 EDF 栈与参数重新提交后台作业。
- `Clear Processing` 会同步清理队列、处理结果和重试快照，避免误用已清空数据。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 拼接多图全局优化

已完成：

- `ImageRegistration` 新增围绕已有位置提示进行局部修正的平移配准接口。
- `ImageStitcher` 新增多图 tile 偏移优化，能够根据多张重叠图之间的关系统一修正 tile 位姿。
- 后台 `Stitch` 流程会先执行全局位姿优化，再使用优化后的 tile 生成拼接图。
- `Retry` 会复用上一拼接作业的 tile 快照和搜索参数。
- 自动测试新增四 tile 闭环漂移修正校验。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 真实相机现场验证辅助

已完成：

- 状态栏拆分为左侧操作提示和右侧预览遥测。
- 未打开预览时右侧可显示已加载的 SDK DLL、Ex/Legacy 接口和可选能力，便于排查现场 SDK 环境。
- 真实相机预览时右侧显示设备编号、相机类型、分辨率、FPS 和帧时间戳。
- 预览停止或清空最新帧时会清理遥测，避免残留旧设备信息。
- 右侧面板新增 `Save Diagnostic`，可导出 UTF-8 现场诊断报告，包含 SDK、设备、最新帧、标定、测量和图像处理队列状态。
- 新增 `docs/camera_field_verification.md`，记录真实相机现场验证步骤、通过标准、记录模板和常见问题。
- README、需求追踪矩阵和验收清单已增加现场验证入口和遥测说明。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 相机驱动抽象落地

已完成：

- 新增 `src/camera/ICameraDriver.h`，定义相机驱动统一接口、SDK 诊断信息和打开后的相机信息。
- 新增 `src/camera/MUCamCameraDriver.h/.cpp`，内部封装 `MUCamApi`，负责 MUCam 设备枚举、打开、曝光控制、抓帧、Bayer/RGB 转换和 SDK 诊断。
- `CameraPreviewApp` 不再直接持有 MUCam SDK 句柄，主窗口通过 `MUCamCameraDriver` 枚举、打开、抓帧和导出诊断。
- `CMakeLists.txt` 已加入新的相机驱动源文件。
- README、需求追踪和架构说明已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 最新帧缓冲抽象

已完成：

- 新增 `src/camera/FrameBuffer.h/.cpp`，封装最新帧发布、快照读取、清空和有效性检查。
- `CameraPreviewApp` 不再直接维护 `latest_frame_` 和 `frame_mutex_`，采集线程通过 `FrameBuffer::Publish` 发布帧，UI 和工具通过 `FrameBuffer::Snapshot` 读取稳定副本。
- `CMakeLists.txt` 已加入 `FrameBuffer` 源文件。
- 自动测试新增 `FrameBuffer` 初始状态、发布快照隔离和清空校验。
- README、需求追踪和架构说明已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 图像视口抽象

已完成：

- 新增 `src/imaging/ImageViewport.h/.cpp`，封装图像绘制、鼠标滚轮缩放、平移以及屏幕坐标/图像坐标换算。
- `CameraPreviewApp` 不再直接维护 GDI 图像拉伸绘制细节，预览绘制统一通过 `ImageViewport::DrawFrame` 完成。
- 测量叠加、测量点编辑和鼠标交互统一通过 `ImageViewport` 进行坐标换算，内部继续复用 `ViewTransform` 保持原有缩放和平移行为。
- `CMakeLists.txt` 已加入 `ImageViewport` 源文件。
- 自动测试新增 `ImageViewport` 屏幕到图像坐标转换和滚轮缩放锚点稳定校验。
- README、需求追踪和架构说明已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 测量叠加渲染抽象

已完成：

- 新增 `src/imaging/OverlayRenderer.h/.cpp`，封装长度、角度、矩形面积、多边形面积测量叠加绘制。
- `OverlayRenderer` 统一绘制测量端点、待完成测量点、待完成角度线段、多边形采集线段和测量标签。
- `CameraPreviewApp` 不再直接维护测量叠加的 GDI 绘制细节，只负责组装 `MeasurementOverlayModel`。
- 测量结果列表和预览叠加共用 `OverlayRenderer::FormatLine`，避免同一测量结果出现两套格式。
- `CMakeLists.txt` 已加入 `OverlayRenderer` 源文件。
- 自动测试新增测量叠加标签格式校验。
- README、需求追踪和架构说明已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 测量集合抽象

已完成：

- 新增 `src/domain/MeasurementCollection.h/.cpp`，封装长度、角度、矩形面积、多边形面积测量集合。
- `MeasurementCollection` 统一负责扁平索引、计数、重命名、删除和测量点位更新。
- `CameraPreviewApp` 不再直接维护四组测量向量，创建、删除、重命名、项目保存/打开、CSV 导出和诊断统计均改为通过集合对象访问。
- `CMakeLists.txt` 已加入 `MeasurementCollection` 源文件。
- 自动测试新增集合计数、扁平索引、重命名、点位编辑和删除校验。
- README、需求追踪和架构说明已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-06-29 测量 CSV 导出抽象

已完成：

- 新增 `src/storage/MeasurementCsvExporter.h/.cpp`，封装测量结果 CSV 写入。
- `MeasurementCsvExporter` 负责 UTF-8 BOM、列头、数值格式、逗号/引号转义和多边形完整点列表。
- `CameraPreviewApp::ExportMeasurementsCsv` 不再拼接 CSV 内容，只负责保存对话框和调用导出器。
- `CMakeLists.txt` 已加入 `MeasurementCsvExporter` 源文件。
- 自动测试新增 CSV BOM、转义和多边形点列表校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新，`docs/uml/rendered/` PNG 已重新渲染。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-29 预览帧合成抽象

已完成：

- 新增 `src/imaging/PreviewFrameComposer.h/.cpp`，统一生成当前预览帧。
- 预览帧优先级固定为：处理结果/焦点图优先，其次荧光融合预览，其次伪彩，最后原始帧。
- `CameraPreviewApp::BuildPreviewFrame` 不再直接调用 `ChannelFusionEngine` 和 `PseudoColorMapper`，只负责组装合成参数。
- `CMakeLists.txt` 已加入 `PreviewFrameComposer` 源文件。
- 自动测试新增处理结果、荧光融合和伪彩优先级校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新，`docs/uml/rendered/` PNG 已重新渲染。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-29 后台作业状态抽象

已完成：

- 新增 `src/imaging/ProcessingJobState.h/.cpp`，封装后台处理作业编号、运行状态、取消令牌和待发布结果。
- `CameraPreviewApp` 不再直接维护 `processing_running_`、`processing_cancel_`、`pending_processing_result_` 和活动作业编号。
- 拼接和 EDF 后台 worker 通过 `ProcessingJobState` 获取作业编号和取消令牌，并通过统一入口发布结果。
- `CMakeLists.txt` 已加入 `ProcessingJobState` 源文件。
- 自动测试新增作业开始、取消、过期结果忽略和当前结果发布校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-29 诊断报告生成抽象

已完成：

- 新增 `src/storage/DiagnosticReportBuilder.h/.cpp`，封装现场诊断报告文本生成和 SDK 遥测摘要格式化。
- `CameraPreviewApp::BuildDiagnosticsReport` 不再直接拼接大段诊断文本，只负责采集当前状态并交给构建器。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `DiagnosticReportBuilder` 源文件，并同步补齐此前已拆出的模块文件。
- 自动测试新增报告时间、SDK 摘要、相机帧、测量统计和图像处理统计字段校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-29 测量文本格式化抽象

已完成：

- 新增 `src/domain/MeasurementFormatter.h/.cpp`，统一长度、角度、矩形面积、多边形面积测量文本格式。
- `OverlayRenderer` 的叠加文字改为调用 `MeasurementFormatter`，避免绘制层单独维护格式化逻辑。
- `CameraPreviewApp` 不再维护四个 `BuildMeasurementLine` 包装函数，测量结果列表通过 `MeasurementFormatter::FormatCollection` 生成。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementFormatter` 源文件。
- 自动测试新增测量格式化器与叠加绘制文本一致性、测量集合列表顺序和格式校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 项目会话映射抽象

已完成：

- 新增 `src/storage/ProjectSessionMapper.h/.cpp`，封装当前会话状态与 `ProjectDocument` 的互相转换。
- `CameraPreviewApp::SaveProject` 不再直接拼装项目文档，改为调用 `ProjectSessionMapper::ToDocument`。
- `CameraPreviewApp::OpenProject` 不再直接解析项目文档字段，改为调用 `ProjectSessionMapper::FromDocument` 后刷新 UI。
- 打开项目时继续恢复标定、测量集合、染料资料、荧光通道显示配置、EDF 半径和拼接搜索范围；荧光通道只恢复配置，不恢复图像帧。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProjectSessionMapper` 源文件。
- 自动测试新增会话到项目文档转换、项目文档归一化恢复、默认染料回退、EDF/拼接参数范围夹取和通道配置恢复校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 文本输入解析抽象

已完成：

- 新增 `src/platform/TextInputParser.h/.cpp`，封装文本修剪、正数/非负数解析、RGB 字节范围解析和整数范围解析。
- `CameraPreviewApp` 不再直接在通用输入读取函数中调用 `wcstod`、`wcstol` 或维护本地 `Trim` 函数。
- 标定长度、染料波长、RGB 值、通道黑白范围、拼接搜索范围和 EDF 半径继续通过原有 UI 入口读取，但范围校验改为调用统一解析器。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `TextInputParser` 源文件。
- 自动测试新增空白修剪、正数/非负数、无穷值拒绝、RGB 字节范围、整数范围和数字前缀解析校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 荧光显示文本格式化抽象

已完成：

- 新增 `src/imaging/FluorescenceFormatter.h/.cpp`，封装染料下拉标签和荧光通道列表文本生成。
- `CameraPreviewApp::InitializeDyeCombo` 不再直接拼接染料名称、激发/发射波长和 `nm` 标签。
- `CameraPreviewApp::RefreshChannelList` 不再直接拼接通道可见性、帧尺寸和黑白范围文本。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceFormatter` 源文件。
- 自动测试新增染料标签、带帧通道、隐藏通道和无帧通道格式校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 相机预览遥测格式化抽象

已完成：

- 新增 `src/camera/CameraTelemetryFormatter.h/.cpp`，封装相机预览启动状态、等待 FPS 遥测和带 FPS/时间戳遥测文本。
- `CameraPreviewApp::CaptureThread` 不再直接拼接设备编号、相机类型、分辨率、FPS 和帧时间戳文本。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `CameraTelemetryFormatter` 源文件。
- 自动测试新增预览启动文本、未计算 FPS 遥测文本和带 FPS/时间戳遥测文本校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 后台处理状态文本格式化抽象

已完成：

- 新增 `src/imaging/ProcessingStatusFormatter.h/.cpp`，封装拼接/EDF 后台作业的清空、重试、启动、进度、取消、失败和完成状态文本。
- `CameraPreviewApp` 不再直接拼接拼接/EDF 进度百分比、完成图像尺寸和拼接关系数量文本。
- 拼接和 EDF worker 继续通过 `ProcessingJobState` 发布结果，但结果状态文本改为由 `ProcessingStatusFormatter` 统一生成。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingStatusFormatter` 源文件。
- 自动测试新增清空、重试、启动、进度、取消、失败、拼接完成和 EDF 完成文本校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 相机控制状态文本格式化抽象

已完成：

- 新增 `src/camera/CameraControlStatusFormatter.h/.cpp`，封装相机枚举、选择、打开、曝光设置和曝光错误等状态提示。
- `CameraPreviewApp` 不再直接拼接相机数量、设备编号、曝光设置结果和相机打开失败等相机控制状态文本。
- `TextInputParser` 新增正数 float 解析，曝光输入复用统一解析器，并拒绝超出 float 范围的数值。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `CameraControlStatusFormatter` 源文件。
- 自动测试新增相机控制状态文本、曝光小数解析和过大曝光值拒绝校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 窗口布局计算抽象

已完成：

- 新增 `src/ui/WindowLayout.h/.cpp`，封装工具栏高度、状态栏高度、预览区、右侧面板和状态栏区域计算。
- `CameraPreviewApp` 不再在 `main.cpp` 顶部维护预览区、右侧面板和状态栏的布局细节，只保留从窗口句柄读取客户区的薄包装。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `WindowLayout` 源文件。
- 自动测试新增宽窗口侧栏保留、窄窗口侧栏折叠、预览区和状态栏矩形计算校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 测量工具交互状态抽象

已完成：

- 新增 `src/ui/MeasurementInteractionState.h/.cpp`，封装标定、长度、角度、矩形面积和多边形面积工具的点位采集状态。
- `CameraPreviewApp` 不再直接维护 `InteractionMode`、待完成点、待完成角度点和多边形采集点集，而是根据 `MeasurementInteractionState` 返回的动作创建标定或测量对象。
- 待完成测量叠加由 `MeasurementInteractionState::PendingOverlay()` 提供，`OverlayRenderer` 继续负责实际绘制。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementInteractionState` 源文件。
- 自动测试新增长度两点采集、角度三点采集、多边形点数不足拒绝、待完成叠加和多边形完成校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 测量点命中检测抽象

已完成：

- 新增 `src/ui/MeasurementHitTester.h/.cpp`，封装长度端点、角度端点/顶点、矩形角点和多边形顶点的命中检测。
- `CameraPreviewApp::BeginMeasurementEdit` 不再直接遍历所有测量对象和计算屏幕距离，而是调用 `MeasurementHitTester::FindEditableHandle()`。
- 命中检测继续复用 `ImageViewport::ImageToScreen()`，保证缩放和平移后拖拽编辑命中位置与预览显示一致。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementHitTester` 源文件。
- 自动测试新增长度端点命中、多边形顶点命中和命中半径外忽略校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 测量默认命名抽象

已完成：

- 新增 `src/domain/MeasurementNameFormatter.h/.cpp`，封装长度、角度、矩形面积和多边形面积测量对象默认名称。
- `CameraPreviewApp` 创建测量对象时不再直接拼接 `Length/Angle/Area/Polygon + 序号`，而是调用 `MeasurementNameFormatter::NextDefaultName()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementNameFormatter` 源文件。
- 自动测试新增默认测量名称格式和按集合数量生成下一编号校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 荧光通道默认命名格式化

已完成：

- 扩展 `src/imaging/FluorescenceFormatter.h/.cpp`，新增 `FormatDefaultChannelName()`。
- `CameraPreviewApp::AddCurrentFrameAsChannel()` 不再直接拼接染料名和通道序号，荧光通道默认命名规则由 `FluorescenceFormatter` 统一维护。
- 自动测试新增 FITC 通道默认名称校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 窗口控件布局计算抽象

已完成：

- 新增 `src/ui/ControlIds.h`，集中维护 Win32 控件编号。
- 新增 `src/ui/WindowControlLayout.h/.cpp`，封装工具栏和右侧面板控件的坐标、可见性计算。
- `LayoutControls()` 不再直接维护大段 `MoveWindow()` 坐标逻辑，而是消费 `WindowControlLayout::Compute()` 的布局结果。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ControlIds` 和 `WindowControlLayout` 文件。
- 自动测试新增宽窗口工具栏/侧栏关键控件布局、窄窗口侧栏隐藏和控件布局非负高度校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

后续可改进：

- 右侧面板控件较多，在较低窗口高度下测量列表会被压缩；后续可加入滚动面板或分组折叠。

## 2026-06-30 窗口控件创建定义抽象

已完成：

- 新增 `src/ui/WindowControlDefinitions.h/.cpp`，集中描述 Win32 控件的 ID、窗口类、初始文本和样式。
- `WM_CREATE` 不再逐个手写 `CreateWindowW()` 和 `WM_SETFONT`，而是遍历 `WindowControlDefinitions::All()` 创建控件并设置字体。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `WindowControlDefinitions` 源文件。
- 自动测试新增控件定义数量、ID 唯一性、关键按钮/复选框/数字输入样式和未知 ID 查找校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 伪彩选项映射抽象

已完成：

- 扩展 `src/imaging/PseudoColorMapper.h/.cpp`，新增 `PaletteOptions()` 和 `PaletteAtIndex()`。
- `CameraPreviewApp::UpdatePseudoColor()` 不再维护伪彩下拉索引 switch，`WM_CREATE` 也不再维护本地伪彩数组。
- 自动测试新增伪彩选项数量、顺序、有效索引和非法索引回退校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 标定单位选项映射抽象

已完成：

- 扩展 `src/domain/CalibrationProfile.h/.cpp`，新增 `CalibrationUnitOptions()` 和 `CalibrationUnitAtIndex()`。
- `WM_CREATE` 不再硬编码标定单位下拉框的 `um/mm` 字符串，`BeginCalibration()` 不再直接判断下拉索引。
- 自动测试新增标定单位选项数量、顺序、有效索引和非法索引回退校验。
- 需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 相机设备列表显示模型抽象

已完成：

- 新增 `src/camera/CameraDeviceListFormatter.h/.cpp`，统一相机设备下拉框的 SDK 未加载、未找到相机、设备列表显示文本和默认选择规则。
- `CameraPreviewApp::RefreshCameraList()` 不再直接硬编码设备下拉占位文字和设备显示循环，而是消费 `CameraDeviceListPresentation`。
- `CameraPreviewApp::UpdateSelectedCamera()` 不再直接判断下拉索引边界，而是通过 `SelectionToDeviceIndex()` 映射到设备索引。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `CameraDeviceListFormatter` 源文件。
- 自动测试新增 SDK 未加载、无相机、多相机显示项、空显示名回退和非法索引回退校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 荧光染料库更新删除规则抽象

已完成：

- 扩展 `src/imaging/DyeLibrary.h/.cpp`，新增 `FallbackDye()`、`IndexAtSelection()`、`UpsertByName()` 和 `DeleteAt()`。
- `CameraPreviewApp::SaveDyeProfile()` 不再直接维护同名染料查找、更新或追加逻辑，而是消费 `DyeLibraryUpdateResult`。
- `CameraPreviewApp::DeleteSelectedDye()` 不再直接 `erase()` 并计算下一选择项，而是消费 `DyeLibraryDeleteResult`。
- `SelectedDye()` 和 `SelectedDyeIndex()` 不再直接判断下拉索引边界，空染料库默认通道染料也由 `DyeLibrary` 提供。
- 自动测试新增默认空通道染料、下拉索引映射、同名更新、新增染料、删除后下一选择项和非法删除校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 荧光通道显示设置抽象

已完成：

- 新增 `src/imaging/FluorescenceChannelSettings.h/.cpp`，统一荧光通道可见性、黑白强度范围、通道列表索引映射和默认显示设置。
- `CameraPreviewApp::AddCurrentFrameAsChannel()` 使用统一默认通道显示设置，不再直接设置 `visible/black_level/white_level`。
- `SyncSelectedChannelControls()` 使用统一设置对象回填控件，空选择状态也由统一默认值提供。
- `ApplySelectedChannelSettings()` 不再直接判断白场是否大于黑场，而是通过 `FromLevels()` 生成可应用的设置。
- `SelectedChannelIndex()` 不再直接判断列表索引边界，而是通过 `IndexAtSelection()` 映射。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceChannelSettings` 源文件。
- 自动测试新增默认设置、空选择设置、通道索引映射、合法/非法黑白范围、应用设置和从通道读取设置校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 拼接和 EDF 参数规则抽象

已完成：

- 新增 `src/imaging/ProcessingParameterRules.h/.cpp`，统一拼接搜索百分比、EDF 清晰度半径、默认处理参数、项目恢复夹取、配准搜索半径和拼接优化参数计算。
- `CameraPreviewApp::AddCurrentFrameAsStitchTile()` 和 `BuildStitchPreview()` 不再直接硬编码拼接搜索百分比范围。
- `CameraPreviewApp::BuildEdfPreview()` 不再直接硬编码 EDF 半径范围。
- `StartStitchProcessing()` 不再直接计算拼接优化搜索半径和迭代次数，而是消费 `ProcessingParameterRules::StitchOptimizationOptionsFor()`。
- `ClearProcessing()` 和默认成员初始化改用统一默认拼接搜索百分比与 EDF 参数。
- `ProjectSessionMapper::FromDocument()` 不再直接使用局部 `std::clamp()` 恢复处理参数，而是复用统一规则。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingParameterRules` 源文件。
- 自动测试新增拼接搜索百分比范围、EDF 半径范围、项目恢复夹取、配准搜索半径、拼接优化半径和迭代次数校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 测量列表选择规则抽象

已完成：

- 新增 `src/ui/MeasurementListSelection.h/.cpp`，统一测量结果列表选中索引校验和删除后下一项选择规则。
- `CameraPreviewApp::DeleteSelectedMeasurement()` 不再直接判断 `LB_GETCURSEL` 边界和计算删除后的下一选择项。
- `CameraPreviewApp::RenameSelectedMeasurement()` 刷新列表后使用统一索引规则恢复当前选择。
- `SelectedMeasurement()` 不再直接判断列表索引边界，而是通过 `MeasurementListSelection::IndexAtSelection()` 映射到集合扁平索引。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementListSelection` 源文件。
- 自动测试新增有效/非法列表索引、删除空列表、删除中间项和删除末尾项后的下一选择校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 测量叠加模型构建抽象

已完成：

- 新增 `src/ui/MeasurementOverlayModelBuilder.h/.cpp`，统一把测量集合、标定、显示单位和待完成测量状态组装成 `MeasurementOverlayModel`。
- `CameraPreviewApp::BuildMeasurementOverlayModel()` 不再直接维护待完成点、角度和多边形状态到绘制模型的映射，只负责调用构建器。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementOverlayModelBuilder` 源文件。
- 自动测试新增测量集合指针、标定指针、显示单位、待完成多边形和待完成角度映射校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-06-30 测量点拖拽编辑会话抽象

已完成：

- 新增 `src/ui/MeasurementEditSession.h/.cpp`，统一维护测量点拖拽编辑是否激活、目标测量对象、编辑点类型和顶点索引。
- `CameraPreviewApp::BeginMeasurementEdit()` 不再直接保存拖拽目标字段，而是通过 `MeasurementEditSession::Begin()` 启动编辑会话。
- `CameraPreviewApp::ContinueMeasurementEdit()` 不再直接调用集合点位更新，而是通过 `MeasurementEditSession::ApplyTo()` 更新当前编辑目标并返回需要恢复选中的测量引用。
- `CameraPreviewApp::EndMeasurementEdit()` 不再重置局部拖拽结构，而是通过 `MeasurementEditSession::Clear()` 结束编辑会话。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementEditSession` 源文件。
- 自动测试新增未开始编辑时不更新、长度端点拖动、多边形顶点拖动和清空会话校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 后台处理结果帧状态抽象

已完成：

- 新增 `src/imaging/ProcessingResultFrames.h/.cpp`，统一维护拼接结果、EDF 合成图、EDF 焦点图和当前处理结果显示开关。
- `CameraPreviewApp::AddCurrentFrameAsStitchTile()`、`AddCurrentFrameAsEdfFrame()` 和 `ClearProcessing()` 不再直接重置多份处理结果帧，而是调用 `ProcessingResultFrames::Clear()`。
- `CameraPreviewApp::ShowEdfFocusMap()` 不再直接复制焦点图到当前处理结果，而是调用 `ProcessingResultFrames::ShowEdfFocusMap()`。
- `CameraPreviewApp::ApplyProcessingResult()` 不再直接判断 EDF/拼接结果并维护三份帧对象，而是通过 `ProcessingResultFrames::Apply()` 发布完成结果。
- `PreviewFrameComposer` 的处理结果输入和现场诊断中的处理结果可见状态均改为从 `ProcessingResultFrames` 读取。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingResultFrames` 源文件。
- 自动测试新增拼接结果应用、EDF 合成图/焦点图应用、失败结果忽略和清空状态校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 后台处理重试快照抽象

已完成：

- 新增 `src/imaging/ProcessingRetryState.h/.cpp`，统一维护上一后台处理任务的类型、拼接 tile 快照、拼接搜索范围、EDF 图像栈和 EDF 参数。
- `CameraPreviewApp::ClearProcessing()` 不再直接重置多组 `last_*` 重试字段，而是调用 `ProcessingRetryState::Clear()`。
- `CameraPreviewApp::RetryProcessing()` 不再直接检查 `last_processing_kind_` 和具体栈大小，而是消费 `ProcessingRetryRequest` 与 `CanRetry()`。
- `StartStitchProcessing()` 和 `StartEdfProcessing()` 不再直接维护重试字段，而是分别调用 `RememberStitch()` 和 `RememberEdf()` 保存可重试输入快照。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingRetryState` 源文件。
- 自动测试新增默认重试状态、拼接重试快照、EDF 重试快照、EDF 帧数不足不可重试和清空状态校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 后台处理进度节流抽象

已完成：

- 新增 `src/imaging/ProcessingProgressThrottle.h/.cpp`，统一维护后台处理进度按步长刷新状态栏的节流规则。
- `StartStitchProcessing()` 和 `StartEdfProcessing()` 不再分别维护局部 `last_progress` 变量，而是通过 `ProcessingProgressThrottle::ShouldReport()` 判断是否发布进度状态。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingProgressThrottle` 源文件。
- 自动测试新增默认 10% 进度间隔、自定义 5% 间隔和非法间隔归一化校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 测量交互动作应用抽象

已完成：

- 新增 `src/ui/MeasurementActionApplier.h/.cpp`，统一把 `MeasurementInteractionAction` 应用到标定状态和测量集合。
- `CameraPreviewApp::HandleLeftClick()` 不再直接维护标定完成、长度完成、角度完成和矩形完成的对象创建分支，而是消费 `MeasurementActionApplyResult`。
- `CameraPreviewApp::FinishPolygonAreaMeasurement()` 不再直接生成多边形测量对象，而是复用同一个动作应用模块。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementActionApplier` 源文件。
- 自动测试新增提示状态、两点标定、长度、角度、矩形面积和多边形面积完成动作应用校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 测量工具启动帧可用性抽象

已完成：

- 新增 `src/ui/MeasurementToolAvailability.h/.cpp`，统一维护标定和测量工具启动前的预览帧可用性检查与提示文本。
- `CameraPreviewApp::BeginCalibration()`、`BeginLengthMeasurement()`、`BeginAngleMeasurement()`、`BeginRectangleAreaMeasurement()` 和 `BeginPolygonAreaMeasurement()` 不再分别判断帧状态，而是通过 `EnsureMeasurementToolFrameAvailable()` 复用同一入口校验规则。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementToolAvailability` 源文件。
- 自动测试新增空帧禁止启动标定/测量、有效帧允许启动测量的校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 荧光通道设置应用抽象

已完成：

- 新增 `src/imaging/FluorescenceChannelUpdater.h/.cpp`，统一维护荧光通道显示设置的选择项校验、黑白电平范围校验、设置写回和状态文本。
- `CameraPreviewApp::ApplySelectedChannelSettings()` 不再直接判断通道选择、调用 `FluorescenceChannelSettings::FromLevels()` 和写回通道，而是消费 `FluorescenceChannelUpdateResult` 后刷新列表与预览。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceChannelUpdater` 源文件。
- 自动测试新增未选择、范围越界、黑白电平顺序非法和合法应用通道设置的校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 荧光染料输入解析抽象

已完成：

- 新增 `src/ui/DyeProfileFormParser.h/.cpp`，统一解析荧光染料名称、激发波长、发射波长和 RGB 输入，并返回可显示的错误状态。
- `CameraPreviewApp::SaveDyeProfile()` 不再直接校验名称、波长和 RGB 输入，而是消费 `DyeProfileInputResult` 后更新染料库和界面。
- 移除主窗口中已无调用的 `ReadNonNegativeNumber()` 辅助函数。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `DyeProfileFormParser` 源文件。
- 自动测试新增空名称、非法波长、非法 RGB 和合法染料输入解析校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 荧光通道创建工厂抽象

已完成：

- 新增 `src/imaging/FluorescenceChannelFactory.h/.cpp`，统一按染料、当前图像帧和通道序号创建默认可见的荧光通道。
- `CameraPreviewApp::AddCurrentFrameAsChannel()` 不再直接设置通道名称、图像帧、颜色和默认黑白电平，而是调用 `FluorescenceChannelFactory::CreateFromFrame()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceChannelFactory` 源文件。
- 自动测试新增按 FITC 染料和图像帧创建默认可见通道的校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 拼接图块放置规划抽象

已完成：

- 新增 `src/imaging/StitchTilePlacementPlanner.h/.cpp`，统一维护首张图块原点放置、后续图块自动配准放置和配准失败后的水平回退。
- `CameraPreviewApp::AddCurrentFrameAsStitchTile()` 不再直接调用 `ImageRegistration::EstimateTranslation()` 或手动计算图块偏移，而是消费 `StitchTilePlacementResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `StitchTilePlacementPlanner` 源文件。
- 自动测试新增首张图块原点放置、配准偏移应用和失败回退放置校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 后台处理作业执行器抽象

已完成：

- 新增 `src/imaging/ProcessingJobExecutor.h/.cpp`，统一执行拼接和 EDF 后台算法，并返回成功、失败或取消的 `ProcessingJobResult`。
- `CameraPreviewApp::StartStitchProcessing()` 不再直接维护拼接优化、拼接融合、取消判断和完成状态文本生成，而是在线程内调用 `ProcessingJobExecutor::RunStitch()`。
- `CameraPreviewApp::StartEdfProcessing()` 不再直接调用 EDF 合成和组装焦点图结果，而是在线程内调用 `ProcessingJobExecutor::RunEdf()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingJobExecutor` 源文件。
- 自动测试新增拼接作业成功、拼接作业取消、EDF 作业成功、EDF 作业取消和进度顺序校验。
- README、需求追踪、图像处理设计、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 文件对话框封装

已完成：

- 新增 `src/platform/FileDialog.h/.cpp`，统一封装 CSV、BMP、诊断报告和项目文件的保存/打开对话框。
- `CameraPreviewApp::ExportMeasurementsCsv()`、`ExportImage()`、`SaveDiagnosticsReport()`、`SaveProject()` 和 `OpenProject()` 不再直接填充 `OPENFILENAMEW`，只消费 `FileDialog` 返回的路径。
- `main.cpp` 不再直接包含 `commdlg.h`，Win32 公共文件对话框细节集中在平台层。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FileDialog` 源文件。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 项目打开运行态恢复抽象

已完成：

- 新增 `src/app/ProjectSessionRestorer.h/.cpp`，统一把 `ProjectSessionState` 应用到当前运行态，并生成打开项目后的状态文本。
- `CameraPreviewApp::OpenProject()` 不再手动逐项恢复标定、测量、染料、荧光通道和处理参数，而是调用 `ProjectSessionRestorer::Restore()`。
- 打开项目时会同步清理旧拼接 tile 队列、EDF 栈、后台处理重试快照和已显示的处理结果，避免新项目误用旧运行态数据。
- 打开项目时会清理测量采集状态和拖拽编辑会话，释放可能存在的鼠标捕获。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProjectSessionRestorer` 源文件。
- 自动测试新增项目恢复运行态、清理旧处理队列、清空重试快照和隐藏旧处理结果的校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 测量列表动作抽象

已完成：

- 新增 `src/ui/MeasurementListActions.h/.cpp`，统一封装测量结果列表的删除、重命名、名称修剪、状态文本和删除后下一项选择规则。
- `CameraPreviewApp::DeleteSelectedMeasurement()` 不再直接判断列表选择、删除集合项和计算下一选择项，而是消费 `MeasurementListActionResult`。
- `CameraPreviewApp::RenameSelectedMeasurement()` 不再直接修剪名称、判断空名称、写回集合和恢复选择项，而是调用 `MeasurementListActions::RenameSelected()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementListActions` 源文件。
- 自动测试新增空集合删除、未选择删除、删除后下一项选择、空名称重命名和修剪后重命名校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 荧光通道列表动作抽象

已完成：

- 新增 `src/imaging/FluorescenceChannelListActions.h/.cpp`，统一封装荧光通道添加、无图像拒绝、清空通道、融合预览开关和添加后列表选中新通道规则。
- `CameraPreviewApp::AddCurrentFrameAsChannel()` 不再直接创建通道、推入列表、打开融合预览和计算新通道选中项，而是消费 `FluorescenceChannelListActionResult`。
- `CameraPreviewApp::ClearFluorescenceChannels()` 不再直接清空通道并关闭融合预览，而是调用 `FluorescenceChannelListActions::Clear()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceChannelListActions` 源文件。
- 自动测试新增无图像添加拒绝、添加成功后选中新通道并打开融合预览、清空后关闭融合预览的校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 染料资料动作抽象

已完成：

- 新增 `src/ui/DyeLibraryActions.h/.cpp`，统一封装荧光染料资料保存、无效输入拒绝、删除、状态文本和删除后下一项选择规则。
- `CameraPreviewApp::SaveDyeProfile()` 不再直接解析输入、更新染料库和拼接保存状态，而是消费 `DyeLibraryActionResult`。
- `CameraPreviewApp::DeleteSelectedDye()` 不再直接校验下拉选择、删除染料和计算下一选择项，而是调用 `DyeLibraryActions::DeleteSelected()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `DyeLibraryActions` 源文件。
- 自动测试新增无效保存拒绝、新增染料、同名更新、未选择删除和删除后下一项选择校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-01 染料资料表单展示抽象

已完成：

- 新增 `src/ui/DyeProfileFormPresenter.h/.cpp`，统一封装荧光染料资料表单显示文本和空表单默认值。
- `CameraPreviewApp::SyncSelectedDyeControls()` 不再直接格式化激发/发射波长和 RGB 文本，而是消费 `DyeProfileFormValues`。
- `main.cpp` 移除了只服务于染料表单的 `FormatDouble()` helper，以及对应的 `<iomanip>` / `<sstream>` 依赖。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `DyeProfileFormPresenter` 源文件。
- 自动测试新增空染料表单默认值和 DAPI 表单文本格式校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-02 荧光通道设置表单展示抽象

已完成：

- 新增 `src/ui/FluorescenceChannelFormPresenter.h/.cpp`，统一封装荧光通道设置表单的可见性、黑场和白场显示文本，以及空通道默认值。
- `CameraPreviewApp::SyncSelectedChannelControls()` 不再直接创建默认通道设置、读取通道设置并格式化黑白范围文本，而是消费 `FluorescenceChannelFormValues`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceChannelFormPresenter` 源文件。
- 自动测试新增空通道表单默认值和已选通道表单文本校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-02 拼接图块列表动作抽象

已完成：

- 新增 `src/imaging/StitchTileListActions.h/.cpp`，统一封装当前帧加入拼接队列、无图像拒绝、入队后 tile 数、配准状态和状态栏文本。
- `CameraPreviewApp::AddCurrentFrameAsStitchTile()` 不再直接调用拼接图块放置器、推入 tile 列表或拼接入队状态文本，而是消费 `StitchTileListActionResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `StitchTileListActions` 源文件。
- 自动测试新增无图像加入拒绝、第一张 tile 原点加入、第二张 tile 配准加入和状态文本校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-03 EDF 堆栈列表动作抽象

已完成：

- 新增 `src/imaging/EdfStackListActions.h/.cpp`，统一封装当前帧加入 EDF 堆栈、无图像拒绝、入队后帧数和状态栏文本。
- `CameraPreviewApp::AddCurrentFrameAsEdfFrame()` 不再直接校验帧、推入 EDF 堆栈或拼状态文本，而是消费 `EdfStackListActionResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `EdfStackListActions` 源文件。
- 自动测试新增无图像加入拒绝、第一帧加入和第二帧加入后帧数状态文本校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-03 处理面板动作抽象

已完成：

- 新增 `src/imaging/ProcessingPanelActions.h/.cpp`，统一封装 EDF 焦点图显示、缺少焦点图拒绝、清空处理队列、请求取消运行作业、失效旧后台结果和状态栏文本。
- `CameraPreviewApp::ShowEdfFocusMap()` 不再直接切换 `ProcessingResultFrames` 或拼状态文本，而是消费 `ProcessingPanelActionResult`。
- `CameraPreviewApp::ClearProcessing()` 不再直接清空拼接 tile、EDF 堆栈、重试快照和处理结果帧，而是调用 `ProcessingPanelActions::Clear()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingPanelActions` 源文件。
- 自动测试新增缺少 EDF 焦点图拒绝、显示 EDF 焦点图、清空处理队列和旧后台结果失效校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-03 处理重试动作抽象

已完成：

- 新增 `src/imaging/ProcessingRetryActions.h/.cpp`，统一封装拼接/EDF 重试请求判定、无可重试任务提示和重试启动提示。
- `CameraPreviewApp::RetryProcessing()` 不再直接读取 `ProcessingRetryState` 后判断任务类型和可重试状态，而是消费 `ProcessingRetryActionResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingRetryActions` 源文件。
- 自动测试新增空重试、空拼接重试、有效拼接重试、无效 EDF 重试和有效 EDF 重试动作校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-03 处理结果发布动作抽象

已完成：

- 新增 `src/imaging/ProcessingResultActions.h/.cpp`，统一封装后台完成结果取出、缺少结果忽略、旧作业结果忽略、失败状态发布和成功结果显示动作。
- `CameraPreviewApp::ApplyProcessingResult()` 不再直接调用 `ProcessingJobState::TakePending()`、判断当前作业结果或直接应用处理结果帧，而是消费 `ProcessingResultActionResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingResultActions` 源文件。
- 自动测试新增无待发布结果、旧作业结果、失败结果和成功 EDF 结果发布校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 处理构建请求动作抽象

已完成：

- 新增 `src/imaging/ProcessingBuildActions.h/.cpp`，统一封装拼接/EDF 构建请求前置校验、启动输入拷贝和状态栏文本。
- `CameraPreviewApp::BuildStitchPreview()` 不再直接校验空 tile、拼接搜索范围和拷贝 tile 请求，而是消费 `ProcessingBuildActionResult`。
- `CameraPreviewApp::BuildEdfPreview()` 不再直接校验 EDF 栈长度、焦点半径和拷贝 EDF 请求，而是消费 `ProcessingBuildActionResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingBuildActions` 源文件。
- 自动测试新增空拼接输入、非法拼接搜索、有效拼接请求、EDF 栈不足、非法 EDF 半径和有效 EDF 请求校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 后台处理启动动作抽象

已完成：

- 新增 `src/imaging/ProcessingStartActions.h/.cpp`，统一封装拼接/EDF 后台启动前的运行态检查、旧 worker 回收回调、待发布结果清理、重试快照记录、作业编号创建和启动状态文本。
- `CameraPreviewApp::StartStitchProcessing()` 不再直接校验空 tile、判断运行中作业、清理待发布结果、记录拼接重试快照或直接创建作业编号，而是消费 `ProcessingStartActionResult` 后只负责启动线程。
- `CameraPreviewApp::StartEdfProcessing()` 不再直接校验 EDF 栈长度、判断运行中作业、清理待发布结果、记录 EDF 重试快照或直接创建作业编号，而是消费 `ProcessingStartActionResult` 后只负责启动线程。
- `ProcessingStatusFormatter` 新增已有后台作业运行中的状态文本格式化入口。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingStartActions` 源文件。
- 自动测试新增空拼接启动、运行中作业拒绝、有效拼接启动、EDF 栈不足启动拒绝、有效 EDF 启动和不记录重试快照的启动校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 后台进度回调动作抽象

已完成：

- 新增 `src/imaging/ProcessingProgressActions.h/.cpp`，统一封装拼接/EDF 后台进度回调中的取消检查、节流判定和状态文本生成。
- `CameraPreviewApp::StartStitchProcessing()` 不再直接在 worker 进度 lambda 中检查取消令牌、调用 `ProcessingProgressThrottle` 或拼接进度状态文本，而是消费 `ProcessingProgressActionResult`。
- `CameraPreviewApp::StartEdfProcessing()` 不再直接在 worker 进度 lambda 中检查取消令牌、调用 `ProcessingProgressThrottle` 或拼接 EDF 进度状态文本，而是消费 `ProcessingProgressActionResult`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingProgressActions` 源文件。
- 自动测试新增初始进度上报、节流抑制、下一进度上报、取消后忽略进度和自定义 EDF 进度步长校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 后台 worker 创建动作抽象

已完成：

- 新增 `src/imaging/ProcessingWorkerActions.h/.cpp`，统一封装拼接/EDF worker 线程创建、进度动作接线、后台算法执行和结果发布回调。
- `CameraPreviewApp::StartStitchProcessing()` 不再直接创建拼接线程，也不再在线程 lambda 中调用 `ProcessingJobExecutor` 或 `ProcessingProgressActions`，只消费 `ProcessingWorkerActions::StartStitch()` 返回的线程。
- `CameraPreviewApp::StartEdfProcessing()` 不再直接创建 EDF 线程，也不再在线程 lambda 中调用 `ProcessingJobExecutor` 或 `ProcessingProgressActions`，只消费 `ProcessingWorkerActions::StartEdf()` 返回的线程。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingWorkerActions` 源文件。
- 自动测试新增拼接/EDF worker 线程创建、进度状态回调和结果发布校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 测量工具启动动作抽象

已完成：

- 新增 `src/ui/MeasurementToolStartActions.h/.cpp`，统一封装标定、长度、角度、矩形面积和多边形面积工具启动前的可用性结果处理、标定长度输入状态、交互状态切换和提示文本。
- `CameraPreviewApp::BeginCalibration()` 不再直接判断标定长度、预览帧状态或直接调用 `MeasurementInteractionState::BeginCalibration()`，而是消费 `MeasurementToolStartActionResult` 后只保存待应用的标定长度和单位。
- `CameraPreviewApp::BeginLengthMeasurement()`、`BeginAngleMeasurement()`、`BeginRectangleAreaMeasurement()` 和 `BeginPolygonAreaMeasurement()` 不再直接判断预览帧状态或切换交互状态，而是统一调用 `MeasurementToolStartActions`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementToolStartActions` 源文件。
- 自动测试新增无效标定长度、无预览帧拒绝、有效标定启动以及长度/角度/矩形面积/多边形面积工具启动校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 测量点击和完成动作抽象

已完成：

- 新增 `src/ui/MeasurementInteractionActions.h/.cpp`，统一封装测量点击点应用、多边形完成、无帧提示、图像坐标转换失败提示、测量列表刷新标记和预览刷新标记。
- `CameraPreviewApp::HandleLeftClick()` 不再直接调用 `MeasurementInteractionState::AddPoint()` 和 `MeasurementActionApplier::Apply()`，而是只负责把 Win32 点击位置换算成业务输入并消费 `MeasurementInteractionActionResult`。
- `CameraPreviewApp::FinishPolygonAreaMeasurement()` 不再直接调用 `MeasurementInteractionState::FinishPolygon()` 和 `MeasurementActionApplier::Apply()`，而是复用 `MeasurementInteractionActions::FinishPolygon()`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementInteractionActions` 源文件。
- 自动测试新增空闲点击忽略、预览外点击忽略、无帧提示、图像坐标转换失败提示、长度点击完成和多边形完成校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 导出动作状态抽象

已完成：

- 新增 `src/app/ExportActions.h/.cpp`，统一封装测量 CSV、图像 BMP 和诊断报告保存动作，返回成功、无测量、无图像和写入失败状态文本。
- `CameraPreviewApp::ExportMeasurementsCsv()` 不再直接调用 `MeasurementCsvExporter::Save()`，只负责保存对话框和状态栏刷新。
- `CameraPreviewApp::ExportImage()` 不再直接调用 `ImageExporter::SaveBmp()`，只负责选择当前导出帧和状态栏刷新。
- `CameraPreviewApp::SaveDiagnosticsReport()` 不再直接写 UTF-8 BOM 文件，也不再维护本地 `Utf8FromWide()` 转换函数。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ExportActions` 源文件。
- 自动测试新增空测量拒绝、CSV 保存、无图像拒绝、BMP 保存和 UTF-8 BOM 诊断报告保存校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新，UML PNG 已重新渲染。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 项目保存读取动作抽象

已完成：

- 新增 `src/app/ProjectActions.h/.cpp`，统一封装项目保存、项目读取、会话状态到项目文档映射、项目文档到会话状态映射和文件读写状态文本。
- `CameraPreviewApp::SaveProject()` 不再直接构造 `ProjectDocument`、调用 `ProjectSessionMapper::ToDocument()` 或 `ProjectRepository::Save()`，只负责保存对话框和状态栏刷新。
- `CameraPreviewApp::OpenProject()` 不再直接调用 `ProjectRepository::Load()` 或 `ProjectSessionMapper::FromDocument()`，读取成功后只把 `ProjectActions` 返回的会话状态交给 `ProjectSessionRestorer` 应用到运行态。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProjectActions` 源文件。
- 自动测试新增 `ProjectActions` 保存项目文件和读取项目会话状态校验，覆盖测量、染料、荧光通道设置、EDF 半径和拼接搜索范围。
- README、需求追踪、架构说明、项目保存时序图和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-13 诊断报告动作抽象

已完成：

- 新增 `src/app/DiagnosticReportActions.h/.cpp`，统一封装现场诊断状态快照到诊断报告输入的组装，以及 SDK 遥测摘要入口。
- `CameraPreviewApp::BuildDiagnosticsReport()` 不再直接构造 `DiagnosticReportInput` 或调用 `DiagnosticReportBuilder::Build()`，只负责采集当前窗口、相机、测量和处理队列状态快照。
- `CameraPreviewApp::BuildSdkTelemetry()` 不再直接调用 `DiagnosticReportBuilder`，而是通过 `DiagnosticReportActions` 统一转发。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `DiagnosticReportActions` 源文件。
- 自动测试新增 `DiagnosticReportActions` 报告生成和 SDK 遥测摘要校验，覆盖测量数量、预览状态、处理状态、帧信息、伪彩、染料、荧光通道、拼接和 EDF 摘要。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 预览显示动作抽象

已完成：

- 新增 `src/imaging/PreviewDisplayActions.h/.cpp`，统一封装伪彩下拉标签、伪彩选择结果状态文本，以及当前预览帧构建入口。
- `CameraPreviewApp::UpdatePseudoColor()` 不再直接调用 `PseudoColorMapper`，而是消费 `PreviewDisplayActions::SelectPseudoColor()` 的结果。
- `CameraPreviewApp::BuildPreviewFrame()` 不再直接组装 `PreviewFrameComposition`，而是通过 `PreviewDisplayActions::BuildPreviewFrame()` 转交给 `PreviewFrameComposer`。
- 伪彩下拉初始化改为使用 `PreviewDisplayActions::PseudoColorLabels()`，窗口层不再关心伪彩选项来源。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `PreviewDisplayActions` 源文件。
- 自动测试新增伪彩标签、伪彩选择状态、非法选择回退、处理结果/融合/伪彩预览优先级校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 相机面板动作抽象

已完成：

- 新增 `src/ui/CameraPanelActions.h/.cpp`，统一封装相机设备刷新结果、设备选择结果、曝光输入解析、曝光范围夹取和曝光状态文本。
- `CameraPreviewApp::RefreshCameraList()` 不再直接拼装设备下拉、启用状态、默认选择和枚举状态文本，而是消费 `CameraPanelActions` 返回的刷新动作结果。
- `CameraPreviewApp::UpdateSelectedCamera()` 不再直接调用 `CameraDeviceListFormatter::SelectionToDeviceIndex()`，而是消费 `CameraPanelActions::SelectDevice()`。
- `CameraPreviewApp::ApplyExposure()` 不再直接解析曝光输入或执行范围夹取，窗口层只负责保存请求值、调用驱动并显示动作模块返回的状态文本。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `CameraPanelActions` 源文件。
- 自动测试新增 SDK 未加载、无相机、有相机、无效选择、有效选择、曝光输入解析、曝光夹取、曝光成功/失败/待应用状态校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 视口交互动作抽象

已完成：

- 新增 `src/imaging/ViewportInteractionActions.h/.cpp`，统一封装滚轮缩放校验、拖拽平移开始、拖动过程、结束状态和平移会话状态。
- `CameraPreviewApp::HandleMouseWheel()` 不再直接调用 `ImageViewport::ZoomAt()`，而是通过 `ViewportInteractionActions::ZoomAt()` 处理预览区域、滚轮增量和帧有效性校验。
- `CameraPreviewApp::BeginPan()`、`ContinuePan()` 和 `EndPan()` 不再直接维护 `panning_` 和 `last_pan_point_`，而是使用 `ViewportPanState` 和 `ViewportInteractionActions`。
- 测量点拖拽编辑启动逻辑改为读取 `ViewportPanState::active`，继续避免平移和测量编辑互相抢占。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ViewportInteractionActions` 源文件。
- 自动测试新增滚轮区域/无效帧/零增量拒绝、有效缩放、编辑状态阻止平移、无效帧拖动状态保持、有效拖动触发预览刷新和结束平移状态校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 测量显示动作抽象

已完成：

- 新增 `src/ui/MeasurementDisplayActions.h/.cpp`，统一封装测量显示单位、结果列表文本、选中测量映射和叠加绘制模型入口。
- `CameraPreviewApp::DisplayUnit()`、`BuildMeasurementOverlayModel()`、`SelectedMeasurement()`、`MeasurementCount()` 和 `RefreshMeasurementList()` 改为通过 `MeasurementDisplayActions` 获取显示结果。
- `main.cpp` 不再直接依赖 `MeasurementFormatter`、`MeasurementListSelection` 或 `MeasurementOverlayModelBuilder`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `MeasurementDisplayActions` 源文件。
- 自动测试新增显示单位、列表文本、选中项映射和叠加模型入口校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 荧光显示动作抽象

已完成：

- 新增 `src/ui/FluorescenceDisplayActions.h/.cpp`，统一封装染料下拉文本、当前染料选择、荧光通道列表文本和通道选中索引。
- `CameraPreviewApp::InitializeDyeCombo()`、`SelectedDye()`、`SelectedDyeIndex()`、`SelectedChannelIndex()` 和 `RefreshChannelList()` 改为通过 `FluorescenceDisplayActions` 获取显示与选择结果。
- `main.cpp` 不再直接依赖 `FluorescenceFormatter` 或 `FluorescenceChannelSettings`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `FluorescenceDisplayActions` 源文件。
- 自动测试新增染料下拉标签、染料选择回退、通道列表文本和通道选中索引校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 处理参数输入动作抽象

已完成：

- 新增 `src/ui/ProcessingBuildInputActions.h/.cpp`，统一封装拼接搜索百分比、EDF 半径文本输入解析，以及入队前搜索参数确认。
- `CameraPreviewApp::AddCurrentFrameAsStitchTile()`、`BuildStitchPreview()` 和 `BuildEdfPreview()` 不再直接解析拼接/EDF 参数文本，而是消费 `ProcessingBuildInputActions` 的结果。
- `main.cpp` 不再直接依赖 `ProcessingBuildActions`，拼接/EDF 构建前文本输入由 UI 动作模块转交到处理构建动作。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingBuildInputActions` 源文件。
- 自动测试新增拼接搜索文本、跳过解析、非法输入、EDF 半径文本和构建动作转发校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 处理队列入队动作抽象

已完成：

- 新增 `src/ui/ProcessingQueueActions.h/.cpp`，统一封装拼接/EDF 入队编排、成功入队后的旧处理结果清理和预览刷新请求。
- `CameraPreviewApp::AddCurrentFrameAsStitchTile()` 不再直接解析入队前搜索参数或调用 `StitchTileListActions`，而是消费 `ProcessingQueueActions::AddStitchTile()` 的动作结果。
- `CameraPreviewApp::AddCurrentFrameAsEdfFrame()` 不再直接调用 `EdfStackListActions` 或清理处理结果，改由 `ProcessingQueueActions::AddEdfFrame()` 维护入队副作用。
- `main.cpp` 不再直接依赖 `StitchTileListActions` 或 `EdfStackListActions`。
- `CMakeLists.txt`、`CameraView.vcxproj` 和 `CameraView.vcxproj.filters` 已加入 `ProcessingQueueActions` 源文件。
- 自动测试新增无图像不清旧结果、首个拼接帧跳过搜索解析、已有拼接帧搜索参数拒绝、成功入队清空旧处理结果和 EDF 入队清理校验。
- README、需求追踪、架构说明和 UML 源文件已同步更新。

验证结果：
```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-07-14 BMP 图像打开功能

已完成：

- 新增 `ImageExporter::LoadBmp()`，支持读取未压缩 8 位灰度/调色板 BMP 和 24 位 BMP，并复用现有 `ImageFrame` 作为当前图像帧。
- 新增 `FileDialog::OpenBmp()` 和界面 `Open Image` 按钮，打开离线 BMP 后会停止实时预览、发布为当前帧并刷新视口。
- 打开离线 BMP 时会请求取消并失效旧后台拼接/EDF 作业，避免旧处理结果晚到后覆盖新图像。
- 自动测试复用导出 BMP 文件回读，验证图像尺寸、步幅、像素数据、8 位调色板 BMP 读取和缺失文件错误路径。
- 自动测试新增后台作业取消令牌和失效旧结果校验。
- README、需求追踪、架构说明和 UML 源文件已同步记录 BMP 读取入口。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 Fit 视图按钮

已完成：

- 新增工具栏 `Fit` 按钮，调用现有 `ImageViewport::Reset()` 恢复完整图像视图。
- `CameraPreviewApp::FitView()` 在有当前显示帧时重置缩放/平移并刷新预览，无图像时给出状态提示。
- 自动测试更新工具栏控件数量、`Fit` 按钮布局和控件定义校验。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 EDF 合成图/焦点图切换

已完成：

- 新增右侧处理区域 `EDF Image` 按钮，可从 EDF 焦点图切回 EDF 合成图。
- `ProcessingResultFrames` 新增 `ShowEdfCompositeFrame()`，和已有 `ShowEdfFocusMap()` 共同维护当前处理结果显示帧。
- `ProcessingPanelActions` 新增 EDF 合成图显示动作，统一处理缺少 EDF 结果时的状态提示。
- 自动测试覆盖 EDF 合成图/焦点图来回切换、缺少结果拒绝和状态文本。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 相机断开状态提示保留

已完成：

- 采集线程正常停止仍显示 `Preview stopped.`，但相机断开退出时保留 `Camera disconnected.`，不再被收尾状态覆盖。
- `CameraControlStatusFormatter` 新增停止和断开状态文本入口。
- 自动测试覆盖停止和断开状态文本。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 处理结果诊断摘要

已完成：

- 诊断报告新增当前处理结果类型和尺寸，可区分拼接结果与 EDF 结果。
- EDF 结果会额外记录合成图和焦点图是否可用，便于现场验收保存证据。
- 主窗口生成诊断报告时会从 `ProcessingResultFrames` 读取当前结果状态。
- 自动测试覆盖诊断报告生成器和应用状态组装报告两条路径。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-14 诊断报告设备列表

已完成：

- 诊断报告新增最近一次枚举到的相机设备列表，包含设备显示名和类型。
- 诊断报告新增选中设备摘要，现场多相机或枚举异常时更容易核对。
- 主窗口刷新设备后会保存设备列表快照，并在保存诊断报告时带入。
- 自动测试覆盖诊断报告生成器和应用状态组装报告两条路径。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 当前帧来源诊断

已完成：

- 诊断报告新增 `Latest frame source` 字段，用于记录当前帧来自相机设备还是离线 BMP 文件。
- 打开离线 BMP 后会保存图像文件路径，便于离线测量、伪彩、融合、拼接和 EDF 复现。
- 相机预览打开后会记录设备序号和相机类型，便于现场多设备核对。
- 自动测试覆盖诊断报告生成器和应用状态组装报告两条路径。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 缩放倍率状态提示

已完成：

- 滚轮缩放成功后，状态栏会显示当前缩放倍率，例如 `Zoom: 110%.`。
- 工具栏 `Fit` 恢复完整图像视图后，会显示恢复到 `Zoom: 100%.`。
- `ViewportInteractionActions` 新增缩放倍率状态文本生成入口，自动测试覆盖常见倍率格式。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 32 位 BMP 离线导入

已完成：

- `ImageExporter::LoadBmp()` 新增未压缩 32 位 BGRA BMP 读取支持，导入时忽略 Alpha 并转成当前 BGR 帧。
- 自动测试新增 2x2 top-down 32 位 BMP，覆盖方向、步幅和 BGR 像素值。
- README、需求追踪和架构说明已同步 BMP 支持范围。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 处理结果显示来源诊断

已完成：

- `ProcessingResultFrames` 新增当前显示来源状态，可区分拼接结果、EDF 合成图和 EDF 焦点图。
- 诊断报告新增 `Processing result source` 字段，保存报告时可看到当前处理结果具体显示来源。
- 自动测试扩展处理结果发布、EDF 图切换和诊断报告字段校验。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 图像打开和导出尺寸提示

已完成：

- `CameraPreviewApp::OpenImage()` 打开离线 BMP 后，状态栏显示 `Image opened: 宽x高.`，遥测区显示 `Loaded BMP image | 宽x高`。
- `ExportActions::SaveImageBmp()` 导出 BMP 成功后，状态文本显示导出帧尺寸。
- 自动测试更新导出图片成功提示断言。
- README 已同步离线打开和图像导出的尺寸提示说明。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 预览显示模式诊断

已完成：

- `PreviewDisplayActions` 新增预览显示模式标签，可区分原图、伪彩、荧光融合和处理结果显示。
- 诊断报告新增 `Preview display mode` 字段，现场保存报告时能看到当前预览实际显示模式。
- 自动测试覆盖预览模式优先级和诊断报告字段输出。
- README、需求追踪、架构说明和 UML 类图已同步。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
Rendered 21 .png files to docs\uml\rendered
```

## 2026-07-15 诊断报告视口缩放记录

已完成：

- `DiagnosticReportActions` 和 `DiagnosticReportBuilder` 新增视口缩放字段，诊断报告会写出 `Viewport zoom`。
- `CameraPreviewApp::BuildDiagnosticsReport()` 会记录当前 `ImageViewport` 缩放比例。
- `ViewportInteractionActions` 新增缩放值格式化入口，状态栏和诊断报告共用一致的百分比格式。
- 自动测试覆盖缩放值格式化和诊断报告字段输出。
- README 和需求追踪已同步；按最新要求，本轮未重新渲染 UML。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-07-15 导出图片显示模式提示

已完成：

- `ExportActions::SaveImageBmp()` 支持可选的当前显示模式文本，导出成功后可显示类似 `Image exported: 16x16 (Pseudo color: Hot).`。
- `CameraPreviewApp::ExportImage()` 会把当前预览显示模式传给导出动作，便于区分原图、伪彩、荧光融合、拼接和 EDF 结果。
- 自动测试覆盖保留旧导出提示和新增显示模式提示。
- README 和需求追踪已同步；按最新要求，本轮未重新渲染 UML。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-07-15 可缩放卡片功能面板

已完成：

- `WindowLayout::ComputeSidePanelWidth()` 改为随窗口宽度自适应，较宽窗口会给右侧功能面板更多空间，较窄窗口仍优先保留预览区。
- 右侧面板改为可收缩功能卡片，按 Camera、Image、Fluorescence、Processing、Measurement、Project 显示对应功能，避免所有按钮挤在一列。
- `CameraPreviewApp` 保存当前功能分类，切换分类后会重新排布控件并保留各功能状态。
- 自动测试覆盖卡片按钮位置、默认 Camera 卡、图像卡、测量卡显示、无效分类回退和可缩放侧栏宽度。
- README 和需求追踪已同步；按最新要求，本轮未重新渲染 UML。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-07-15 相机参数功能卡

已完成：

- 右侧功能区新增 Camera 卡，集中放置自动曝光、曝光、增益和白平衡控制，点击其他卡片时自动收起。
- `MUCamApi`、`ICameraDriver` 和 `MUCamCameraDriver` 新增自动曝光、RGB 等比例增益和白平衡 SDK 调用入口；诊断报告同步显示 AutoExp、Gain、WB 能力。
- `CameraPreviewApp` 新增相机参数按钮动作：未打开相机时给出提示，SDK 或相机不支持时显示不可用状态。
- 自动测试覆盖 Camera 卡默认展开、Image/Measurement 卡切换、相机参数控件定义、SDK 新能力遥测和诊断报告字段。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-07-15 图像渲染效率优化

已完成：

- `FrameBuffer` 新增共享帧快照，实时绘制路径可以直接引用当前帧，减少大图深拷贝。
- `CameraPreviewApp` 新增当前显示帧缓存：普通原图预览直接使用源帧；伪彩按源帧变化重算；荧光融合在通道或显示状态变化时重算，避免鼠标缩放、平移、状态栏刷新时反复整图处理。
- 主窗口双缓冲从每次 `WM_PAINT` 创建/销毁位图，改为复用窗口后台缓冲，并只拷贝本次无效区域。
- 导出图片仍保留一次安全拷贝，避免保存对话框期间实时预览刷新导致引用失效。
- 自动测试补充共享帧快照验证。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 2026-07-15 校准第二点点击修复

已完成：

- 图像坐标转换不再把预览区黑边点击强行吸附到图像边缘，点到图像外会提示 `Click inside the image area.`。
- 校准第二点如果与第一点相同，会继续保持在“等待第二点”状态，并提示点击不同位置，不再直接结束校准。
- 主点击动作新增 1 像素最小校准距离判断；第二点太近时提示 `Calibration: click a second point farther away.`，继续等待第二点。
- 校准失败提示拆分为“点距离问题”和“标定长度问题”，避免继续出现含糊的 `Pick two different points and a positive length`。
- 自动测试覆盖重复校准点继续等待、图像外点击拒绝和校准后续完成路径。

验证结果：

```text
[100%] Built target CameraView
[100%] Built target CameraViewDomainTests
100% tests passed, 0 tests failed out of 1
```

## 尚未完成

- 连接真实相机后，按 `docs/camera_field_verification.md` 执行现场预览验证并记录结果。
