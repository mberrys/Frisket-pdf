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

#include "pdfbleedfixup.h"

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocumentbuilder.h"
#include "pdffont.h"
#include "pdfoptionalcontent.h"
#include "pdfpainter.h"

#include <QPainter>
#include <QtMath>

#include <set>

namespace pdf
{

namespace
{

QString formatPDFNumber(PDFReal value)
{
    QString text = QString::number(value, 'f', 6);
    while (text.endsWith(QLatin1Char('0')))
    {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.')))
    {
        text.chop(1);
    }
    if (text.isEmpty() || text == QStringLiteral("-0"))
    {
        text = QStringLiteral("0");
    }
    return text;
}

PDFObjectReference createContentStream(PDFDocumentBuilder* builder, const QByteArray& content)
{
    PDFDictionary dictionary;
    dictionary.addEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(content.size()));
    PDFObject streamObject = PDFObject::createStream(std::make_shared<PDFStream>(std::move(dictionary), QByteArray(content)));
    return builder->addObject(std::move(streamObject));
}

void appendContentReference(PDFDocumentBuilder* builder,
                            std::vector<PDFObjectReference>& contentReferences,
                            PDFObject contentObject)
{
    const PDFObject dereferenced = builder->getObject(contentObject);
    if (dereferenced.isNull())
    {
        return;
    }

    if (dereferenced.isStream())
    {
        if (contentObject.isReference())
        {
            contentReferences.push_back(contentObject.getReference());
        }
        else
        {
            contentReferences.push_back(builder->addObject(dereferenced));
        }
        return;
    }

    if (dereferenced.isArray())
    {
        for (const PDFObject& object : *dereferenced.getArray())
        {
            if (object.isReference())
            {
                contentReferences.push_back(object.getReference());
            }
        }
    }
}

void prependContentTranslate(PDFDocumentBuilder* builder,
                             PDFObjectReference pageReference,
                             PDFReal tx,
                             PDFReal ty)
{
    const PDFObjectStorage* storage = builder->getStorage();
    const PDFDictionary* pageDictionary = storage->getDictionaryFromObject(storage->getObjectByReference(pageReference));
    if (!pageDictionary)
    {
        return;
    }

    std::vector<PDFObjectReference> contentReferences;
    appendContentReference(builder, contentReferences, pageDictionary->get("Contents"));
    if (contentReferences.empty())
    {
        return;
    }

    QByteArray prefix;
    prefix.append("q 1 0 0 1 ");
    prefix.append(formatPDFNumber(tx).toLatin1());
    prefix.append(' ');
    prefix.append(formatPDFNumber(ty).toLatin1());
    prefix.append(" cm\n");

    contentReferences.insert(contentReferences.begin(), createContentStream(builder, prefix));
    contentReferences.push_back(createContentStream(builder, QByteArray("Q\n")));

    PDFObjectFactory pageUpdateFactory;
    pageUpdateFactory.beginDictionary();
    pageUpdateFactory.beginDictionaryItem("Contents");
    pageUpdateFactory << contentReferences;
    pageUpdateFactory.endDictionaryItem();
    pageUpdateFactory.endDictionary();
    builder->mergeTo(pageReference, pageUpdateFactory.takeObject());
}

std::vector<PDFInteger> selectPageIndices(const PDFDocument* document,
                                          const QString& pageRange,
                                          QString* errorMessage)
{
    std::vector<PDFInteger> result;
    if (!document)
    {
        if (errorMessage)
        {
            *errorMessage = PDFTranslationContext::tr("Invalid document.");
        }
        return result;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();
    if (pageCount <= 0)
    {
        return result;
    }

    std::set<PDFInteger> rangeSelection;
    const QString rangeText = pageRange.simplified();
    if (!rangeText.isEmpty())
    {
        QString parseError;
        const PDFClosedIntervalSet closedIntervalSet = PDFClosedIntervalSet::parse(1, pageCount, rangeText, &parseError);
        if (!parseError.isEmpty())
        {
            if (errorMessage)
            {
                *errorMessage = parseError;
            }
            return {};
        }

        for (const PDFInteger pageNumber : closedIntervalSet.unfold())
        {
            rangeSelection.insert(pageNumber);
        }
    }

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFInteger pageNumber = pageIndex + 1;
        if (!rangeSelection.empty() && !rangeSelection.count(pageNumber))
        {
            continue;
        }
        result.push_back(pageIndex);
    }

    return result;
}

QRect mapPageRectToImage(const QRectF& pageRect, const QTransform& pageToDevice, const QSize& imageSize)
{
    const QRectF mapped = pageToDevice.mapRect(pageRect).normalized();
    QRect pixelRect = mapped.toAlignedRect();
    return pixelRect.intersected(QRect(QPoint(0, 0), imageSize));
}

QString sideName(PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return QStringLiteral("left");
        case PDFBleedFixupSide::Right: return QStringLiteral("right");
        case PDFBleedFixupSide::Top: return QStringLiteral("top");
        case PDFBleedFixupSide::Bottom: return QStringLiteral("bottom");
    }
    return QStringLiteral("unknown");
}

} // namespace

