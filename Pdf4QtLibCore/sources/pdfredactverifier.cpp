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

#include "pdfredactverifier.h"

#include "pdfannotation.h"
#include "pdfconstants.h"
#include "pdfcms.h"
#include "pdffont.h"
#include "pdftextlayoutgenerator.h"

#include <QFile>
#include <QPainterPath>

namespace pdf
{

namespace
{

struct RedactRegion
{
    PDFInteger pageIndex = 0;
    QPainterPath region;
};

void addIssue(PDFRedactVerification* verification, const QString& checkId, const QString& message)
{
    PDFRedactVerificationIssue issue;
    issue.checkId = checkId;
    issue.message = message;
    verification->issues.push_back(issue);
}

QVector<RedactRegion> collectRedactRegions(const PDFDocument* document)
{
    QVector<RedactRegion> regions;

    if (!document)
    {
        return regions;
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

        for (const PDFObjectReference& annotationReference : page->getAnnotations())
        {
            PDFAnnotationPtr annotation = PDFAnnotation::parse(&document->getStorage(), annotationReference);
            if (!annotation || annotation->getType() != AnnotationType::Redact)
            {
                continue;
            }

            const PDFRedactAnnotation* redactAnnotation = dynamic_cast<const PDFRedactAnnotation*>(annotation.get());
            if (!redactAnnotation)
            {
                continue;
            }

            RedactRegion region;
            region.pageIndex = pageIndex;
            region.region = redactAnnotation->getRedactionRegion().getPath();
            regions.push_back(region);
        }
    }

    return regions;
}

QString extractTextInRegion(const PDFDocument* document, PDFInteger pageIndex, const QPainterPath& regionPath)
{
    const PDFCatalog* catalog = document->getCatalog();
    const PDFPage* page = catalog->getPage(pageIndex);
    if (!page)
    {
        return QString();
    }

    PDFFontCache fontCache(DEFAULT_FONT_CACHE_LIMIT, DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    PDFOptionalContentActivity optionalContentActivity(document, OCUsage::Export, nullptr);
    PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    PDFCMSPointer cms = cmsManager.getCurrentCMS();
    PDFModifiedDocument modifiedDocument(const_cast<PDFDocument*>(document), &optionalContentActivity);
    fontCache.setDocument(modifiedDocument);
    fontCache.setCacheShrinkEnabled(nullptr, false);
    PDFMeshQualitySettings meshQualitySettings;

    PDFTextLayoutGenerator generator(PDFRenderer::IgnoreOptionalContent,
                                     page,
                                     document,
                                     &fontCache,
                                     cms.get(),
                                     &optionalContentActivity,
                                     QTransform(),
                                     meshQualitySettings);
    PDFTextLayout layout = generator.createTextLayout();

    const QRectF bounds = regionPath.boundingRect();
    if (bounds.isEmpty())
    {
        return QString();
    }

    const PDFTextSelection selection = layout.createTextSelection(pageIndex,
                                                                  bounds.bottomLeft(),
                                                                  bounds.topRight(),
                                                                  Qt::black,
                                                                  true);
    return layout.getTextFromSelection(selection, pageIndex).trimmed();
}

bool hasRedactAnnotations(const PDFDocument* document)
{
    if (!document)
    {
        return false;
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

        for (const PDFObjectReference& annotationReference : page->getAnnotations())
        {
            PDFAnnotationPtr annotation = PDFAnnotation::parse(&document->getStorage(), annotationReference);
            if (annotation && annotation->getType() == AnnotationType::Redact)
            {
                return true;
            }
        }
    }

    return false;
}

bool fileContainsIncrementalMarker(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    const QByteArray tail = file.readAll().right(8192);
    return tail.contains("/Prev");
}

}   // namespace

PDFRedactVerification PDFRedactVerifier::verify(const PDFDocument* originalDocument,
                                                const PDFDocument* redactedDocument,
                                                const PDFRedactVerificationSettings& settings,
                                                const QString& redactedFilePath)
{
    PDFRedactVerification verification;

    if (!originalDocument || !redactedDocument)
    {
        addIssue(&verification, QStringLiteral("input"), QStringLiteral("Original and redacted documents are required."));
        return verification;
    }

    if (hasRedactAnnotations(redactedDocument))
    {
        addIssue(&verification, QStringLiteral("annotation-cleanup"), QStringLiteral("Redact annotations remain in the output document."));
    }

    const QVector<RedactRegion> regions = collectRedactRegions(originalDocument);
    if (regions.isEmpty())
    {
        addIssue(&verification, QStringLiteral("input"), QStringLiteral("Original document has no redact annotations."));
        return verification;
    }

    for (const RedactRegion& region : regions)
    {
        const QString originalText = extractTextInRegion(originalDocument, region.pageIndex, region.region);
        const QString redactedText = extractTextInRegion(redactedDocument, region.pageIndex, region.region);

        if (!originalText.isEmpty() && redactedText == originalText)
        {
            addIssue(&verification,
                     QStringLiteral("text-residue"),
                     QStringLiteral("Page %1 still contains recoverable text in a redacted region.")
                         .arg(region.pageIndex + 1));
        }
    }

    const PDFDocumentInfo* originalInfo = originalDocument->getInfo();
    const PDFDocumentInfo* redactedInfo = redactedDocument->getInfo();
    if (!settings.redactOptions.testFlag(PDFRedact::CopyMetadata))
    {
        if (!originalInfo->author.isEmpty() && redactedInfo->author == originalInfo->author)
        {
            addIssue(&verification, QStringLiteral("metadata"), QStringLiteral("Author metadata was copied without CopyMetadata."));
        }
        if (!originalInfo->title.isEmpty() && redactedInfo->title == originalInfo->title)
        {
            addIssue(&verification, QStringLiteral("metadata"), QStringLiteral("Title metadata was copied without CopyMetadata."));
        }
        if (!originalInfo->subject.isEmpty() && redactedInfo->subject == originalInfo->subject)
        {
            addIssue(&verification, QStringLiteral("metadata"), QStringLiteral("Subject metadata was copied without CopyMetadata."));
        }
        if (!originalInfo->keywords.isEmpty() && redactedInfo->keywords == originalInfo->keywords)
        {
            addIssue(&verification, QStringLiteral("metadata"), QStringLiteral("Keywords metadata was copied without CopyMetadata."));
        }
    }

    if (settings.checkIncrementalRewrite && !redactedFilePath.isEmpty() && fileContainsIncrementalMarker(redactedFilePath))
    {
        addIssue(&verification,
                 QStringLiteral("incremental-save"),
                 QStringLiteral("Redacted output appears to be an incremental update (trailer contains /Prev)."));
    }

    return verification;
}

}   // namespace pdf
