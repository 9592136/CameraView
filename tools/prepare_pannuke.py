#!/usr/bin/env python3
"""Audit and convert PanNuke into YOLO detect/segment/classify datasets.

The source is the MedOtter PanNuke Parquet mirror.  The script verifies the
official LFS object hashes, audits every decoded sample, preserves the three
published folds, and derives three task-specific datasets from the same nuclei
instances.
"""

from __future__ import annotations

import argparse
import hashlib
import heapq
import json
import os
import shutil
import sys
import time
from collections import Counter
from dataclasses import dataclass, field
from io import BytesIO
from pathlib import Path
from typing import Any, Iterator

import cv2
import numpy as np
import polars as pl
from PIL import Image


CLASS_NAMES = (
    "neoplastic",
    "inflammatory",
    "connective",
    "dead",
    "epithelial",
)
FOLD_TO_SPLIT = {1: "train", 2: "val", 3: "test"}

# SHA-256 values are the Git LFS object IDs exposed by the dataset repository.
SHARDS: dict[str, tuple[int, str]] = {
    "fold1-00000-of-00002.parquet": (
        148_730_239,
        "033166d98dbc1fae4aa8409d1c8ca9d7c160b73137d7032a9806d601f17f7389",
    ),
    "fold1-00001-of-00002.parquet": (
        157_996_238,
        "d1eca2058b64a2e5e890b385ff1ba349919a9a116a12bbd2133912dc446c3611",
    ),
    "fold2-00000-of-00002.parquet": (
        143_155_989,
        "f9b38bdc3ca93f6c14b5352d6a6e980eca65aa2b31d604e26e41a354e3274972",
    ),
    "fold2-00001-of-00002.parquet": (
        145_810_514,
        "6351f7e1bae80c6f705900737d02b5c9f56d2d1bc3b44d1b0567a01d6f614355",
    ),
    "fold3-00000-of-00002.parquet": (
        156_082_085,
        "7600caf6445f4e20cea55293b395cbc356f91a1eb19afa6cce2bcbb1422e411c",
    ),
    "fold3-00001-of-00002.parquet": (
        160_186_121,
        "2da2cfc054a341f3310c310892480c414bcce82a1560a68339147c7f38caae73",
    ),
}


@dataclass
class Audit:
    images: Counter[str] = field(default_factory=Counter)
    nuclei: dict[str, Counter[str]] = field(
        default_factory=lambda: {split: Counter() for split in FOLD_TO_SPLIT.values()}
    )
    tissues: dict[str, Counter[str]] = field(
        default_factory=lambda: {split: Counter() for split in FOLD_TO_SPLIT.values()}
    )
    issues: Counter[str] = field(default_factory=Counter)
    sample_ids: set[str] = field(default_factory=set)

    def as_dict(self) -> dict[str, Any]:
        nuclei = {split: dict(counter) for split, counter in self.nuclei.items()}
        return {
            "image_count": sum(self.images.values()),
            "images_by_split": dict(self.images),
            "nucleus_count": sum(sum(c.values()) for c in self.nuclei.values()),
            "nuclei_by_split_and_class": nuclei,
            "nuclei_by_class": {
                name: sum(c[name] for c in self.nuclei.values()) for name in CLASS_NAMES
            },
            "tissues_by_split": {
                split: dict(counter) for split, counter in self.tissues.items()
            },
            "issues": dict(self.issues),
            "unique_sample_ids": len(self.sample_ids),
        }


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_shards(source: Path) -> list[Path]:
    verified: list[Path] = []
    for name, (expected_size, expected_hash) in SHARDS.items():
        path = source / name
        if not path.is_file():
            raise FileNotFoundError(f"Missing PanNuke shard: {path}")
        actual_size = path.stat().st_size
        if actual_size != expected_size:
            raise ValueError(
                f"Size mismatch for {name}: {actual_size} != {expected_size}"
            )
        actual_hash = sha256(path)
        if actual_hash.lower() != expected_hash:
            raise ValueError(f"SHA-256 mismatch for {name}: {actual_hash}")
        print(f"verified {name} ({actual_size:,} bytes)", flush=True)
        verified.append(path)
    return verified


