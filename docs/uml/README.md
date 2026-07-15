# UML 图源索引

本目录保存显微观察与测量软件的独立 UML 源文件。主说明文档见 `../architecture_uml.md`，需求追踪矩阵见 `../requirements_traceability.md`，PNG 图集见 `../uml_gallery.md`，渲染说明见 `../uml_rendering.md`。

| 文件 | 图类型 | 内容 |
| --- | --- | --- |
| `use_case.puml` | 用例图 | 操作者、相机 SDK、文件系统与核心用例 |
| `component.puml` | 组件图 | UI、应用服务、领域模型、基础设施组件关系 |
| `package.puml` | 包图 | `ui`、`app`、`domain`、`camera`、`imaging`、`storage`、`platform` 包边界 |
| `deployment.puml` | 部署图 | Windows 工作站、MUCam SDK、显微相机和项目文件部署关系 |
| `class_model.puml` | 类图 | 第一阶段建议类、接口和依赖关系 |
| `preview_sequence.puml` | 时序图 | 枚举相机、打开设备、实时预览 |
| `calibration_sequence.puml` | 时序图 | 两点标定流程 |
| `measurement_sequence.puml` | 时序图 | 创建长度测量流程 |
| `project_save_sequence.puml` | 时序图 | 项目保存、图像文件写入和重新打开流程 |
| `export_sequence.puml` | 时序图 | CSV 导出和图片导出流程 |
| `calibration_activity.puml` | 活动图 | 两点标定操作和校验流程 |
| `measurement_activity.puml` | 活动图 | 测量点采集、坐标转换和结果刷新流程 |
| `fluorescence_fusion_activity.puml` | 活动图 | 伪彩、荧光染料选择和多通道融合流程 |
| `stitching_activity.puml` | 活动图 | 图像图块导入、配准、融合和全景输出流程 |
| `edf_activity.puml` | 活动图 | Z 轴图像序列对齐、清晰度评估和 EDF 合成流程 |
| `fluorescence_fusion_sequence.puml` | 时序图 | 荧光通道选择、融合配方应用和视口刷新 |
| `stitching_sequence.puml` | 时序图 | 拼接任务提交、后台配准、全景图发布 |
| `edf_sequence.puml` | 时序图 | EDF 任务提交、Z 序列对齐、合成图发布 |
| `session_state.puml` | 状态图 | 观察会话状态流转 |
| `measurement_tool_state.puml` | 状态图 | 测量工具交互状态 |
| `processing_job_state.puml` | 状态图 | 拼接、EDF 等后台图像处理任务状态 |

渲染方式示例：

```powershell
plantuml docs\uml\*.puml
```

当前 PNG 渲染结果位于：

```text
docs/uml/rendered/
```
