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

"""Regenerate the hand-built golden PDF fixtures for the Frisket custom checks
(MIC-145): color-mode, embedded-fonts, image-resolution, trim and page-size.

The bleed pair (bleed-adequate/bleed-missing) is generated separately by the
C++ tool tools/generate_fixtures.cpp and is NOT touched here.

Each fixture isolates a single custom check: it is built so that every *other*
check in its profile passes, and only the target check is exercised (pass or
fail). See frisket-preflight/README.md ("Hand-built custom-check fixtures") and
testdata/fixtures/manifest.json for the expected outcome of each file.

Usage:
    python3 tools/generate_fixtures.py [output-dir]

`output-dir` defaults to ../testdata/fixtures relative to this script. Requires
reportlab (page/content authoring), Pillow (raster images) and pikepdf (precise
page-box metadata):

    pip install reportlab pillow pikepdf
"""

import io
import os
import re
import sys
import tempfile

from reportlab import rl_config
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

from PIL import Image, ImageDraw

import pikepdf

# Deterministic output: fixed /CreationDate, /ModDate and document /ID so a
# regeneration produces a byte-stable-ish diff instead of gratuitous churn.
rl_config.invariant = 1

EMBEDDABLE_TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


# reportlab writes a default-font preamble ("BT /F1 12 Tf 14.4 TL ET") into every
# page even when no text is drawn, which registers a non-embedded Helvetica the
# fixture never uses. Strip that exact preamble so each page carries only the
# fonts it actually draws.
_RL_FONT_PREAMBLE = re.compile(rb"BT\s+/\S+\s+[\d.]+\s+Tf\s+[\d.]+\s+TL\s+ET\s*")
_TF_USE = re.compile(rb"/([^\s/]+)\s+[\d.]+\s+Tf")


def _finalize(pdf_path, media, trim=None, bleed=None):
    """Strip reportlab's unused default-font preamble, prune font resources not
    actually referenced by a Tf operator, set the page boxes precisely, and
    normalise volatile metadata so fixtures are reproducible."""
    with pikepdf.open(pdf_path, allow_overwriting_input=True) as pdf:
        for page in pdf.pages:
            contents = page.obj.get("/Contents")
            data = contents.read_bytes()
            data = _RL_FONT_PREAMBLE.sub(b"", data, count=1)

            # reportlab names image XObjects with a non-deterministic hash; rename
            # them to stable /Im0, /Im1... so the output is reproducible.
            xobjects = page.obj.get("/Resources", {}).get("/XObject")
            if xobjects is not None:
                for index, old in enumerate([str(k)[1:] for k in xobjects.keys()]):
                    new = "Im%d" % index
                    if new != old:
                        xobjects[pikepdf.Name("/" + new)] = xobjects[pikepdf.Name("/" + old)]
                        del xobjects[pikepdf.Name("/" + old)]
                        data = data.replace(b"/" + old.encode("latin-1"), b"/" + new.encode("latin-1"))

            used = {name.decode("latin-1") for name in _TF_USE.findall(data)}
            fonts = page.obj.get("/Resources", {}).get("/Font")
            if fonts is not None:
                for key in [str(k)[1:] for k in fonts.keys()]:
                    if key not in used:
                        del fonts[pikepdf.Name("/" + key)]
                if len(fonts.keys()) == 0:
                    del page.obj["/Resources"]["/Font"]

            contents.write(data)
            page.MediaBox = pikepdf.Array([float(v) for v in media])
            if trim is not None:
                page.TrimBox = pikepdf.Array([float(v) for v in trim])
            if bleed is not None:
                page.BleedBox = pikepdf.Array([float(v) for v in bleed])

        # Drop the XMP packet: its MetadataDate is rewritten to "now" on save and
        # would make the output non-reproducible. reportlab's invariant mode pins
        # the DocInfo dates, which is enough for a fixture.
        if "/Metadata" in pdf.Root:
            del pdf.Root["/Metadata"]
        pdf.save(pdf_path, deterministic_id=True, fix_metadata_version=True)


