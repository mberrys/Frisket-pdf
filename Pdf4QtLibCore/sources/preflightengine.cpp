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

#include "preflightengine.h"

#include "pdfbleedmarginprobe.h"
#include "pdfcatalog.h"
#include "pdfcms.h"
#include "pdfcolorspaces.h"
#include "pdfconstants.h"
#include "pdfdocument.h"
#include "pdfexception.h"
#include "pdffont.h"
#include "pdfglobal.h"
#include "pdfimage.h"
#include "pdfmeshqualitysettings.h"
#include "pdfoptionalcontent.h"
#include "pdfpage.h"
#include "pdfpagecontentprocessor.h"
#include "pdfpreflightchecks.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>

namespace pdf
{

namespace
{

QJsonArray rectToBbox(const QRectF& rect)
{
    return QJsonArray{ rect.left(), rect.top(), rect.right(), rect.bottom() };
}

bool hasMeaningfulBbox(const QRectF& rect)
{
    return rect.isValid() && rect.width() > 0.0 && rect.height() > 0.0;
}

QJsonObject findingToJson(const PreflightFinding& finding)
{
    QJsonObject object;
    object.insert(QStringLiteral("scope"), finding.scope);
    object.insert(QStringLiteral("type"), finding.type);
    object.insert(QStringLiteral("severity"), finding.severity);
    object.insert(QStringLiteral("message"), finding.message);

    if (finding.scope != QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_DOCUMENT))
    {
        object.insert(QStringLiteral("page"), finding.page);
    }

    if (!finding.objectId.isNull())
    {
        object.insert(QStringLiteral("object_id"), finding.objectId.isEmpty() ? QJsonValue::Null : QJsonValue(finding.objectId));
    }

    if (hasMeaningfulBbox(finding.bbox))
    {
        object.insert(QStringLiteral("bbox"), rectToBbox(finding.bbox));
    }

    if (!finding.checkId.isEmpty())
    {
        object.insert(QStringLiteral("check_id"), finding.checkId);
    }

