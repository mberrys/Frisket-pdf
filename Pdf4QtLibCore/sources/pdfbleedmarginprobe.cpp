// MIT License
//
// Copyright (c) 2018-2025 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit purposes only
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

#include "pdfbleedmarginprobe.h"

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocument.h"
#include "pdfoptionalcontent.h"
#include "pdfpage.h"
#include "pdfpainter.h"
#include "pdfrenderer.h"
#include "pdffont.h"

#include <QImage>
#include <QPainter>
#include <QtMath>

namespace pdf
{

namespace
{

QRectF referenceRectFromPage(const PDFPage* page, PDFBleedFixupSettings::ReferenceBox referenceBox)
{
    if (!page)
    {
        return QRectF();
    }

    switch (referenceBox)
    {
        case PDFBleedFixupSettings::ReferenceBox::MediaBox:
            return page->getMediaBox();
        case PDFBleedFixupSettings::ReferenceBox::CropBox:
            return page->getCropBox();
        case PDFBleedFixupSettings::ReferenceBox::TrimBox:
        default:
            return page->getTrimBox().isValid() ? page->getTrimBox() : page->getCropBox();
    }
}

QRectF targetBleedRect(const QRectF& reference, const QMarginsF& bleedMM)
{
    const PDFReal left = bleedMM.left() * PDF_MM_TO_POINT;
    const PDFReal right = bleedMM.right() * PDF_MM_TO_POINT;
    const PDFReal top = bleedMM.top() * PDF_MM_TO_POINT;
    const PDFReal bottom = bleedMM.bottom() * PDF_MM_TO_POINT;
    return QRectF(reference.left() - left,
                  reference.top() - bottom,
                  reference.width() + left + right,
                  reference.height() + top + bottom).normalized();
}

QRect mapPageRectToImage(const QRectF& pageRect, const QTransform& pageToDevice, const QSize& imageSize)
{
    const QRectF mapped = pageToDevice.mapRect(pageRect).normalized();
    return mapped.toAlignedRect().intersected(QRect(QPoint(0, 0), imageSize));
}

QRectF sideStripRect(const QRectF& reference, PDFBleedFixupSide side, PDFReal depthPt)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left:
            return QRectF(reference.left() - depthPt, reference.top(), depthPt, reference.height());
        case PDFBleedFixupSide::Right:
            return QRectF(reference.right(), reference.top(), depthPt, reference.height());
        case PDFBleedFixupSide::Bottom:
            return QRectF(reference.left(), reference.top() - depthPt, reference.width(), depthPt);
        case PDFBleedFixupSide::Top:
            return QRectF(reference.left(), reference.bottom(), reference.width(), depthPt);
    }
    return QRectF();
}

PDFReal sideBleedPt(const QMarginsF& bleedMM, PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return bleedMM.left() * PDF_MM_TO_POINT;
        case PDFBleedFixupSide::Right: return bleedMM.right() * PDF_MM_TO_POINT;
        case PDFBleedFixupSide::Top: return bleedMM.top() * PDF_MM_TO_POINT;
        case PDFBleedFixupSide::Bottom: return bleedMM.bottom() * PDF_MM_TO_POINT;
    }
    return 0.0;
}

} // namespace

PDFBleedMarginProbe::PDFBleedMarginProbe(PDFDocumentSession* session) :
    m_session(session)
{
}

