#!/usr/bin/env python3
"""CameraView YOLO backend.

The Qt application invokes this process for dependency probing, Ultralytics
YOLO inference, training, model inspection, and export. Every machine-readable
message is emitted as a single JSON line so ordinary Ultralytics log output can
coexist on stdout.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any


def emit(event: str, **payload: Any) -> None:
    print(json.dumps({"event": event, **payload}, ensure_ascii=False), flush=True)


def fail(message: str, *, detail: str = "", code: int = 2) -> int:
    emit("error", message=message, detail=detail)
    return code


def load_runtime():
    try:
        import ultralytics
        from ultralytics import YOLO
    except Exception as exc:  # pragma: no cover - depends on local runtime
        raise RuntimeError(
            "Ultralytics is unavailable. Run tools/setup_yolo.ps1 first."
        ) from exc
    return ultralytics, YOLO


def command_probe(_: argparse.Namespace) -> int:
    info: dict[str, Any] = {
        "python": sys.version.split()[0],
        "executable": sys.executable,
        "ultralytics": False,
        "torch": False,
        "cuda": False,
        "devices": ["cpu"],
    }
    try:
        import torch

        info["torch"] = True
        info["torch_version"] = torch.__version__
        info["cuda"] = bool(torch.cuda.is_available())
        if torch.cuda.is_available():
            info["devices"] += [
                f"cuda:{index} {torch.cuda.get_device_name(index)}"
                for index in range(torch.cuda.device_count())
            ]
    except Exception as exc:
        info["torch_error"] = str(exc)
    try:
        import ultralytics

        info["ultralytics"] = True
        info["ultralytics_version"] = ultralytics.__version__
    except Exception as exc:
        info["ultralytics_error"] = str(exc)
    emit("probe", **info)
    return 0


def model_metadata(model: Any, model_path: str) -> dict[str, Any]:
    names = getattr(model, "names", {}) or {}
    if isinstance(names, list):
        names = {str(index): name for index, name in enumerate(names)}
    else:
        names = {str(key): value for key, value in names.items()}
    return {
        "path": str(Path(model_path).resolve()),
        "task": str(getattr(model, "task", "detect")),
        "names": names,
        "class_count": len(names),
    }


def command_inspect(args: argparse.Namespace) -> int:
    try:
        _, YOLO = load_runtime()
        model = YOLO(args.model, task=args.task or None)
        emit("model", **model_metadata(model, args.model))
        return 0
    except Exception as exc:
        return fail("Failed to inspect YOLO model.", detail=str(exc))


def normalized_box(values: list[float], width: int, height: int) -> list[float]:
    x1, y1, x2, y2 = values
    return [x1 / width, y1 / height, x2 / width, y2 / height]


def command_infer(args: argparse.Namespace) -> int:
    try:
        _, YOLO = load_runtime()
        model = YOLO(args.model, task=args.task or None)
        started = time.perf_counter()
        results = model.predict(
            source=args.source,
            conf=args.conf,
            iou=args.iou,
            imgsz=args.imgsz,
            device=args.device,
            max_det=args.max_det,
            verbose=False,
            save=False,
        )
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if not results:
            emit("result", task=model.task, elapsed_ms=elapsed_ms, predictions=[])
            return 0

        result = results[0]
        height, width = result.orig_shape
        predictions: list[dict[str, Any]] = []
        names = result.names or model.names or {}

        if model.task == "classify" and result.probs is not None:
            top_ids = result.probs.top5
            top_conf = result.probs.top5conf.tolist()
            predictions = [
                {
                    "class_id": int(class_id),
                    "label": str(names[int(class_id)]),
                    "confidence": float(confidence),
                }
                for class_id, confidence in zip(top_ids, top_conf)
            ]
        elif result.boxes is not None:
            boxes = result.boxes.xyxy.cpu().tolist()
            classes = result.boxes.cls.cpu().tolist()
            confidences = result.boxes.conf.cpu().tolist()
            polygons = []
            if model.task == "segment" and result.masks is not None:
                polygons = result.masks.xy
            for index, (box, class_id, confidence) in enumerate(
                zip(boxes, classes, confidences)
            ):
                prediction: dict[str, Any] = {
                    "class_id": int(class_id),
                    "label": str(names[int(class_id)]),
                    "confidence": float(confidence),
                    "box": normalized_box(box, width, height),
                }
                if index < len(polygons):
                    prediction["polygon"] = [
                        [float(point[0]) / width, float(point[1]) / height]
                        for point in polygons[index].tolist()
                    ]
                predictions.append(prediction)

        emit(
            "result",
            task=str(model.task),
            elapsed_ms=elapsed_ms,
            width=width,
            height=height,
            predictions=predictions,
            model=model_metadata(model, args.model),
        )
        return 0
    except Exception as exc:
        return fail("YOLO inference failed.", detail=str(exc))


def metric_value(metrics: dict[str, Any], *names: str) -> float | None:
    for name in names:
        value = metrics.get(name)
        if value is not None:
            try:
                return float(value)
            except (TypeError, ValueError):
                pass
    return None


def command_train(args: argparse.Namespace) -> int:
    try:
        _, YOLO = load_runtime()
        model = YOLO(args.model, task=args.task or None)
        emit(
            "train_start",
            task=str(args.task or model.task),
            model=args.model,
            data=args.data,
            epochs=args.epochs,
        )

        def on_epoch_end(trainer: Any) -> None:
            metrics = dict(getattr(trainer, "metrics", {}) or {})
            loss = None
            for attribute in ("tloss", "loss_items"):
                loss_items = getattr(trainer, attribute, None)
                if loss_items is None:
                    continue
                try:
                    loss = float(loss_items.detach().sum().cpu())
                except Exception:
                    try:
                        loss = float(sum(loss_items))
                    except Exception:
                        pass
                if loss is not None:
                    break
            emit(
                "train_progress",
                epoch=int(getattr(trainer, "epoch", 0)) + 1,
                epochs=args.epochs,
                progress=(int(getattr(trainer, "epoch", 0)) + 1) / args.epochs,
                loss=loss,
                metrics={key: float(value) for key, value in metrics.items() if isinstance(value, (int, float))},
            )

        model.add_callback("on_train_epoch_end", on_epoch_end)
        result = model.train(
            data=args.data,
            epochs=args.epochs,
            imgsz=args.imgsz,
            batch=args.batch,
            device=args.device,
            workers=args.workers,
            project=str(Path(args.project).resolve()),
            name=args.name,
            exist_ok=args.exist_ok,
            patience=args.patience,
            seed=args.seed,
            deterministic=True,
            pretrained=True,
            verbose=True,
        )
        save_dir = Path(str(getattr(result, "save_dir", "")))
        best = save_dir / "weights" / "best.pt"
        last = save_dir / "weights" / "last.pt"
        metrics = dict(getattr(result, "results_dict", {}) or {})
        emit(
            "train_complete",
            task=str(args.task or model.task),
            save_dir=str(save_dir.resolve()) if save_dir else "",
            best=str(best.resolve()) if best.exists() else "",
            last=str(last.resolve()) if last.exists() else "",
            metrics={key: float(value) for key, value in metrics.items() if isinstance(value, (int, float))},
            score=metric_value(metrics, "metrics/mAP50-95(B)", "metrics/mAP50-95(M)", "metrics/accuracy_top1"),
        )
        return 0
    except KeyboardInterrupt:
        return fail("Training cancelled by user.", code=130)
    except Exception as exc:
        return fail("YOLO training failed.", detail=str(exc))


def command_validate(args: argparse.Namespace) -> int:
    try:
        _, YOLO = load_runtime()
        model = YOLO(args.model, task=args.task or None)
        emit(
            "validation_start",
            task=str(args.task or model.task),
            model=args.model,
            data=args.data,
            split=args.split,
        )
        result = model.val(
            data=args.data,
            split=args.split,
            imgsz=args.imgsz,
            batch=args.batch,
            device=args.device,
            workers=args.workers,
            project=str(Path(args.project).resolve()),
            name=args.name,
            exist_ok=args.exist_ok,
            plots=True,
            verbose=True,
        )
        metrics = dict(getattr(result, "results_dict", {}) or {})
        save_dir = Path(str(getattr(result, "save_dir", "")))
        names = getattr(result, "names", {}) or getattr(model, "names", {}) or {}
        if isinstance(names, list):
            names = {index: name for index, name in enumerate(names)}
        per_class: dict[str, Any] = {}
        for metric_name, attribute in (("boxes", "box"), ("masks", "seg")):
            metric = getattr(result, attribute, None)
            if metric is None or not hasattr(metric, "class_result"):
                continue
            values: dict[str, Any] = {}
            for class_id in range(len(names)):
                try:
                    precision, recall, map50, map50_95 = metric.class_result(class_id)
                except (IndexError, TypeError, ValueError):
                    continue
                values[str(names.get(class_id, class_id))] = {
                    "precision": float(precision),
                    "recall": float(recall),
                    "mAP50": float(map50),
                    "mAP50_95": float(map50_95),
                }
            if values:
                per_class[metric_name] = values

        confusion_payload: dict[str, Any] = {}
        confusion = getattr(result, "confusion_matrix", None)
        matrix = getattr(confusion, "matrix", None)
        if matrix is not None:
            matrix_values = matrix.tolist()
            confusion_payload["matrix_rows_predicted_columns_actual"] = matrix_values
            if str(args.task or model.task) == "classify":
                class_metrics: dict[str, Any] = {}
                for class_id in range(len(names)):
                    true_positive = float(matrix[class_id, class_id])
                    predicted = float(matrix[class_id, :].sum())
                    actual = float(matrix[:, class_id].sum())
                    precision = true_positive / predicted if predicted else 0.0
                    recall = true_positive / actual if actual else 0.0
                    f1 = 2 * precision * recall / (precision + recall) if precision + recall else 0.0
                    class_metrics[str(names.get(class_id, class_id))] = {
                        "precision": precision,
                        "recall": recall,
                        "f1": f1,
                        "support": int(actual),
                    }
                per_class["classification"] = class_metrics

        payload = {
            "task": str(args.task or model.task),
            "split": args.split,
            "save_dir": str(save_dir.resolve()) if save_dir else "",
            "metrics": {
                key: float(value)
                for key, value in metrics.items()
                if isinstance(value, (int, float))
            },
            "speed": {
                key: float(value)
                for key, value in dict(getattr(result, "speed", {}) or {}).items()
                if isinstance(value, (int, float))
            },
            "per_class": per_class,
            "confusion_matrix": confusion_payload,
        }
        if args.report:
            report_path = Path(args.report)
            report_path.parent.mkdir(parents=True, exist_ok=True)
            report_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
        emit("validation_complete", **payload)
        return 0
    except KeyboardInterrupt:
        return fail("Validation cancelled by user.", code=130)
    except Exception as exc:
        return fail("YOLO validation failed.", detail=str(exc))


def command_export(args: argparse.Namespace) -> int:
    try:
        _, YOLO = load_runtime()
        model = YOLO(args.model, task=args.task or None)
        output = model.export(
            format=args.format,
            imgsz=args.imgsz,
            device=args.device,
            simplify=False,
        )
        emit("export_complete", source=args.model, format=args.format, output=str(output))
        return 0
    except Exception as exc:
        return fail("YOLO model export failed.", detail=str(exc))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="CameraView Ultralytics YOLO backend")
    subparsers = parser.add_subparsers(dest="command", required=True)

    probe = subparsers.add_parser("probe", help="report runtime availability")
    probe.set_defaults(func=command_probe)

    inspect = subparsers.add_parser("inspect", help="inspect a model")
    inspect.add_argument("--model", required=True)
    inspect.add_argument("--task", choices=["detect", "classify", "segment"])
    inspect.set_defaults(func=command_inspect)

    infer = subparsers.add_parser("infer", help="run inference on one image")
    infer.add_argument("--model", required=True)
    infer.add_argument("--source", required=True)
    infer.add_argument("--task", choices=["detect", "classify", "segment"])
    infer.add_argument("--conf", type=float, default=0.25)
    infer.add_argument("--iou", type=float, default=0.45)
    infer.add_argument("--imgsz", type=int, default=640)
    infer.add_argument("--max-det", type=int, default=300)
    infer.add_argument("--device", default="cpu")
    infer.set_defaults(func=command_infer)

    train = subparsers.add_parser("train", help="train or fine-tune a YOLO model")
    train.add_argument("--model", required=True)
    train.add_argument("--data", required=True)
    train.add_argument("--task", choices=["detect", "classify", "segment"])
    train.add_argument("--epochs", type=int, default=50)
    train.add_argument("--imgsz", type=int, default=640)
    train.add_argument("--batch", type=int, default=8)
    train.add_argument("--workers", type=int, default=4)
    train.add_argument("--device", default="cpu")
    train.add_argument("--project", required=True)
    train.add_argument("--name", default="train")
    train.add_argument("--patience", type=int, default=20)
    train.add_argument("--seed", type=int, default=42)
    train.add_argument("--exist-ok", action="store_true")
    train.set_defaults(func=command_train)

    validate = subparsers.add_parser("validate", help="evaluate a model on a dataset split")
    validate.add_argument("--model", required=True)
    validate.add_argument("--data", required=True)
    validate.add_argument("--task", choices=["detect", "classify", "segment"])
    validate.add_argument("--split", choices=["train", "val", "test"], default="test")
    validate.add_argument("--imgsz", type=int, default=640)
    validate.add_argument("--batch", type=int, default=8)
    validate.add_argument("--workers", type=int, default=4)
    validate.add_argument("--device", default="cpu")
    validate.add_argument("--project", required=True)
    validate.add_argument("--name", default="validation")
    validate.add_argument("--report")
    validate.add_argument("--exist-ok", action="store_true")
    validate.set_defaults(func=command_validate)

    export = subparsers.add_parser("export", help="export a trained model")
    export.add_argument("--model", required=True)
    export.add_argument("--task", choices=["detect", "classify", "segment"])
    export.add_argument("--format", default="onnx")
    export.add_argument("--imgsz", type=int, default=640)
    export.add_argument("--device", default="cpu")
    export.set_defaults(func=command_export)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