    return object;
}

constexpr int PREFLIGHT_MAX_FORM_DEPTH = 32;

QString classifyPaintedColorSpace(const PDFAbstractColorSpace* colorSpace)
{
    if (!colorSpace)
    {
        return QString();
    }

    const PDFAbstractColorSpace* base = colorSpace;
    while (base && base->getColorSpace() == PDFAbstractColorSpace::ColorSpace::Indexed)
    {
        base = static_cast<const PDFIndexedColorSpace*>(base)->getBaseColorSpace().get();
    }

    if (!base)
    {
        return QString();
    }

    switch (base->getColorSpace())
    {
        case PDFAbstractColorSpace::ColorSpace::DeviceRGB:
            return QStringLiteral("DeviceRGB");
        case PDFAbstractColorSpace::ColorSpace::DeviceCMYK:
            return QStringLiteral("DeviceCMYK");
        case PDFAbstractColorSpace::ColorSpace::DeviceGray:
            return QStringLiteral("DeviceGray");
        case PDFAbstractColorSpace::ColorSpace::CalRGB:
        case PDFAbstractColorSpace::ColorSpace::ICCBased:
        case PDFAbstractColorSpace::ColorSpace::Lab:
            return QStringLiteral("DeviceRGB");
        case PDFAbstractColorSpace::ColorSpace::CalGray:
            return QStringLiteral("DeviceGray");
        case PDFAbstractColorSpace::ColorSpace::DeviceN:
        case PDFAbstractColorSpace::ColorSpace::Separation:
            return QStringLiteral("DeviceCMYK");
        default:
            return QString();
    }
}

void recordPaintedColorSpace(const PDFAbstractColorSpace* colorSpace, QSet<QString>* paintedSpaces)
{
    if (!paintedSpaces)
    {
        return;
    }

    const QString name = classifyPaintedColorSpace(colorSpace);
    if (!name.isEmpty())
    {
        paintedSpaces->insert(name);
    }
}

void recordPaintedImageColorSpace(const PDFImage& image, QSet<QString>* paintedSpaces)
{
    if (!paintedSpaces)
    {
        return;
    }

    const QString name = classifyPaintedColorSpace(image.getColorSpace().get());
    if (!name.isEmpty())
    {
        paintedSpaces->insert(name);
        return;
    }

    switch (image.getImageData().getComponents())
    {
        case 1:
            paintedSpaces->insert(QStringLiteral("DeviceGray"));
            break;
        case 3:
            paintedSpaces->insert(QStringLiteral("DeviceRGB"));
            break;
        case 4:
            paintedSpaces->insert(QStringLiteral("DeviceCMYK"));
            break;
        default:
            break;
    }
}

void recordColorSpaceObject(const PDFDocument* document,
                            const PDFDictionary* colorSpaceDictionary,
                            const PDFObject& colorSpaceObject,
                            QSet<QString>* paintedSpaces)
{
    if (!document || !paintedSpaces)
    {
        return;
    }

    if (!colorSpaceObject.isName() && !colorSpaceObject.isArray())
    {
        return;
    }

    try
    {
        const PDFColorSpacePointer colorSpace = PDFAbstractColorSpace::createColorSpace(
            colorSpaceDictionary, document, colorSpaceObject);
        recordPaintedColorSpace(colorSpace.get(), paintedSpaces);
    }
    catch (const PDFException&)
    {
        // Ignore invalid color space entries.
    }
}

void collectColorSpacesFromResources(const PDFDocument* document,
                                     const PDFObject& resourcesObject,
                                     QSet<QString>* paintedSpaces,
                                     std::set<PDFObjectReference>& visitedForms,
                                     int depth)
{
    if (!document || !paintedSpaces || depth > PREFLIGHT_MAX_FORM_DEPTH)
    {
        return;
    }

    const PDFObject resources = document->getObject(resourcesObject);
    if (!resources.isDictionary())
    {
        return;
    }

    const PDFDictionary* resourcesDict = resources.getDictionary();
    const PDFDictionary* colorSpaceDictionary = document->getDictionaryFromObject(resourcesDict->get("ColorSpace"));

    if (colorSpaceDictionary)
    {
        for (size_t i = 0; i < colorSpaceDictionary->getCount(); ++i)
        {
            recordColorSpaceObject(document,
                                   colorSpaceDictionary,
                                   document->getObject(colorSpaceDictionary->getValue(i)),
                                   paintedSpaces);
        }
    }

    const PDFDictionary* xobjectDict = document->getDictionaryFromObject(resourcesDict->get("XObject"));
    if (!xobjectDict)
    {
        return;
    }

    PDFDocumentDataLoaderDecorator loader(document);
    for (size_t i = 0; i < xobjectDict->getCount(); ++i)
    {
        const PDFObject& entry = xobjectDict->getValue(i);
        const PDFObject xobject = document->getObject(entry);
        if (!xobject.isStream())
        {
            continue;
        }

        const PDFDictionary* streamDict = xobject.getStream()->getDictionary();
        if (!streamDict)
        {
            continue;
        }

        const QByteArray subtype = loader.readNameFromDictionary(streamDict, "Subtype");
        if (subtype == "Image")
        {
            if (streamDict->hasKey("ColorSpace"))
            {
                recordColorSpaceObject(document,
                                       colorSpaceDictionary,
                                       document->getObject(streamDict->get("ColorSpace")),
                                       paintedSpaces);
            }
        }
        else if (subtype == "Form")
        {
            PDFObjectReference formRef;
            if (entry.isReference())
            {
                formRef = entry.getReference();
            }
            else if (xobject.getReference().isValid())
            {
                formRef = xobject.getReference();
            }

            if (formRef.isValid() && visitedForms.insert(formRef).second)
            {
                collectColorSpacesFromResources(document,
                                                streamDict->get("Resources"),
                                                paintedSpaces,
                                                visitedForms,
                                                depth + 1);
            }
        }
    }
}

QRectF imageBoundsFromCtm(const QTransform& ctm, int pixelWidth, int pixelHeight)
{
    const QPointF corners[4] = {
        ctm.map(QPointF(0, 0)),
        ctm.map(QPointF(pixelWidth, 0)),
        ctm.map(QPointF(pixelWidth, pixelHeight)),
        ctm.map(QPointF(0, pixelHeight))
    };

    QRectF bounds;
    for (const QPointF& corner : corners)
    {
        bounds |= QRectF(corner, corner);
    }
    return bounds.normalized();
}

void processAnnotationAppearanceStreams(PDFDocument* document,
                                        const PDFPage* page,
                                        int pageNumber,
                                        const std::function<void(const PDFPage*, PDFObjectReference)>& processForm)
{
    if (!document || !page)
    {
        return;
    }

    for (const PDFObjectReference& annotRef : page->getAnnotations())
    {
        PDFObject annotObject = document->getObjectByReference(annotRef);
        if (!annotObject.isDictionary())
        {
            continue;
        }

        const PDFDictionary* annotDict = annotObject.getDictionary();
        PDFObject appearance = document->getObject(annotDict->get("AP"));
        if (!appearance.isDictionary())
        {
            continue;
        }

        const PDFDictionary* apDict = appearance.getDictionary();
        for (size_t i = 0; i < apDict->getCount(); ++i)
        {
            PDFObject appearanceStream = document->getObject(apDict->getValue(i));
            if (appearanceStream.isDictionary())
            {
                const PDFDictionary* stateDict = appearanceStream.getDictionary();
                for (size_t j = 0; j < stateDict->getCount(); ++j)
                {
                    PDFObject nested = document->getObject(stateDict->getValue(j));
                    if (nested.isStream())
                    {
                        processForm(page, nested.getReference());
                    }
                }
            }
            else if (appearanceStream.isStream())
            {
                processForm(page, appearanceStream.getReference());
            }
        }
    }

    Q_UNUSED(pageNumber);
}

QString defaultFixupDescription(const QString& fixupId)
{
    if (fixupId == QStringLiteral("rgb-to-cmyk"))
    {
        return PDFTranslationContext::tr("Convert all RGB colors to CMYK");
    }
    if (fixupId == QStringLiteral("add-bleed"))
    {
        return PDFTranslationContext::tr("Extend page boxes / artwork to provide bleed");
    }
    if (fixupId == QStringLiteral("downsample-images"))
    {
        return PDFTranslationContext::tr("Downsample images above target DPI");
    }

    return PDFTranslationContext::tr("Apply fixup '%1'").arg(fixupId);
}

bool isBleedAdequate(const PDFPage* page, qreal amountPt, qreal tolerancePt, QRectF& bboxForReport)
{
    if (!page)
    {
        bboxForReport = QRectF();
        return false;
    }

    const QRectF media = page->getMediaBox().normalized();
    const QRectF trim = preflight::resolveEffectiveBox(page->getTrimBox(), page->getCropBox(), media);
    const QRectF bleed = page->getBleedBox().normalized();
    bboxForReport = bleed.isEmpty() ? media : bleed;

    return preflight::bleedAdequate(trim, bleed, amountPt, tolerancePt);
}

void runBleedCheck(PDFDocumentSession* session,
                   const PreflightCheckConfig& check,
                   QList<PreflightFinding>& errors,
                   QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    const PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        QRectF bbox;
        const bool adequate = isBleedAdequate(page, check.amountPt, check.tolerancePt, bbox);
        if (adequate)
        {
            continue;
        }

        PreflightFinding finding;
        finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_PAGE);
        finding.page = int(pageIndex + 1);
        finding.objectId = QString();
        finding.type = QStringLiteral("bleed");
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.bbox = bbox;
        finding.message = PDFTranslationContext::tr("BleedBox is missing or less than %1 pt on one or more edges").arg(check.amountPt);

        if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
        {
            warnings.push_back(finding);
        }
        else
        {
            errors.push_back(finding);
        }
    }
}

enum class SizeCheckKind
{
    Trim,       ///< Measures the TrimBox (with trim -> crop -> media fallback).
    PageSize    ///< Measures the MediaBox (physical page size).
};

void runSizeCheck(SizeCheckKind kind,
                  PDFDocumentSession* session,
                  const PreflightCheckConfig& check,
                  QList<PreflightFinding>& errors,
                  QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    if (!check.hasExpectedSize)
    {
        return;
    }

    const PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const QString type = (kind == SizeCheckKind::Trim) ? QStringLiteral("trim") : QStringLiteral("page-size");
    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        const QRectF media = page->getMediaBox().normalized();
        const QRectF box = (kind == SizeCheckKind::Trim)
                               ? preflight::resolveEffectiveBox(page->getTrimBox(), page->getCropBox(), media)
                               : media;

        if (preflight::sizeWithinTolerance(box.width(), box.height(), check.expectedWidthPt, check.expectedHeightPt, check.tolerancePt))
        {
            continue;
        }

        PreflightFinding finding;
        finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_PAGE);
        finding.page = int(pageIndex + 1);
        finding.objectId = QString();
        finding.type = type;
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.bbox = box;
        finding.message = (kind == SizeCheckKind::Trim)
                              ? PDFTranslationContext::tr("TrimBox %1 x %2 pt does not match expected %3 x %4 pt (tolerance %5 pt)")
                                    .arg(box.width()).arg(box.height()).arg(check.expectedWidthPt).arg(check.expectedHeightPt).arg(check.tolerancePt)
                              : PDFTranslationContext::tr("Page size %1 x %2 pt does not match expected %3 x %4 pt (tolerance %5 pt)")
                                    .arg(box.width()).arg(box.height()).arg(check.expectedWidthPt).arg(check.expectedHeightPt).arg(check.tolerancePt);

        if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
        {
            warnings.push_back(finding);
        }
        else
        {
            errors.push_back(finding);
        }
    }
}

