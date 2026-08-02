# PanNuke 细胞检测、分类与实例分割训练

本文记录 CameraView 的首套显微病理细胞模型。数据准备、训练和测试已于 2026-08-02 在完整 PanNuke 数据集上实际执行；这里报告的是从未参与训练或调参的 fold3 测试结果，不是训练集分数。

## 数据集和使用边界

- 数据：PanNuke，7,901 张 256×256、40× H&E 图像块，覆盖 19 种组织。
- 标注：189,744 个细胞核实例，类别为肿瘤、炎症、结缔/软组织、死亡和上皮细胞。
- 划分：fold1 训练（2,656 张）、fold2 验证（2,523 张）、fold3 独立测试（2,722 张）。三个 fold 按原始数据保持，未随机混合。
- 来源：[PanNuke 论文](https://arxiv.org/abs/2003.10778)；[使用的 Parquet 镜像](https://huggingface.co/datasets/MedOtter/PanNuke)。
- 许可：**CC BY-NC-SA 4.0，仅适合研究和非商业用途**。模型由该数据训练，发布、再训练或部署时也必须先确认数据许可与医学合规要求。

完整审计确认三份数据没有重复 sample ID。少量实例（356 个）在类别边界上含有多种像素值，转换时以该实例占多数的像素类别为准，并记录了 2,253 个被消解的少数像素。检测和分割保留全部 189,744 个实例；分类训练集按最少的死亡细胞类平衡为每类 967 张裁剪图。

## 实验配置

| 任务 | 基础模型 | 轮数 | 输入 | 批量 | 训练样本 |
|---|---|---:|---:|---:|---:|
| 检测 | YOLO11n | 5 | 256 | 32 | 2,656 图 / 63,218 核 |
| 实例分割 | YOLO11n-seg | 5 | 256 | 16 | 2,656 图 / 63,218 核 |
| 分类 | YOLO11n-cls | 10 | 128 | 64 | 4,835 个平衡细胞裁剪 |

公共设置为随机种子 42、确定性训练、预训练权重、CPU、Ultralytics 8.4.114、PyTorch 2.5.1+cpu。当前结果是一套可运行的轻量基线，轮数受到 CPU 训练时间限制，不应视作临床级模型。

## fold3 独立测试结果

| 任务 | 主要指标 | 结果 |
|---|---|---:|
| 检测 | Precision / Recall | 0.6615 / 0.4534 |
| 检测 | mAP50 / mAP50-95 | 0.4565 / 0.2744 |
| 实例分割 | Mask Precision / Recall | 0.6512 / 0.4345 |
| 实例分割 | Mask mAP50 / mAP50-95 | 0.4348 / 0.2061 |
| 分类 | Top-1 / Top-5 accuracy | 0.6617 / 1.0000 |

分类各类 F1：结缔组织 0.6260、死亡 0.6733、上皮 0.6558、炎症 0.7122、肿瘤 0.6495。

重要限制：死亡细胞只占总标注的 2,908 / 189,744。检测和分割模型在 fold3 上对这个类别的召回率均为 0，不能可靠识别死亡细胞；分类模型是在单细胞裁剪图上评估，因此其死亡细胞分数不能替代整图检测能力。实际使用应增加死亡细胞数据、采用类别重采样或损失加权，并延长 GPU 训练后重新验证。

## 产物和完整性

权重位于本地 `runs/pannuke/models/`（大文件按仓库规则不提交 Git）：

| 模型 | 相对路径 | SHA-256 |
|---|---|---|
| 检测 | `detect-yolo11n-e5/weights/best.pt` | `c94ce51e7fab0691be40f21d90c2e1f751ba5353d2e99aafe62adda0279e13e3` |
| 分割 | `segment-yolo11n-e5/weights/best.pt` | `52dc93709039c145b0e04c9184406abfa8000df550a5c0a46cf7297587d365cc` |
| 分类 | `classify-yolo11n-e10-fixed/weights/best.pt` | `6a581e71bef19ea45e586396b88ba6f72cdaf73efd0d0ef1807d39eb21772910` |

详细 JSON、混淆矩阵和预测图分别保存在 `runs/pannuke/validation/` 与 `runs/pannuke/visualizations/`。三个最终权重已导入当前用户的 CameraView 模型库，其中实例分割模型设为当前模型。

## 复现

先创建无需管理员权限的 CPU 环境：

```powershell
.\tools\setup_yolo.ps1 -CpuOnly
```

随后可逐阶段执行，避免意外重复数小时训练：

```powershell
.\tools\run_pannuke.ps1 -Stage download
.\tools\run_pannuke.ps1 -Stage prepare
.\tools\run_pannuke.ps1 -Stage train
.\tools\run_pannuke.ps1 -Stage test
.\tools\build_qt.ps1 -BuildDirectory build-qt-scroll-test
.\tools\run_pannuke.ps1 -Stage import -BuildDirectory build-qt-scroll-test
```

`download` 阶段会下载六个 Parquet 分片并核对固定 SHA-256；`prepare` 会重新审计和生成检测、分割、分类数据；`test` 始终使用 fold3。具备 CUDA 环境时可给训练和测试命令增加 `-Device cuda:0`。
