# 显微观察与测量软件需求追踪矩阵

本文档用于把用户目标、功能需求、架构模块和 UML 图源对应起来。它是设计评审材料，当前代码已在用户确认后进入第一阶段基础拆分。

## 1. 需求范围

当前阶段已完成架构设计和 UML 建模，并已在用户确认后开始模块拆分和代码实现。

目标软件范围：

- 显微相机实时观察。
- 图像缩放、平移和叠加显示。
- 标定比例尺，将像素坐标转换为真实单位。
- 创建、编辑、删除和管理测量对象。
- 图像伪彩、荧光通道融合和荧光染料资料管理。
- 图像拼接和 EDF 景深扩展。
- 保存项目和导出测量结果。
- 保持相机采集、图像显示、测量计算和存储导出之间边界清晰。

## 2. 功能需求追踪

| 编号 | 功能需求 | 负责模块 | 领域对象 | UML 证据 | 第一阶段实现建议 |
| --- | --- | --- | --- | --- | --- |
| FR-01 | 枚举相机设备 | `CameraPanel`, `CameraPanelActions`, `CameraController`, `ICameraDriver`, `MUCamCameraDriver`, `CameraDeviceListFormatter`, `CameraControlStatusFormatter` | `CameraDevice` | `use_case.puml`, `component.puml`, `preview_sequence.puml` | 已支持 `Refresh` 枚举已连接 MUCam 设备，并可在 `Device` 下拉框中选择目标设备，刷新后的控件启用状态、默认设备索引、遥测文本和状态文本由 `CameraPanelActions` 统一返回，设备下拉占位、显示文本和选择索引由 `CameraDeviceListFormatter` 统一生成，枚举和选择提示由 `CameraControlStatusFormatter` 统一生成 |
| FR-02 | 选择并打开指定相机 | `CameraController`, `ObservationSession` | `CameraDevice` | `preview_sequence.puml`, `session_state.puml` | 保留当前下拉选择能力，改为通过控制器调用 |
| FR-03 | 开始/停止实时预览 | `ObservationSession`, `CameraController`, `FrameBuffer` | `ImageFrame` | `preview_sequence.puml`, `session_state.puml` | 已新增 `FrameBuffer`，采集线程发布最新帧，UI 读取稳定快照并可开始/停止实时预览；预览线程退出时会关闭相机句柄，正常停止显示停止提示，相机断开时保留断开提示 |
| FR-04 | 设置曝光参数 | `CameraPanel`, `CameraPanelActions`, `CameraController`, `ICameraDriver`, `TextInputParser`, `CameraControlStatusFormatter` | `CameraSettings` | `use_case.puml`, `component.puml` | 已通过 `ICameraDriver` 暴露曝光范围和设置命令，`MUCamCameraDriver` 内部调用 MUCam SDK；曝光输入解析、范围夹取、待应用状态和设置结果文本由 `CameraPanelActions` 统一封装，底层解析使用 `TextInputParser`，状态提示由 `CameraControlStatusFormatter` 生成 |
| FR-05 | 图像显示与实时刷新 | `WindowLayout`, `WindowControlLayout`, `WindowControlDefinitions`, `ImageViewport`, `OverlayRenderer`, `CameraTelemetryFormatter` | `ImageFrame` | `class_model.puml`, `preview_sequence.puml` | 已新增 `WindowLayout` 封装预览区、可缩放右侧面板和状态栏区域计算，`WindowControlLayout` 封装工具栏、可收缩功能卡片和卡片内控件摆放，`WindowControlDefinitions` 封装控件创建定义；Camera 卡已提供自动曝光、曝光、增益、白平衡入口；`ImageViewport` 封装图像绘制和坐标换算，主窗口继续使用双缓冲绘制实时预览，`CameraTelemetryFormatter` 统一生成状态栏右侧设备、分辨率、FPS 和帧时间戳用于现场验证 |
| FR-06 | 鼠标滚轮缩放 | `ViewportInteractionActions`, `ImageViewport`, `ViewTransform` | `ScreenPoint`, `ImagePoint` | `class_model.puml`, `architecture_uml.md` 第 10 节 | 已通过 `ViewportInteractionActions` 校验滚轮事件、预览区域和帧有效性，再由 `ImageViewport` 调用 `ViewTransform`，并验证缩放锚点稳定；滚轮缩放和工具栏 `Fit` 按钮会在状态栏显示当前缩放倍率，`Fit` 可调用 `ImageViewport::Reset()` 恢复完整图像视图 |
| FR-07 | 后续支持图像平移 | `ViewportInteractionActions`, `ImageViewport`, `ViewTransform` | `ScreenPoint`, `ImagePoint` | `component.puml`, `class_model.puml` | 已通过 `ViewportInteractionActions` 统一维护右键/中键拖拽平移开始、拖动、结束和无效帧状态；实际平移由 `ImageViewport` 和 `ViewTransform` 完成 |
| FR-08 | 两点标定比例尺 | `CalibrationDialog`, `CalibrationService`, `MeasurementToolAvailability`, `MeasurementToolStartActions`, `MeasurementInteractionActions`, `MeasurementActionApplier`, `ObservationSession` | `CalibrationProfile` | `calibration_sequence.puml`, `class_model.puml` | 已实现两点标定核心和右侧面板交互，标定单位下拉选项和索引映射由 `CalibrationProfile` 统一提供；`MeasurementToolAvailability` 负责标定启动前的预览帧可用性检查，`MeasurementToolStartActions` 负责标定长度输入状态、预览可用性结果、交互状态切换和待应用标定长度/单位传递，点击完成动作、无帧/坐标转换失败提示和刷新结果由 `MeasurementInteractionActions` 统一处理，完成标定动作由 `MeasurementActionApplier` 统一应用到当前标定状态 |
| FR-09 | 像素到真实单位转换 | `CalibrationService`, `MeasurementEntity` | `CalibrationProfile`, `MeasurementResult` | `class_model.puml`, `architecture_uml.md` 第 10 节 | 已实现像素到 um/mm 的核心换算，单位标签、标定单位选项和非法索引回退由 `CalibrationProfile` 统一维护 |
| FR-10 | 创建长度测量 | `MeasurementPanel`, `MeasurementToolAvailability`, `MeasurementToolStartActions`, `MeasurementInteractionActions`, `MeasurementInteractionState`, `MeasurementActionApplier`, `MeasurementController`, `ImageViewport` | `LineMeasurement` | `measurement_sequence.puml`, `measurement_tool_state.puml` | 已实现长度测量核心、两点交互和预览叠加；`MeasurementToolAvailability` 负责测量工具启动前的预览帧可用性检查，`MeasurementToolStartActions` 负责长度测量启动动作和状态文本，`MeasurementInteractionState` 负责两点采集状态和下一步提示，`MeasurementInteractionActions` 负责点击点应用、无帧/坐标转换失败提示和刷新结果，`MeasurementActionApplier` 负责把完成动作新增为长度测量 |
| FR-11 | 创建角度测量 | `MeasurementToolAvailability`, `MeasurementToolStartActions`, `MeasurementInteractionActions`, `MeasurementInteractionState`, `MeasurementActionApplier`, `MeasurementController` | `AngleMeasurement` | `class_model.puml`, `measurement_tool_state.puml` | 已新增 `AngleMeasurement`，支持三点角度测量、叠加绘制、结果列表、CSV/图片导出和项目保存；工具启动前由 `MeasurementToolAvailability` 检查预览帧，`MeasurementToolStartActions` 负责角度测量启动动作和状态文本，三点采集状态由 `MeasurementInteractionState` 管理，点击点应用和刷新结果由 `MeasurementInteractionActions` 统一处理，完成动作由 `MeasurementActionApplier` 新增为角度测量 |
| FR-12 | 创建面积测量 | `MeasurementToolAvailability`, `MeasurementToolStartActions`, `MeasurementInteractionActions`, `MeasurementInteractionState`, `MeasurementActionApplier`, `MeasurementController` | `AreaMeasurement` | `class_model.puml`, `measurement_tool_state.puml` | 已新增矩形面积和多边形面积测量，支持标定后平方单位换算、叠加绘制、CSV/图片导出、项目保存和顶点拖拽编辑；工具启动前由 `MeasurementToolAvailability` 检查预览帧，`MeasurementToolStartActions` 负责矩形/多边形面积测量启动动作和状态文本，矩形两点和多边形点集采集由 `MeasurementInteractionState` 管理，点击点、多边形完成和刷新结果由 `MeasurementInteractionActions` 统一处理，完成动作由 `MeasurementActionApplier` 新增为面积测量 |
| FR-13 | 测量叠加绘制 | `MeasurementInteractionState`, `MeasurementDisplayActions`, `MeasurementOverlayModelBuilder`, `ImageViewport`, `OverlayRenderer` | `MeasurementEntity` | `component.puml`, `class_model.puml` | 已新增 `OverlayRenderer`，统一绘制长度线、角度射线、矩形/多边形边框、端点、测量文字和待完成测量点；`MeasurementDisplayActions` 统一选择显示单位并转交 `MeasurementOverlayModelBuilder` 组装绘制模型 |
| FR-14 | 测量结果列表 | `MeasurementPanel`, `MeasurementController`, `MeasurementCollection`, `MeasurementDisplayActions`, `MeasurementFormatter`, `MeasurementNameFormatter`, `MeasurementListActions`, `MeasurementListSelection` | `MeasurementResult` | `component.puml`, `class_model.puml` | 已在右侧列表显示长度、角度、面积的名称、数值和单位，`MeasurementDisplayActions` 统一生成列表文本、显示单位和选中测量映射，`MeasurementFormatter` 统一生成列表/状态/叠加文本，`MeasurementNameFormatter` 统一生成新测量对象默认名称，`MeasurementListActions` 统一执行列表删除和重命名规则，`MeasurementListSelection` 统一维护列表选中索引有效性，集合内扁平索引用于保持列表顺序 |
| FR-15 | 编辑/删除测量 | `MeasurementPanel`, `MeasurementHitTester`, `MeasurementEditSession`, `MeasurementListActions`, `MeasurementListSelection`, `MeasurementController`, `MeasurementCollection` | `MeasurementEntity` | `use_case.puml`, `measurement_tool_state.puml` | 已支持清空、单条删除、结果重命名和预览区拖拽点位编辑；`MeasurementHitTester` 负责端点/顶点命中检测，`MeasurementEditSession` 负责拖拽编辑会话和点位更新，`MeasurementListActions` 负责删除、重命名和状态文本，`MeasurementListSelection` 负责删除后下一项选择规则，集合模块负责重命名、点位更新和删除 |
| FR-16 | 保存项目 | `ExportService`, `ProjectActions`, `ProjectSessionMapper`, `ProjectSessionRestorer`, `ProjectRepository` | `ProjectDocument` | `use_case.puml`, `component.puml`, `class_model.puml`, `project_save_sequence.puml` | 已支持轻量 JSON 项目文件，`ProjectActions` 统一编排项目保存、项目读取、项目文档映射和文件读写状态文本，`ProjectSessionMapper` 负责会话状态与项目文档互转，保存/恢复标定、长度、角度、矩形面积、多边形面积测量、荧光染料资料、荧光通道配置和拼接/EDF 处理参数；`ProjectSessionRestorer` 负责打开项目后的运行态应用，并清理旧拼接/EDF 队列、处理结果和重试快照 |
| FR-17 | 导出 CSV | `ExportService`, `ExportActions`, `MeasurementCsvExporter` | `MeasurementResult` | `use_case.puml`, `component.puml`, `export_sequence.puml` | 已新增 `MeasurementCsvExporter`，通过保存对话框导出 UTF-8 BOM CSV 测量表格，支持逗号/引号转义和多边形点列表；`ExportActions` 统一维护无测量拒绝、CSV 写入结果和状态文本 |
| FR-18 | 导出图片 | `ExportService`, `ExportActions`, `DiagnosticReportActions`, `ImageExporter`, `OverlayRenderer` | `ImageFrame`, `MeasurementEntity` | `component.puml`, `export_sequence.puml` | 已支持导出带长度、角度、矩形面积和多边形面积测量叠加的 BMP 图片，并可打开未压缩 8 位灰度/调色板 BMP、24 位 BMP 或 32 位 BGRA BMP 作为当前帧；打开离线 BMP 时会取消并忽略旧后台处理结果，避免旧拼接/EDF 结果覆盖新图，并在状态栏/遥测区显示载入图像尺寸；`ExportActions` 统一维护无图像拒绝、BMP 写入结果、导出尺寸和当前显示模式提示、诊断报告 UTF-8 BOM 写入和状态文本；`DiagnosticReportActions` 统一维护现场诊断状态快照、SDK 遥测摘要、设备列表和选中设备摘要、当前帧来源、视口缩放、当前预览显示模式、拼接/EDF 结果类型、当前显示来源和尺寸摘要以及诊断报告文本生成入口 |
| FR-19 | 图像伪彩显示 | `ProcessingPanel`, `ImageProcessingService`, `PreviewDisplayActions`, `PreviewFrameComposer`, `PseudoColorMapper` | `ImageFrame`, `FluorescenceChannel` | `use_case.puml`, `component.puml`, `class_model.puml`, `fluorescence_fusion_activity.puml`, `fluorescence_fusion_sequence.puml`, `image_processing_design.md` | 已支持实时伪彩显示和伪彩图片导出，不修改原始帧；`PreviewDisplayActions` 统一封装伪彩下拉显示、选择结果、当前预览帧构建入口和预览显示模式标签；`PseudoColorMapper` 统一提供伪彩选项顺序、下拉索引映射和图像映射，当前预览帧由 `PreviewFrameComposer` 统一决定处理结果/融合/伪彩优先级 |
| FR-20 | 荧光多通道融合 | `ChannelPanel`, `ChannelFusionService`, `PreviewDisplayActions`, `PreviewFrameComposer`, `ChannelFusionEngine`, `FluorescenceDisplayActions`, `FluorescenceChannelFactory`, `FluorescenceChannelFormPresenter`, `FluorescenceChannelListActions`, `FluorescenceChannelSettings`, `FluorescenceChannelUpdater`, `FluorescenceFormatter` | `FluorescenceChannel`, `ChannelFusionRecipe` | `use_case.puml`, `component.puml`, `class_model.puml`, `fluorescence_fusion_activity.puml`, `fluorescence_fusion_sequence.puml`, `image_processing_design.md` | 已新增 `ChannelFusionEngine`，支持多通道加法融合、可见性和黑白强度范围；`PreviewDisplayActions` 会通过 `PreviewFrameComposer` 在融合预览开启时优先生成融合帧；`FluorescenceDisplayActions` 统一维护通道列表文本和通道选中索引；`FluorescenceChannelFactory` 按染料和当前帧创建默认可见通道；`FluorescenceChannelFormPresenter` 统一维护通道设置表单的可见性、黑场和白场显示文本；`FluorescenceChannelListActions` 统一维护通道添加、无图像拒绝、清空通道、融合预览开关和添加后选中新通道规则；`FluorescenceChannelSettings` 统一维护通道可见性、黑白范围和通道列表索引规则；`FluorescenceChannelUpdater` 统一应用通道显示设置并返回未选择、范围非法和更新完成状态；`FluorescenceFormatter` 统一生成通道默认名称和通道列表文本；项目文件会保存通道显示配置 |
| FR-21 | 荧光染料管理 | `ChannelPanel`, `DyeProfileFormParser`, `DyeProfileFormPresenter`, `DyeLibraryActions`, `DyeLibraryService`, `DyeLibrary`, `DyeRepository`, `FluorescenceDisplayActions`, `FluorescenceChannelFactory`, `FluorescenceChannelListActions`, `FluorescenceFormatter` | `DyeProfile` | `use_case.puml`, `component.puml`, `class_model.puml`, `fluorescence_fusion_activity.puml`, `fluorescence_fusion_sequence.puml`, `image_processing_design.md` | 已新增 `DyeLibrary` 和默认 DAPI/FITC/TRITC/Cy5 染料资料；右侧面板可新增、更新、删除项目内染料，并按染料创建通道；染料名称、激发/发射波长和 RGB 输入由 `DyeProfileFormParser` 统一解析校验，选中染料到表单文本的转换和空表单默认值由 `DyeProfileFormPresenter` 统一维护，染料保存/删除动作、状态文本和删除后下一选择项由 `DyeLibraryActions` 统一维护，同名染料更新、删除后下一选择项、空通道默认染料和下拉索引校验由 `DyeLibrary` 统一维护，染料下拉文本和当前染料选择由 `FluorescenceDisplayActions` 统一维护，按染料创建默认通道由 `FluorescenceChannelFactory` 统一维护，按当前选择染料加入通道后的列表动作由 `FluorescenceChannelListActions` 统一维护，底层染料文本格式由 `FluorescenceFormatter` 统一生成 |
| FR-22 | 图像拼接 | `StitchingPanel`, `StitchingService`, `ImageStitcher`, `ImageRegistration`, `StitchTileListActions`, `StitchTilePlacementPlanner`, `ProcessingParameterRules`, `ProcessingBuildActions`, `ProcessingBuildInputActions`, `ProcessingQueueActions`, `ProcessingStartActions`, `ProcessingWorkerActions`, `ProcessingQueue`, `ProcessingJobExecutor`, `ProcessingPanelActions`, `ProcessingRetryActions`, `ProcessingResultActions`, `ProcessingResultFrames`, `ProcessingRetryState`, `ProcessingProgressActions`, `ProcessingProgressThrottle`, `ProcessingStatusFormatter` | `StitchingProject`, `ProcessingJob`, `ImageFrame` | `use_case.puml`, `component.puml`, `class_model.puml`, `stitching_activity.puml`, `stitching_sequence.puml`, `processing_job_state.puml`, `image_processing_design.md` | 已新增 `ImageStitcher` 和 `ImageRegistration`，支持当前帧加入 tile 队列、调节搜索范围、相邻图自动平移配准、多图全局位姿优化、后台拼接预览和导出；拼接搜索百分比、配准搜索半径和优化搜索半径由 `ProcessingParameterRules` 统一维护；`StitchTileListActions` 统一维护当前帧加入拼接队列、无图像拒绝、加入后 tile 数和状态文本；`StitchTilePlacementPlanner` 统一维护新 tile 相对上一 tile 的配准放置和配准失败后的水平回退；`ProcessingBuildInputActions` 统一解析拼接搜索文本并转交构建动作；`ProcessingQueueActions` 统一编排拼接入队时的搜索文本确认、入队动作调用、成功后旧处理结果清理和预览刷新请求；`ProcessingBuildActions` 统一维护拼接构建前的空队列拒绝、搜索范围错误提示和后台启动输入拷贝；`ProcessingStartActions` 统一维护拼接后台启动前的运行态拒绝、旧 worker 回收回调、重试快照记录和作业编号创建；`ProcessingWorkerActions` 统一创建拼接 worker 线程、接线进度状态回调和结果发布回调；`ProcessingJobExecutor` 统一执行后台拼接作业并生成成功、失败或取消结果；`ProcessingPanelActions` 统一维护清空拼接/EDF 队列和忽略旧后台结果的动作；`ProcessingResultActions` 统一维护后台完成结果取出、旧作业结果忽略、失败状态发布和成功结果交付；拼接结果帧由 `ProcessingResultFrames` 统一发布到当前处理结果显示状态并标记为 `Stitch result` 来源；拼接重试快照由 `ProcessingRetryState` 统一维护，重试可用性判定和重试提示由 `ProcessingRetryActions` 统一维护；后台进度取消检查、状态文本和是否上报由 `ProcessingProgressActions` 统一维护，进度刷新节流由 `ProcessingProgressThrottle` 统一维护；`ProcessingStatusFormatter` 统一生成进度、取消、失败和完成文本 |
| FR-23 | EDF 景深扩展 | `EdfPanel`, `EdfService`, `EdfStackListActions`, `EdfProcessor`, `ImageRegistration`, `ProcessingParameterRules`, `ProcessingBuildActions`, `ProcessingBuildInputActions`, `ProcessingQueueActions`, `ProcessingStartActions`, `ProcessingWorkerActions`, `ProcessingQueue`, `ProcessingJobExecutor`, `ProcessingPanelActions`, `ProcessingRetryActions`, `ProcessingResultActions`, `ProcessingResultFrames`, `ProcessingRetryState`, `ProcessingProgressActions`, `ProcessingProgressThrottle`, `ProcessingStatusFormatter` | `ImageStack`, `EdfResult`, `ProcessingJob` | `use_case.puml`, `component.puml`, `class_model.puml`, `edf_activity.puml`, `edf_sequence.puml`, `processing_job_state.puml`, `image_processing_design.md` | 已新增 `EdfProcessor`，支持当前帧加入 EDF 队列、调节清晰度评分半径，并后台生成 EDF 合成图和焦点图预览；右侧面板可用 `EDF Image` 和 `Focus Map` 在合成图与焦点图之间切换，当前显示来源会记录为 `EDF composite` 或 `EDF focus map`；`EdfStackListActions` 统一维护当前帧加入 EDF 堆栈、无图像拒绝、加入后帧数和状态文本；`ProcessingQueueActions` 统一编排 EDF 入队动作调用、成功后旧处理结果清理和预览刷新请求；EDF 半径默认值、范围和项目恢复夹取由 `ProcessingParameterRules` 统一维护；`ProcessingBuildInputActions` 统一解析 EDF 半径文本并转交构建动作；`ProcessingBuildActions` 统一维护 EDF 构建前的栈长度拒绝、半径错误提示和后台启动输入拷贝；`ProcessingStartActions` 统一维护 EDF 后台启动前的运行态拒绝、旧 worker 回收回调、重试快照记录和作业编号创建；`ProcessingWorkerActions` 统一创建 EDF worker 线程、接线进度状态回调和结果发布回调；`ProcessingJobExecutor` 统一执行后台 EDF 作业并生成成功、失败或取消结果；`ProcessingPanelActions` 统一维护 EDF 合成图显示、焦点图显示、缺少结果拒绝和清空处理队列动作；`ProcessingResultActions` 统一维护后台完成结果取出、旧作业结果忽略、失败状态发布和成功结果交付；EDF 合成图、焦点图和当前显示结果由 `ProcessingResultFrames` 统一维护；EDF 重试快照由 `ProcessingRetryState` 统一维护，重试可用性判定和重试提示由 `ProcessingRetryActions` 统一维护；后台进度取消检查、状态文本和是否上报由 `ProcessingProgressActions` 统一维护，进度刷新节流由 `ProcessingProgressThrottle` 统一维护；`ProcessingStatusFormatter` 统一生成 EDF 后台作业状态文本 |