QString sideNameForFinding(PDFBleedFixupSide side)
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

void pushPreflightFinding(const PreflightFinding& finding,
                          const QString& severity,
                          QList<PreflightFinding>& errors,
                          QList<PreflightFinding>& warnings)
{
    if (severity == QStringLiteral("warning") || severity == QStringLiteral("info"))
    {
        warnings.push_back(finding);
    }
    else
    {
        errors.push_back(finding);
    }
}

bool edgeHasContent(const PDFBleedMarginProbeResult& result, PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return result.left.hasContent;
        case PDFBleedFixupSide::Right: return result.right.hasContent;
        case PDFBleedFixupSide::Top: return result.top.hasContent;
        case PDFBleedFixupSide::Bottom: return result.bottom.hasContent;
    }
    return false;
}

QRectF edgeStripRect(const PDFBleedMarginProbeResult& result, PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return result.left.stripRect;
        case PDFBleedFixupSide::Right: return result.right.stripRect;
        case PDFBleedFixupSide::Top: return result.top.stripRect;
        case PDFBleedFixupSide::Bottom: return result.bottom.stripRect;
    }
    return QRectF();
}

void emitNeedsAutoBleedFinding(int pageNumber,
                               const QRectF& pageBbox,
                               const PreflightCheckConfig& check,
                               QList<PreflightFinding>& errors,
                               QList<PreflightFinding>& warnings)
{
    PreflightFinding finding;
    finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_PAGE);
    finding.page = pageNumber;
    finding.objectId = QString();
    finding.type = QStringLiteral("needs-auto-bleed");
    finding.severity = QStringLiteral("info");
    finding.checkId = check.id;
    finding.bbox = pageBbox;
    finding.message = PDFTranslationContext::tr("Page is a candidate for the add-bleed fixup");
    pushPreflightFinding(finding, finding.severity, errors, warnings);
}

void runContentBleedCheck(PDFDocumentSession* session,
                           const PreflightCheckConfig& check,
                           QList<PreflightFinding>& errors,
                           QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    PDFBleedMarginProbe probe(session);

    PDFBleedMarginProbeSettings probeSettings;
    const PDFReal bleedMm = convertPDFPointToMM(check.amountPt);
    probeSettings.bleedMM = QMarginsF(bleedMm, bleedMm, bleedMm, bleedMm);
    probeSettings.dpi = check.probeDpi;
    probeSettings.threshold = check.probeThreshold;
    probeSettings.whiteCoverageThreshold = check.rasterWhiteThreshold;
    probeSettings.fastOnly = !check.rasterConfirm;

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        const PDFBleedMarginProbeResult result = probe.probe(page, static_cast<size_t>(pageIndex), probeSettings);

        if (result.allEdgesCovered())
        {
            continue;
        }

        const PDFBleedFixupSide sides[4] = {
            PDFBleedFixupSide::Left, PDFBleedFixupSide::Right,
            PDFBleedFixupSide::Top, PDFBleedFixupSide::Bottom
        };

        bool pageHasBleedGap = false;

        if (check.rasterConfirm)
        {
            for (PDFBleedFixupSide side : sides)
            {
                if (edgeHasContent(result, side))
                {
                    continue;
                }

                const QRectF stripRect = edgeStripRect(result, side);
                PreflightFinding finding;
                finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_OBJECT);
                finding.page = int(pageIndex + 1);
                finding.objectId = QString();
                finding.type = QStringLiteral("bleed-margin-empty");
                finding.severity = check.severity;
                finding.checkId = check.id;
                finding.bbox = stripRect.isValid() ? stripRect : QRectF();
                finding.message = PDFTranslationContext::tr("Bleed margin empty on %1 edge").arg(sideNameForFinding(side));
                pushPreflightFinding(finding, check.severity, errors, warnings);
                pageHasBleedGap = true;
            }
        }
        else
        {
            QStringList missingSides;
            QRectF unionMissingBbox;
            for (PDFBleedFixupSide side : sides)
            {
                if (edgeHasContent(result, side))
                {
                    continue;
                }

                missingSides.append(sideNameForFinding(side));
                const QRectF stripRect = edgeStripRect(result, side);
                if (stripRect.isValid())
                {
                    unionMissingBbox = unionMissingBbox.united(stripRect);
                }
            }

            if (missingSides.isEmpty())
            {
                continue;
            }

            PreflightFinding finding;
            finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_OBJECT);
            finding.page = int(pageIndex + 1);
            finding.objectId = QString();
            finding.type = QStringLiteral("content-bleed");
            finding.severity = check.severity;
            finding.checkId = check.id;
            finding.bbox = unionMissingBbox.isValid() ? unionMissingBbox : QRectF();
            finding.message = PDFTranslationContext::tr("Artwork does not extend into bleed margin on %1").arg(missingSides.join(QStringLiteral(", ")));
            pushPreflightFinding(finding, check.severity, errors, warnings);
            pageHasBleedGap = true;
        }

        if (pageHasBleedGap)
        {
            const QRectF media = page->getMediaBox().normalized();
            const QRectF pageBbox = preflight::resolveEffectiveBox(page->getTrimBox(), page->getCropBox(), media);
            emitNeedsAutoBleedFinding(int(pageIndex + 1), pageBbox, check, errors, warnings);
        }
    }
}

bool hasBleedGapFinding(const QList<PreflightFinding>& findings)
{
    for (const PreflightFinding& finding : findings)
    {
        if (finding.checkId == QStringLiteral("bleed")
            || finding.type == QStringLiteral("content-bleed")
            || finding.type == QStringLiteral("bleed-margin-empty")
            || finding.type == QStringLiteral("needs-auto-bleed"))
        {
            return true;
        }
    }
    return false;
}