def iter_records(shards: list[Path]) -> Iterator[dict[str, Any]]:
    for shard in shards:
        frame = pl.read_parquet(shard)
        expected = {"image", "inst_map", "type_map", "tissue", "tissue_name", "fold", "sample_id"}
        missing = expected.difference(frame.columns)
        if missing:
            raise ValueError(f"{shard.name} is missing columns: {sorted(missing)}")
        yield from frame.iter_rows(named=True)


def decode(field: dict[str, Any]) -> np.ndarray:
    return np.asarray(Image.open(BytesIO(field["bytes"])))


def instance_geometry(
    inst_map: np.ndarray, type_map: np.ndarray
) -> tuple[
    np.ndarray,
    np.ndarray,
    np.ndarray,
    np.ndarray,
    np.ndarray,
    np.ndarray,
    int,
    int,
]:
    foreground = inst_map > 0
    if not np.any(foreground):
        empty = np.empty(0, dtype=np.int32)
        return empty, empty, empty, empty, empty, empty, 0, 0

    ys, xs = np.nonzero(foreground)
    pixel_ids = inst_map[ys, xs].astype(np.int32, copy=False)
    max_id = int(pixel_ids.max())
    ids = np.unique(pixel_ids)

    pixel_types = type_map[ys, xs]
    pair_base = len(CLASS_NAMES) + 1
    pair_codes, pair_counts = np.unique(
        pixel_ids * pair_base + pixel_types.astype(np.int32), return_counts=True
    )
    classes_by_id = np.zeros(max_id + 1, dtype=np.uint8)
    best_counts = np.zeros(max_id + 1, dtype=np.int32)
    total_counts = np.zeros(max_id + 1, dtype=np.int32)
    type_counts = np.zeros(max_id + 1, dtype=np.uint8)
    for code, count in zip(pair_codes.tolist(), pair_counts.tolist()):
        instance_id, class_value = divmod(code, pair_base)
        if instance_id == 0:
            continue
        type_counts[instance_id] += 1
        total_counts[instance_id] += count
        if count > best_counts[instance_id]:
            best_counts[instance_id] = count
            classes_by_id[instance_id] = class_value
    conflicting = type_counts[ids] > 1
    conflict_instances = int(np.count_nonzero(conflicting))
    conflict_pixels = int(
        np.sum(total_counts[ids][conflicting] - best_counts[ids][conflicting])
    )

    min_x = np.full(max_id + 1, inst_map.shape[1], dtype=np.int32)
    min_y = np.full(max_id + 1, inst_map.shape[0], dtype=np.int32)
    max_x = np.full(max_id + 1, -1, dtype=np.int32)
    max_y = np.full(max_id + 1, -1, dtype=np.int32)
    np.minimum.at(min_x, pixel_ids, xs)
    np.minimum.at(min_y, pixel_ids, ys)
    np.maximum.at(max_x, pixel_ids, xs)
    np.maximum.at(max_y, pixel_ids, ys)
    return (
        ids,
        classes_by_id[ids],
        min_x[ids],
        min_y[ids],
        max_x[ids],
        max_y[ids],
        conflict_instances,
        conflict_pixels,
    )