def _new_canvas(buf, size):
    c = canvas.Canvas(buf, pagesize=size, pageCompression=1)
    c.setTitle("Frisket custom-check fixture")
    c.setProducer("FrisketGenerateFixtures (MIC-145)")
    return c


def _cmyk_image(px, path):
    # Solid cyan patch in DeviceCMYK; JPEG is the only common CMYK raster PIL
    # writes, and reportlab passes it through as a /DeviceCMYK image.
    Image.new("CMYK", (px, px), (255, 0, 0, 0)).save(path, "JPEG", quality=90)


def _rgb_image(px, path):
    Image.new("RGB", (px, px), (200, 40, 40)).save(path, "PNG")


def _gray_image(px, path):
    Image.new("L", (px, px), 128).save(path, "PNG")


# ---------------------------------------------------------------------------
# color-mode
# ---------------------------------------------------------------------------

def color_rgb(out_dir, tmp):
    """FAIL color-mode: a DeviceRGB image on a page that otherwise passes.
    Image is >=300 DPI so image-resolution stays clean and only color-mode fires."""
    path = os.path.join(out_dir, "color-rgb.pdf")
    img = os.path.join(tmp, "rgb.png")
    _rgb_image(620, img)  # 620 px / 2 in = 310 DPI
    buf = io.BytesIO()
    c = _new_canvas(buf, (312, 312))
    c.drawImage(img, 84, 84, width=144, height=144)  # 2x2 in placement
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 312, 312), trim=(12, 12, 300, 300), bleed=(0, 0, 312, 312))


def color_cmyk(out_dir, tmp):
    """PASS color-mode: the same page with a DeviceCMYK image (allowed)."""
    path = os.path.join(out_dir, "color-cmyk.pdf")
    img = os.path.join(tmp, "cmyk.jpg")
    _cmyk_image(620, img)  # 620 px / 2 in = 310 DPI
    buf = io.BytesIO()
    c = _new_canvas(buf, (312, 312))
    c.drawImage(img, 84, 84, width=144, height=144)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 312, 312), trim=(12, 12, 300, 300), bleed=(0, 0, 312, 312))


# ---------------------------------------------------------------------------
# image-resolution
# ---------------------------------------------------------------------------

def image_dpi_low(out_dir, tmp):
    """WARNING image-resolution: a grayscale image whose effective DPI is far
    below min_dpi (300). Grayscale keeps color-mode clean; the finding lands in
    warnings[] so report.pass stays true with the default profile."""
    path = os.path.join(out_dir, "image-dpi-low.pdf")
    img = os.path.join(tmp, "low.png")
    _gray_image(50, img)  # 50 px / 2 in = 25 DPI
    buf = io.BytesIO()
    c = _new_canvas(buf, (312, 312))
    c.drawImage(img, 84, 84, width=144, height=144)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 312, 312), trim=(12, 12, 300, 300), bleed=(0, 0, 312, 312))


def image_dpi_ok(out_dir, tmp):
    """PASS image-resolution: a grayscale image at >=300 DPI."""
    path = os.path.join(out_dir, "image-dpi-ok.pdf")
    img = os.path.join(tmp, "ok.png")
    _gray_image(620, img)  # 620 px / 2 in = 310 DPI
    buf = io.BytesIO()
    c = _new_canvas(buf, (312, 312))
    c.drawImage(img, 84, 84, width=144, height=144)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 312, 312), trim=(12, 12, 300, 300), bleed=(0, 0, 312, 312))


# ---------------------------------------------------------------------------
# embedded-fonts
# ---------------------------------------------------------------------------

def font_not_embedded(out_dir, tmp):
    """FAIL embedded-fonts: text set in Helvetica (a standard-14 font reportlab
    references without embedding). Gray fill keeps color-mode clean."""
    path = os.path.join(out_dir, "font-not-embedded.pdf")
    buf = io.BytesIO()
    c = _new_canvas(buf, (312, 312))
    c.setFillGray(0)
    c.setFont("Helvetica", 18)
    c.drawString(40, 150, "Frisket fixture: non-embedded font")
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 312, 312), trim=(12, 12, 300, 300), bleed=(0, 0, 312, 312))