void adjustFixupsAvailable(QList<PreflightFixupConfig>& fixups, bool needsAddBleed, qreal addBleedAmountPt)
{
    PreflightFixupConfig addBleedConfig;
    bool hasProfileAddBleed = false;

    for (const PreflightFixupConfig& fixup : fixups)
    {
        if (fixup.id == QStringLiteral("add-bleed"))
        {
            addBleedConfig = fixup;
            hasProfileAddBleed = true;
            break;
        }
    }

    // Remove advertised fixups; only implemented fixups are re-added below.
    auto it = std::remove_if(fixups.begin(), fixups.end(), [](const PreflightFixupConfig& fixup)
    {
        return fixup.id == QStringLiteral("add-bleed")
            || fixup.id == QStringLiteral("rgb-to-cmyk")
            || fixup.id == QStringLiteral("downsample-images");
    });
    fixups.erase(it, fixups.end());

    if (needsAddBleed)
    {
        if (!hasProfileAddBleed)
        {
            addBleedConfig.id = QStringLiteral("add-bleed");
            addBleedConfig.confirm = true;
        }
        if (addBleedConfig.description.isEmpty())
        {
            addBleedConfig.description = PDFTranslationContext::tr("Extend page boxes / artwork to provide bleed");
        }
        if (addBleedConfig.amountPt <= 0.0 && addBleedAmountPt > 0.0)
        {
            addBleedConfig.amountPt = addBleedAmountPt;
        }

        QJsonObject params = addBleedConfig.params;
        if (!params.contains(QStringLiteral("mode")))
        {
            params.insert(QStringLiteral("mode"), QStringLiteral("mirror"));
        }
        addBleedConfig.params = params;

        fixups.push_back(addBleedConfig);
    }
}

void runColorModeCheck(PDFDocumentSession* session,
                       const PreflightCheckConfig& check,
                       QList<PreflightFinding>& errors,
                       QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    if (check.allowedColorModes.isEmpty())
    {
        return;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    QSet<QString> allowed;
    for (const QString& mode : check.allowedColorModes)
    {
        if (mode.compare(QStringLiteral("RGB"), Qt::CaseInsensitive) == 0)
        {
            allowed.insert(QStringLiteral("DeviceRGB"));
        }
        else if (mode.compare(QStringLiteral("CMYK"), Qt::CaseInsensitive) == 0)
        {
            allowed.insert(QStringLiteral("DeviceCMYK"));
        }
        else if (mode.compare(QStringLiteral("Grayscale"), Qt::CaseInsensitive) == 0)
        {
            allowed.insert(QStringLiteral("DeviceGray"));
        }
    }

    PDFOptionalContentActivity ocActivity(document, OCUsage::Export, nullptr);
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFModifiedDocument md(document, &ocActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);
    PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    PDFCMSPointer cms = cmsManager.getCurrentCMS();
    PDFMeshQualitySettings meshQuality;

    class ColorModeProcessor : public PDFPageContentProcessor
    {
    public:
        ColorModeProcessor(const PDFPage* page,
                           const PDFDocument* doc,
                           const PDFFontCache* fc,
                           const PDFCMS* cms_p,
                           const PDFOptionalContentActivity* oc,
                           const PDFMeshQualitySettings& mq,
                           QSet<QString>* paintedSpaces)
            : PDFPageContentProcessor(page, doc, fc, cms_p, oc, QTransform(), mq)
            , m_paintedSpaces(paintedSpaces)
        {
        }

        void processFormStream(const PDFStream* stream)
        {
            if (!stream || isContentSuppressed())
            {
                return;
            }
            processForm(stream);
        }

    protected:
        bool isContentKindSuppressed(ContentKind kind) const override
        {
            switch (kind)
            {
                case ContentKind::Images:
                case ContentKind::Tiling:
                case ContentKind::Forms:
                case ContentKind::Shapes:
                case ContentKind::Text:
                    return false;
                default:
                    return true;
            }
        }

        void performPathPainting(const QPainterPath& path, bool stroke, bool fill, bool text, Qt::FillRule fillRule) override
        {
            Q_UNUSED(path);
            Q_UNUSED(text);
            Q_UNUSED(fillRule);

            if (isContentSuppressed())
            {
                return;
            }

            const PDFPageContentProcessorState* state = getGraphicState();
            if (fill)
            {
                recordPaintedColorSpace(state->getFillColorSpace(), m_paintedSpaces);
            }
            if (stroke)
            {
                recordPaintedColorSpace(state->getStrokeColorSpace(), m_paintedSpaces);
            }
        }

        bool performOriginalImagePainting(const PDFImage& image,
                                           const PDFStream* stream,
                                           PDFObjectReference reference) override
        {
            Q_UNUSED(reference);
            if (isContentSuppressed())
            {
                return true;
            }

            if (stream)
            {
                const PDFDictionary* streamDictionary = stream->getDictionary();
                if (streamDictionary && streamDictionary->hasKey("ColorSpace"))
                {
                    recordColorSpaceObject(getDocument(),
                                           getColorSpaceDictionary(),
                                           getDocument()->getObject(streamDictionary->get("ColorSpace")),
                                           m_paintedSpaces);
                }
            }

            recordPaintedImageColorSpace(image, m_paintedSpaces);
            return true;
        }

    private:
        QSet<QString>* m_paintedSpaces;
    };

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        QSet<QString> paintedSpaces;
        std::set<PDFObjectReference> visitedForms;
        collectColorSpacesFromResources(document, page->getResources(), &paintedSpaces, visitedForms, 0);

        ColorModeProcessor processor(page, document, &fontCache, cms.get(), &ocActivity, meshQuality, &paintedSpaces);
        processor.processContents();

        processAnnotationAppearanceStreams(document, page, int(pageIndex + 1), [&](const PDFPage* /*pageRef*/, PDFObjectReference formRef) {
            PDFObject formObject = document->getObjectByReference(formRef);
            if (formObject.isStream())
            {
                processor.processFormStream(formObject.getStream());
            }
        });

        QStringList disallowed;
        for (const QString& cs : paintedSpaces)
        {
            if (!allowed.contains(cs))
            {
                disallowed.append(cs);
            }
        }

        if (!disallowed.isEmpty())
        {
            PreflightFinding finding;
            finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_PAGE);
            finding.page = int(pageIndex + 1);
            finding.type = QStringLiteral("color-mode");
            finding.severity = check.severity;
            finding.checkId = check.id;
            finding.bbox = QRectF();
            QString modeList;
            for (const QString& mode : check.allowedColorModes)
            {
                if (!modeList.isEmpty())
                {
                    modeList += QStringLiteral(", ");
                }
                modeList += mode;
            }
            finding.message = PDFTranslationContext::tr(
                "Disallowed color space(s) found on page %1: %2 (allowed: %3)")
                .arg(pageIndex + 1)
                .arg(disallowed.join(QStringLiteral(", ")))
                .arg(modeList);

            if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
            {
                warnings.push_back(finding);
            }
            else
            {
                errors.push_back(finding);
            }
        }
    }
}

bool isNearWhiteDevicePaint(const PDFAbstractColorSpace* colorSpace, const PDFColor& color)
{
    if (!colorSpace)
    {
        return false;
    }

    switch (colorSpace->getColorSpace())
    {
        case PDFAbstractColorSpace::ColorSpace::DeviceGray:
            return color[0] >= 0.99f;
        case PDFAbstractColorSpace::ColorSpace::DeviceRGB:
            return color[0] >= 0.99f && color[1] >= 0.99f && color[2] >= 0.99f;
        case PDFAbstractColorSpace::ColorSpace::DeviceCMYK:
            return color[0] <= 0.01f && color[1] <= 0.01f && color[2] <= 0.01f && color[3] <= 0.01f;
        default:
            return false;
    }
}

