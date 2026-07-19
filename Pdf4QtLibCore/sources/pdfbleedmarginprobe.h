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

#ifndef PDFBLEEDMARGINPROBE_H
#define PDFBLEEDMARGINPROBE_H

#include "pdfglobal.h"
#include "pdfbleedfixup.h"
#include "pdfdocumentsession.h"

#include <QRectF>

namespace pdf
{

struct PDF4QTLIBCORESHARED_EXPORT PDFBleedMarginProbeSettings
{
    int dpi = 150;
    int threshold = 16;
    qreal whiteCoverageThreshold = 0.9975;
    PDFBleedFixupSettings::ReferenceBox referenceBox = PDFBleedFixupSettings::ReferenceBox::TrimBox;
    QMarginsF bleedMM = QMarginsF(3.0, 3.0, 3.0, 3.0);
    bool fastOnly = false;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFBleedMarginProbeEdgeResult
{
    bool hasContent = false;
    int inkPixels = 0;
    int totalPixels = 0;
    QRectF stripRect;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFBleedMarginProbeResult
{
    PDFBleedMarginProbeEdgeResult left;
    PDFBleedMarginProbeEdgeResult top;
    PDFBleedMarginProbeEdgeResult right;
    PDFBleedMarginProbeEdgeResult bottom;

    bool allEdgesCovered() const
    {
        return left.hasContent && right.hasContent && top.hasContent && bottom.hasContent;
    }
};

/// Probes whether rendered artwork on a page extends into the bleed margin.
///
/// Fast path: unions bounding rects from `PDFPrecompiledPage::calculateGraphicPieceInfos`
/// and compares against the reference box expanded by the bleed amount. No rasterization.
///
/// Raster path (raster_confirm): renders only the four edge strips at probe_dpi
/// and counts non-background pixels against a threshold.
class PDF4QTLIBCORESHARED_EXPORT PDFBleedMarginProbe
{
public:
    explicit PDFBleedMarginProbe(PDFDocumentSession* session);

    /// Full probe: fast bounds pass first, then raster confirmation if the settings
    /// request it and the fast pass flagged missing content on any side.
    PDFBleedMarginProbeResult probe(const PDFPage* page,
                                    size_t pageIndex,
                                    const PDFBleedMarginProbeSettings& settings);

    /// Fast vector-content bounds pass only. No rasterization.
    PDFBleedMarginProbeResult probeFast(const PDFPage* page,
                                        size_t pageIndex,
                                        const PDFBleedMarginProbeSettings& settings);

private:
    static bool pixelIsInk(const QImage& image, int x, int y, int threshold);
    PDFBleedMarginProbeResult probeRaster(const PDFPage* page,
                                          size_t pageIndex,
                                          const PDFBleedMarginProbeSettings& settings,
                                          const QRectF& reference,
                                          const QRectF& targetBleed);

    PDFDocumentSession* m_session;
};

} // namespace pdf

#endif // PDFBLEEDMARGINPROBE_H
