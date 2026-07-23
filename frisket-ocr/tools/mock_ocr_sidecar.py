"""Mock OCR sidecar for unit tests and CI (no EasyOCR / PyTorch)."""

from __future__ import annotations

import json
import sys


def handle_request(request: dict) -> dict:
    page = int(request.get("page", 0))
    image_path = request.get("image", "")
    if not image_path:
        return {"page": page, "ok": False, "error": "missing image path"}

    media_box = request.get("media_box") or {"x": 0.0, "y": 0.0, "width": 612.0, "height": 792.0}
    x = float(media_box.get("x", 72.0))
    y = float(media_box.get("y", 700.0))
    return {
        "page": page,
        "ok": True,
        "text": "MOCK OCR TEXT",
        "lines": [
            {
                "text": "MOCK OCR TEXT",
                "confidence": 0.99,
                "bbox": {"x": x, "y": y, "width": 120.0, "height": 14.0},
            }
        ],
    }


def main() -> int:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError as exc:
            response = {"ok": False, "error": f"invalid json: {exc}"}
        else:
            response = handle_request(request)
        sys.stdout.write(json.dumps(response, separators=(",", ":")) + "\n")
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