void runWhiteOverprintCheck(PDFDocumentSession* session,
                            const PreflightCheckConfig& check,
                            QList<PreflightFinding>& errors,
                            QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    PDFOptionalContentActivity ocActivity(document, OCUsage::Export, nullptr);
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFModifiedDocument md(document, &ocActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);
    PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    PDFCMSPointer cms = cmsManager.getCurrentCMS();
    PDFMeshQualitySettings meshQuality;

    class WhiteOverprintProcessor : public PDFPageContentProcessor
    {
    public:
        WhiteOverprintProcessor(const PDFPage* page,
                                const PDFDocument* doc,
                                const PDFFontCache* fc,
                                const PDFCMS* cms_p,
                                const PDFOptionalContentActivity* oc,
                                const PDFMeshQualitySettings& mq,
                                bool* foundWhiteOverprint)
            : PDFPageContentProcessor(page, doc, fc, cms_p, oc, QTransform(), mq)
            , m_foundWhiteOverprint(foundWhiteOverprint)
        {
        }

        void processFormStream(const PDFStream* stream)
        {
            if (!stream || isContentSuppressed())
            {
                return;
            }
            processForm(stream);
        }

    protected:
        bool isContentKindSuppressed(ContentKind kind) const override
        {
            switch (kind)
            {
                case ContentKind::Shapes:
                case ContentKind::Text:
                case ContentKind::Forms:
                    return false;
                default:
                    return true;
            }
        }

        void performPathPainting(const QPainterPath& path, bool stroke, bool fill, bool text, Qt::FillRule fillRule) override
        {
            Q_UNUSED(path);
            Q_UNUSED(text);
            Q_UNUSED(fillRule);

            if (isContentSuppressed() || m_foundWhiteOverprint == nullptr)
            {
                return;
            }

            const PDFPageContentProcessorState* state = getGraphicState();
            const PDFOverprintMode overprintMode = state->getOverprintMode();

            if (fill && overprintMode.overprintFilling
                && isNearWhiteDevicePaint(state->getFillColorSpace(), state->getFillColorOriginal()))
            {
                *m_foundWhiteOverprint = true;
            }

            if (stroke && overprintMode.overprintStroking
                && isNearWhiteDevicePaint(state->getStrokeColorSpace(), state->getStrokeColorOriginal()))
            {
                *m_foundWhiteOverprint = true;
            }
        }

    private:
        bool* m_foundWhiteOverprint = nullptr;
    };

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        bool foundWhiteOverprint = false;
        WhiteOverprintProcessor processor(page, document, &fontCache, cms.get(), &ocActivity, meshQuality, &foundWhiteOverprint);
        processor.processContents();

        processAnnotationAppearanceStreams(document, page, int(pageIndex + 1), [&](const PDFPage* /*pageRef*/, PDFObjectReference formRef) {
            PDFObject formObject = document->getObjectByReference(formRef);
            if (formObject.isStream())
            {
                processor.processFormStream(formObject.getStream());
            }
        });

        if (!foundWhiteOverprint)
        {
            continue;
        }

        PreflightFinding finding;
        finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_PAGE);
        finding.page = int(pageIndex + 1);
        finding.type = QStringLiteral("white-overprint");
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.message = PDFTranslationContext::tr(
            "White or near-white paint is set to overprint on page %1.")
                              .arg(pageIndex + 1);

        if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
        {
            warnings.push_back(finding);
        }
        else
        {
            errors.push_back(finding);
        }
    }
}

// Scans Font resource dictionaries on the page and nested Form XObjects /
// annotation appearance streams (resource recursion with cycle guard).
void runEmbeddedFontsCheck(PDFDocumentSession* session,
                            const PreflightCheckConfig& check,
                            QList<PreflightFinding>& errors,
                            QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();
    std::set<PDFObjectReference> processedFonts;
    std::set<PDFObjectReference> processedResources;

    auto emitFontFinding = [&](int pageNumber, const QString& message)
    {
        PreflightFinding finding;
        finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_OBJECT);
        finding.page = pageNumber;
        finding.type = QStringLiteral("embedded-fonts");
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.bbox = QRectF();
        finding.message = message;
        if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
        {
            warnings.push_back(finding);
        }
        else
        {
            errors.push_back(finding);
        }
    };

    std::function<void(const PDFObject&, int)> scanResources;
    scanResources = [&](const PDFObject& resourcesObject, int pageNumber)
    {
        PDFObject resources = document->getObject(resourcesObject);
        if (!resources.isDictionary())
        {
            return;
        }

        if (resourcesObject.isReference())
        {
            const PDFObjectReference ref = resourcesObject.getReference();
            if (processedResources.contains(ref))
            {
                return;
            }
            processedResources.insert(ref);
        }

        const PDFDictionary* fontsDict = document->getDictionaryFromObject(
            resources.getDictionary()->get("Font"));
        if (fontsDict)
        {
            for (size_t i = 0; i < fontsDict->getCount(); ++i)
            {
                PDFObject fontObj = fontsDict->getValue(i);
                if (fontObj.isReference())
                {
                    const PDFObjectReference ref = fontObj.getReference();
                    if (processedFonts.contains(ref))
                    {
                        continue;
                    }
                    processedFonts.insert(ref);
                }

                try
                {
                    PDFFontPointer font = PDFFont::createFont(
                        fontObj, fontsDict->getKey(i).getString(), document);
                    if (!font || font->getFontType() == FontType::Type3)
                    {
                        continue;
                    }

                    const FontDescriptor* fd = font->getFontDescriptor();
                    const QString keyName = QString::fromLatin1(fontsDict->getKey(i).getString());
                    if (!fd)
                    {
                        emitFontFinding(pageNumber,
                                        PDFTranslationContext::tr(
                                            "Font '%1' on page %2 has no font descriptor (not embedded)")
                                            .arg(keyName)
                                            .arg(pageNumber));
                        continue;
                    }

                    if (!fd->isEmbedded())
                    {
                        const QString fontName = fd->fontName.isEmpty() ? keyName : fd->fontName;
                        emitFontFinding(pageNumber,
                                        PDFTranslationContext::tr(
                                            "Font '%1' on page %2 is not embedded")
                                            .arg(fontName)
                                            .arg(pageNumber));
                    }
                }
                catch (const PDFException&)
                {
                }
            }
        }

        const PDFDictionary* xobjectDict = document->getDictionaryFromObject(
            resources.getDictionary()->get("XObject"));
        if (!xobjectDict)
        {
            return;
        }

        for (size_t i = 0; i < xobjectDict->getCount(); ++i)
        {
            PDFObject xobject = document->getObject(xobjectDict->getValue(i));
            if (!xobject.isStream())
            {
                continue;
            }

            const PDFDictionary* streamDict = xobject.getStream()->getDictionary();
            if (!streamDict)
            {
                continue;
            }

            const PDFObject subtype = document->getObject(streamDict->get("Subtype"));
            if (subtype.getString() != "Form")
            {
                continue;
            }

            if (streamDict->hasKey("Resources"))
            {
                scanResources(streamDict->get("Resources"), pageNumber);
            }
        }
    };

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        const int pageNumber = int(pageIndex + 1);
        scanResources(page->getResources(), pageNumber);

        for (const PDFObjectReference& annotRef : page->getAnnotations())
        {
            PDFObject annotObject = document->getObjectByReference(annotRef);
            if (!annotObject.isDictionary())
            {
                continue;
            }

            const PDFDictionary* annotDict = annotObject.getDictionary();
            PDFObject appearance = document->getObject(annotDict->get("AP"));
            if (!appearance.isDictionary())
            {
                continue;
            }

            const PDFDictionary* apDict = appearance.getDictionary();
            for (size_t i = 0; i < apDict->getCount(); ++i)
            {
                PDFObject appearanceStream = document->getObject(apDict->getValue(i));
                if (appearanceStream.isDictionary())
                {
                    const PDFDictionary* stateDict = appearanceStream.getDictionary();
                    for (size_t j = 0; j < stateDict->getCount(); ++j)
                    {
                        PDFObject nested = document->getObject(stateDict->getValue(j));
                        if (nested.isStream() && nested.getStream()->getDictionary()->hasKey("Resources"))
                        {
                            scanResources(nested.getStream()->getDictionary()->get("Resources"), pageNumber);
                        }
                    }
                }
                else if (appearanceStream.isStream() && appearanceStream.getStream()->getDictionary()->hasKey("Resources"))
                {
                    scanResources(appearanceStream.getStream()->getDictionary()->get("Resources"), pageNumber);
                }
            }
        }
    }
}

