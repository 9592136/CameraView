# 显微观察与测量软件设计评审报告

本文档汇总当前架构设计和 UML 建模的完成度，用于在进入代码实现前快速评审。

## 1. 当前结论

当前阶段已完成设计总索引、设计完成审计、架构设计稿、需求追踪矩阵、设计确认单、UML 渲染说明、图像处理算法设计、实现计划、架构决策记录、验收检查表和独立 PlantUML 图源。用户已确认默认方案，代码已开始第一阶段基础拆分。

## 2. 需求覆盖

| 类别 | 覆盖范围 | 证据 |
| --- | --- | --- |
| 相机与实时预览 | 设备枚举、选择打开、曝光、取帧、实时显示 | `requirements_traceability.md` FR-01 到 FR-05 |
| 图像视口 | 防闪烁、滚轮缩放、后续平移、叠加绘制 | `requirements_traceability.md` FR-05 到 FR-07、FR-13 |
| 标定与测量 | 两点标定、真实单位转换、长度/角度/面积测量、结果列表、编辑删除 | `requirements_traceability.md` FR-08 到 FR-15 |
| 保存与导出 | 项目保存、CSV 导出、图片导出 | `requirements_traceability.md` FR-16 到 FR-18 |
| 荧光图像 | 伪彩、通道融合、染料管理 | `requirements_traceability.md` FR-19 到 FR-21，`image_processing_design.md` |
| 高级处理 | 图像拼接、EDF 景深扩展、后台处理任务 | `requirements_traceability.md` FR-22 到 FR-23、NFR-09，`image_processing_design.md` |
| 非功能要求 | UI 响应、防闪烁、数据可追溯、SDK 可替换、渐进式实现 | `requirements_traceability.md` NFR-01 到 NFR-09 |
| 设计入口 | 阅读顺序、设计范围、UML 概况、确认方式 | `design_index.md` |
| 设计完成审计 | 设计交付物、UML、需求范围和未实现边界审计 | `design_completion_audit.md` |
| UML PNG 图集 | 已渲染 PNG 的分类索引 | `uml_gallery.md` |
| UML 渲染 | PlantUML 渲染方式、源码级检查和渲染级检查 | `uml_rendering.md` |
| UML 检查脚本 | 源码级检查和可选 PNG/SVG 渲染入口 | `tools/render_uml.ps1` |
| 实现计划 | 分阶段拆分、验证重点、暂缓项 | `implementation_plan.md` |
| 架构决策 | UI、构建、相机抽象、测量坐标、派生图像、后台任务、项目文件 | `architecture_decisions.md` |
| 验收检查 | 设计确认项、实现基础验收、高级图像处理验收 | `acceptance_checklist.md` |

## 3. UML 图源清单

| 图类型 | 文件 |
| --- | --- |
| 用例图 | `docs/uml/use_case.puml` |
| 组件图 | `docs/uml/component.puml` |
| 包图 | `docs/uml/package.puml` |
| 部署图 | `docs/uml/deployment.puml` |
| 类图 | `docs/uml/class_model.puml` |
| 时序图 | `docs/uml/preview_sequence.puml`, `docs/uml/calibration_sequence.puml`, `docs/uml/measurement_sequence.puml`, `docs/uml/project_save_sequence.puml`, `docs/uml/export_sequence.puml`, `docs/uml/fluorescence_fusion_sequence.puml`, `docs/uml/stitching_sequence.puml`, `docs/uml/edf_sequence.puml` |
| 活动图 | `docs/uml/calibration_activity.puml`, `docs/uml/measurement_activity.puml`, `docs/uml/fluorescence_fusion_activity.puml`, `docs/uml/stitching_activity.puml`, `docs/uml/edf_activity.puml` |
| 状态图 | `docs/uml/session_state.puml`, `docs/uml/measurement_tool_state.puml`, `docs/uml/processing_job_state.puml` |

## 4. 静态校验结果

已对 `docs/uml/*.puml` 做源码级完整性检查：

| 检查项 | 结果 |
| --- | --- |
| PlantUML 图源数量 | 21 个 |
| 每个图源包含 `@startuml` | 通过 |
| 每个图源包含 `@enduml` | 通过 |
| PNG 渲染输出数量 | 21 个 |
| UML 索引是否引用新增图源 | 通过 |
| 需求追踪矩阵是否覆盖 FR-01 到 FR-23 | 通过 |
| 主设计文档是否引用确认单和评审报告 | 通过 |
| 图像处理算法设计是否可追踪 | 通过 |
| UML 渲染说明是否可追踪 | 通过 |
| UML PNG 图集是否可追踪 | 通过 |
| UML 检查脚本是否可运行源码级检查 | 通过 |
| 实现计划和架构决策是否可追踪 | 通过 |
| 设计总索引和验收检查表是否可追踪 | 通过 |
| 设计完成审计是否可追踪 | 通过 |

当前已生成 PNG 图像到 `docs/uml/rendered/`。SVG 尚未生成。

## 5. 已确认的默认方案

| 编号 | 已确认项 | 默认方案 |
| --- | --- | --- |
| C-01 | 是否接受当前整体架构 | 接受四层架构：UI、应用服务、领域模型、基础设施 |
| C-02 | UI 技术路线 | 第一阶段继续 Win32 原生界面 |
| C-03 | 第一批测量工具 | 长度、角度、圆/直径、矩形面积、多边形面积 |
| C-04 | 标定方式 | 两点标定 + 输入真实长度 |
| C-05 | 项目保存 | JSON 元数据 + 图像文件 |
| C-06 | 荧光染料资料 | 先支持用户自定义，后续可预置常见染料 |
| C-07 | 图像拼接 | 先支持多图导入拼接，自动扫描后续扩展 |
| C-08 | EDF | 先支持导入 Z 序列合成，电动 Z 轴联动后续扩展 |
| C-09 | 后台处理 | 拼接和 EDF 使用 `ProcessingQueue` |

## 6. 当前实现入口

用户已确认设计，当前按以下顺序进入实现：

1. 保持 MSVC + CMake 构建方式，先建立目标目录结构。
2. 抽出 `ICameraDriver`、`MUCamCameraDriver`、`ImageFrame` 和 `FrameBuffer`。
3. 抽出 `ViewTransform` 和 `ImageViewport`，把缩放、防闪烁和绘制边界固定下来。
4. 增加标定和测量领域对象，先实现长度测量。
5. 增加项目保存、CSV/图片导出。
6. 增加伪彩、荧光通道融合和染料管理。
7. 增加拼接、EDF 和 `ProcessingQueue` 后台任务模型。

当前实现进度见 `docs/implementation_progress.md`，详细阶段计划见 `docs/implementation_plan.md`，关键决策背景见 `docs/architecture_decisions.md`。

完整设计入口见 `docs/design_index.md`，设计确认和后续实现验收标准见 `docs/acceptance_checklist.md`。