def validate_record(record: dict[str, Any], audit: Audit) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    sample_id = str(record["sample_id"])
    fold = int(record["fold"])
    if fold not in FOLD_TO_SPLIT:
        raise ValueError(f"Unexpected fold {fold} in {sample_id}")
    split = FOLD_TO_SPLIT[fold]
    if sample_id in audit.sample_ids:
        raise ValueError(f"Duplicate sample_id: {sample_id}")
    audit.sample_ids.add(sample_id)

    image = decode(record["image"])
    inst_map = decode(record["inst_map"])
    type_map = decode(record["type_map"])
    if image.shape != (256, 256, 3) or image.dtype != np.uint8:
        raise ValueError(f"Invalid RGB image in {sample_id}: {image.shape} {image.dtype}")
    if inst_map.shape != (256, 256) or inst_map.dtype != np.uint16:
        raise ValueError(f"Invalid instance map in {sample_id}: {inst_map.shape} {inst_map.dtype}")
    if type_map.shape != (256, 256) or type_map.dtype != np.uint8:
        raise ValueError(f"Invalid type map in {sample_id}: {type_map.shape} {type_map.dtype}")
    if int(type_map.max(initial=0)) > len(CLASS_NAMES):
        raise ValueError(f"Out-of-range class value in {sample_id}")
    if np.any((inst_map == 0) != (type_map == 0)):
        raise ValueError(f"Foreground mismatch between maps in {sample_id}")

    ids, classes, *_, conflict_instances, conflict_pixels = instance_geometry(
        inst_map, type_map
    )
    audit.issues["instances_with_multiple_pixel_types"] += conflict_instances
    audit.issues["minority_type_pixels_resolved_by_majority"] += conflict_pixels
    for class_value in classes.tolist():
        if not 1 <= class_value <= len(CLASS_NAMES):
            raise ValueError(f"Invalid nucleus class {class_value} in {sample_id}")
        audit.nuclei[split][CLASS_NAMES[class_value - 1]] += 1
    audit.images[split] += 1
    audit.tissues[split][str(record["tissue_name"])] += 1
    return image, inst_map, type_map


def audit_dataset(shards: list[Path], progress_every: int = 250) -> Audit:
    audit = Audit()
    started = time.perf_counter()
    for index, record in enumerate(iter_records(shards), 1):
        validate_record(record, audit)
        if index % progress_every == 0:
            elapsed = time.perf_counter() - started
            print(f"audited {index:,} images in {elapsed:.1f}s", flush=True)
    return audit


def stable_score(key: str) -> int:
    return int.from_bytes(hashlib.blake2b(key.encode("utf-8"), digest_size=8).digest())


def select_classification_instances(
    shards: list[Path], caps: dict[str, int]
) -> dict[str, set[str]]:
    heaps: dict[tuple[str, int], list[tuple[int, str]]] = {
        (split, class_index): []
        for split in FOLD_TO_SPLIT.values()
        for class_index in range(len(CLASS_NAMES))
    }
    for index, record in enumerate(iter_records(shards), 1):
        split = FOLD_TO_SPLIT[int(record["fold"])]
        inst_map = decode(record["inst_map"])
        type_map = decode(record["type_map"])
        ids, classes, *_ = instance_geometry(inst_map, type_map)
        for instance_id, class_value in zip(ids.tolist(), classes.tolist()):
            class_index = class_value - 1
            key = f"{record['sample_id']}:{instance_id}"
            score = stable_score(key)
            heap = heaps[(split, class_index)]
            cap = caps[split]
            item = (-score, key)
            if len(heap) < cap:
                heapq.heappush(heap, item)
            elif score < -heap[0][0]:
                heapq.heapreplace(heap, item)
        if index % 500 == 0:
            print(f"selected candidates from {index:,} images", flush=True)

    selected = {split: set() for split in FOLD_TO_SPLIT.values()}
    for (split, _), heap in heaps.items():
        selected[split].update(key for _, key in heap)
    return selected


def write_yaml(root: Path, task: str) -> Path:
    task_root = (root / task).resolve().as_posix()
    path = root / task / "dataset.yaml"
    names = "\n".join(f"  {index}: {name}" for index, name in enumerate(CLASS_NAMES))
    path.write_text(
        f"path: {task_root}\ntrain: images/train\nval: images/val\ntest: images/test\nnames:\n{names}\n",
        encoding="utf-8",
    )
    return path


def ensure_layout(output: Path) -> None:
    for task in ("detect", "segment"):
        for split in FOLD_TO_SPLIT.values():
            (output / task / "images" / split).mkdir(parents=True, exist_ok=True)
            (output / task / "labels" / split).mkdir(parents=True, exist_ok=True)
    for split in FOLD_TO_SPLIT.values():
        for name in CLASS_NAMES:
            (output / "classify" / split / name).mkdir(parents=True, exist_ok=True)


