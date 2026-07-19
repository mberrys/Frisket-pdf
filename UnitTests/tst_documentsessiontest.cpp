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

#include "pdfdocumentsession.h"
#include "pdfdocumentbuilder.h"
#include "pdfobject.h"

#include <QtTest>

class DocumentSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void nullDocument_sessionIsInvalid();
    void compilePage_cachesResult();
    void getDecodedStream_cachesResult();
    void invalidate_clearsCaches();
    void setRendererFeatures_invalidatesCompileCache();
};

void DocumentSessionTest::nullDocument_sessionIsInvalid()
{
    pdf::PDFDocumentSession session(nullptr);
    QVERIFY(!session.isValid());
    QCOMPARE(session.getDocument(), nullptr);
    QCOMPARE(session.compilePage(0), nullptr);
    QVERIFY(session.getDecodedStream(pdf::PDFObjectReference()).isEmpty());
}

void DocumentSessionTest::compilePage_cachesResult()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 100, 100));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    QVERIFY(session.isValid());

    const pdf::PDFPrecompiledPage* first = session.compilePage(0);
    QVERIFY(first != nullptr);

    const pdf::PDFPrecompiledPage* second = session.compilePage(0);
    QCOMPARE(second, first);

    QCOMPARE(session.compilePage(99), nullptr);
}

void DocumentSessionTest::getDecodedStream_cachesResult()
{
    pdf::PDFDocumentBuilder builder;

    pdf::PDFDictionary dictionary;
    dictionary.addEntry(pdf::PDFInplaceOrMemoryString("Length"), pdf::PDFObject::createInteger(5));
    pdf::PDFObject streamObject = pdf::PDFObject::createStream(std::make_shared<pdf::PDFStream>(std::move(dictionary), QByteArray("hello")));
    pdf::PDFObjectReference streamReference = builder.addObject(std::move(streamObject));

    builder.appendPage(QRectF(0, 0, 100, 100));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    QByteArray first = session.getDecodedStream(streamReference);
    QCOMPARE(first, QByteArray("hello"));

    QByteArray second = session.getDecodedStream(streamReference);
    QCOMPARE(second, first);
}

void DocumentSessionTest::invalidate_clearsCaches()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 100, 100));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    const pdf::PDFPrecompiledPage* compiled = session.compilePage(0);
    QVERIFY(compiled != nullptr);
    QCOMPARE(session.compilePage(0), compiled);

    session.invalidate();

    const pdf::PDFPrecompiledPage* after = session.compilePage(0);
    QVERIFY(after != nullptr);
    QCOMPARE(session.compilePage(0), after);
}

void DocumentSessionTest::setRendererFeatures_invalidatesCompileCache()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 100, 100));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    const pdf::PDFPrecompiledPage* compiled = session.compilePage(0);
    QVERIFY(compiled != nullptr);

    pdf::PDFRenderer::Features newFeatures = pdf::PDFRenderer::getDefaultFeatures();
    newFeatures.setFlag(pdf::PDFRenderer::ClipToCropBox, !newFeatures.testFlag(pdf::PDFRenderer::ClipToCropBox));
    session.setRendererFeatures(newFeatures);
    QCOMPARE(session.getRendererFeatures(), newFeatures);

    const pdf::PDFPrecompiledPage* after = session.compilePage(0);
    QVERIFY(after != nullptr);
    QCOMPARE(session.compilePage(0), after);
}

QTEST_GUILESS_MAIN(DocumentSessionTest)

#include "tst_documentsessiontest.moc"
