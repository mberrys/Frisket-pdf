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

#ifndef PDFBLEEDFIXUP_H
#define PDFBLEEDFIXUP_H

#include "pdfglobal.h"
#include "pdfdocument.h"
#include "pdfrenderer.h"
#include "pdfutils.h"

#include <QImage>
#include <QMarginsF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace pdf
{

enum class PDFBleedFixupMode
{
    Mirror,
    PixelRepeat,
    Stretch
};

enum class PDFBleedFixupSide
{
    Left = 0,
    Bottom = 1,
    Right = 2,
    Top = 3
};

struct PDF4QTLIBCORESHARED_EXPORT PDFBleedFixupSettings
{
    enum class ReferenceBox
    {
        CropBox,
        TrimBox,
        MediaBox
    };

    PDFBleedFixupMode mode = PDFBleedFixupMode::Mirror;
    QString pageRange = "-";
    ReferenceBox referenceBox = ReferenceBox::TrimBox;
    QMarginsF bleedMM = QMarginsF(3.0, 3.0, 3.0, 3.0); ///< left, top, right, bottom
    bool expandMediaBox = true;
    bool expandCropBox = true;
    bool expandBleedBox = true;
    bool expandTrimBox = false;
    int dpi = 300;
    int samplePixels = 1; ///< PixelRepeat / Stretch sample depth
    bool skipIfAlreadyBleeding = true;
    bool force = false;
    PDFRenderer::Features renderFeatures = PDFRenderer::Features(PDFRenderer::Antialiasing | PDFRenderer::TextAntialiasing);
};

struct PDF4QTLIBCORESHARED_EXPORT PDFBleedFixupPageReport
{
    PDFInteger pageIndex = 0;
    QRectF originalMediaBox;
    QRectF originalCropBox;
    QRectF originalBleedBox;
    QRectF originalTrimBox;
    QRectF newMediaBox;
    QRectF newCropBox;
    QRectF newBleedBox;
    QRectF newTrimBox;
    QVector<PDFBleedFixupSide> sidesApplied;
    QStringList skipReasons;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFBleedFixupReport
{
    QVector<PDFBleedFixupPageReport> pages;
};

/// Shared helpers exposed for unit tests (rect / skip / strip image builders).
namespace PDFBleedFixupMath
{

PDF4QTLIBCORESHARED_EXPORT QRectF referenceRect(const PDFPage* page, PDFBleedFixupSettings::ReferenceBox referenceBox);
PDF4QTLIBCORESHARED_EXPORT QRectF targetBleedRect(const QRectF& reference, const QMarginsF& bleedMM);
PDF4QTLIBCORESHARED_EXPORT QRectF expandBoxTo(const QRectF& box, const QRectF& target);
PDF4QTLIBCORESHARED_EXPORT bool sideAlreadyBleeding(const QRectF& reference,
                                                    const QRectF& bleedBox,
                                                    PDFBleedFixupSide side,
                                                    PDFReal requiredBleedPt);
PDF4QTLIBCORESHARED_EXPORT PDFReal sideBleedMM(const QMarginsF& bleedMM, PDFBleedFixupSide side);
PDF4QTLIBCORESHARED_EXPORT int stripWidthPx(PDFBleedFixupMode mode, int bleedDepthPx, int samplePixels);
PDF4QTLIBCORESHARED_EXPORT QRectF edgeStripSourceRect(const QRectF& reference,
                                                      PDFBleedFixupSide side,
                                                      PDFReal depthPt);
PDF4QTLIBCORESHARED_EXPORT QRectF edgeStripDestRect(const QRectF& reference,
                                                    PDFBleedFixupSide side,
                                                    PDFReal depthPt);
PDF4QTLIBCORESHARED_EXPORT QImage buildEdgeFillImage(const QImage& pageImage,
                                                     const QRect& sourcePx,
                                                     PDFBleedFixupSide side,
                                                     PDFBleedFixupMode mode,
                                                     int bleedDepthPx);

} // namespace PDFBleedFixupMath

class PDF4QTLIBCORESHARED_EXPORT PDFBleedFixup
{
public:
    static PDFOperationResult apply(PDFDocument* document,
                                    const PDFBleedFixupSettings& settings,
                                    PDFBleedFixupReport* report = nullptr,
                                    PDFModifiedDocument::ModificationFlags* modificationFlags = nullptr);
};

} // namespace pdf

#endif // PDFBLEEDFIXUP_H