// LOW CONFIDENCE NOTE: DPI calculation uses getCurrentTransformationMatrix()
// from the PDFPageContentProcessor state, which is in PDF user space.
// This matches the existing PDFImageCollectorProcessor pattern in
// pdfimagecompressor.cpp. The identity QTransform passed to the processor
// constructor is correct because we only read the CTM from content stream
// operators, not render to a device. Page rotation is not explicitly handled
// but the existing pattern in calculateDpi works with the CTM as-is.
void runImageResolutionCheck(PDFDocumentSession* session,
                              const PreflightCheckConfig& check,
                              QList<PreflightFinding>& errors,
                              QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    if (check.minDpi <= 0)
    {
        return;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    PDFOptionalContentActivity ocActivity(document, OCUsage::Export, nullptr);
    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFModifiedDocument md(document, &ocActivity);
    fontCache.setDocument(md);
    fontCache.setCacheShrinkEnabled(nullptr, false);
    PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    PDFCMSPointer cms = cmsManager.getCurrentCMS();
    PDFMeshQualitySettings meshQuality;

    class ImageDpiProcessor : public PDFPageContentProcessor
    {
    public:
        struct ImageDpiInfo
        {
            PDFObjectReference ref;
            qreal minDpiX = std::numeric_limits<qreal>::max();
            qreal minDpiY = std::numeric_limits<qreal>::max();
            QRectF bbox;
        };

        ImageDpiProcessor(const PDFPage* page,
                          const PDFDocument* doc,
                          const PDFFontCache* fc,
                          const PDFCMS* cms_p,
                          const PDFOptionalContentActivity* oc,
                          const PDFMeshQualitySettings& mq,
                          std::vector<ImageDpiInfo>* results)
            : PDFPageContentProcessor(page, doc, fc, cms_p, oc, QTransform(), mq)
            , m_results(results)
        {
        }

        void processFormStream(const PDFStream* stream)
        {
            if (!stream || isContentSuppressed())
            {
                return;
            }
            processForm(stream);
        }

    protected:
        bool isContentKindSuppressed(ContentKind kind) const override
        {
            switch (kind)
            {
                case ContentKind::Images:
                case ContentKind::Tiling:
                case ContentKind::Forms:
                    return false;
                default:
                    return true;
            }
        }

        bool performOriginalImagePainting(const PDFImage& image,
                                           const PDFStream* stream,
                                           PDFObjectReference reference) override
        {
            Q_UNUSED(stream);
            if (isContentSuppressed())
            {
                return true;
            }

            const QTransform ctm = getGraphicState()->getCurrentTransformationMatrix();

            const auto axisLength = [](qreal x, qreal y) -> double {
                return std::hypot(static_cast<double>(x), static_cast<double>(y)) * PDF_POINT_TO_INCH;
            };

            const double widthInches = axisLength(ctm.m11(), ctm.m12());
            const double heightInches = axisLength(ctm.m21(), ctm.m22());

            if (widthInches <= std::numeric_limits<double>::epsilon() ||
                heightInches <= std::numeric_limits<double>::epsilon())
            {
                return true;
            }

            ImageDpiInfo info;
            info.ref = reference;
            info.minDpiX = static_cast<qreal>(image.getImageData().getWidth()) / widthInches;
            info.minDpiY = static_cast<qreal>(image.getImageData().getHeight()) / heightInches;
            info.bbox = imageBoundsFromCtm(ctm,
                                           image.getImageData().getWidth(),
                                           image.getImageData().getHeight());
            m_results->push_back(info);
            return true;
        }

    private:
        std::vector<ImageDpiInfo>* m_results;
    };

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        std::vector<ImageDpiProcessor::ImageDpiInfo> images;
        ImageDpiProcessor processor(page, document, &fontCache, cms.get(), &ocActivity, meshQuality, &images);
        processor.processContents();

        processAnnotationAppearanceStreams(document, page, int(pageIndex + 1), [&](const PDFPage* /*pageRef*/, PDFObjectReference formRef) {
            PDFObject formObject = document->getObjectByReference(formRef);
            if (formObject.isStream())
            {
                processor.processFormStream(formObject.getStream());
            }
        });

        for (const auto& img : images)
        {
            const qreal dpi = qMin(img.minDpiX, img.minDpiY);
            if (dpi < static_cast<qreal>(check.minDpi))
            {
                PreflightFinding finding;
                finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_OBJECT);
                finding.page = int(pageIndex + 1);
                finding.objectId = img.ref.isValid()
                    ? QString::number(img.ref.objectNumber) : QString();
                finding.type = QStringLiteral("image-resolution");
                finding.severity = check.severity;
                finding.checkId = check.id;
                finding.bbox = img.bbox;
                finding.message = PDFTranslationContext::tr(
                    "Image resolution %1 DPI is below minimum %2 DPI on page %3")
                    .arg(qRound(dpi))
                    .arg(check.minDpi)
                    .arg(pageIndex + 1);

                if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
                {
                    warnings.push_back(finding);
                }
                else
                {
                    errors.push_back(finding);
                }
            }
        }
    }
}

} // namespace

