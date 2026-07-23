"""FrisketOcrService — stdio JSON-line EasyOCR sidecar."""

from __future__ import annotations

import json
import os
import sys

from engine import run_ocr

# PdfTool reads the first LF-terminated stdout line as the IPC response.
# EasyOCR/torch native code can write to OS fd 1 and bypass sys.stdout, so
# keep a private fd for JSON and point fd 1 at stderr for the process lifetime.
_JSON_FD = os.dup(1)
os.dup2(2, 1)


def _write_response(response: dict) -> None:
    payload = (json.dumps(response, separators=(",", ":"), allow_nan=False) + "\n").encode("utf-8")
    os.write(_JSON_FD, payload)


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
        _write_response(response)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
