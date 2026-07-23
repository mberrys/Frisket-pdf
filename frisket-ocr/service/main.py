"""FrisketOcrService — stdio JSON-line EasyOCR sidecar."""

from __future__ import annotations

import json
import sys

from engine import run_ocr


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
            try:
                response = run_ocr(request)
            except Exception as exc:  # noqa: BLE001 — surface to PdfTool
                response = {
                    "page": request.get("page", 0),
                    "ok": False,
                    "error": str(exc),
                }
        sys.stdout.write(json.dumps(response, separators=(",", ":")) + "\n")
        sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
