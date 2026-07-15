# UML PNG 图集索引

本文档用于快速查看已渲染的 UML PNG。PlantUML 源文件仍以 `docs/uml/*.puml` 为准，PNG 仅用于评审阅读。

## 1. 总览

| 项目 | 当前状态 |
| --- | --- |
| PlantUML 源文件 | 21 个 |
| PNG 渲染文件 | 21 个 |
| PNG 输出目录 | `docs/uml/rendered/` |
| 源码级检查脚本 | `tools/render_uml.ps1 -Mode check` |

## 2. 结构类图

| 图 | 源文件 | PNG |
| --- | --- | --- |
| 用例图 | `docs/uml/use_case.puml` | `docs/uml/rendered/use_case.png` |
| 组件图 | `docs/uml/component.puml` | `docs/uml/rendered/component.png` |
| 包图 | `docs/uml/package.puml` | `docs/uml/rendered/package.png` |
| 部署图 | `docs/uml/deployment.puml` | `docs/uml/rendered/deployment.png` |
| 类图 | `docs/uml/class_model.puml` | `docs/uml/rendered/class_model.png` |

## 3. 时序图

| 图 | 源文件 | PNG |
| --- | --- | --- |
| 实时预览 | `docs/uml/preview_sequence.puml` | `docs/uml/rendered/preview_sequence.png` |
| 标定流程 | `docs/uml/calibration_sequence.puml` | `docs/uml/rendered/calibration_sequence.png` |
| 长度测量 | `docs/uml/measurement_sequence.puml` | `docs/uml/rendered/measurement_sequence.png` |
| 项目保存 | `docs/uml/project_save_sequence.puml` | `docs/uml/rendered/project_save_sequence.png` |
| 导出流程 | `docs/uml/export_sequence.puml` | `docs/uml/rendered/export_sequence.png` |
| 荧光融合 | `docs/uml/fluorescence_fusion_sequence.puml` | `docs/uml/rendered/fluorescence_fusion_sequence.png` |
| 图像拼接 | `docs/uml/stitching_sequence.puml` | `docs/uml/rendered/stitching_sequence.png` |
| EDF 景深扩展 | `docs/uml/edf_sequence.puml` | `docs/uml/rendered/edf_sequence.png` |

## 4. 活动图

| 图 | 源文件 | PNG |
| --- | --- | --- |
| 标定活动 | `docs/uml/calibration_activity.puml` | `docs/uml/rendered/calibration_activity.png` |
| 测量活动 | `docs/uml/measurement_activity.puml` | `docs/uml/rendered/measurement_activity.png` |
| 荧光融合活动 | `docs/uml/fluorescence_fusion_activity.puml` | `docs/uml/rendered/fluorescence_fusion_activity.png` |
| 拼接活动 | `docs/uml/stitching_activity.puml` | `docs/uml/rendered/stitching_activity.png` |
| EDF 活动 | `docs/uml/edf_activity.puml` | `docs/uml/rendered/edf_activity.png` |

## 5. 状态图

| 图 | 源文件 | PNG |
| --- | --- | --- |
| 观察会话状态 | `docs/uml/session_state.puml` | `docs/uml/rendered/session_state.png` |
| 测量工具状态 | `docs/uml/measurement_tool_state.puml` | `docs/uml/rendered/measurement_tool_state.png` |
| 后台处理任务状态 | `docs/uml/processing_job_state.puml` | `docs/uml/rendered/processing_job_state.png` |

## 6. 重新渲染

重新生成 PNG：

```powershell
.\tools\render_uml.ps1 -Mode png -PlantUmlJar tools\plantuml.jar
```

