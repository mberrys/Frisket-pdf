@echo off
setlocal
set "OCR_ROOT=%~dp0.."
"%OCR_ROOT%\.venv\Scripts\python.exe" "%OCR_ROOT%\service\main.py"