QJsonObject PreflightResult::toJson(const QString& pdfPath) const
{
    QJsonArray errorsArray;
    for (const PreflightFinding& finding : errors)
    {
        errorsArray.append(findingToJson(finding));
    }

    QJsonArray warningsArray;
    for (const PreflightFinding& finding : warnings)
    {
        warningsArray.append(findingToJson(finding));
    }

    QJsonArray fixupsArray;
    for (const PreflightFixupConfig& fixup : fixupsAvailable)
    {
        QJsonObject fixupObject;
        fixupObject.insert(QStringLiteral("id"), fixup.id);
        fixupObject.insert(QStringLiteral("safe"), false);
        fixupObject.insert(QStringLiteral("description"), fixup.description.isEmpty() ? defaultFixupDescription(fixup.id) : fixup.description);

        QJsonObject params = fixup.params;
        if (fixup.amountPt > 0.0 && !params.contains(QStringLiteral("amount_pt")))
        {
            params.insert(QStringLiteral("amount_pt"), fixup.amountPt);
        }
        if (!params.isEmpty())
        {
            fixupObject.insert(QStringLiteral("params"), params);
        }

        fixupsArray.append(fixupObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema_version"), PREFLIGHT_REPORT_SCHEMA_VERSION);
    root.insert(QStringLiteral("inspection_complete"), inspectionComplete);
    root.insert(QStringLiteral("pass"), pass);
    root.insert(QStringLiteral("profile"), profileName);
    root.insert(QStringLiteral("engine_version"), QCoreApplication::applicationVersion());
    if (!pdfPath.isEmpty())
    {
        root.insert(QStringLiteral("pdf"), pdfPath);
    }
    root.insert(QStringLiteral("errors"), errorsArray);
    root.insert(QStringLiteral("warnings"), warningsArray);
    root.insert(QStringLiteral("fixups_available"), fixupsArray);

    QJsonArray checksArray;
    for (const PreflightCheckStatus& status : checkStatuses)
    {
        QJsonObject checkObject;
        checkObject.insert(QStringLiteral("id"), status.id);
        checkObject.insert(QStringLiteral("status"), status.status);
        if (!status.reason.isEmpty())
        {
            checkObject.insert(QStringLiteral("reason"), status.reason);
        }
        checksArray.append(checkObject);
    }
    root.insert(QStringLiteral("checks"), checksArray);

    return root;
}

PreflightEngine::PreflightEngine(PDFDocumentSession* session) :
    m_session(session)
{
    registerBuiltInChecks();
}

void PreflightEngine::registerCheck(const QString& id, CheckRunner runner)
{
    m_checks[id] = std::move(runner);
}

bool PreflightEngine::hasCheck(const QString& id) const
{
    return m_checks.count(id) > 0;
}

PreflightResult PreflightEngine::run(const QJsonObject& profile)
{
    PreflightProfileData data;
    QString errorMessage;
    if (!parseProfile(profile, data, errorMessage))
    {
        // Profile parsing errors are treated as a failing preflight result so
        // callers can surface them in the same report shape.
        PreflightResult result;
        result.pass = false;
        result.profileName = profile.value(QStringLiteral("name")).toString();

        PreflightFinding finding;
        finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_DOCUMENT);
        finding.type = QStringLiteral("profile");
        finding.severity = QStringLiteral("error");
        finding.message = errorMessage;
        finding.bbox = QRectF();
        result.errors.push_back(finding);

        return result;
    }

    return run(data);
}

PreflightResult PreflightEngine::run(const PreflightProfileData& profile)
{
    PreflightResult result;
    result.profileName = profile.name;
    result.inspectionComplete = true;

    for (const PreflightCheckConfig& check : profile.checks)
    {
        PreflightCheckStatus status;
        status.id = check.id;

        if (!check.enabled)
        {
            status.status = QStringLiteral("skipped");
            status.reason = QStringLiteral("disabled");
            result.checkStatuses.push_back(status);
            continue;
        }

        auto it = m_checks.find(check.id);
        if (it == m_checks.end())
        {
            status.status = QStringLiteral("unsupported");
            status.reason = QStringLiteral("unknown check id");
            result.checkStatuses.push_back(status);
            result.inspectionComplete = false;

            PreflightFinding finding;
            finding.scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_DOCUMENT);
            finding.type = QStringLiteral("profile");
            finding.severity = QStringLiteral("error");
            finding.checkId = check.id;
            finding.message = PDFTranslationContext::tr("Unknown preflight check '%1'.").arg(check.id);
            result.errors.push_back(finding);
            continue;
        }

        const int errorsBefore = result.errors.size();
        const int warningsBefore = result.warnings.size();
        it->second(m_session, check, result.errors, result.warnings);

        const bool checkFailed = result.errors.size() > errorsBefore;
        const bool checkWarned = result.warnings.size() > warningsBefore;
        status.status = (checkFailed || checkWarned) ? QStringLiteral("failed") : QStringLiteral("ok");
        result.checkStatuses.push_back(status);
    }

    result.pass = result.errors.isEmpty() && result.inspectionComplete;
    result.fixupsAvailable = profile.fixups;

    qreal addBleedAmountPt = 0.0;
    for (const PreflightCheckConfig& check : profile.checks)
    {
        if (check.enabled && (check.id == QStringLiteral("bleed") || check.id == QStringLiteral("content-bleed")) && check.amountPt > 0.0)
        {
            addBleedAmountPt = check.amountPt;
            break;
        }
    }

    const bool needsAddBleed = hasBleedGapFinding(result.errors) || hasBleedGapFinding(result.warnings);
    adjustFixupsAvailable(result.fixupsAvailable, needsAddBleed, addBleedAmountPt);

    return result;
}

PDFDocumentSession* PreflightEngine::getSession() const
{
    return m_session;
}

