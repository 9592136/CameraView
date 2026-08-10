# PanNuke YOLO support models

These files are the final `best.pt` weights from the reproducible PanNuke
baseline described in [`docs/pannuke_training.md`](../../../docs/pannuke_training.md).
They are checked in as optional support data so CameraView can be evaluated
without repeating the CPU training runs.

| File | Task | Training run | Independent fold3 result | Bytes | SHA-256 |
|---|---|---|---|---:|---|
| `pannuke-detect-yolo11n-e5.pt` | Object detection | YOLO11n, 5 epochs | mAP50 0.4565; mAP50-95 0.2744 | 5,422,547 | `c94ce51e7fab0691be40f21d90c2e1f751ba5353d2e99aafe62adda0279e13e3` |
| `pannuke-segment-yolo11n-e5.pt` | Instance segmentation | YOLO11n-seg, 5 epochs | mask mAP50 0.4348; mask mAP50-95 0.2061 | 5,952,797 | `52dc93709039c145b0e04c9184406abfa8000df550a5c0a46cf7297587d365cc` |
| `pannuke-classify-yolo11n-e10.pt` | Cell classification | YOLO11n-cls, 10 epochs | top-1 0.6617; top-5 1.0000 | 3,193,275 | `6a581e71bef19ea45e586396b88ba6f72cdaf73efd0d0ef1807d39eb21772910` |

## Use in CameraView

Open the AI workspace, choose the matching task, and import the corresponding
`.pt` file. The segmentation model is the most complete option for locating
and outlining nuclei; the classification model expects single-cell crops.

## Limitations and license

These are lightweight research baselines, not clinical-grade models. In the
independent fold3 evaluation, the detection and segmentation models had zero
recall for dead cells because that class is severely underrepresented.

PanNuke data is licensed under CC BY-NC-SA 4.0. Treat these derived weights as
research/non-commercial support data and verify dataset licensing, attribution,
medical-device, privacy, and deployment requirements before redistribution or
use. Ultralytics and YOLO licensing requirements also apply.
