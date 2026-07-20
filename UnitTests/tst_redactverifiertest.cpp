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

#include <QtTest>

#include "pdfcms.h"
#include "pdfconstants.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdffont.h"
#include "pdfoptionalcontent.h"
#include "pdfredact.h"
#include "pdfredactverifier.h"

#include <QPainter>
#include <QTemporaryFile>

#include <algorithm>

class RedactVerifierTest : public QObject
{
    Q_OBJECT

private slots:
    void test_redacted_document_passes_verification();
    void test_redact_annotations_remaining_fail();
};

namespace
{

pdf::PDFDocument createRedactableDocument()
{
    pdf::PDFDocumentBuilder builder;
    builder.createDocument();
    pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 400, 400));

    pdf::PDFPageContentStreamBuilder streamBuilder(&builder);
    QPainter* painter = streamBuilder.begin(page);
    if (!painter)
    {
        return pdf::PDFDocument();
    }

    painter->setPen(Qt::black);
    painter->drawText(QPointF(50, 100), QStringLiteral("Secret Text"));
    streamBuilder.end(painter);

    builder.createAnnotationRedact(page, QRectF(40, 80, 220, 40), Qt::black);
    return builder.build();
}

pdf::PDFDocument performRedaction(pdf::PDFDocument& original)
{
    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    pdf::PDFOptionalContentActivity optionalContentActivity(&original, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(&original);
    pdf::PDFCMSPointer cms = cmsManager.getCurrentCMS();
    pdf::PDFMeshQualitySettings meshQualitySettings;
    fontCache.setDocument(pdf::PDFModifiedDocument(&original, &optionalContentActivity));
    fontCache.setCacheShrinkEnabled(nullptr, false);

    pdf::PDFRedact redactProcessor(&original,
                                   &fontCache,
                                   cms.get(),
                                   &optionalContentActivity,
                                   &meshQualitySettings,
                                   Qt::black);
    return redactProcessor.perform(pdf::PDFRedact::None);
}

}   // namespace

void RedactVerifierTest::test_redacted_document_passes_verification()
{
    pdf::PDFDocument originalDocument = createRedactableDocument();
    QVERIFY(originalDocument.getCatalog());
    const pdf::PDFDocument redactedDocument = performRedaction(originalDocument);

    QTemporaryFile redactedFile;
    QVERIFY(redactedFile.open());
    redactedFile.close();

    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult = writer.write(redactedFile.fileName(), &redactedDocument, false);
    QVERIFY(writeResult);

    pdf::PDFRedactVerificationSettings settings;
    const pdf::PDFRedactVerification verification = pdf::PDFRedactVerifier::verify(&originalDocument,
                                                                                     &redactedDocument,
                                                                                     settings,
                                                                                     redactedFile.fileName());
    QVERIFY2(verification.passed(), qPrintable(verification.issues.isEmpty()
                                                   ? QStringLiteral("verification failed")
                                                   : verification.issues.front().message));
}

void RedactVerifierTest::test_redact_annotations_remaining_fail()
{
    const pdf::PDFDocument originalDocument = createRedactableDocument();
    QVERIFY(originalDocument.getCatalog());

    pdf::PDFRedactVerificationSettings settings;
    const pdf::PDFRedactVerification verification = pdf::PDFRedactVerifier::verify(&originalDocument,
                                                                                     &originalDocument,
                                                                                     settings);
    QVERIFY(!verification.passed());
    QVERIFY(std::any_of(verification.issues.cbegin(),
                        verification.issues.cend(),
                        [](const pdf::PDFRedactVerificationIssue& issue)
                        {
                            return issue.checkId == QStringLiteral("annotation-cleanup");
                        }));
}

QTEST_MAIN(RedactVerifierTest)
#include "tst_redactverifiertest.moc"
