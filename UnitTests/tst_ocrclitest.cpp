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
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

class OcrCliTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pdftoolOcr_withMockSidecar_emitsReport();

private:
    QString pdfToolPath() const;
    QString mockSidecarPath() const;
    QString fixturePdfPath() const;
};

void OcrCliTest::initTestCase()
{
    if (!qEnvironmentVariableIsEmpty("FRISKET_OCR_SKIP"))
    {
        QSKIP("FRISKET_OCR_SKIP is set");
    }
}

QString OcrCliTest::pdfToolPath() const
{
    return QStringLiteral(PDFTOOL_EXECUTABLE_PATH);
}

QString OcrCliTest::mockSidecarPath() const
{
    // Drive the Python mock directly so CI does not depend on +x bits or
    // shebang/CRLF behavior of the thin .sh/.cmd wrappers.
    return QDir(QStringLiteral(FRISKET_OCR_SOURCE_DIR))
        .filePath(QStringLiteral("tools/mock_ocr_sidecar.py"));
}

QString OcrCliTest::fixturePdfPath() const
{
    return QDir(QStringLiteral(FRISKET_PREFLIGHT_SOURCE_DIR))
        .filePath(QStringLiteral("testdata/fixtures/image-dpi-low.pdf"));
}

void OcrCliTest::pdftoolOcr_withMockSidecar_emitsReport()
{
    QVERIFY2(QFileInfo::exists(pdfToolPath()), qPrintable(QStringLiteral("PdfTool not found at ") + pdfToolPath()));
    QVERIFY2(QFileInfo::exists(mockSidecarPath()), qPrintable(QStringLiteral("Mock sidecar not found at ") + mockSidecarPath()));
    QVERIFY2(QFileInfo::exists(fixturePdfPath()), qPrintable(QStringLiteral("Fixture not found at ") + fixturePdfPath()));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(environment);
    process.setProgram(pdfToolPath());
    process.setArguments({ QStringLiteral("ocr"),
                           fixturePdfPath(),
                           QStringLiteral("--console-format"),
                           QStringLiteral("json"),
                           QStringLiteral("--sidecar"),
                           mockSidecarPath() });
    process.start();
    QVERIFY(process.waitForFinished(120000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);

    const QByteArray stdOut = process.readAllStandardOutput();
    const QByteArray stdErr = process.readAllStandardError();
    QVERIFY2(process.exitCode() == 0 || process.exitCode() == 1,
             qPrintable(QStringLiteral("unexpected exit %1\nstdout: %2\nstderr: %3")
                            .arg(process.exitCode())
                            .arg(QString::fromUtf8(stdOut))
                            .arg(QString::fromUtf8(stdErr))));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdOut, &parseError);
    QVERIFY2(parseError.error == QJsonParseError::NoError, qPrintable(parseError.errorString()));
    QVERIFY(document.isObject());

    const QJsonObject report = document.object();
    QCOMPARE(report.value(QStringLiteral("schema_version")).toInt(), 1);
    QVERIFY(report.contains(QStringLiteral("pages")));

    bool foundOcrPage = false;
    const auto pages = report.value(QStringLiteral("pages")).toArray();
    for (const QJsonValue& pageValue : pages)
    {
        const QJsonObject pageObject = pageValue.toObject();
        if (pageObject.value(QStringLiteral("status")).toString() == QStringLiteral("ocr"))
        {
            foundOcrPage = true;
            QCOMPARE(pageObject.value(QStringLiteral("text")).toString(), QStringLiteral("MOCK OCR TEXT"));
        }
    }
    QVERIFY(foundOcrPage);
}

QTEST_MAIN(OcrCliTest)
#include "tst_ocrclitest.moc"
