# 显微观察与测量软件设计确认单

本文档记录用户已确认的架构和 UML 默认方案。后续若需要调整，可指出对应编号并追加新的设计决策。

## 1. 默认设计决策

| 编号 | 确认项 | 默认方案 |
| --- | --- | --- |
| D-01 | UI 技术路线 | 第一阶段继续使用 Win32 原生界面，后续按需要评估 Qt |
| D-02 | 编译和工程 | 使用 Visual Studio C++/MSVC + CMake |
| D-03 | 相机驱动抽象 | 使用 `ICameraDriver` + `MUCamCameraDriver` 隔离 MUCam SDK |
| D-04 | 实时预览线程 | UI 线程负责交互和绘制，采集线程负责 SDK 取帧，最新帧通过 `FrameBuffer` 发布 |
| D-05 | 界面防闪烁 | `ImageViewport` 统一负责双缓冲绘制和局部刷新 |
| D-06 | 缩放和平移 | 鼠标滚轮缩放和后续平移统一进入 `ViewTransform` |
| D-07 | 测量坐标 | 测量点只保存图像像素坐标，不保存屏幕坐标 |
| D-08 | 标定方式 | 第一阶段使用两点标定，保存 microns-per-pixel |
| D-09 | 第一批测量工具 | 长度、角度、圆/直径、矩形面积、多边形面积 |
| D-10 | 项目保存 | JSON 元数据 + 图像文件，保留源图和处理参数 |
| D-11 | 导出能力 | 第一阶段 CSV 和图片导出，报告/PDF 后续扩展 |
| D-12 | 图像伪彩 | 伪彩只影响显示映射，不破坏原始灰度或单通道数据 |
| D-13 | 荧光通道融合 | 第一阶段支持 2-4 通道、强度范围、可见性和加法融合 |
| D-14 | 荧光染料管理 | 支持用户自定义染料，记录激发波长、发射波长、默认颜色和备注 |
| D-15 | 图像拼接 | 第一阶段支持导入多张图片进行重叠配准和全景融合 |
| D-16 | EDF 景深扩展 | 第一阶段支持导入 Z 序列图片进行合成 |
| D-17 | 后台处理 | 拼接和 EDF 通过 `ProcessingQueue` 后台执行，支持进度、失败、取消和重试 |

## 2. 已确认的实现条件

当前已按默认方案确认：

1. 是否接受上表默认设计。
2. 是否需要调整第一批测量工具。
3. 是否需要预置常见荧光染料库。
4. 图像拼接是否需要第一阶段就联动电动载物台或自动扫描。
5. EDF 是否需要第一阶段就联动电动 Z 轴。

## 3. 相关设计文件

- `docs/design_index.md`
- `docs/design_completion_audit.md`
- `docs/architecture_uml.md`
- `docs/requirements_traceability.md`
- `docs/uml_gallery.md`
- `docs/uml_rendering.md`
- `docs/image_processing_design.md`
- `docs/implementation_plan.md`
- `docs/architecture_decisions.md`
- `docs/uml/README.md`
- `docs/design_review_report.md`
- `docs/acceptance_checklist.md`
