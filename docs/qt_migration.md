# CameraView Qt 迁移说明

## 迁移结果

默认桌面应用现已使用 Qt 6 Widgets。原来的 Win32 窗口、消息循环、控件布局和 GDI 视口不再进入默认 `CameraView` 目标；MUCam SDK 动态加载、相机驱动以及既有领域算法继续复用。

Qt 前端位于 `src/qt/`：

- `CameraMainWindow`：菜单、工具栏、可停靠功能面板和业务状态管理。
- `CameraWorker`：在独立 Qt 线程中枚举设备、打开相机和采集帧，避免阻塞界面线程。
- `ImageCanvas`：Qt 绘制的图像视口，支持适合窗口、光标中心缩放、平移和测量交互。
- `HistogramWidget`：Qt 绘制的多通道直方图与统计信息。
- `ai/YoloWorkspaceWidget`：YOLO 检测、分类、分割、训练和模型管理工作台。
- `ai/YoloProcessController`：异步 Python 后端进程与 JSON 事件协议。
- `ai/YoloModelRegistry`：用户级模型权重和元数据持久化。

默认 Qt 版已经接入：

- MUCam 设备刷新、打开/停止、曝光、自动曝光、增益和白平衡；
- 离线图像打开、拖放、翻转/旋转、PNG/JPEG/TIFF/BMP 导出；
- 亮度、对比度、伽马、窗位/窗宽、伪彩和多通道直方图；
- 长度、角度、矩形面积、多边形面积、两点标定、结果重命名和 CSV 导出；
- 荧光通道采集、可见性/黑白电平设置及融合预览；
- 当前帧或批量文件加入多帧拼接与 EDF 景深扩展，耗时任务通过 QtConcurrent 后台执行；
- CameraView 项目保存与恢复、现场诊断报告导出；
- Qt 与 MUCam 运行时自动部署到可执行文件目录。
- YOLO 模型导入/管理、检测/分类/实例分割、训练进度与日志、训练产物自动入库和 ONNX 导出。

## 构建

建议使用仓库自带的 PowerShell 构建入口，它会查找 Qt、CMake、Ninja 和匹配 Qt ABI 的 MinGW/MSVCRT 编译器：

```powershell
.\tools\build_qt.ps1
```

指定 Qt 安装目录：

```powershell
.\tools\build_qt.ps1 -QtRoot C:\Qt6\6.9.0\mingw_64
```

可选参数：

- `-WithOpenCV`：启用 OpenCV 拼接后端；未安装时仍使用内置后端。
- `-SkipTests`：仅构建，不运行领域测试。
- `-BuildDirectory <目录>`：指定构建目录。

构建完成后，`CameraView.exe`、Qt DLL、Qt 插件、MinGW 运行时以及 MUCam DLL 位于同一个构建目录，可直接启动。

## CMake 目标

- `CameraView`：默认 Qt 6 应用。
- `CameraViewDomainTests`：原有核心领域测试。
- `CameraViewYoloRegistryTests`：YOLO 模型库导入、元数据、活动模型和托管文件生命周期测试。

旧 Win32 源码继续保留在 `src/ui/` 与 `src/main.cpp`，用于迁移差异追踪，但不再作为产品构建目标。

## 平台边界

界面和图像交互已迁移到 Qt，但当前随仓库提供的 MUCam SDK 是 Windows DLL，因此真实相机采集仍限于 Windows。离线图像、测量和处理层保持与 Qt 界面解耦，后续可以为其他平台增加不同的 `ICameraDriver` 实现。
