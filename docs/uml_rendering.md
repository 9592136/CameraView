# UML 渲染与复核说明

本文档说明如何把 `docs/uml/*.puml` 渲染为图片，并区分源码级检查和渲染级检查。当前项目已生成 PNG 到 `docs/uml/rendered/`。

## 1. 前置环境

需要具备以下工具之一：

| 方案 | 要求 | 说明 |
| --- | --- | --- |
| PlantUML 命令行 | `plantuml` 可执行命令 | 最简单，推荐 |
| PlantUML Jar | Java + `plantuml.jar` | 适合没有安装命令行工具的机器 |

可在 PowerShell 中检查：

```powershell
Get-Command plantuml
Get-Command java
```

## 2. 推荐输出目录

建议把渲染结果输出到：

```text
docs/uml/rendered/
```

该目录只保存生成图片，不作为 UML 源文件的权威来源。权威图源仍然是 `docs/uml/*.puml`。

## 3. 推荐脚本

项目提供了一个本地 PowerShell 脚本：

```text
tools/render_uml.ps1
```

源码级检查：

```powershell
.\tools\render_uml.ps1 -Mode check
```

渲染 PNG：

```powershell
.\tools\render_uml.ps1 -Mode png
```

渲染 SVG：

```powershell
.\tools\render_uml.ps1 -Mode svg
```

如果使用 `plantuml.jar`：

```powershell
.\tools\render_uml.ps1 -Mode png -PlantUmlJar .\plantuml.jar
```

## 4. 使用 PlantUML 命令行渲染

在项目根目录执行：

```powershell
New-Item -ItemType Directory -Force docs\uml\rendered
plantuml -tpng -o rendered docs\uml\*.puml
```

如果需要 SVG：

```powershell
New-Item -ItemType Directory -Force docs\uml\rendered
plantuml -tsvg -o rendered docs\uml\*.puml
```

## 5. 使用 PlantUML Jar 渲染

如果使用 `plantuml.jar`：

```powershell
New-Item -ItemType Directory -Force docs\uml\rendered
java -jar .\plantuml.jar -tpng -o rendered docs\uml\*.puml
```

## 6. 源码级检查

源码级检查用于确认每个 `.puml` 文件都有 PlantUML 起止标记：

```powershell
$files = Get-ChildItem docs\uml\*.puml
$files | Sort-Object Name | ForEach-Object {
  $text = Get-Content -Raw -Encoding UTF8 $_.FullName
  [PSCustomObject]@{
    File = $_.Name
    Start = ([regex]::Matches($text, '@startuml')).Count
    End = ([regex]::Matches($text, '@enduml')).Count
  }
}
"TOTAL=$($files.Count)"
```

当前源码级检查结果：

| 检查项 | 当前结果 |
| --- | --- |
| PlantUML 图源数量 | 21 个 |
| 每个图源包含 `@startuml` | 通过 |
| 每个图源包含 `@enduml` | 通过 |
| PNG 渲染输出 | 21 个 |

## 7. 渲染级检查

有 PlantUML 环境后，需要检查：

| 检查项 | 标准 |
| --- | --- |
| 渲染命令 | 无错误退出 |
| 图片数量 | 与 `.puml` 图源数量一致 |
| 文件内容 | 图片非空 |
| 中文显示 | 图中文字可读，无乱码 |
| 关系线 | 类图、组件图、包图关系线无明显断裂 |
| 时序图 | 参与者、消息方向和返回关系清晰 |
| 活动图 | 分支、循环和结束节点可读 |
| 状态图 | 状态迁移可读，起止节点清晰 |

## 8. 交付说明

设计评审以 `.puml` 源文件为准。渲染图像用于阅读和评审展示，若图片与源文件不一致，应重新渲染而不是手工改图。
