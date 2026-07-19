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

// Regenerates the golden PDF fixtures under frisket-preflight/testdata/fixtures/.
// The fixtures are committed to git; re-run this tool only when a fixture's
// geometry needs to change (see frisket-preflight/README.md).
//
// Usage: generate_fixtures [output-directory]  (defaults to the current directory)

#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QPainter>

namespace
{

constexpr qreal MEDIA_SIZE_PT = 220.0;
constexpr qreal TRIM_INSET_PT = 10.0;
constexpr qreal TRIM_SIZE_PT = 200.0;

void writeFixture(const QDir& outputDir, const QString& fileName, const pdf::PDFDocument& document)
{
    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult result = writer.write(outputDir.filePath(fileName), &document, false);
    if (!result)
    {
        qFatal("Failed to write fixture '%s': %s", qPrintable(fileName), qPrintable(result.getErrorMessage()));
    }
}

pdf::PDFObjectReference appendTieredBleedPage(pdf::PDFDocumentBuilder& builder, const QRectF& contentRect)
{
  // MediaBox 220x220, TrimBox 200x200 inset 10pt on every side. BleedBox is left
  // unset; per PDFPage::parse it falls back to CropBox -> MediaBox, giving a
  // 10pt margin over TrimBox (>= the 9pt amount_pt used by tiered-bleed profiles).
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, MEDIA_SIZE_PT, MEDIA_SIZE_PT));
    builder.setPageTrimBox(page, QRectF(TRIM_INSET_PT, TRIM_INSET_PT, TRIM_SIZE_PT, TRIM_SIZE_PT));

    pdf::PDFPageContentStreamBuilder pageContentStreamBuilder(&builder,
                                                              pdf::PDFContentStreamBuilder::CoordinateSystem::PDF);
    if (QPainter* painter = pageContentStreamBuilder.begin(page))
    {
        painter->fillRect(contentRect, Qt::black);
        pageContentStreamBuilder.end(painter);
    }

    return page;
}

void generateBleedAdequateFixture(const QDir& outputDir)
{
    // MediaBox 220x220, TrimBox 200x200 inset 10pt on every side. BleedBox is left
    // unset; per PDFPage::parse (pdfpage.cpp) it then falls back to CropBox, which
    // itself falls back to MediaBox - a 10pt margin over TrimBox, above the 9pt
    // amount_pt required by frisket-default.json's bleed check.
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Frisket fixture - bleed adequate");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("frisket-preflight golden corpus: adequate bleed");

    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, MEDIA_SIZE_PT, MEDIA_SIZE_PT));
    builder.setPageTrimBox(page, QRectF(TRIM_INSET_PT, TRIM_INSET_PT, TRIM_SIZE_PT, TRIM_SIZE_PT));

    writeFixture(outputDir, "bleed-adequate.pdf", builder.build());
}

void generateBleedMissingFixture(const QDir& outputDir)
{
    // Only MediaBox is set. Per PDFPage::parse (pdfpage.cpp), an unset CropBox falls
    // back to MediaBox, and unset BleedBox/TrimBox both fall back to CropBox - so
    // bleed, trim, and media all end up equal here, giving zero bleed margin and
    // failing the bleed check (< amount_pt on every edge).
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Frisket fixture - bleed missing");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("frisket-preflight golden corpus: missing bleed");

    builder.appendPage(QRectF(0, 0, 200, 200));

    writeFixture(outputDir, "bleed-missing.pdf", builder.build());
}

void generateContentBleedAdequateFixture(const QDir& outputDir)
{
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Frisket fixture - content bleed adequate");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("frisket-preflight golden corpus: artwork extends into bleed");

    appendTieredBleedPage(builder, QRectF(0, 0, MEDIA_SIZE_PT, MEDIA_SIZE_PT));
    writeFixture(outputDir, "content-bleed-adequate.pdf", builder.build());
}

void generateContentBleedMissingFixture(const QDir& outputDir)
{
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Frisket fixture - content bleed missing");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("frisket-preflight golden corpus: artwork stops at trim");

    appendTieredBleedPage(builder, QRectF(TRIM_INSET_PT, TRIM_INSET_PT, TRIM_SIZE_PT, TRIM_SIZE_PT));
    writeFixture(outputDir, "content-bleed-missing.pdf", builder.build());
}

void generateContentBleedRasterConfirmFixture(const QDir& outputDir)
{
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Frisket fixture - content bleed raster confirm");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("frisket-preflight golden corpus: fast bounds flags bleed gap");

    appendTieredBleedPage(builder, QRectF(TRIM_INSET_PT, TRIM_INSET_PT, TRIM_SIZE_PT, TRIM_SIZE_PT));
    writeFixture(outputDir, "content-bleed-raster-confirm.pdf", builder.build());
}

void generateContentBleedThreeOfFourFixture(const QDir& outputDir)
{
    pdf::PDFDocumentBuilder builder;
    builder.setDocumentTitle("Frisket fixture - content bleed three of four");
    builder.setDocumentCreator(QCoreApplication::applicationName());
    builder.setDocumentSubject("frisket-preflight golden corpus: bleed on three edges only");

    // Artwork covers left, right, and top bleed strips but stops above the bottom strip.
    appendTieredBleedPage(builder, QRectF(0, TRIM_INSET_PT + 1.0, MEDIA_SIZE_PT, MEDIA_SIZE_PT - TRIM_INSET_PT - 1.0));
    writeFixture(outputDir, "content-bleed-three-of-four.pdf", builder.build());
}

}   // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName("MelkaJ");
    QCoreApplication::setApplicationName("FrisketGenerateFixtures");

    const QStringList arguments = QCoreApplication::arguments();
    const QDir outputDir(arguments.size() > 1 ? arguments.at(1) : QDir::currentPath());

    generateBleedAdequateFixture(outputDir);
    generateBleedMissingFixture(outputDir);
    generateContentBleedAdequateFixture(outputDir);
    generateContentBleedMissingFixture(outputDir);
    generateContentBleedRasterConfirmFixture(outputDir);
    generateContentBleedThreeOfFourFixture(outputDir);

    return 0;
}
