# Frisket OCR sidecar (EasyOCR)

Bundled Windows sidecar for read-only OCR. Invoked by `PdfTool ocr` via stdio JSON lines.

## Dev setup (Python 3.12 venv)

```powershell
cd frisket-ocr
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python tools\download_models.py
```

Run interactively:

```powershell
python service\main.py
```

For `PdfTool ocr` without `--sidecar`, resolution order is:

1. `FRISKET_OCR_SIDECAR` environment variable
2. Bundled `FrisketOcrService/FrisketOcrService.exe` next to `PdfTool`
3. Dev launcher `frisket-ocr/tools/dev_ocr_sidecar.cmd` (relative to a build-tree `PdfTool`)

```powershell
PdfTool ocr scan.pdf --console-format json --sidecar frisket-ocr\tools\dev_ocr_sidecar.cmd
```

Send one JSON line per request on stdin, read one JSON line per response on stdout.

## Build PyInstaller bundle

```powershell
pip install pyinstaller
pyinstaller tools\pyinstaller.spec --distpath prebuilt --workpath build\pyinstaller
```

Output: `prebuilt/FrisketOcrService/FrisketOcrService.exe`

Enable CMake install with `-DPDF4QT_BUNDLE_OCR_SERVICE=ON` (copies `prebuilt/FrisketOcrService` next to `PdfTool`).

## Mock sidecar (tests / CI)

```powershell
python tools\mock_ocr_sidecar.py
```

Or `tools\mock_ocr_sidecar.cmd` on Windows.

## IPC request (stdin)

```json
{"image": "C:/temp/page-1.png", "page": 1, "dpi": 300, "languages": ["en"], "media_box": {"x": 0, "y": 0, "width": 612, "height": 792}}
```

## IPC response (stdout)

```json
{"page": 1, "ok": true, "text": "Hello", "lines": [{"text": "Hello", "confidence": 0.95, "bbox": {"x": 72, "y": 700, "width": 40, "height": 12}}]}
```
