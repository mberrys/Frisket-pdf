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
#include "pdfglobal.h"

#include <QtTest>
#include <QColor>
#include <QImage>

class BleedFixupTest : public QObject
{
    Q_OBJECT

private slots:
    void targetBleedRect_expandsByMillimeters();
    void sideAlreadyBleeding_detectsSufficientMargin();
    void stripWidthPx_dependsOnMode();
    void edgeStripRects_areOutsideAndInsideReference();
    void buildEdgeFillImage_mirrorFlipsHorizontally();
    void buildEdgeFillImage_pixelRepeatTilesEdge();
    void buildEdgeFillImage_stretchScalesToBleedDepth();
};

void BleedFixupTest::targetBleedRect_expandsByMillimeters()
{
    const QRectF reference(0.0, 0.0, 100.0, 200.0);
    const QMarginsF bleedMM(3.0, 3.0, 3.0, 3.0);
    const QRectF target = pdf::PDFBleedFixupMath::targetBleedRect(reference, bleedMM);
    const qreal expected = 3.0 * pdf::PDF_MM_TO_POINT;

    QCOMPARE(target.left(), reference.left() - expected);
    QCOMPARE(target.right(), reference.right() + expected);
    QCOMPARE(target.top(), reference.top() - expected);
    QCOMPARE(target.bottom(), reference.bottom() + expected);
}

void BleedFixupTest::sideAlreadyBleeding_detectsSufficientMargin()
{
    const QRectF reference(10.0, 10.0, 100.0, 100.0);
    const QRectF bleed(0.0, 0.0, 120.0, 120.0); // 10pt each side
    QVERIFY(pdf::PDFBleedFixupMath::sideAlreadyBleeding(reference, bleed, pdf::PDFBleedFixupSide::Left, 9.0));
    QVERIFY(!pdf::PDFBleedFixupMath::sideAlreadyBleeding(reference, bleed, pdf::PDFBleedFixupSide::Left, 11.0));
}

void BleedFixupTest::stripWidthPx_dependsOnMode()
{
    QCOMPARE(pdf::PDFBleedFixupMath::stripWidthPx(pdf::PDFBleedFixupMode::Mirror, 35, 1), 35);
    QCOMPARE(pdf::PDFBleedFixupMath::stripWidthPx(pdf::PDFBleedFixupMode::PixelRepeat, 35, 1), 1);
    QCOMPARE(pdf::PDFBleedFixupMath::stripWidthPx(pdf::PDFBleedFixupMode::Stretch, 35, 2), 2);
}

void BleedFixupTest::edgeStripRects_areOutsideAndInsideReference()
{
    const QRectF reference(10.0, 20.0, 100.0, 200.0);
    const qreal depth = 5.0;

    const QRectF leftSrc = pdf::PDFBleedFixupMath::edgeStripSourceRect(reference, pdf::PDFBleedFixupSide::Left, depth);
    const QRectF leftDst = pdf::PDFBleedFixupMath::edgeStripDestRect(reference, pdf::PDFBleedFixupSide::Left, depth);
    QCOMPARE(leftSrc.left(), 10.0);
    QCOMPARE(leftDst.right(), 10.0);
    QCOMPARE(leftDst.left(), 5.0);

    const QRectF topSrc = pdf::PDFBleedFixupMath::edgeStripSourceRect(reference, pdf::PDFBleedFixupSide::Top, depth);
    const QRectF topDst = pdf::PDFBleedFixupMath::edgeStripDestRect(reference, pdf::PDFBleedFixupSide::Top, depth);
    QCOMPARE(topSrc.bottom(), reference.bottom());
    QCOMPARE(topDst.top(), reference.bottom());
}

void BleedFixupTest::buildEdgeFillImage_mirrorFlipsHorizontally()
{
    QImage page(4, 2, QImage::Format_ARGB32);
    page.fill(Qt::black);
    page.setPixel(0, 0, qRgb(255, 0, 0));
    page.setPixel(1, 0, qRgb(0, 255, 0));
    page.setPixel(2, 0, qRgb(0, 0, 255));
    page.setPixel(3, 0, qRgb(255, 255, 0));

    const QImage fill = pdf::PDFBleedFixupMath::buildEdgeFillImage(page, QRect(0, 0, 2, 2),
                                                                   pdf::PDFBleedFixupSide::Left,
                                                                   pdf::PDFBleedFixupMode::Mirror,
                                                                   2);
    QVERIFY(!fill.isNull());
    QCOMPARE(fill.width(), 2);
    QCOMPARE(qRed(fill.pixel(0, 0)), 0);
    QCOMPARE(qGreen(fill.pixel(0, 0)), 255);
    QCOMPARE(qRed(fill.pixel(1, 0)), 255);
}

void BleedFixupTest::buildEdgeFillImage_pixelRepeatTilesEdge()
{
    QImage page(3, 2, QImage::Format_ARGB32);
    page.fill(Qt::black);
    page.setPixel(0, 0, qRgb(10, 20, 30));
    page.setPixel(1, 0, qRgb(40, 50, 60));

    const QImage fill = pdf::PDFBleedFixupMath::buildEdgeFillImage(page, QRect(0, 0, 1, 2),
                                                                   pdf::PDFBleedFixupSide::Left,
                                                                   pdf::PDFBleedFixupMode::PixelRepeat,
                                                                   4);
    QCOMPARE(fill.width(), 4);
    QCOMPARE(fill.height(), 2);
    for (int x = 0; x < 4; ++x)
    {
        QCOMPARE(qRed(fill.pixel(x, 0)), 10);
        QCOMPARE(qGreen(fill.pixel(x, 0)), 20);
        QCOMPARE(qBlue(fill.pixel(x, 0)), 30);
    }
}

void BleedFixupTest::buildEdgeFillImage_stretchScalesToBleedDepth()
{
    QImage page(2, 4, QImage::Format_ARGB32);
    page.fill(qRgb(7, 8, 9));

    const QImage fill = pdf::PDFBleedFixupMath::buildEdgeFillImage(page, QRect(0, 0, 1, 4),
                                                                   pdf::PDFBleedFixupSide::Left,
                                                                   pdf::PDFBleedFixupMode::Stretch,
                                                                   5);
    QCOMPARE(fill.width(), 5);
    QCOMPARE(fill.height(), 4);
}

QTEST_APPLESS_MAIN(BleedFixupTest)

#include "tst_bleedfixuptest.moc"
