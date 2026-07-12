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

// Unit tests for the pure geometry math behind the PdfTool `preflight` custom
// checks (bleed, trim, page-size). Exercises pdftoolpreflightchecks.h directly,
// with no PDF document or process involved. The full command is covered
// end-to-end by UnitTestsPreflightCorpus; this target locks the math (MIC-134
// asks for "bleed math, trim matching" to be unit-testable).

#include "pdftoolpreflightchecks.h"

#include <QtTest>
#include <QRectF>

class PreflightChecksTest : public QObject
{
    Q_OBJECT

private slots:
    void resolveEffectiveBox_prefersTrimThenCropThenMedia();
    void bleedAdequate_sufficientMarginPasses();
    void bleedAdequate_missingBleedFails();
    void bleedAdequate_shortEdgeFails();
    void bleedAdequate_withinTolerancePasses();
    void sizeWithinTolerance_exactMatchPasses();
    void sizeWithinTolerance_withinTolerancePasses();
    void sizeWithinTolerance_widthOutOfToleranceFails();
    void sizeWithinTolerance_heightOutOfToleranceFails();
    void sizeWithinTolerance_swappedOrientationFails();
};

void PreflightChecksTest::resolveEffectiveBox_prefersTrimThenCropThenMedia()
{
    const QRectF media(0, 0, 400, 400);
    const QRectF crop(10, 10, 380, 380);
    const QRectF trim(20, 20, 360, 360);

    QCOMPARE(pdf::preflight::resolveEffectiveBox(trim, crop, media), trim.normalized());
    QCOMPARE(pdf::preflight::resolveEffectiveBox(QRectF(), crop, media), crop.normalized());
    QCOMPARE(pdf::preflight::resolveEffectiveBox(QRectF(), QRectF(), media), media.normalized());
}

void PreflightChecksTest::bleedAdequate_sufficientMarginPasses()
{
    // Trim inset 10 pt inside the bleed box on every edge; requirement 9 pt.
    const QRectF bleed(0, 0, 220, 220);
    const QRectF trim(10, 10, 200, 200);
    QVERIFY(pdf::preflight::bleedAdequate(trim, bleed, 9.0, 0.25));
}

void PreflightChecksTest::bleedAdequate_missingBleedFails()
{
    const QRectF trim(0, 0, 200, 200);
    QVERIFY(!pdf::preflight::bleedAdequate(trim, QRectF(), 9.0, 0.25));
}

void PreflightChecksTest::bleedAdequate_shortEdgeFails()
{
    // Only 5 pt of bleed on the right edge, below the 9 pt requirement.
    const QRectF bleed(0, 0, 205, 220);
    const QRectF trim(10, 10, 190, 200);
    QVERIFY(!pdf::preflight::bleedAdequate(trim, bleed, 9.0, 0.25));
}

void PreflightChecksTest::bleedAdequate_withinTolerancePasses()
{
    // Exactly 8.8 pt of bleed, which is within 0.25 pt of the 9 pt requirement.
    const QRectF bleed(0.0, 0.0, 217.6, 217.6);
    const QRectF trim(8.8, 8.8, 200.0, 200.0);
    QVERIFY(pdf::preflight::bleedAdequate(trim, bleed, 9.0, 0.25));
}

void PreflightChecksTest::sizeWithinTolerance_exactMatchPasses()
{
    QVERIFY(pdf::preflight::sizeWithinTolerance(612.0, 792.0, 612.0, 792.0, 1.0));
}

void PreflightChecksTest::sizeWithinTolerance_withinTolerancePasses()
{
    QVERIFY(pdf::preflight::sizeWithinTolerance(611.5, 792.5, 612.0, 792.0, 1.0));
}

void PreflightChecksTest::sizeWithinTolerance_widthOutOfToleranceFails()
{
    QVERIFY(!pdf::preflight::sizeWithinTolerance(610.0, 792.0, 612.0, 792.0, 1.0));
}

void PreflightChecksTest::sizeWithinTolerance_heightOutOfToleranceFails()
{
    QVERIFY(!pdf::preflight::sizeWithinTolerance(612.0, 795.0, 612.0, 792.0, 1.0));
}

void PreflightChecksTest::sizeWithinTolerance_swappedOrientationFails()
{
    // 792 x 612 must not satisfy an expected 612 x 792 (strict orientation).
    QVERIFY(!pdf::preflight::sizeWithinTolerance(792.0, 612.0, 612.0, 792.0, 1.0));
}

QTEST_APPLESS_MAIN(PreflightChecksTest)

#include "tst_preflightchecks.moc"
