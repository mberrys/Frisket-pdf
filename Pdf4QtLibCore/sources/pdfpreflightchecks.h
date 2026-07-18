// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef PDFPREFLIGHTCHECKS_H
#define PDFPREFLIGHTCHECKS_H

// Pure geometry math for the Frisket preflight checks (bleed, trim, page-size).
// Header-only and free of any PDF/document dependency so the engine, the PdfTool
// command, and the unit tests can share the exact same logic. Page-box
// extraction stays in the engine; only rectangle math lives here.

#include <QRectF>

#include <cmath>

namespace pdf
{

namespace preflight
{

/// Resolves the effective content box for a page, applying the trim -> crop ->
/// media fallback used throughout the preflight checks. Mirrors the box
/// inheritance in pdf::PDFPage::parse (unset boxes default to the crop box,
/// which defaults to the media box).
/// \param trim TrimBox (may be empty)
/// \param crop CropBox (may be empty)
/// \param media MediaBox
inline QRectF resolveEffectiveBox(const QRectF& trim, const QRectF& crop, const QRectF& media)
{
    QRectF box = trim.normalized();
    if (box.isEmpty())
    {
        box = crop.normalized();
    }
    if (box.isEmpty())
    {
        box = media.normalized();
    }
    return box;
}

/// Returns true if the bleed box extends at least \p amountPt beyond the trim
/// box on every edge (within \p tolerancePt). A missing/empty bleed box is
/// never adequate.
/// \param trim Effective trim box
/// \param bleed BleedBox (may be empty)
/// \param amountPt Required bleed distance in points
/// \param tolerancePt Allowed shortfall in points
inline bool bleedAdequate(const QRectF& trim, const QRectF& bleed, qreal amountPt, qreal tolerancePt)
{
    const QRectF normalizedTrim = trim.normalized();
    const QRectF normalizedBleed = bleed.normalized();

    if (normalizedBleed.isEmpty())
    {
        return false;
    }

    return (normalizedTrim.left() - normalizedBleed.left() >= amountPt - tolerancePt)
        && (normalizedTrim.top() - normalizedBleed.top() >= amountPt - tolerancePt)
        && (normalizedBleed.right() - normalizedTrim.right() >= amountPt - tolerancePt)
        && (normalizedBleed.bottom() - normalizedTrim.bottom() >= amountPt - tolerancePt);
}

/// Strict per-dimension size comparison for the trim and page-size checks. The
/// actual width must match the expected width, and the actual height the
/// expected height, each within \p tolerancePt. Orientation matters: a page
/// whose dimensions are swapped relative to the expectation does not match.
inline bool sizeWithinTolerance(qreal actualWidthPt,
                                qreal actualHeightPt,
                                qreal expectedWidthPt,
                                qreal expectedHeightPt,
                                qreal tolerancePt)
{
    return (std::abs(actualWidthPt - expectedWidthPt) <= tolerancePt)
        && (std::abs(actualHeightPt - expectedHeightPt) <= tolerancePt);
}

}   // namespace preflight

}   // namespace pdf

#endif // PDFPREFLIGHTCHECKS_H