def font_embedded(out_dir, tmp):
    """PASS embedded-fonts: text set in a subsetted TrueType font, embedded with
    a /FontFile2 stream."""
    path = os.path.join(out_dir, "font-embedded.pdf")
    pdfmetrics.registerFont(TTFont("DejaVuSans", EMBEDDABLE_TTF))
    buf = io.BytesIO()
    c = _new_canvas(buf, (312, 312))
    c.setFillGray(0)
    c.setFont("DejaVuSans", 18)
    c.drawString(40, 150, "Frisket fixture: embedded font")
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 312, 312), trim=(12, 12, 300, 300), bleed=(0, 0, 312, 312))


# ---------------------------------------------------------------------------
# trim / page-size
# ---------------------------------------------------------------------------
# Checked against testdata/profiles/test-trim-pagesize.json, whose page-size and
# trim checks carry expected_width_pt/expected_height_pt = 612 x 792 (US Letter)
# at tolerance 1 pt and severity error. Those pages are flat (no bleed) because
# that profile does not run the bleed check.

def trim_pagesize_ok(out_dir, tmp):
    """PASS trim + page-size: MediaBox == TrimBox == expected 612 x 792."""
    path = os.path.join(out_dir, "trim-pagesize-ok.pdf")
    buf = io.BytesIO()
    c = _new_canvas(buf, (612, 792))
    c.setFillGray(0.5)  # DeviceGray marker rectangle, no fonts/images to isolate
    c.rect(72, 72, 468, 648, stroke=0, fill=1)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 612, 792), trim=(0, 0, 612, 792))


def trim_pagesize_mismatch(out_dir, tmp):
    """FAIL trim + page-size: MediaBox == TrimBox == 540 x 720, clearly off the
    expected 612 x 792 (72 pt on each dimension, far beyond the 1 pt tolerance)."""
    path = os.path.join(out_dir, "trim-pagesize-mismatch.pdf")
    buf = io.BytesIO()
    c = _new_canvas(buf, (540, 720))
    c.setFillGray(0.5)  # DeviceGray marker rectangle, no fonts/images to isolate
    c.rect(72, 72, 396, 576, stroke=0, fill=1)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 540, 720), trim=(0, 0, 540, 720))


# ---------------------------------------------------------------------------
# AI-artwork bleed stress fixtures (MIC-316)
# ---------------------------------------------------------------------------

