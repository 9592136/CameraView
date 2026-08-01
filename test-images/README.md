# YOLO 测试图片

`coco80/` 中的三张合成图片用于测试通用 `yolo11n.pt`（COCO 80 类）检测，分别覆盖街景、室内物品和宠物场景。`results/yolo11n/` 是对应的实际推理结果，可用于核对 CameraView 叠加显示是否一致。

这些图片不适合测试 PanNuke 细胞模型。PanNuke 模型只面向 40× H&E 病理图中的五类细胞核；相关数据、指标与复现步骤见 [`../docs/pannuke_training.md`](../docs/pannuke_training.md)。
