"""EasyOCR reader singleton for FrisketOcrService."""

from __future__ import annotations

import math
import os
from typing import Any

_reader = None


def _finite(value: Any, default: float = 0.0) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return default
    return number if math.isfinite(number) else default


def model_storage_directory() -> str:
    program_data = os.environ.get("PROGRAMDATA")
    if program_data:
        return os.path.join(program_data, "Frisket", "ocr-models")
    return os.path.join(os.path.expanduser("~"), ".frisket", "ocr-models")


def get_reader(languages: list[str]):
    global _reader
    if _reader is None:
        import easyocr

        os.makedirs(model_storage_directory(), exist_ok=True)
        _reader = easyocr.Reader(
            languages,
            gpu=False,
            model_storage_directory=model_storage_directory(),
            verbose=False,
        )
    return _reader


def pixel_bbox_to_pdf(bbox_pixels: list[list[float]], image_width: int, image_height: int, media_box: dict[str, float]) -> dict[str, float]:
    xs = [point[0] for point in bbox_pixels]
    ys = [point[1] for point in bbox_pixels]
    left_px = min(xs)
    right_px = max(xs)
    top_px = min(ys)
    bottom_px = max(ys)

    media_x = float(media_box.get("x", 0.0))
    media_y = float(media_box.get("y", 0.0))
    media_w = float(media_box.get("width", 1.0))
    media_h = float(media_box.get("height", 1.0))

    if image_width <= 0 or image_height <= 0:
        return {"x": media_x, "y": media_y, "width": 0.0, "height": 0.0}

    width_pt = (right_px - left_px) / image_width * media_w
    height_pt = (bottom_px - top_px) / image_height * media_h
    x_pt = media_x + left_px / image_width * media_w
    y_pt = media_y + (1.0 - bottom_px / image_height) * media_h

    return {
        "x": _finite(x_pt),
        "y": _finite(y_pt),
        "width": _finite(width_pt),
        "height": _finite(height_pt),
    }


def run_ocr(request: dict[str, Any]) -> dict[str, Any]:
    from PIL import Image

    image_path = request.get("image")
    page = int(request.get("page", 0))
    languages = request.get("languages") or ["en"]
    media_box = request.get("media_box") or {"x": 0.0, "y": 0.0, "width": 612.0, "height": 792.0}

    if not image_path:
        return {"page": page, "ok": False, "error": "missing image path"}

    if not os.path.isfile(image_path):
        return {"page": page, "ok": False, "error": f"image not found: {image_path}"}

    reader = get_reader(list(languages))
    with Image.open(image_path) as image:
        image_width, image_height = image.size

    results = reader.readtext(image_path)
    lines = []
    text_parts: list[str] = []
    for bbox_pixels, text, confidence in results:
        text = str(text)
        text_parts.append(text)
        lines.append(
            {
                "text": text,
                "confidence": _finite(confidence),
                "bbox": pixel_bbox_to_pdf(bbox_pixels, image_width, image_height, media_box),
            }
        )

    return {
        "page": page,
        "ok": True,
        "text": "\n".join(text_parts),
        "lines": lines,
    }
