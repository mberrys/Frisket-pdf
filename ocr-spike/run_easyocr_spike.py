#!/usr/bin/env python3
# MIT License
#
# Copyright (c) 2018-2025 Jakub Melka and Contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""MIC-128: run EasyOCR on fixture rasters and score vs ground truth.

Output shape matches planned MIC-123 OCR service contract:
  { page, text, boxes, confidence, elapsed_ms, detections[] }

Writes results/spike_report.json and prints a summary table.
"""

from __future__ import annotations

import argparse
import json
import re
import time
from pathlib import Path

import easyocr
from rapidfuzz import fuzz


ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "fixtures" / "manifest.json"
RESULTS = ROOT / "results"


def normalize(text: str) -> str:
    text = text.lower()
    text = re.sub(r"[^a-z0-9]+", " ", text)
    return " ".join(text.split())


def quad_to_aabb(quad: list[list[float]]) -> list[float]:
    xs = [p[0] for p in quad]
    ys = [p[1] for p in quad]
    return [min(xs), min(ys), max(xs), max(ys)]


def run_on_image(reader: easyocr.Reader, raster_path: Path, page: int) -> dict:
    t0 = time.perf_counter()
    raw = reader.readtext(str(raster_path))
    elapsed_ms = int((time.perf_counter() - t0) * 1000)

    detections = []
    boxes: list[list[float]] = []
    confidences: list[float] = []
    text_parts: list[str] = []

    for quad, text, conf in raw:
        aabb = [float(v) for v in quad_to_aabb(quad)]
        detections.append({"text": text, "box": aabb, "confidence": float(conf)})
        boxes.append(aabb)
        confidences.append(float(conf))
        text_parts.append(text)

    joined = " ".join(text_parts)
    mean_conf = sum(confidences) / len(confidences) if confidences else 0.0

    return {
        "page": page,
        "text": joined,
        "boxes": boxes,
        "confidence": confidences,
        "mean_confidence": round(mean_conf, 4),
        "detection_count": len(detections),
        "elapsed_ms": elapsed_ms,
        "detections": detections,
        "raster": str(raster_path.relative_to(ROOT)),
    }


def score_vs_ground_truth(ocr_text: str, ground_truth: str) -> dict:
    n_ocr = normalize(ocr_text)
    n_gt = normalize(ground_truth)
    return {
        "token_sort_ratio": round(fuzz.token_sort_ratio(n_ocr, n_gt), 2),
        "partial_ratio": round(fuzz.partial_ratio(n_ocr, n_gt), 2),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="EasyOCR spike (MIC-128)")
    parser.add_argument("--lang", default="en", help="EasyOCR language code")
    parser.add_argument("--gpu", action="store_true", help="Use GPU if available")
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--out", type=Path, default=RESULTS / "spike_report.json")
    args = parser.parse_args()

    if not args.manifest.is_file():
        raise SystemExit(f"Missing manifest: {args.manifest}. Run generate_fixtures.py first.")

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    fixtures = manifest["fixtures"]

    print("Initializing EasyOCR reader (first run downloads models)...")
    t_init = time.perf_counter()
    reader = easyocr.Reader([args.lang], gpu=args.gpu)
    init_ms = int((time.perf_counter() - t_init) * 1000)
    print(f"Reader ready in {init_ms} ms")

    pages = []
    for entry in fixtures:
        raster = ROOT / entry["raster"]
        result = run_on_image(reader, raster, entry["page"])
        result["fixture_id"] = entry["id"]
        result["kind"] = entry["kind"]
        if entry.get("ground_truth"):
            result["accuracy"] = score_vs_ground_truth(result["text"], entry["ground_truth"])
            result["ground_truth_chars"] = len(entry["ground_truth"])
        pages.append(result)
        acc = result.get("accuracy", {})
        print(
            f"  {entry['id']:20s}  {result['elapsed_ms']:5d} ms  "
            f"det={result['detection_count']:2d}  "
            f"acc={acc.get('token_sort_ratio', 'n/a')}%  "
            f"mean_conf={result['mean_confidence']:.2f}"
        )

    elapsed = [p["elapsed_ms"] for p in pages]
    report = {
        "engine": "easyocr",
        "lang": args.lang,
        "gpu": args.gpu,
        "init_ms": init_ms,
        "fixture_count": len(pages),
        "elapsed_ms": {
            "min": min(elapsed),
            "max": max(elapsed),
            "mean": round(sum(elapsed) / len(elapsed), 1),
            "total": sum(elapsed),
        },
        "schema_parity_mic123": {
            "text": True,
            "boxes": True,
            "confidence": True,
            "page": True,
        },
        "pages": pages,
    }

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"\nWrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
