# CameraView YOLO AI 模块

## 能力范围

Qt 版 CameraView 的 `AI` 页签提供完整的 Ultralytics YOLO 工作流：

- 目标检测：显示类别、置信度和矩形框；
- 图像分类：显示 Top-5 类别及置信度；
- 实例分割：显示类别、置信度和分割多边形；
- 模型管理：导入 `.pt` / `.onnx`、读取类别元数据、设为当前模型、删除托管权重、导出 ONNX；
- 模型训练：选择基础模型和数据集，配置 epoch、图像尺寸、批大小、数据线程、早停和计算设备，查看进度与日志，停止任务；
- 训练产物：训练完成后自动把 `best.pt` 复制到模型库并保存指标。

模型库默认位于当前用户的应用数据目录，不会把大模型写入 Git 仓库。元数据保存在 `registry.json`，权重保存在相邻的 `weights` 目录。

## 无管理员权限安装

推荐安装 CPU 环境：

```powershell
.\tools\setup_yolo.ps1 -CpuOnly
```

脚本只使用当前用户目录：必要时通过 `winget --scope user` 安装 Python 3.12，并在仓库内创建 `.venv-yolo`。它固定使用已验证的 PyTorch 2.5.1 CPU 版，然后安装 Ultralytics、ONNX 和 ONNX Runtime。CameraView 从构建目录启动时会自动发现这个虚拟环境。

如果已有 Python 3.10—3.12，可以显式指定：

```powershell
.\tools\setup_yolo.ps1 -PythonExecutable C:\path\to\python.exe -CpuOnly
```

使用自行安装的 CUDA 版 PyTorch 时，可在 AI 页签直接选择对应 Python，并把设备填为 `cuda:0`。点击“检测环境”可查看 Python、Ultralytics、PyTorch 和 CUDA 状态。

## 推理

1. 打开图像或连接相机取得画面。
2. 在 `AI > 推理与模型` 中导入模型；文件名包含 `-cls` 或 `-seg` 时会自动识别任务类型，也可在导入前手动选择。
3. 点击“读取信息”同步模型任务和类别表，设置置信度、IOU、输入尺寸与设备。
4. 点击“识别当前图像”。检测框或分割轮廓会叠加到主视图，结果列表显示类别、置信度和耗时。
5. 双击检测或分割结果列表中的目标，主视图会自动居中并放大到对应检测框或分割轮廓；分类结果和耗时行不会改变视图。

图像导出会同时绘制测量标记和 AI 结果。

## 创建和标注训练数据集

`AI > 数据集` 提供与主图画布联动的数据集编辑器：

1. 点击“新建”，选择保存位置、目标检测/实例分割/图像分类任务，并填写第一个类别；也可以打开已有 CameraView 数据集继续编辑。
2. 管理类别并为当前样本选择训练集、验证集或测试集。类别名称会直接成为 YOLO 类别名称；分类任务中已包含样本的类别需先清空样本才能重命名。
3. 检测任务点击“绘制检测框”，在图像上点击两个对角点；分割任务点击“绘制分割多边形”，依次点击顶点并双击结束；分类任务选择类别后点击“设置当前图像分类”。
4. 标注列表支持双击定位、删除选中项和清空。检测/分割任务允许保存没有目标的背景负样本，分类任务必须选择且只能选择一个类别。
5. 点击“保存当前图像到数据集”。双击样本列表可重新打开图像、修改标注或划分后再次保存；删除样本会同时删除项目目录中由 CameraView 管理的图像与标签。
6. 点击“用于训练”，数据集路径和任务会自动带入训练页。

每个项目包含可继续编辑的 `dataset.json`。检测和分割项目同步生成 `images/{train,val,test}`、`labels/{train,val,test}` 与 `dataset.yaml`；分类项目生成 `{train,val,test}/类别/图片`。所有标签坐标写入前都会裁剪到图像范围并验证，清单使用相对路径且拒绝目录穿越路径。

## 数据集与训练

检测和分割使用 Ultralytics 数据集 YAML，例如：

```yaml
path: D:/datasets/cells
train: images/train
val: images/val
names:
  0: cell
  1: debris
```

标签采用标准 YOLO 格式。检测标签为 `class x_center y_center width height`；分割标签为 `class x1 y1 x2 y2 ...`，坐标均归一化到 0—1。

分类数据集选择目录，目录结构为：

```text
dataset/
  train/
    class_a/
    class_b/
  val/
    class_a/
    class_b/
```

在 `AI > 训练` 中选择基础 `.pt` 模型、数据集和输出目录。任务类型跟随当前模型；Windows 默认数据线程为 0，确保停止训练时不会遗留数据加载子进程。训练日志最多保留 2000 行。

## 进程边界

Qt 应用不直接链接 Python。`YoloProcessController` 以独立进程调用随程序部署的 `yolo/yolo_backend.py`，双方通过逐行 JSON 事件通信。这一边界使相机采集和界面线程不受模型加载、推理或训练阻塞，并允许用户替换虚拟环境而无需重新编译 Qt 应用。

后端支持以下命令，可用于诊断：

```powershell
.\.venv-yolo\Scripts\python.exe .\tools\yolo_backend.py probe
.\.venv-yolo\Scripts\python.exe .\tools\yolo_backend.py inspect --model model.pt --task detect
.\.venv-yolo\Scripts\python.exe .\tools\yolo_backend.py infer --model model.pt --source image.jpg --task detect
```

## 验证

构建会把后端脚本部署到可执行文件旁的 `yolo` 目录，并新增 `CameraViewYoloRegistryTests`，覆盖模型导入、托管复制、元数据持久化、当前模型和删除。完整测试：

```powershell
.\tools\build_qt.ps1
```

本地端到端验收使用 YOLO11n 系列真实权重，已验证检测、分类、实例分割的 JSON 结果，以及一轮检测训练的进度事件、`best.pt` 和 `last.pt` 产物。

完整 PanNuke 细胞数据训练、独立测试结果、许可限制与复现步骤见 [`pannuke_training.md`](pannuke_training.md)。
