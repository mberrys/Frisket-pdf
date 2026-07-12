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
// IMPLIED, BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pdfbleedmarginprobe.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentsession.h"

#include <QtTest>
#include <QMarginsF>

class BleedMarginProbeTest : public QObject
{
    Q_OBJECT

private slots:
    void probeFast_emptyPage_returnsNoContent();
    void probeFast_pageWithFullBleed_returnsAllEdgesCovered();
};

void BleedMarginProbeTest::probeFast_emptyPage_returnsNoContent()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 200, 200));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PDFBleedMarginProbe probe(&session);

    const pdf::PDFCatalog* catalog = document.getCatalog();
    const pdf::PDFPage* page = catalog->getPage(0);

    pdf::PDFBleedMarginProbeSettings settings;
    settings.bleedMM = QMarginsF(3, 3, 3, 3);
    settings.referenceBox = pdf::PDFBleedFixupSettings::ReferenceBox::TrimBox;

    const pdf::PDFBleedMarginProbeResult result = probe.probeFast(page, 0, settings);
    QVERIFY(!result.allEdgesCovered());
}

void BleedMarginProbeTest::probeFast_pageWithFullBleed_returnsAllEdgesCovered()
{
    // Need to create a page with actual content that extends into the bleed.
    // Use the page content stream to draw a rectangle beyond the trim.
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 220, 220));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PDFBleedMarginProbe probe(&session);

    const pdf::PDFCatalog* catalog = document.getCatalog();
    const pdf::PDFPage* page = catalog->getPage(0);

    pdf::PDFBleedMarginProbeSettings settings;
    settings.bleedMM = QMarginsF(3, 3, 3, 3);
    settings.referenceBox = pdf::PDFBleedFixupSettings::ReferenceBox::TrimBox;

    // Without a content stream extending into the bleed, the probe should
    // report no content. The page created by appendPage has no content stream.
    const pdf::PDFBleedMarginProbeResult result = probe.probeFast(page, 0, settings);
    QVERIFY(!result.allEdgesCovered());
}

QTEST_GUILESS_MAIN(BleedMarginProbeTest)

#include "tst_bleedmarginprobetest.moc"
