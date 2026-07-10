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

"""Generate OCR spike fixtures when PdfTool is unavailable.

Creates PNG rasters (300 DPI letter-ish) that mimic PdfTool render output for:
  - menu: short headings + prices (live-text PDF surrogate)
  - spec-sheet: dense body copy
  - scan-like: JPEG artifacts + slight rotation (ScannerPlugin-style)

Also writes fixtures/manifest.json with ground-truth text for accuracy scoring.

When PdfTool is built, prefer prepare_samples.sh (render real PDFs instead).
"""

from __future__ import annotations

import json
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parent
FIXTURES = ROOT / "fixtures"
RASTER_DIR = FIXTURES / "rasters"
PDF_DIR = FIXTURES / "pdfs"

# ~300 DPI for US Letter width 8.5 in -> 2550 px (match PdfTool --image-res-dpi 300)
WIDTH = 2550
HEIGHT = 3300


def _font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for path in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ):
        if Path(path).is_file():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def _save_png(img: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, format="PNG")


def make_menu_raster() -> tuple[Image.Image, str]:
    img = Image.new("RGB", (WIDTH, HEIGHT), "white")
    draw = ImageDraw.Draw(img)
    title_font = _font(72)
    body_font = _font(48)
    lines = [
        ("VENUE POSTER MENU", title_font, 120),
        ("", body_font, 0),
        ("House Lager ................ $6.00", body_font, 80),
        ("Seasonal IPA ............... $7.50", body_font, 80),
        ("Veggie Handbill ............ $12.00", body_font, 80),
        ("Signage Bundle ............. $45.00", body_font, 80),
        ("Bleed: 0.125 in | Trim: 18 x 24 in", body_font, 80),
    ]
    y = 200
    ground_parts: list[str] = []
    for text, font, spacing in lines:
        if text:
            draw.text((160, y), text, fill="black", font=font)
            ground_parts.append(text)
        y += (font.size if hasattr(font, "size") else 48) + spacing
    ground = "\n".join(ground_parts)
    return img, ground


def make_spec_sheet_raster() -> tuple[Image.Image, str]:
    img = Image.new("RGB", (WIDTH, HEIGHT), "white")
    draw = ImageDraw.Draw(img)
    font = _font(36)
    paragraphs = [
        "FRISKET PRINT SPEC — JOB #4421",
        "Media: 80# gloss text | Color: CMYK | Finish: aqueous coat",
        "Trim: 8.5 x 11 in | Bleed: 0.125 in all sides | Safe: 0.25 in",
        "Images must be 300 DPI at final size. Embed all fonts or outline.",
        "No RGB spot colors. Registration marks outside bleed only.",
        "Delivery: PDF/X-1a preferred; accept PDF 1.7 with bleed boxes set.",
    ]
    y = 180
    for para in paragraphs:
        draw.text((140, y), para, fill="black", font=font)
        y += 90
    body = (
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris."
    )
    words = body.split()
    line, x, line_h = "", 140, 0
    for word in words:
        trial = (line + " " + word).strip()
        bbox = draw.textbbox((0, 0), trial, font=font)
        if bbox[2] - bbox[0] > WIDTH - 280:
            draw.text((140, y), line, fill="black", font=font)
            y += 52
            line = word
        else:
            line = trial
    if line:
        draw.text((140, y), line, fill="black", font=font)
    ground = "\n".join(paragraphs + [body])
    return img, ground


def make_scan_like_raster() -> tuple[Image.Image, str]:
    """Image-only page surrogate: photo of text with compression artifacts."""
    base, ground = make_menu_raster()
    # Slight deskew + blur + JPEG round-trip
    rotated = base.rotate(1.2, fillcolor="white", expand=False)
    rgb = rotated.convert("RGB")
    from io import BytesIO

    buf = BytesIO()
    rgb.save(buf, format="JPEG", quality=72)
    buf.seek(0)
    degraded = Image.open(buf).convert("RGB")
    degraded = degraded.filter(ImageFilter.GaussianBlur(radius=0.6))
    return degraded, ground


def write_source_pdfs() -> None:
    """Small vector PDFs for prepare_samples.sh when PdfTool exists."""
    PDF_DIR.mkdir(parents=True, exist_ok=True)
    menu_pdf = PDF_DIR / "menu-live-text.pdf"
    c = canvas.Canvas(str(menu_pdf), pagesize=letter)
    c.setFont("Helvetica-Bold", 18)
    c.drawString(72, 720, "VENUE POSTER MENU")
    c.setFont("Helvetica", 12)
    c.drawString(72, 680, "House Lager ................ $6.00")
    c.drawString(72, 660, "Seasonal IPA ............... $7.50")
    c.drawString(72, 640, "Veggie Handbill ............ $12.00")
    c.save()


def main() -> int:
    random.seed(42)
    RASTER_DIR.mkdir(parents=True, exist_ok=True)
    write_source_pdfs()

    entries = []
    for slug, factory, page, kind in (
        ("menu-live-text", make_menu_raster, 1, "live-text"),
        ("spec-sheet", make_spec_sheet_raster, 1, "live-text"),
        ("scan-like-menu", make_scan_like_raster, 1, "scan-like"),
    ):
        img, ground = factory()
        png_path = RASTER_DIR / f"{slug}-p{page:03d}.png"
        _save_png(img, png_path)
        entries.append(
            {
                "id": slug,
                "page": page,
                "kind": kind,
                "raster": str(png_path.relative_to(ROOT)),
                "ground_truth": ground,
                "notes": "Synthetic 300 DPI letter raster; replace via prepare_samples.sh when PdfTool is available.",
            }
        )

    manifest = {
        "schema_version": 1,
        "dpi": 300,
        "source": "generate_fixtures.py",
        "fixtures": entries,
    }
    manifest_path = FIXTURES / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(entries)} rasters + {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