def _edge_line_cmyk_image(px, path):
    """CMYK raster with high-contrast geometric lines on all four edges."""
    im = Image.new("CMYK", (px, px), (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)
    ink = (0, 255, 255, 0)
    line = max(4, px // 80)
    draw.rectangle((0, 0, px - 1, line - 1), fill=ink)
    draw.rectangle((0, px - line, px - 1, px - 1), fill=ink)
    draw.rectangle((0, 0, line - 1, px - 1), fill=ink)
    draw.rectangle((px - line, 0, px - 1, px - 1), fill=ink)
    im.save(path, "JPEG", quality=92)


def _corner_checker_cmyk_image(px, path):
    """High-contrast corner blocks where mirror seams are most visible."""
    im = Image.new("CMYK", (px, px), (0, 0, 0, 0))
    draw = ImageDraw.Draw(im)
    block = px // 5
    black = (0, 0, 0, 255)
    white = (0, 0, 0, 0)
    for y in range(0, block, 12):
        for x in range(0, block, 12):
            color = black if ((x // 12) + (y // 12)) % 2 == 0 else white
            draw.rectangle((x, y, x + 11, y + 11), fill=color)
            draw.rectangle((px - block + x, y, px - block + x + 11, y + 11), fill=color)
            draw.rectangle((x, px - block + y, x + 11, px - block + y + 11), fill=color)
            draw.rectangle((px - block + x, px - block + y, px - block + x + 11, px - block + y + 11), fill=color)
    draw.rectangle((0, 0, px - 1, 3), fill=(0, 255, 255, 0))
    draw.rectangle((0, px - 4, px - 1, px - 1), fill=(0, 255, 255, 0))
    draw.rectangle((0, 0, 3, px - 1), fill=(0, 255, 255, 0))
    draw.rectangle((px - 4, 0, px - 1, px - 1), fill=(0, 255, 255, 0))
    im.save(path, "JPEG", quality=92)


def _photo_gradient_rgb_image(px, path):
    """Photo-like smooth RGB gradient flush to the trim boundary."""
    im = Image.new("RGB", (px, px))
    draw = ImageDraw.Draw(im)
    for y in range(px):
        r = int(40 + (215 * y) / max(px - 1, 1))
        g = int(90 + (120 * (px - y)) / max(px - 1, 1))
        b = int(160 + (80 * y) / max(px - 1, 1))
        draw.line((0, y, px - 1, y), fill=(r, g, b))
    draw.rectangle((0, 0, px - 1, 2), fill=(255, 220, 40))
    draw.rectangle((0, px - 3, px - 1, px - 1), fill=(255, 220, 40))
    draw.rectangle((0, 0, 2, px - 1), fill=(255, 220, 40))
    draw.rectangle((px - 3, 0, px - 1, px - 1), fill=(255, 220, 40))
    im.save(path, "PNG")


def ai_art_missing_bleed(out_dir, tmp):
    """FAIL bleed: raster artwork flush to trim, zero box bleed margin."""
    path = os.path.join(out_dir, "ai-art-missing-bleed.pdf")
    img = os.path.join(tmp, "ai_missing.jpg")
    _edge_line_cmyk_image(900, img)
    buf = io.BytesIO()
    c = _new_canvas(buf, (400, 400))
    c.drawImage(img, 0, 0, width=400, height=400)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 400, 400), trim=(0, 0, 400, 400))


def ai_art_partial_bleed(out_dir, tmp):
    """FAIL bleed: adequate left/bottom bleed boxes, short on right/top."""
    path = os.path.join(out_dir, "ai-art-partial-bleed.pdf")
    img = os.path.join(tmp, "ai_partial.jpg")
    _edge_line_cmyk_image(900, img)
    buf = io.BytesIO()
    c = _new_canvas(buf, (400, 400))
    c.drawImage(img, 20, 20, width=360, height=360)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 400, 400), trim=(20, 20, 380, 380), bleed=(0, 0, 380, 380))


def ai_art_hard_corners(out_dir, tmp):
    """FAIL bleed: checker corners at trim for mirror seam stress."""
    path = os.path.join(out_dir, "ai-art-hard-corners.pdf")
    img = os.path.join(tmp, "ai_corners.jpg")
    _corner_checker_cmyk_image(900, img)
    buf = io.BytesIO()
    c = _new_canvas(buf, (400, 400))
    c.drawImage(img, 0, 0, width=400, height=400)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 400, 400), trim=(0, 0, 400, 400))


def ai_art_raster_trim_edge(out_dir, tmp):
    """FAIL bleed: photo-like RGB raster touching trim edges."""
    path = os.path.join(out_dir, "ai-art-raster-trim-edge.pdf")
    img = os.path.join(tmp, "ai_photo.png")
    _photo_gradient_rgb_image(900, img)
    buf = io.BytesIO()
    c = _new_canvas(buf, (400, 400))
    c.drawImage(img, 0, 0, width=400, height=400)
    c.showPage()
    c.save()
    with open(path, "wb") as f:
        f.write(buf.getvalue())
    _finalize(path, media=(0, 0, 400, 400), trim=(0, 0, 400, 400))