PDFBleedMarginProbeResult PDFBleedMarginProbe::probe(const PDFPage* page,
                                                      size_t pageIndex,
                                                      const PDFBleedMarginProbeSettings& settings)
{
    PDFBleedMarginProbeResult result = probeFast(page, pageIndex, settings);

    if (settings.fastOnly || result.allEdgesCovered() || !m_session)
    {
        return result;
    }

    const QRectF reference = referenceRectFromPage(page, settings.referenceBox);
    if (!reference.isValid() || reference.isEmpty())
    {
        return result;
    }

    const QRectF target = targetBleedRect(reference, settings.bleedMM);
    if (!target.isValid() || target.isEmpty())
    {
        return result;
    }

    PDFBleedMarginProbeResult rasterResult = probeRaster(page, pageIndex, settings, reference, target);

    auto rasterUpgradesEmptyEdge = [](const PDFBleedMarginProbeEdgeResult& rasterEdge) -> bool
    {
        if (rasterEdge.totalPixels <= 0)
        {
            return false;
        }

        const qreal inkCoverage = static_cast<qreal>(rasterEdge.inkPixels)
            / static_cast<qreal>(rasterEdge.totalPixels);
        // Tier-2 only downgrades Tier-1 false positives when raster sees substantial
        // margin ink. Trim-edge antialiasing on otherwise empty strips stays below this.
        constexpr qreal kMinInkCoverageToUpgrade = 0.10;
        return inkCoverage >= kMinInkCoverageToUpgrade;
    };

    const bool upgradeLeft = !result.left.hasContent && rasterUpgradesEmptyEdge(rasterResult.left);
    const bool upgradeRight = !result.right.hasContent && rasterUpgradesEmptyEdge(rasterResult.right);
    const bool upgradeTop = !result.top.hasContent && rasterUpgradesEmptyEdge(rasterResult.top);
    const bool upgradeBottom = !result.bottom.hasContent && rasterUpgradesEmptyEdge(rasterResult.bottom);

    // Raster confirmation may upgrade fast-path empty edges when strips have real content.
    if (upgradeLeft)
    {
        result.left = rasterResult.left;
    }
    if (upgradeRight)
    {
        result.right = rasterResult.right;
    }
    if (upgradeTop)
    {
        result.top = rasterResult.top;
    }
    if (upgradeBottom)
    {
        result.bottom = rasterResult.bottom;
    }

    return result;
}

PDFBleedMarginProbeResult PDFBleedMarginProbe::probeFast(const PDFPage* page,
                                                          size_t pageIndex,
                                                          const PDFBleedMarginProbeSettings& settings)
{
    PDFBleedMarginProbeResult result;

    if (!page || !m_session)
    {
        return result;
    }

    const QRectF reference = referenceRectFromPage(page, settings.referenceBox);
    if (!reference.isValid() || reference.isEmpty())
    {
        return result;
    }

    const PDFPrecompiledPage* compiled = m_session->compilePage(pageIndex);
    if (!compiled || !compiled->isValid())
    {
        return result;
    }

    // Use the existing calculateGraphicPieceInfos to extract per-piece bounds.
    const PDFPrecompiledPage::GraphicPieceInfos infos = compiled->calculateGraphicPieceInfos(page->getMediaBox(), 0.01);

    QRectF contentBounds;
    for (const PDFPrecompiledPage::GraphicPieceInfo& info : infos)
    {
        if (info.boundingRect.isValid())
        {
            contentBounds = contentBounds.united(info.boundingRect);
        }
    }

    if (!contentBounds.isValid() || contentBounds.isEmpty())
    {
        // No content at all — all sides missing.
        return result;
    }

    const PDFBleedFixupSide sides[4] = {
        PDFBleedFixupSide::Left, PDFBleedFixupSide::Right,
        PDFBleedFixupSide::Top, PDFBleedFixupSide::Bottom
    };

    for (PDFBleedFixupSide side : sides)
    {
        const PDFReal depthPt = sideBleedPt(settings.bleedMM, side);
        if (!(depthPt > 0.0))
        {
            continue;
        }

        const QRectF strip = sideStripRect(reference, side, depthPt);
        PDFBleedMarginProbeEdgeResult edgeResult;
        edgeResult.stripRect = strip;

        // Check if content bounds overlap the strip rect.
        edgeResult.hasContent = contentBounds.intersects(strip);

        switch (side)
        {
            case PDFBleedFixupSide::Left: result.left = edgeResult; break;
            case PDFBleedFixupSide::Right: result.right = edgeResult; break;
            case PDFBleedFixupSide::Top: result.top = edgeResult; break;
            case PDFBleedFixupSide::Bottom: result.bottom = edgeResult; break;
        }
    }

    return result;
}

