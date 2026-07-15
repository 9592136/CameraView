# 显微观察与测量软件实现计划

本文档描述用户确认设计后的实现顺序。当前代码已进入第一阶段实现，实际完成情况见 `docs/implementation_progress.md`。

## 1. 实现原则

- 每个阶段保持可编译、可运行。
- 先拆出边界稳定的基础模块，再增加测量和图像处理能力。
- 相机 SDK、Win32 UI、图像算法和项目存储互相隔离。
- 原始图像只读保存，伪彩、融合、拼接和 EDF 都生成派生数据。
- 高级图像处理通过后台任务执行，避免影响实时预览和鼠标交互。

## 2. 阶段计划

| 阶段 | 目标 | 主要模块 | 验证重点 |
| --- | --- | --- | --- |
| P0 | 保持现有程序可构建 | `CMakeLists.txt`, `src/main.cpp`, `MUCamApi` | 现有预览、设备选择、缩放、防闪烁不退化 |
| P1 | 抽出相机驱动和帧模型 | `ICameraDriver`, `MUCamCameraDriver`, `CameraDevice`, `ImageFrame`, `FrameBuffer` | 能枚举、打开相机并取帧 |
| P2 | 抽出图像视口和坐标变换 | `ImageViewport`, `ViewTransform`, `OverlayRenderer` | 鼠标滚轮缩放、局部刷新、双缓冲绘制稳定 |
| P3 | 建立应用层会话 | `ObservationSession`, `CameraController`, `MainWindow` | UI 只编排交互，采集线程和 UI 状态分离 |
| P4 | 实现标定和基础测量 | `CalibrationProfile`, `CalibrationService`, `MeasurementEntity`, `MeasurementController` | 两点标定、长度测量、缩放后结果不变 |
| P5 | 增加测量结果管理 | `MeasurementPanel`, `MeasurementResult` | 结果列表、删除、清空、重算 |
| P6 | 增加项目保存和导出 | `ProjectDocument`, `ProjectRepository`, `MeasurementCsvExporter`, `ImageExporter`, `ExportService` | 保存再打开后标定、测量、图像引用一致 |
| P7 | 增加伪彩、荧光融合和染料管理 | `PseudoColorMapper`, `ChannelFusionService`, `DyeLibraryService`, `DyeRepository` | 原始灰度不变，融合结果可回溯 |
| P8 | 增加拼接和 EDF | `ProcessingQueue`, `ImageRegistration`, `ImageStitcher`, `EdfProcessor` | 后台进度、取消、失败、结果发布 |

## 3. 第一批代码拆分顺序

已确认默认方案，建议先做以下最小拆分：

1. 创建 `src/domain/ImageFrame.h`、`src/camera/CameraDevice.h`、`src/camera/ICameraDriver.h`。
2. 创建 `src/camera/MUCamCameraDriver.h/.cpp`，内部复用现有 `MUCamApi`。
3. 创建 `src/camera/FrameBuffer.h/.cpp`，封装最新帧发布。
4. 创建 `src/imaging/ViewTransform.h/.cpp`，迁移缩放、屏幕坐标和图像坐标转换。
5. 创建 `src/ui/ImageViewport.h/.cpp`，迁移双缓冲绘制和局部刷新。
6. 保持 `src/main.cpp` 作为装配入口，逐步缩小职责。

## 4. 验证策略

| 能力 | 验证方式 |
| --- | --- |
| 编译 | MSVC + CMake 构建通过 |
| 设备枚举 | 下拉列表能刷新并显示 MUCam 设备 |
| 实时预览 | 连接相机后能持续显示图像，UI 可操作 |
| 防闪烁 | 预览刷新和窗口变化时没有明显整窗闪烁 |
| 滚轮缩放 | 鼠标所在位置附近保持视觉锚点，缩放流畅 |
| 测量坐标 | 同一图像不同缩放下测量结果一致 |
| 保存项目 | 关闭后重新打开，标定、测量、通道和处理参数可恢复 |
| 后台处理 | 拼接和 EDF 运行时 UI 不阻塞，可报告进度 |

## 5. 暂缓项

以下内容建议放到后续阶段，避免第一版实现过重：

- Qt 迁移。
- 报告/PDF 导出。
- 电动载物台自动扫描。
- 电动 Z 轴联动 EDF 采集。
- 常见荧光染料预置库。
- GPU 加速图像处理。