def link_or_copy(source: Path, destination: Path) -> None:
    try:
        os.link(source, destination)
    except OSError:
        shutil.copy2(source, destination)


def square_crop(
    image: np.ndarray, min_x: int, min_y: int, max_x: int, max_y: int
) -> Image.Image:
    width = max_x - min_x + 1
    height = max_y - min_y + 1
    side = max(width, height)
    padding = max(3, int(round(side * 0.25)))
    side = max(12, side + 2 * padding)
    center_x = (min_x + max_x) / 2.0
    center_y = (min_y + max_y) / 2.0
    x1 = max(0, int(round(center_x - side / 2)))
    y1 = max(0, int(round(center_y - side / 2)))
    x2 = min(image.shape[1], x1 + side)
    y2 = min(image.shape[0], y1 + side)
    x1 = max(0, x2 - side)
    y1 = max(0, y2 - side)
    return Image.fromarray(image[y1:y2, x1:x2], mode="RGB")


def convert_dataset(
    shards: list[Path], output: Path, selected: dict[str, set[str]]
) -> dict[str, Any]:
    ensure_layout(output)
    report: dict[str, Any] = {
        "images": Counter(),
        "detect_instances": Counter(),
        "segment_instances": Counter(),
        "classification_crops": {
            split: Counter() for split in FOLD_TO_SPLIT.values()
        },
        "segmentation_tiny_box_fallbacks": 0,
        "segmentation_disconnected_instances": 0,
    }
    started = time.perf_counter()

    for index, record in enumerate(iter_records(shards), 1):
        split = FOLD_TO_SPLIT[int(record["fold"])]
        sample_id = str(record["sample_id"])
        image = decode(record["image"])
        inst_map = decode(record["inst_map"])
        type_map = decode(record["type_map"])
        (
            ids,
            classes,
            min_xs,
            min_ys,
            max_xs,
            max_ys,
            conflict_instances,
            conflict_pixels,
        ) = instance_geometry(inst_map, type_map)
        report.setdefault("instances_with_multiple_pixel_types", 0)
        report.setdefault("minority_type_pixels_resolved_by_majority", 0)
        report["instances_with_multiple_pixel_types"] += conflict_instances
        report["minority_type_pixels_resolved_by_majority"] += conflict_pixels

        detect_image = output / "detect" / "images" / split / f"{sample_id}.png"
        if not detect_image.exists():
            Image.fromarray(image, mode="RGB").save(detect_image, optimize=True)
        segment_image = output / "segment" / "images" / split / f"{sample_id}.png"
        if not segment_image.exists():
            link_or_copy(detect_image, segment_image)

        detect_lines: list[str] = []
        segment_lines: list[str] = []
        height, width = inst_map.shape
        for instance_id, class_value, min_x, min_y, max_x, max_y in zip(
            ids.tolist(),
            classes.tolist(),
            min_xs.tolist(),
            min_ys.tolist(),
            max_xs.tolist(),
            max_ys.tolist(),
        ):
            class_index = class_value - 1
            box_width = max_x - min_x + 1
            box_height = max_y - min_y + 1
            center_x = (min_x + max_x + 1) / 2.0
            center_y = (min_y + max_y + 1) / 2.0
            detect_lines.append(
                f"{class_index} {center_x / width:.8f} {center_y / height:.8f} "
                f"{box_width / width:.8f} {box_height / height:.8f}"
            )
            report["detect_instances"][split] += 1

            mask = np.asarray(inst_map == instance_id, dtype=np.uint8)
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            if len(contours) > 1:
                report["segmentation_disconnected_instances"] += 1
            if contours:
                if len(contours) > 1:
                    contour = cv2.convexHull(np.vstack(contours)).reshape(-1, 2)
                else:
                    contour = contours[0].reshape(-1, 2)
                if len(contour) < 3:
                    # YOLO polygons need at least three vertices.  Preserve
                    # one- and two-pixel nuclei as their inclusive pixel box.
                    contour = np.asarray(
                        [
                            [min_x, min_y],
                            [min(max_x + 1, width), min_y],
                            [min(max_x + 1, width), min(max_y + 1, height)],
                            [min_x, min(max_y + 1, height)],
                        ],
                        dtype=np.int32,
                    )
                    report["segmentation_tiny_box_fallbacks"] += 1
                points = " ".join(
                    f"{x / width:.8f} {y / height:.8f}" for x, y in contour.tolist()
                )
                segment_lines.append(f"{class_index} {points}")
                report["segment_instances"][split] += 1

            key = f"{sample_id}:{instance_id}"
            if key in selected[split]:
                class_name = CLASS_NAMES[class_index]
                crop = square_crop(image, min_x, min_y, max_x, max_y)
                crop_path = (
                    output
                    / "classify"
                    / split
                    / class_name
                    / f"{sample_id}_n{instance_id:04d}.png"
                )
                needs_write = not crop_path.exists()
                if not needs_write:
                    with Image.open(crop_path) as existing_crop:
                        needs_write = min(existing_crop.size) < 10
                if needs_write:
                    crop.save(crop_path, optimize=True)
                report["classification_crops"][split][class_name] += 1

        (output / "detect" / "labels" / split / f"{sample_id}.txt").write_text(
            "\n".join(detect_lines) + ("\n" if detect_lines else ""), encoding="utf-8"
        )
        (output / "segment" / "labels" / split / f"{sample_id}.txt").write_text(
            "\n".join(segment_lines) + ("\n" if segment_lines else ""), encoding="utf-8"
        )
        report["images"][split] += 1

        if index % 100 == 0:
            elapsed = time.perf_counter() - started
            print(f"converted {index:,} images in {elapsed:.1f}s", flush=True)

    report["images"] = dict(report["images"])
    report["detect_instances"] = dict(report["detect_instances"])
    report["segment_instances"] = dict(report["segment_instances"])
    report["classification_crops"] = {
        split: dict(counter) for split, counter in report["classification_crops"].items()
    }
    report["detect_yaml"] = str(write_yaml(output, "detect").resolve())
    report["segment_yaml"] = str(write_yaml(output, "segment").resolve())
    report["classify_root"] = str((output / "classify").resolve())
    return report


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def validate_converted(output: Path) -> dict[str, Any]:
    result: dict[str, Any] = {"tasks": {}, "classification": {}}
    split_stems: dict[str, set[str]] = {}
    for task in ("detect", "segment"):
        task_counts: dict[str, Any] = {}
        for split in FOLD_TO_SPLIT.values():
            image_dir = output / task / "images" / split
            label_dir = output / task / "labels" / split
            image_paths = sorted(image_dir.glob("*.png"))
            label_paths = sorted(label_dir.glob("*.txt"))
            image_stems = {path.stem for path in image_paths}
            label_stems = {path.stem for path in label_paths}
            if image_stems != label_stems:
                raise ValueError(
                    f"{task}/{split} image-label mismatch: "
                    f"{len(image_stems)} images, {len(label_stems)} labels"
                )
            split_stems.setdefault(split, image_stems)
            if split_stems[split] != image_stems:
                raise ValueError(f"Detect/segment sample mismatch in {split}")

            instance_count = 0
            for label_path in label_paths:
                for line_number, line in enumerate(
                    label_path.read_text(encoding="utf-8").splitlines(), 1
                ):
                    parts = line.split()
                    minimum = 5 if task == "detect" else 7
                    if len(parts) < minimum or (task == "detect" and len(parts) != 5):
                        raise ValueError(f"Malformed {task} label {label_path}:{line_number}")
                    if task == "segment" and len(parts) % 2 != 1:
                        raise ValueError(f"Odd polygon coordinate count {label_path}:{line_number}")
                    class_id = int(parts[0])
                    coordinates = np.asarray(parts[1:], dtype=np.float64)
                    if not 0 <= class_id < len(CLASS_NAMES):
                        raise ValueError(f"Invalid class ID in {label_path}:{line_number}")
                    if not np.all(np.isfinite(coordinates)) or np.any(coordinates < 0) or np.any(coordinates > 1):
                        raise ValueError(f"Invalid coordinates in {label_path}:{line_number}")
                    if task == "detect" and (coordinates[2] <= 0 or coordinates[3] <= 0):
                        raise ValueError(f"Non-positive box in {label_path}:{line_number}")
                    instance_count += 1
            task_counts[split] = {
                "images": len(image_paths),
                "labels": len(label_paths),
                "instances": instance_count,
            }
        result["tasks"][task] = task_counts

    splits = list(FOLD_TO_SPLIT.values())
    for index, left in enumerate(splits):
        for right in splits[index + 1 :]:
            overlap = split_stems[left].intersection(split_stems[right])
            if overlap:
                raise ValueError(f"Sample leakage between {left} and {right}: {len(overlap)}")

    expected_fold = {"train": "fold1_", "val": "fold2_", "test": "fold3_"}
    for split in FOLD_TO_SPLIT.values():
        class_counts: dict[str, int] = {}
        for class_name in CLASS_NAMES:
            paths = sorted((output / "classify" / split / class_name).glob("*.png"))
            for path in paths:
                if not path.name.startswith(expected_fold[split]):
                    raise ValueError(f"Classification split leakage: {path}")
                if path.stat().st_size == 0:
                    raise ValueError(f"Empty classification crop: {path}")
                with Image.open(path) as image:
                    if min(image.size) < 10:
                        raise ValueError(f"Classification crop is smaller than 10px: {path}")
                    image.verify()
            class_counts[class_name] = len(paths)
        result["classification"][split] = class_counts

    for split in FOLD_TO_SPLIT.values():
        sample = next((output / "detect" / "images" / split).glob("*.png"))
        linked = output / "segment" / "images" / split / sample.name
        if not os.path.samefile(sample, linked):
            result.setdefault("warnings", []).append(
                f"{split} detect and segment images are copies rather than hard links"
            )
    result["status"] = "valid"
    return result


