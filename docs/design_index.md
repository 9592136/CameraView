# 显微观察与测量软件设计文档总索引

本文档是显微观察与测量软件设计资料的入口。用户已确认默认方案，当前已进入第一阶段模块拆分和代码实现。

## 1. 推荐阅读顺序

| 顺序 | 文件 | 用途 |
| --- | --- | --- |
| 1 | `docs/design_review_report.md` | 快速了解当前设计完成度和限制 |
| 2 | `docs/design_completion_audit.md` | 查看设计阶段完成证据和当前阻塞点 |
| 3 | `docs/design_confirmation.md` | 查看需要用户确认的默认方案 |
| 4 | `docs/architecture_uml.md` | 阅读整体架构、模块边界和主要 UML 说明 |
| 5 | `docs/requirements_traceability.md` | 检查需求、模块、领域对象和 UML 证据是否对应 |
| 6 | `docs/uml/README.md` | 查看全部 PlantUML 图源 |
| 7 | `docs/uml_gallery.md` | 查看已渲染 PNG 图集 |
| 8 | `docs/uml_rendering.md` | 查看 UML 渲染和复核方法 |
| 9 | `docs/image_processing_design.md` | 查看伪彩、荧光融合、拼接、EDF 算法策略 |
| 10 | `docs/implementation_plan.md` | 查看确认后的分阶段实现计划 |
| 11 | `docs/implementation_progress.md` | 查看设计确认后的代码实现进度 |
| 12 | `docs/architecture_decisions.md` | 查看关键架构决策背景 |
| 13 | `docs/acceptance_checklist.md` | 查看设计确认和后续实现验收标准 |
| 14 | `tools/render_uml.ps1` | 执行 UML 源码级检查和可选渲染 |

## 2. 当前设计范围

- 显微相机设备枚举、选择、曝光设置和实时预览。
- 图像显示、防闪烁、鼠标滚轮缩放和后续平移。
- 标定、像素到真实单位转换、测量对象和结果列表。
- 项目保存、CSV 导出和图片导出。
- 图像伪彩、荧光通道融合和荧光染料管理。
- 图像拼接和 EDF 景深扩展。
- 后台处理任务队列、进度、取消、失败和重试。

## 3. UML 图源概况

PlantUML 图源位于 `docs/uml/`，当前包含：

| 图类型 | 数量 | 说明 |
| --- | --- | --- |
| 用例图 | 1 | 用户、相机 SDK、文件系统和核心用例 |
| 组件图 | 1 | UI、应用服务、领域、图像处理、基础设施组件 |
| 包图 | 1 | 代码目录和依赖方向 |
| 部署图 | 1 | Windows 工作站、MUCam SDK、相机和项目文件 |
| 类图 | 1 | 领域对象、服务、驱动和处理任务 |
| 时序图 | 8 | 预览、标定、测量、保存、导出、荧光融合、拼接、EDF |
| 活动图 | 5 | 标定、测量、荧光融合、拼接、EDF |
| 状态图 | 3 | 会话、测量工具、后台处理任务 |

合计 21 个 PlantUML 图源。

## 4. 设计阶段状态

| 项目 | 状态 |
| --- | --- |
| 架构设计 | 已完成初稿 |
| 设计完成审计 | 已完成 |
| UML 源文件 | 已完成源码级检查 |
| UML PNG 图集 | 已完成 |
| UML 渲染说明 | 已完成 |
| UML 检查脚本 | 已完成 |
| 需求追踪 | 已覆盖 FR-01 到 FR-23、NFR-01 到 NFR-09 |
| 图像处理算法策略 | 已完成初稿 |
| 实现计划 | 已完成初稿 |
| 架构决策记录 | 已完成初稿 |
| PlantUML 渲染 | 已完成 PNG 输出，位于 `docs/uml/rendered/` |
| 代码实现 | 已开始，完成基础帧对象、设备描述和视图缩放模块拆分 |

## 5. 当前实现入口

当前代码实现进度见 `docs/implementation_progress.md`。后续继续按 `docs/implementation_plan.md` 推进标定、测量、项目保存、伪彩、荧光融合、拼接和 EDF。
