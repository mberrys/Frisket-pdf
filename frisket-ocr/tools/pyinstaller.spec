# -*- mode: python ; coding: utf-8 -*-

import os

block_cipher = None
spec_dir = os.path.dirname(os.path.abspath(SPEC))
service_dir = os.path.normpath(os.path.join(spec_dir, "..", "service"))

a = Analysis(
    [os.path.join(service_dir, "main.py")],
    pathex=[service_dir],
    binaries=[],
    datas=[],
    hiddenimports=["easyocr", "torch", "torchvision"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)
pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="FrisketOcrService",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="FrisketOcrService",
)