namespace PDFBleedFixupMath
{

QRectF referenceRect(const PDFPage* page, PDFBleedFixupSettings::ReferenceBox referenceBox)
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

QRectF expandBoxTo(const QRectF& box, const QRectF& target)
{
    if (!target.isValid())
    {
        return box;
    }
    if (!box.isValid())
    {
        return target;
    }
    return box.united(target);
}

PDFReal sideBleedMM(const QMarginsF& bleedMM, PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return bleedMM.left();
        case PDFBleedFixupSide::Right: return bleedMM.right();
        case PDFBleedFixupSide::Top: return bleedMM.top();
        case PDFBleedFixupSide::Bottom: return bleedMM.bottom();
    }
    return 0.0;
}

bool sideAlreadyBleeding(const QRectF& reference,
                         const QRectF& bleedBox,
                         PDFBleedFixupSide side,
                         PDFReal requiredBleedPt)
{
    if (!(requiredBleedPt > 0.0) || !reference.isValid() || !bleedBox.isValid())
    {
        return false;
    }

    constexpr PDFReal epsilon = 0.5;
    switch (side)
    {
        case PDFBleedFixupSide::Left:
            return (reference.left() - bleedBox.left()) + epsilon >= requiredBleedPt;
        case PDFBleedFixupSide::Right:
            return (bleedBox.right() - reference.right()) + epsilon >= requiredBleedPt;
        case PDFBleedFixupSide::Bottom:
            return (reference.top() - bleedBox.top()) + epsilon >= requiredBleedPt;
        case PDFBleedFixupSide::Top:
            return (bleedBox.bottom() - reference.bottom()) + epsilon >= requiredBleedPt;
    }
    return false;
}

int stripWidthPx(PDFBleedFixupMode mode, int bleedDepthPx, int samplePixels)
{
    if (mode == PDFBleedFixupMode::Mirror)
    {
        return qMax(1, bleedDepthPx);
    }
    return qMax(1, samplePixels);
}

QRectF edgeStripSourceRect(const QRectF& reference, PDFBleedFixupSide side, PDFReal depthPt)
{
    if (!(depthPt > 0.0) || !reference.isValid())
    {
        return QRectF();
    }

    switch (side)
    {
        case PDFBleedFixupSide::Left:
            return QRectF(reference.left(), reference.top(), depthPt, reference.height());
        case PDFBleedFixupSide::Right:
            return QRectF(reference.right() - depthPt, reference.top(), depthPt, reference.height());
        case PDFBleedFixupSide::Bottom:
            return QRectF(reference.left(), reference.top(), reference.width(), depthPt);
        case PDFBleedFixupSide::Top:
            return QRectF(reference.left(), reference.bottom() - depthPt, reference.width(), depthPt);
    }
    return QRectF();
}