## 3. 非功能需求追踪

| 编号 | 非功能需求 | 架构响应 | UML/文档证据 | 验证方式 |
| --- | --- | --- | --- | --- |
| NFR-01 | 实时预览不卡 UI | UI 线程和采集线程分离，使用 `FrameBuffer` 发布最新帧 | `architecture_uml.md` 第 12 节，`preview_sequence.puml` | `FrameBuffer` 已封装最新帧互斥访问，状态栏右侧显示 FPS 遥测，现场按 `docs/camera_field_verification.md` 观察帧率和 UI 响应 |
| NFR-02 | 减少界面闪烁 | `WindowLayout` 负责预览区/状态栏/面板区域计算，`WindowControlLayout` 负责控件摆放和可收缩功能卡片显示，`ImageViewport` 负责图像绘制，主窗口负责双缓冲和局部刷新 | `class_model.puml`, `component.puml` | 已采用窗口双缓冲绘制，并改为复用后台缓冲位图；当前预览帧使用缓存，普通实时预览避免大图深拷贝，伪彩/融合只在源帧或显示模式变化时重算；自动测试覆盖宽/窄窗口布局区域、右侧卡片面板和关键控件摆放，现场按 `docs/camera_field_verification.md` 观察实时刷新和窗口缩放 |
| NFR-03 | 测量结果不受缩放影响 | 测量保存图像坐标，缩放只属于 `ViewportInteractionActions`、`ImageViewport` 和 `ViewTransform` | `architecture_uml.md` 第 10 节，`class_model.puml` | 已自动验证视口缩放时光标锚点稳定、平移状态稳定，对同一图像在不同缩放下测量应一致 |
| NFR-04 | 便于更换相机 SDK | 相机能力通过 `ICameraDriver` 抽象 | `component.puml`, `class_model.puml` | 已新增 `ICameraDriver` 和 `MUCamCameraDriver`，主窗口不再直接持有 MUCam SDK 句柄，并在状态栏显示 SDK 接口和可选能力诊断 |
| NFR-05 | 可维护性 | UI、应用服务、领域和基础设施分层 | `architecture_uml.md` 第 3 节，`component.puml` | 已抽出 `ICameraDriver`、`CameraPanelActions`、`CameraDeviceListFormatter`、`CameraControlStatusFormatter`、`CameraTelemetryFormatter`、`FrameBuffer`、`DiagnosticReportActions`、`ExportActions`、`ProjectActions`、`MeasurementActionApplier`、`MeasurementDisplayActions`、`MeasurementInteractionActions`、`MeasurementInteractionState`、`MeasurementHitTester`、`MeasurementEditSession`、`MeasurementListActions`、`MeasurementListSelection`、`MeasurementOverlayModelBuilder`、`MeasurementToolAvailability`、`MeasurementToolStartActions`、`MeasurementCollection`、`MeasurementFormatter`、`MeasurementNameFormatter`、`MeasurementCsvExporter`、`DiagnosticReportBuilder`、`ProjectSessionMapper`、`ProjectSessionRestorer`、`FileDialog`、`TextInputParser`、`DyeProfileFormParser`、`DyeProfileFormPresenter`、`DyeLibraryActions`、`DyeLibrary`、`FluorescenceDisplayActions`、`FluorescenceChannelFactory`、`FluorescenceChannelFormPresenter`、`FluorescenceChannelListActions`、`FluorescenceChannelSettings`、`FluorescenceChannelUpdater`、`FluorescenceFormatter`、`ProcessingParameterRules`、`ProcessingBuildActions`、`ProcessingBuildInputActions`、`ProcessingQueueActions`、`ProcessingStartActions`、`ProcessingProgressActions`、`ProcessingWorkerActions`、`ProcessingJobExecutor`、`ProcessingPanelActions`、`ProcessingRetryActions`、`ProcessingResultActions`、`ProcessingResultFrames`、`ProcessingRetryState`、`ProcessingProgressThrottle`、`ProcessingStatusFormatter`、`PreviewDisplayActions`、`StitchTileListActions`、`StitchTilePlacementPlanner`、`EdfStackListActions`、`ViewportInteractionActions`、`ControlIds`、`WindowLayout`、`WindowControlLayout`、`WindowControlDefinitions`、`ImageViewport` 和 `OverlayRenderer`，继续减少主窗口单文件职责 |
| NFR-06 | 数据可追溯 | `ProjectDocument` 保存标定、测量和图像处理配置 | `class_model.puml`, `architecture_uml.md` 第 11 节 | 保存再打开后测量结果和荧光通道配置保持一致 |
| NFR-07 | 渐进式实现 | 先抽模块，再做长度测量，再扩展其他测量 | `architecture_uml.md` 第 13 节 | 每阶段保持可编译 |
| NFR-08 | 图像处理不破坏原始数据 | 伪彩、融合、拼接和 EDF 生成派生图像，原始帧和源序列保留引用 | `class_model.puml`, `fluorescence_fusion_activity.puml`, `stitching_activity.puml`, `edf_activity.puml`, `image_processing_design.md` | `PreviewDisplayActions` 只通过 `PreviewFrameComposer` 返回派生预览帧，自动测试覆盖处理结果、融合和伪彩优先级 |
| NFR-09 | 大图处理保持 UI 可响应 | 拼接和 EDF 作为后台处理任务，完成后再发布结果到视口 | `component.puml`, `processing_job_state.puml`, `architecture_uml.md` 图像处理扩展模型 | 已将拼接和 EDF 构建切到后台 worker，并新增 `ProcessingQueueActions` 在队列变化时清理旧处理结果，`ProcessingBuildInputActions` 和 `ProcessingBuildActions` 在启动 worker 前准备拼接/EDF 后台构建输入；`ProcessingStartActions` 负责启动 worker 前的运行态检查、旧 worker 回收回调、待发布结果清理、重试快照记录和作业编号创建；`ProcessingWorkerActions` 负责创建拼接/EDF worker 线程、接线进度状态回调和结果发布回调；`ProcessingJobState` 管理作业编号、取消令牌、运行状态和待发布结果；`ProcessingJobExecutor` 负责在线程内执行拼接/EDF 并生成统一结果；`ProcessingResultActions` 负责忽略旧作业结果、发布失败状态或成功结果，`ProcessingResultFrames` 负责把完成结果发布成当前显示帧，`ProcessingRetryState` 负责保存可重试输入快照，`ProcessingRetryActions` 负责判断重试是否可启动并返回对应状态文本，`ProcessingPanelActions` 负责清空处理队列时请求取消并失效旧作业结果，`ProcessingProgressActions` 负责后台进度取消检查、状态文本和上报判定，`ProcessingProgressThrottle` 负责减少状态栏进度刷新频率，支持进度反馈、清空取消和 `Retry` 重试 |