PDFBleedMarginProbeResult PDFBleedMarginProbe::probeRaster(const PDFPage* page,
                                                            size_t pageIndex,
                                                            const PDFBleedMarginProbeSettings& settings,
                                                            const QRectF& reference,
                                                            const QRectF& targetBleed)
{
    PDFBleedMarginProbeResult result;

    if (!page || !m_session)
    {
        return result;
    }

    PDFDocument* document = m_session->getDocument();
    if (!document)
    {
        return result;
    }

    // Set up rendering infrastructure (same pattern as PDFBleedFixup).
    PDFOptionalContentActivity optionalContentActivity(document, OCUsage::Export, nullptr);
    PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    PDFCMSPointer cms = cmsManager.getCurrentCMS();
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFModifiedDocument md(document, &optionalContentActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);

    PDFRenderer::Features features = PDFRenderer::Features(PDFRenderer::Antialiasing | PDFRenderer::TextAntialiasing);
    features.setFlag(PDFRenderer::ClipToCropBox, false);
    features.setFlag(PDFRenderer::DisplayAnnotations, false);

    PDFMeshQualitySettings meshQualitySettings;
    PDFRenderer renderer(document, &fontCache, cms.get(), &optionalContentActivity, features, meshQualitySettings);

    PDFRasterizer rasterizer(nullptr);
    rasterizer.reset(RendererEngine::QPainter);

    const QSizeF mediaSize = page->getRotatedMediaBox().size();
    const PDFReal pointToPixel = settings.dpi / 72.0;

    // Render full page at probe DPI, then crop each strip.
    const int fullW = qMax(1, qCeil(mediaSize.width() * pointToPixel));
    const int fullH = qMax(1, qCeil(mediaSize.height() * pointToPixel));
    QImage fullImage(QSize(fullW, fullH), QImage::Format_ARGB32_Premultiplied);
    fullImage.fill(Qt::white);
    QPainter fullPainter(&fullImage);

    const QTransform pageToDevice = PDFRenderer::createPagePointToDevicePointMatrix(page, QRect(QPoint(0, 0), QSize(fullW, fullH)));
    renderer.render(&fullPainter, pageToDevice, pageIndex);
    fullPainter.end();

    const PDFBleedFixupSide sides[4] = {
        PDFBleedFixupSide::Left, PDFBleedFixupSide::Right,
        PDFBleedFixupSide::Top, PDFBleedFixupSide::Bottom
    };

    for (PDFBleedFixupSide side : sides)
    {
        const PDFReal depthPt = sideBleedPt(settings.bleedMM, side);
        if (!(depthPt > 0.0))
        {
            continue;
        }

        const QRectF stripRect = sideStripRect(reference, side, depthPt);
        if (!stripRect.isValid() || stripRect.isEmpty())
        {
            continue;
        }

        const QRect stripPxRect = mapPageRectToImage(stripRect, pageToDevice, fullImage.size());
        if (stripPxRect.isEmpty() || stripPxRect.width() <= 0 || stripPxRect.height() <= 0)
        {
            continue;
        }

        const QImage stripCropped = fullImage.copy(stripPxRect);
        int inkCount = 0;
        const int pixelCount = stripCropped.width() * stripCropped.height();

        for (int y = 0; y < stripCropped.height(); ++y)
        {
            for (int x = 0; x < stripCropped.width(); ++x)
            {
                if (pixelIsInk(stripCropped, x, y, settings.threshold))
                {
                    ++inkCount;
                }
            }
        }

        PDFBleedMarginProbeEdgeResult edgeResult;
        const qreal inkCoverage = pixelCount > 0 ? static_cast<qreal>(inkCount) / static_cast<qreal>(pixelCount) : 0.0;
        edgeResult.hasContent = inkCoverage > (1.0 - settings.whiteCoverageThreshold);
        edgeResult.inkPixels = inkCount;
        edgeResult.totalPixels = pixelCount;
        edgeResult.stripRect = stripRect;

        switch (side)
        {
            case PDFBleedFixupSide::Left: result.left = edgeResult; break;
            case PDFBleedFixupSide::Right: result.right = edgeResult; break;
            case PDFBleedFixupSide::Top: result.top = edgeResult; break;
            case PDFBleedFixupSide::Bottom: result.bottom = edgeResult; break;
        }
    }

    return result;
}

bool PDFBleedMarginProbe::pixelIsInk(const QImage& image, int x, int y, int threshold)
{
    if (image.isNull() || x < 0 || x >= image.width() || y < 0 || y >= image.height())
    {
        return false;
    }

    const QRgb pixel = image.pixel(x, y);

    if (qAlpha(pixel) < threshold)
    {
        return false;
    }

    // Premultiplied white/near-white background is empty margin, not ink.
    if (qRed(pixel) >= 255 - threshold
        && qGreen(pixel) >= 255 - threshold
        && qBlue(pixel) >= 255 - threshold)
    {
        return false;
    }

    return true;
}

} // namespace pdf