QRectF edgeStripDestRect(const QRectF& reference, PDFBleedFixupSide side, PDFReal depthPt)
{
    if (!(depthPt > 0.0) || !reference.isValid())
    {
        return QRectF();
    }

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

QImage buildEdgeFillImage(const QImage& pageImage,
                          const QRect& sourcePx,
                          PDFBleedFixupSide side,
                          PDFBleedFixupMode mode,
                          int bleedDepthPx)
{
    if (pageImage.isNull() || !sourcePx.isValid() || bleedDepthPx <= 0)
    {
        return QImage();
    }

    const QRect clipped = sourcePx.intersected(pageImage.rect());
    if (!clipped.isValid() || clipped.isEmpty())
    {
        return QImage();
    }

    QImage strip = pageImage.copy(clipped);
    if (strip.isNull() || strip.width() <= 0 || strip.height() <= 0)
    {
        return QImage();
    }

    switch (mode)
    {
        case PDFBleedFixupMode::Mirror:
        {
            const bool horizontal = (side == PDFBleedFixupSide::Left || side == PDFBleedFixupSide::Right);
            return strip.mirrored(horizontal, !horizontal);
        }
        case PDFBleedFixupMode::PixelRepeat:
        {
            if (side == PDFBleedFixupSide::Left || side == PDFBleedFixupSide::Right)
            {
                QImage out(bleedDepthPx, strip.height(), QImage::Format_ARGB32_Premultiplied);
                const int srcX = (side == PDFBleedFixupSide::Left) ? 0 : (strip.width() - 1);
                for (int x = 0; x < bleedDepthPx; ++x)
                {
                    for (int y = 0; y < strip.height(); ++y)
                    {
                        out.setPixel(x, y, strip.pixel(srcX, y));
                    }
                }
                return out;
            }

            QImage out(strip.width(), bleedDepthPx, QImage::Format_ARGB32_Premultiplied);
            // After page->device Y flip, page top maps near image y=0.
            const int edgeY = (side == PDFBleedFixupSide::Top) ? 0 : (strip.height() - 1);
            for (int y = 0; y < bleedDepthPx; ++y)
            {
                for (int x = 0; x < strip.width(); ++x)
                {
                    out.setPixel(x, y, strip.pixel(x, edgeY));
                }
            }
            return out;
        }
        case PDFBleedFixupMode::Stretch:
        {
            if (side == PDFBleedFixupSide::Left || side == PDFBleedFixupSide::Right)
            {
                return strip.scaled(bleedDepthPx, strip.height(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
            }
            return strip.scaled(strip.width(), bleedDepthPx, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }
    }

    return QImage();
}

} // namespace PDFBleedFixupMath

PDFOperationResult PDFBleedFixup::apply(PDFDocument* document,
                                        const PDFBleedFixupSettings& settings,
                                        PDFBleedFixupReport* report,
                                        PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    if (modificationFlags)
    {
        *modificationFlags = PDFModifiedDocument::ModificationFlags();
    }
    if (report)
    {
        report->pages.clear();
    }

    if (!document)
    {
        return PDFTranslationContext::tr("Invalid document.");
    }
    if (settings.dpi <= 0)
    {
        return PDFTranslationContext::tr("DPI must be positive.");
    }
    if (settings.samplePixels <= 0)
    {
        return PDFTranslationContext::tr("Sample pixel count must be positive.");
    }

    QString pageSelectionError;
    const std::vector<PDFInteger> pageIndices = selectPageIndices(document, settings.pageRange, &pageSelectionError);
    if (!pageSelectionError.isEmpty())
    {
        return pageSelectionError;
    }
    if (pageIndices.empty())
    {
        return true;
    }

    PDFOptionalContentActivity optionalContentActivity(document, OCUsage::Export, nullptr);
    PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    PDFCMSPointer cms = cmsManager.getCurrentCMS();
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFModifiedDocument md(document, &optionalContentActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);

    PDFMeshQualitySettings meshQualitySettings;
    PDFRenderer::Features features = settings.renderFeatures;
    features.setFlag(PDFRenderer::ClipToCropBox, false);
    features.setFlag(PDFRenderer::DisplayAnnotations, false);

    PDFRenderer renderer(document, &fontCache, cms.get(), &optionalContentActivity, features, meshQualitySettings);
    PDFRasterizer rasterizer(nullptr);
    rasterizer.reset(RendererEngine::QPainter);

    PDFDocumentModifier modifier(document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    Q_ASSERT(builder);

    PDFModifiedDocument::ModificationFlags flags = PDFModifiedDocument::ModificationFlags(PDFModifiedDocument::Reset | PDFModifiedDocument::PreserveUndoRedo);
    bool isPageContentChanged = false;

    const PDFBleedFixupSide allSides[4] = {
        PDFBleedFixupSide::Left,
        PDFBleedFixupSide::Bottom,
        PDFBleedFixupSide::Right,
        PDFBleedFixupSide::Top
    };

    for (const PDFInteger pageIndex : pageIndices)
    {
        const PDFPage* page = document->getCatalog()->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        PDFBleedFixupPageReport pageReport;
        pageReport.pageIndex = pageIndex;
        pageReport.originalMediaBox = page->getMediaBox();
        pageReport.originalCropBox = page->getCropBox();
        pageReport.originalBleedBox = page->getBleedBox();
        pageReport.originalTrimBox = page->getTrimBox();

        const QRectF reference = PDFBleedFixupMath::referenceRect(page, settings.referenceBox);
        if (!reference.isValid() || reference.isEmpty())
        {
            return PDFTranslationContext::tr("Reference box on page %1 is invalid.").arg(pageIndex + 1);
        }

        const QRectF targetBleed = PDFBleedFixupMath::targetBleedRect(reference, settings.bleedMM);
        if (!targetBleed.isValid() || targetBleed.isEmpty())
        {
            return PDFTranslationContext::tr("Target bleed box on page %1 is invalid.").arg(pageIndex + 1);
        }

        struct SideWork
        {
            PDFBleedFixupSide side = PDFBleedFixupSide::Left;
            PDFReal depthPt = 0.0;
        };
        std::vector<SideWork> sidesToApply;

        for (PDFBleedFixupSide side : allSides)
        {
            const PDFReal depthMM = PDFBleedFixupMath::sideBleedMM(settings.bleedMM, side);
            if (!(depthMM > 0.0))
            {
                continue;
            }

            const PDFReal depthPt = depthMM * PDF_MM_TO_POINT;
            if (!settings.force && settings.skipIfAlreadyBleeding &&
                PDFBleedFixupMath::sideAlreadyBleeding(reference, page->getBleedBox(), side, depthPt))
            {
                pageReport.skipReasons.append(PDFTranslationContext::tr("Skipped %1: BleedBox already sufficient.")
                                              .arg(sideName(side)));
                continue;
            }

            sidesToApply.push_back(SideWork{side, depthPt});
        }

        QRectF newBleed = page->getBleedBox();
        QRectF newCrop = page->getCropBox();
        QRectF newMedia = page->getMediaBox();
        QRectF newTrim = page->getTrimBox();

        if (settings.expandBleedBox)
        {
            newBleed = PDFBleedFixupMath::expandBoxTo(newBleed, targetBleed);
        }
        if (settings.expandCropBox)
        {
            newCrop = PDFBleedFixupMath::expandBoxTo(newCrop, newBleed);
        }
        if (settings.expandMediaBox)
        {
            newMedia = PDFBleedFixupMath::expandBoxTo(newMedia, newCrop);
        }
        if (settings.expandTrimBox)
        {
            newTrim = PDFBleedFixupMath::expandBoxTo(newTrim, targetBleed);
        }

        const PDFObjectReference pageReference = page->getPageReference();
        const PDFReal translateX = -newMedia.left();
        const PDFReal translateY = -newMedia.top();
        const bool needsOriginShift = !qFuzzyIsNull(translateX) || !qFuzzyIsNull(translateY);
        const QRectF normalizedMedia(0.0, 0.0, newMedia.width(), newMedia.height());

        auto shiftRect = [translateX, translateY](const QRectF& rect) -> QRectF
        {
            return QRectF(rect.left() + translateX, rect.top() + translateY, rect.width(), rect.height());
        };

        QImage pageImage;
        QTransform pageToDevice;
        if (!sidesToApply.empty())
        {
            PDFPrecompiledPage compiledPage;
            renderer.compile(&compiledPage, static_cast<size_t>(pageIndex));

            const QSizeF mediaSize = page->getRotatedMediaBox().size();
            const int widthPx = qMax(1, qCeil(mediaSize.width() * PDF_POINT_TO_INCH * settings.dpi));
            const int heightPx = qMax(1, qCeil(mediaSize.height() * PDF_POINT_TO_INCH * settings.dpi));
            const QSize imageSize(widthPx, heightPx);
            pageToDevice = PDFRenderer::createPagePointToDevicePointMatrix(page, QRect(QPoint(0, 0), imageSize));
            pageImage = rasterizer.render(pageIndex, page, &compiledPage, imageSize, features, nullptr, cms.get(), PageRotation::None);
            if (pageImage.isNull())
            {
                return PDFTranslationContext::tr("Failed to rasterize page %1.").arg(pageIndex + 1);
            }
        }

        if (needsOriginShift)
        {
            prependContentTranslate(builder, pageReference, translateX, translateY);
            isPageContentChanged = true;
        }

        if (settings.expandMediaBox || needsOriginShift)
        {
            builder->setPageMediaBox(pageReference, normalizedMedia);
        }
        if (settings.expandCropBox || needsOriginShift)
        {
            builder->setPageCropBox(pageReference, shiftRect(newCrop));
        }
        if (settings.expandBleedBox || needsOriginShift)
        {
            builder->setPageBleedBox(pageReference, shiftRect(newBleed));
        }
        // Always rewrite Trim when origin shifts so it stays aligned with content.
        builder->setPageTrimBox(pageReference, shiftRect(settings.expandTrimBox ? newTrim : page->getTrimBox()));

        pageReport.newMediaBox = normalizedMedia;
        pageReport.newCropBox = shiftRect(newCrop);
        pageReport.newBleedBox = shiftRect(newBleed);
        pageReport.newTrimBox = shiftRect(settings.expandTrimBox ? newTrim : page->getTrimBox());

        if (!sidesToApply.empty())
        {
            PDFPageContentStreamBuilder pageContentStreamBuilder(builder,
                                                                 PDFContentStreamBuilder::CoordinateSystem::PDF,
                                                                 PDFPageContentStreamBuilder::Mode::PlaceBefore);
            QPainter* painter = pageContentStreamBuilder.begin(pageReference);
            if (!painter)
            {
                return PDFTranslationContext::tr("Failed to open content stream for page %1.").arg(pageIndex + 1);
            }

            for (const SideWork& work : sidesToApply)
            {
                const PDFReal sampleDepthPt = (settings.mode == PDFBleedFixupMode::Mirror)
                        ? work.depthPt
                        : (72.0 / PDFReal(settings.dpi)) * PDFReal(settings.samplePixels);

                const QRectF sourcePageRect = PDFBleedFixupMath::edgeStripSourceRect(reference, work.side, sampleDepthPt);
                const QRectF destPageRect = PDFBleedFixupMath::edgeStripDestRect(reference, work.side, work.depthPt);
                if (!sourcePageRect.isValid() || !destPageRect.isValid())
                {
                    continue;
                }

                const QRect sourcePx = mapPageRectToImage(sourcePageRect, pageToDevice, pageImage.size());
                const int bleedDepthPx = qMax(1, qCeil(work.depthPt * settings.dpi / 72.0));
                QImage fill = PDFBleedFixupMath::buildEdgeFillImage(pageImage, sourcePx, work.side, settings.mode, bleedDepthPx);
                if (fill.isNull())
                {
                    pageReport.skipReasons.append(PDFTranslationContext::tr("Empty edge strip on %1.").arg(sideName(work.side)));
                    continue;
                }

                painter->drawImage(shiftRect(destPageRect), fill);
                pageReport.sidesApplied.append(work.side);
                isPageContentChanged = true;
            }

            pageContentStreamBuilder.end(painter);
        }

        if (report)
        {
            report->pages.append(pageReport);
        }
    }

    modifier.markReset();
    if (isPageContentChanged)
    {
        modifier.markPageContentsChanged();
        flags |= PDFModifiedDocument::PageContents;
    }

    if (modifier.finalize())
    {
        *document = *modifier.getDocument();
    }

    if (modificationFlags)
    {
        *modificationFlags = flags;
    }

    return true;
}

} // namespace pdf