## 4. UML 图源覆盖矩阵

| UML 图源 | 覆盖内容 | 关联需求 |
| --- | --- | --- |
| `docs/uml/use_case.puml` | 用户、相机 SDK、文件系统与主要用例 | FR-01 到 FR-23 |
| `docs/uml/component.puml` | 组件边界和依赖方向 | 全部功能需求，重点 FR-01、FR-05、FR-13、FR-16、FR-19 到 FR-23 |
| `docs/uml/package.puml` | 源码包边界和包依赖方向 | 全部功能需求，重点模块拆分和图像处理扩展 |
| `docs/uml/deployment.puml` | 可执行程序、SDK、相机和项目文件部署关系 | FR-01 到 FR-04、FR-16 到 FR-18 |
| `docs/uml/class_model.puml` | 核心类、接口和领域对象 | FR-01 到 FR-23 |
| `docs/uml/preview_sequence.puml` | 枚举、打开、取帧、显示流程 | FR-01 到 FR-05 |
| `docs/uml/calibration_sequence.puml` | 两点标定流程 | FR-08、FR-09 |
| `docs/uml/measurement_sequence.puml` | 长度测量创建流程 | FR-10、FR-13、FR-14 |
| `docs/uml/project_save_sequence.puml` | 项目保存和重新打开流程 | FR-16、NFR-06、NFR-08 |
| `docs/uml/export_sequence.puml` | CSV 和图片导出流程 | FR-17、FR-18 |
| `docs/uml/calibration_activity.puml` | 两点标定操作活动 | FR-08、FR-09 |
| `docs/uml/measurement_activity.puml` | 测量工具点位采集和结果更新 | FR-10 到 FR-15 |
| `docs/uml/fluorescence_fusion_activity.puml` | 伪彩、染料管理和通道融合流程 | FR-19 到 FR-21 |
| `docs/uml/stitching_activity.puml` | 图像拼接流程 | FR-22 |
| `docs/uml/edf_activity.puml` | EDF 景深扩展流程 | FR-23 |
| `docs/uml/fluorescence_fusion_sequence.puml` | 荧光通道融合交互时序 | FR-19 到 FR-21 |
| `docs/uml/stitching_sequence.puml` | 拼接后台任务时序 | FR-22、NFR-09 |
| `docs/uml/edf_sequence.puml` | EDF 后台任务时序 | FR-23、NFR-09 |
| `docs/uml/session_state.puml` | 会话状态转换 | FR-01 到 FR-05、FR-08、FR-10 |
| `docs/uml/measurement_tool_state.puml` | 测量工具交互状态 | FR-10 到 FR-15 |
| `docs/uml/processing_job_state.puml` | 图像处理后台任务状态 | FR-22、FR-23、NFR-09 |
| `docs/image_processing_design.md` | 图像处理算法和数据保真策略 | FR-19 到 FR-23、NFR-08、NFR-09 |

## 5. 设计确认出口标准

进入代码实现前需要满足，当前已确认：

1. 架构分层被确认：UI、应用服务、领域、基础设施四层是否接受。
2. 第一阶段 UI 路线被确认：继续 Win32 或迁移 Qt。
3. 第一批测量工具被确认。
4. 标定方式被确认。
5. 项目保存和导出方式被确认。
6. 当前 UML 图是否需要增删改。
7. 高级图像处理是否接受后台任务队列和进度状态模型。

当前已开始创建模块目录并拆分 `main.cpp`。下一步继续收敛主窗口职责，并连接真实相机完成现场预览验证。