def command_audit(args: argparse.Namespace) -> int:
    shards = verify_shards(args.source)
    audit = audit_dataset(shards)
    result = audit.as_dict()
    write_json(args.report, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    print(f"audit report: {args.report.resolve()}")
    return 0


def command_prepare(args: argparse.Namespace) -> int:
    shards = verify_shards(args.source)
    if args.output.exists() and any(args.output.iterdir()) and not args.force:
        raise FileExistsError(f"Output is not empty; pass --force to reuse it: {args.output}")
    args.output.mkdir(parents=True, exist_ok=True)
    caps = {"train": args.train_cap, "val": args.val_cap, "test": args.test_cap}
    selected = select_classification_instances(shards, caps)
    report = convert_dataset(shards, args.output, selected)
    report["classification_caps_per_class"] = caps
    write_json(args.output / "conversion-report.json", report)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


def command_validate(args: argparse.Namespace) -> int:
    report = validate_converted(args.output)
    write_json(args.report, report)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    audit = subparsers.add_parser("audit", help="verify and audit all PanNuke samples")
    audit.add_argument("--source", type=Path, required=True)
    audit.add_argument("--report", type=Path, required=True)
    audit.set_defaults(func=command_audit)

    prepare = subparsers.add_parser("prepare", help="build three YOLO datasets")
    prepare.add_argument("--source", type=Path, required=True)
    prepare.add_argument("--output", type=Path, required=True)
    prepare.add_argument("--train-cap", type=int, default=12_000)
    prepare.add_argument("--val-cap", type=int, default=3_000)
    prepare.add_argument("--test-cap", type=int, default=3_000)
    prepare.add_argument("--force", action="store_true")
    prepare.set_defaults(func=command_prepare)

    validate = subparsers.add_parser("validate", help="validate converted YOLO datasets")
    validate.add_argument("--output", type=Path, required=True)
    validate.add_argument("--report", type=Path, required=True)
    validate.set_defaults(func=command_validate)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return int(args.func(args))
    except (FileNotFoundError, FileExistsError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