def white_overprint_fail(out_dir, tmp):
    """FAIL white-overprint: CMYK paper white with overprint enabled on a filled path."""
    del tmp
    path = os.path.join(out_dir, "white-overprint.pdf")
    pdf = pikepdf.Pdf.new()
    page = pdf.add_blank_page(page_size=(612, 792))
    page.MediaBox = pikepdf.Array([0, 0, 612, 792])
    page.TrimBox = pikepdf.Array([0, 0, 612, 792])
    gs = pdf.make_indirect(pikepdf.Dictionary({
        "/Type": pikepdf.Name("/ExtGState"),
        "/OP": True,
        "/op": True,
    }))
    page.Resources = pdf.make_indirect(pikepdf.Dictionary({
        "/ExtGState": pdf.make_indirect(pikepdf.Dictionary({
            "/GS1": gs,
        })),
    }))
    stream = b"""/GS1 gs
/DeviceCMYK cs
0 0 0 0 sc
50 50 200 200 re
f
"""
    page.Contents = pdf.make_stream(stream)
    pdf.save(path)


def white_overprint_ok(out_dir, tmp):
    """PASS white-overprint: same white fill without overprint enabled."""
    del tmp
    path = os.path.join(out_dir, "white-overprint-ok.pdf")
    pdf = pikepdf.Pdf.new()
    page = pdf.add_blank_page(page_size=(612, 792))
    page.MediaBox = pikepdf.Array([0, 0, 612, 792])
    page.TrimBox = pikepdf.Array([0, 0, 612, 792])
    stream = b"""q
/DeviceCMYK cs
0 0 0 0 sc
50 50 200 200 re
f
Q
"""
    page.Contents = pdf.make_stream(stream)
    pdf.save(path)


def white_overprint_in_form(out_dir, tmp):
    """FAIL white-overprint: white overprint paint inside a Form XObject."""
    del tmp
    path = os.path.join(out_dir, "white-overprint-form.pdf")
    pdf = pikepdf.Pdf.new()
    page = pdf.add_blank_page(page_size=(612, 792))
    page.MediaBox = pikepdf.Array([0, 0, 612, 792])
    page.TrimBox = pikepdf.Array([0, 0, 612, 792])
    gs = pdf.make_indirect(pikepdf.Dictionary({
        "/Type": pikepdf.Name("/ExtGState"),
        "/OP": True,
        "/op": True,
    }))
    form_resources = pdf.make_indirect(pikepdf.Dictionary({
        "/ExtGState": pdf.make_indirect(pikepdf.Dictionary({
            "/GS1": gs,
        })),
    }))
    form_stream = b"""/GS1 gs
/DeviceCMYK cs
0 0 0 0 sc
50 50 200 200 re
f
"""
    form = pdf.make_stream(form_stream, {
        "/Type": pikepdf.Name("/XObject"),
        "/Subtype": pikepdf.Name("/Form"),
        "/BBox": pikepdf.Array([0, 0, 612, 792]),
        "/Resources": form_resources,
    })
    page.Resources = pdf.make_indirect(pikepdf.Dictionary({
        "/XObject": pdf.make_indirect(pikepdf.Dictionary({
            "/Fm1": form,
        })),
    }))
    page.Contents = pdf.make_stream(b"""q
1 0 0 1 0 0 cm
/Fm1 Do
Q
""")
    pdf.save(path)


FIXTURES = [
    color_rgb,
    color_cmyk,
    image_dpi_low,
    image_dpi_ok,
    font_not_embedded,
    font_embedded,
    trim_pagesize_ok,
    trim_pagesize_mismatch,
    ai_art_missing_bleed,
    ai_art_partial_bleed,
    ai_art_hard_corners,
    ai_art_raster_trim_edge,
    white_overprint_fail,
    white_overprint_ok,
    white_overprint_in_form,
]


def main(argv):
    default_out = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "testdata", "fixtures")
    )
    out_dir = argv[1] if len(argv) > 1 else default_out
    os.makedirs(out_dir, exist_ok=True)

    if not os.path.exists(EMBEDDABLE_TTF):
        sys.exit(f"Embeddable TrueType font not found at {EMBEDDABLE_TTF!r}; "
                 "install fonts-dejavu-core or edit EMBEDDABLE_TTF.")

    with tempfile.TemporaryDirectory() as tmp:
        for fixture in FIXTURES:
            fixture(out_dir, tmp)
            print(f"wrote {fixture.__name__} -> {out_dir}")


if __name__ == "__main__":
    main(sys.argv)
