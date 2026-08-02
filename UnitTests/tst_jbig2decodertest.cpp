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
#include "pdfjbig2decoder.h"
#include "pdfexception.h"

// Regression coverage for the fuzz-found JBIG2 "code tables" (segment type 53,
// 7.4.3) bug: entry.rangeBitLength is read straight from the stream (up to 255,
// from an 8-bit-derived field) with no inherent bound. `1 << rangeBitLength` in
// 32-bit int is undefined behavior for shifts >= 32, and even a valid shift can
// overflow the accumulation into currentRangeLow (UBSan caught this as a signed
// integer overflow on master's fuzz CI). These tests hand-build the minimal
// segment header + segment body byte sequence for a standalone "Tables" segment
// and feed it through the public decode() entry point, the same path the fuzzer
// uses.
class Jbig2DecoderTest : public QObject
{
    Q_OBJECT

private slots:
    void test_codeTables_rejectsOversizedRangeBitLength();
    void test_codeTables_acceptsValidSmallTable();
};

void Jbig2DecoderTest::test_codeTables_rejectsOversizedRangeBitLength()
{
    // Segment header (7.2) + "Tables" segment body (7.4.3), hand-built to match
    // PDFJBIG2SegmentHeader::read()'s exact field layout:
    static const unsigned char data[] = {
        0x00, 0x00, 0x00, 0x00, // segment number = 0
        0x35,                   // flags: type = 53 (Tables), 1-byte page association
        0x00,                   // retention field: 0 referred-to segments
        0x01,                   // page association = 1
        0x00, 0x00, 0x00, 0x0B, // segment data length = 11 bytes (body below)
        // --- segment body: processCodeTables ---
        0x70,                   // flags: hasOOB=0, htps=1, htrs=8
        0x00, 0x00, 0x00, 0x00, // htLow = 0
        0x7F, 0xFF, 0xFF, 0xFF, // htHigh = 0x7FFFFFFF
        0x7F, 0x80               // first entry: prefixBitLength=0, rangeBitLength=255 (invalid)
    };

    QByteArray stream(reinterpret_cast<const char*>(data), sizeof(data));

    pdf::PDFRenderErrorReporterDummy errorReporter;
    pdf::PDFJBIG2Decoder decoder(stream, QByteArray(), &errorReporter);

    bool threw = false;
    QString message;
    try
    {
        decoder.decode(pdf::PDFImageData::MaskingType::None);
    }
    catch (const pdf::PDFException& e)
    {
        threw = true;
        message = e.getMessage();
    }

    QVERIFY2(threw, "A huffman table entry with an out-of-range bit length must be rejected, "
                     "not fed into an undefined-behavior shift / overflowing accumulation.");
    QVERIFY2(message.contains(QStringLiteral("range bit length")), qPrintable(message));
}

void Jbig2DecoderTest::test_codeTables_acceptsValidSmallTable()
{
    // Same segment type, but a small, well-formed table (htLow=0, htHigh=2, a
    // single entry with rangeBitLength=1) to confirm the added validation
    // doesn't reject legitimate custom huffman tables.
    static const unsigned char data[] = {
        0x00, 0x00, 0x00, 0x00, // segment number = 0
        0x35,                   // flags: type = 53 (Tables), 1-byte page association
        0x00,                   // retention field: 0 referred-to segments
        0x01,                   // page association = 1
        0x00, 0x00, 0x00, 0x0A, // segment data length = 10 bytes (body below)
        // --- segment body: processCodeTables ---
        0x00,                   // flags: hasOOB=0, htps=1, htrs=1
        0x00, 0x00, 0x00, 0x00, // htLow = 0
        0x00, 0x00, 0x00, 0x02, // htHigh = 2
        0x40                     // entry prefixBitLength=0, rangeBitLength=1, low/high prefixBitLength=0
    };

    QByteArray stream(reinterpret_cast<const char*>(data), sizeof(data));

    pdf::PDFRenderErrorReporterDummy errorReporter;
    pdf::PDFJBIG2Decoder decoder(stream, QByteArray(), &errorReporter);

    try
    {
        decoder.decode(pdf::PDFImageData::MaskingType::None);
    }
    catch (const pdf::PDFException& e)
    {
        QFAIL(qPrintable(QStringLiteral("A valid small huffman table must not throw: %1").arg(e.getMessage())));
    }
}

QTEST_GUILESS_MAIN(Jbig2DecoderTest)

#include "tst_jbig2decodertest.moc"
