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

#include "pdfpagecontentprocessor.h"

#include <QtTest>

class OverprintTest : public QObject
{
    Q_OBJECT

private slots:
    void overprintMode_appliesToContent_respectsFillStrokeFlags();
};

void OverprintTest::overprintMode_appliesToContent_respectsFillStrokeFlags()
{
    pdf::PDFOverprintMode mode;
    mode.overprintFilling = true;
    mode.overprintStroking = false;

    QVERIFY(mode.appliesToContent(true, false));
    QVERIFY(!mode.appliesToContent(false, true));
    QVERIFY(!mode.appliesToContent(false, false));

    mode.overprintFilling = false;
    mode.overprintStroking = true;

    QVERIFY(!mode.appliesToContent(true, false));
    QVERIFY(mode.appliesToContent(false, true));

    mode.overprintFilling = true;
    mode.overprintStroking = true;

    QVERIFY(mode.appliesToContent(true, false));
    QVERIFY(mode.appliesToContent(false, true));
    QVERIFY(!mode.appliesToContent(false, false));
}

QTEST_APPLESS_MAIN(OverprintTest)
#include "tst_overprinttest.moc"