bool PreflightEngine::loadProfile(const QString& profilePath, QJsonObject& profile, QString& errorMessage)
{
    QFile profileFile(profilePath);
    if (!profileFile.open(QIODevice::ReadOnly))
    {
        errorMessage = PDFTranslationContext::tr("Cannot open profile '%1'.").arg(profilePath);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(profileFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        errorMessage = PDFTranslationContext::tr("Invalid profile JSON in '%1': %2").arg(profilePath, parseError.errorString());
        return false;
    }

    profile = document.object();
    return true;
}

bool PreflightEngine::parseProfile(const QJsonObject& profileObject, PreflightProfileData& profile, QString& errorMessage)
{
    errorMessage.clear();
    profile = PreflightProfileData();

    profile.name = profileObject.value(QStringLiteral("name")).toString();
    if (profile.name.isEmpty())
    {
        errorMessage = PDFTranslationContext::tr("Profile is missing required field 'name'.");
        return false;
    }

    const QJsonArray checks = profileObject.value(QStringLiteral("checks")).toArray();
    if (checks.isEmpty())
    {
        errorMessage = PDFTranslationContext::tr("Profile must define at least one check.");
        return false;
    }

    for (const QJsonValue& checkValue : checks)
    {
        const QJsonObject checkObject = checkValue.toObject();
        PreflightCheckConfig check;
        check.id = checkObject.value(QStringLiteral("id")).toString();
        if (check.id.isEmpty())
        {
            errorMessage = PDFTranslationContext::tr("Profile contains a check without 'id'.");
            return false;
        }

        check.severity = checkObject.value(QStringLiteral("severity")).toString(check.severity);
        check.enabled = checkObject.value(QStringLiteral("enabled")).toBool(true);
        check.amountPt = checkObject.value(QStringLiteral("amount_pt")).toDouble(check.amountPt);
        check.required = checkObject.value(QStringLiteral("required")).toBool(check.required);
        check.expectedWidthPt = checkObject.value(QStringLiteral("expected_width_pt")).toDouble(check.expectedWidthPt);
        check.expectedHeightPt = checkObject.value(QStringLiteral("expected_height_pt")).toDouble(check.expectedHeightPt);
        check.tolerancePt = checkObject.value(QStringLiteral("tolerance_pt")).toDouble(check.tolerancePt);
        check.hasExpectedSize = check.expectedWidthPt > 0.0 && check.expectedHeightPt > 0.0;

        check.rasterConfirm = checkObject.value(QStringLiteral("raster_confirm")).toBool(false);
        check.probeDpi = checkObject.value(QStringLiteral("probe_dpi")).toInt(150);
        check.probeThreshold = checkObject.value(QStringLiteral("probe_threshold")).toInt(16);
        check.rasterWhiteThreshold = checkObject.value(QStringLiteral("raster_white_threshold")).toDouble(0.9975);

        check.minDpi = checkObject.value(QStringLiteral("min_dpi")).toInt(0);
        if (check.id == QStringLiteral("image-resolution") && check.minDpi <= 0)
        {
            errorMessage = PDFTranslationContext::tr("Check '%1' requires positive min_dpi.").arg(check.id);
            return false;
        }

        const QJsonArray allowedModes = checkObject.value(QStringLiteral("allowed")).toArray();
        for (const QJsonValue& val : allowedModes)
        {
            check.allowedColorModes.append(val.toString());
        }
        if (check.id == QStringLiteral("color-mode") && check.allowedColorModes.isEmpty())
        {
            errorMessage = PDFTranslationContext::tr("Check '%1' requires non-empty allowed color modes.").arg(check.id);
            return false;
        }

        profile.checks.push_back(check);
    }

    const QJsonArray fixups = profileObject.value(QStringLiteral("fixups")).toArray();
    for (const QJsonValue& fixupValue : fixups)
    {
        const QJsonObject fixupObject = fixupValue.toObject();
        PreflightFixupConfig fixup;
        fixup.id = fixupObject.value(QStringLiteral("id")).toString();
        if (fixup.id.isEmpty())
        {
            continue;
        }

        fixup.confirm = fixupObject.value(QStringLiteral("confirm")).toBool(true);
        fixup.amountPt = fixupObject.value(QStringLiteral("amount_pt")).toDouble(0.0);
        fixup.description = fixupObject.value(QStringLiteral("description")).toString();
        fixup.params = fixupObject.value(QStringLiteral("params")).toObject();

        profile.fixups.push_back(fixup);
    }

    return true;
}

void PreflightEngine::registerBuiltInChecks()
{
    m_checks[QStringLiteral("bleed")] = [](PDFDocumentSession* session,
                                            const PreflightCheckConfig& check,
                                            QList<PreflightFinding>& errors,
                                            QList<PreflightFinding>& warnings)
    {
        runBleedCheck(session, check, errors, warnings);
    };

    m_checks[QStringLiteral("trim")] = [](PDFDocumentSession* session,
                                           const PreflightCheckConfig& check,
                                           QList<PreflightFinding>& errors,
                                           QList<PreflightFinding>& warnings)
    {
        runSizeCheck(SizeCheckKind::Trim, session, check, errors, warnings);
    };

    m_checks[QStringLiteral("page-size")] = [](PDFDocumentSession* session,
                                                 const PreflightCheckConfig& check,
                                                 QList<PreflightFinding>& errors,
                                                 QList<PreflightFinding>& warnings)
    {
        runSizeCheck(SizeCheckKind::PageSize, session, check, errors, warnings);
    };

    m_checks[QStringLiteral("content-bleed")] = [](PDFDocumentSession* session,
                                                     const PreflightCheckConfig& check,
                                                     QList<PreflightFinding>& errors,
                                                     QList<PreflightFinding>& warnings)
    {
        runContentBleedCheck(session, check, errors, warnings);
    };

    m_checks[QStringLiteral("color-mode")] = [](PDFDocumentSession* session,
                                                  const PreflightCheckConfig& check,
                                                  QList<PreflightFinding>& errors,
                                                  QList<PreflightFinding>& warnings)
    {
        runColorModeCheck(session, check, errors, warnings);
    };

    m_checks[QStringLiteral("embedded-fonts")] = [](PDFDocumentSession* session,
                                                     const PreflightCheckConfig& check,
                                                     QList<PreflightFinding>& errors,
                                                     QList<PreflightFinding>& warnings)
    {
        runEmbeddedFontsCheck(session, check, errors, warnings);
    };

    m_checks[QStringLiteral("image-resolution")] = [](PDFDocumentSession* session,
                                                       const PreflightCheckConfig& check,
                                                       QList<PreflightFinding>& errors,
                                                       QList<PreflightFinding>& warnings)
    {
        runImageResolutionCheck(session, check, errors, warnings);
    };

    m_checks[QStringLiteral("white-overprint")] = [](PDFDocumentSession* session,
                                                       const PreflightCheckConfig& check,
                                                       QList<PreflightFinding>& errors,
                                                       QList<PreflightFinding>& warnings)
    {
        runWhiteOverprintCheck(session, check, errors, warnings);
    };
}

} // namespace pdf
