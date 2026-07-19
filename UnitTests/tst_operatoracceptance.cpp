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

// End-to-end operator-loop acceptance for MIC-300 (Phase C).
// Drives PdfTool preflight + add-bleed via QProcess (same sidecar contract as the
// Editor plugin) and validates report handling helpers used by FrisketPreflightPlugin.
// GUI navigation/overlay/cancel flows are covered in docs/v1-operator-acceptance.md.

#include "preflightsidecarutils.h"

#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace
{

constexpr char DEFAULT_PROFILE_REL[] = "profiles/frisket-default.json";
constexpr char BLEED_MM[] = "3.175"; // 9 pt

struct OperatorCorpusEntry
{
    const char* id;
    const char* pdf;
    bool expectPass;
    const char* expectCheckId;
};

constexpr OperatorCorpusEntry OPERATOR_CORPUS[] = {
    { "clean-adequate-bleed", "bleed-adequate.pdf", true, nullptr },
    { "missing-bleed", "bleed-missing.pdf", false, "bleed" },
    { "live-text-embedded", "font-embedded.pdf", true, nullptr },
    { "image-only-raster", "image-dpi-ok.pdf", true, nullptr },
    { "malformed-input", "malformed-not-pdf.pdf", false, nullptr },
};

QString fixturesDir()
{
    return QStringLiteral(FRISKET_PREFLIGHT_SOURCE_DIR "/testdata/fixtures");
}

QString sourceDir()
{
    return QStringLiteral(FRISKET_PREFLIGHT_SOURCE_DIR);
}

QString defaultProfilePath()
{
    return QDir(sourceDir()).filePath(QString::fromLatin1(DEFAULT_PROFILE_REL));
}

QStringList checkIdsOf(const QJsonObject& report)
{
    QStringList ids;
    for (const QString& section : { QStringLiteral("errors"), QStringLiteral("warnings") })
    {
        for (const QJsonValue& finding : report.value(section).toArray())
        {
            ids << finding.toObject().value(QStringLiteral("check_id")).toString();
        }
    }
    return ids;
}

bool hasAddBleedFixup(const QJsonObject& report)
{
    for (const QJsonValue& fixupValue : report.value(QStringLiteral("fixups_available")).toArray())
    {
        if (fixupValue.toObject().value(QStringLiteral("id")).toString() == QStringLiteral("add-bleed"))
        {
            return true;
        }
    }
    return false;
}

qint64 currentPeakMemoryKb()
{
#ifdef Q_OS_LINUX
    QFile statusFile(QStringLiteral("/proc/self/status"));
    if (!statusFile.open(QIODevice::ReadOnly))
    {
        return -1;
    }

    const QList<QByteArray> lines = statusFile.readAll().split('\n');
    for (const QByteArray& line : lines)
    {
        if (line.startsWith("VmHWM:"))
        {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2)
            {
                return parts.at(1).toLongLong();
            }
        }
    }
    return -1;
#elif defined(Q_OS_WIN)
    return -1;
#else
    return -1;
#endif
}

QByteArray fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result();
}

}   // namespace

class OperatorAcceptanceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void representativeCorpus_preflight_data();
    void representativeCorpus_preflight();

    void operatorLoop_bleedFixupAndRevalidate();
    void operatorLoop_preservesOriginalBytes();

    void unicodeAndSpacePaths_preflightSucceeds();

    void malformedInput_failsWithoutCrash();
    void invalidProfile_returnsActionableError();

    void reportContract_rejectsUnsupportedSchema();
    void reportContract_classifiesVisualOverlays();

    void sidecarCancellation_terminatesCleanly();

    void corpus_performanceBaseline();

private:
    bool runPdfTool(const QStringList& arguments, QByteArray* stdOut, QByteArray* stdErr, int* exitCode) const;
    bool runPreflight(const QString& pdfPath, const QString& profilePath, QJsonObject* report, int* exitCode) const;
    bool runAddBleed(const QString& inputPath, const QString& outputPath, int* exitCode) const;
    QString fixturePath(const QString& pdf) const;

    QString m_defaultProfilePath;
};

void OperatorAcceptanceTest::initTestCase()
{
    m_defaultProfilePath = defaultProfilePath();
    QVERIFY2(QFile::exists(m_defaultProfilePath),
             qPrintable(QStringLiteral("Missing default profile at %1").arg(m_defaultProfilePath)));
}

bool OperatorAcceptanceTest::runPdfTool(const QStringList& arguments, QByteArray* stdOut, QByteArray* stdErr, int* exitCode) const
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(PDFTOOL_EXECUTABLE_PATH), arguments);
    if (!process.waitForFinished(120000))
    {
        process.kill();
        process.waitForFinished(5000);
        return false;
    }

    if (exitCode)
    {
        *exitCode = process.exitCode();
    }

    if (stdOut)
    {
        *stdOut = process.readAllStandardOutput();
    }

    if (stdErr)
    {
        *stdErr = process.readAllStandardError();
    }

    return process.exitStatus() == QProcess::NormalExit;
}

bool OperatorAcceptanceTest::runPreflight(const QString& pdfPath, const QString& profilePath, QJsonObject* report, int* exitCode) const
{
    QByteArray stdOut;
    if (!runPdfTool({ QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), profilePath }, &stdOut, nullptr, exitCode))
    {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(stdOut, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return false;
    }

    if (report)
    {
        *report = document.object();
    }

    return true;
}

bool OperatorAcceptanceTest::runAddBleed(const QString& inputPath, const QString& outputPath, int* exitCode) const
{
    return runPdfTool({
                          QStringLiteral("add-bleed"),
                          inputPath,
                          QStringLiteral("--output"),
                          outputPath,
                          QStringLiteral("--mode"),
                          QStringLiteral("mirror"),
                          QStringLiteral("--bleed-mm"),
                          QString::fromLatin1(BLEED_MM),
                          QStringLiteral("--force"),
                      },
                      nullptr,
                      nullptr,
                      exitCode);
}

QString OperatorAcceptanceTest::fixturePath(const QString& pdf) const
{
    return QDir(fixturesDir()).filePath(pdf);
}

void OperatorAcceptanceTest::representativeCorpus_preflight_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("pdf");
    QTest::addColumn<bool>("expectPass");
    QTest::addColumn<QString>("expectCheckId");

    for (const OperatorCorpusEntry& entry : OPERATOR_CORPUS)
    {
        QTest::newRow(entry.id) << QString::fromLatin1(entry.id)
                                << QString::fromLatin1(entry.pdf)
                                << entry.expectPass
                                << (entry.expectCheckId ? QString::fromLatin1(entry.expectCheckId) : QString());
    }
}

void OperatorAcceptanceTest::representativeCorpus_preflight()
{
    QFETCH(QString, pdf);
    QFETCH(bool, expectPass);
    QFETCH(QString, expectCheckId);

    const QString pdfPath = fixturePath(pdf);
    QVERIFY2(QFile::exists(pdfPath), qPrintable(QStringLiteral("Missing fixture %1").arg(pdfPath)));

    if (pdf == QStringLiteral("malformed-not-pdf.pdf"))
    {
        int exitCode = -1;
        QByteArray stdErr;
        QVERIFY(runPdfTool({ QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), m_defaultProfilePath },
                           nullptr,
                           &stdErr,
                           &exitCode));
        QVERIFY(exitCode != 0);
        QVERIFY(exitCode != 1);
        return;
    }

    QJsonObject report;
    int exitCode = -1;
    QVERIFY(runPreflight(pdfPath, m_defaultProfilePath, &report, &exitCode));

    QVERIFY(pdfplugin::preflight::validateNormalizedReport(report));
    QCOMPARE(report.value(QStringLiteral("pass")).toBool(), expectPass);
    QCOMPARE(exitCode, expectPass ? 0 : 1);

    if (!expectCheckId.isEmpty())
    {
        QVERIFY(checkIdsOf(report).contains(expectCheckId));
    }
}

void OperatorAcceptanceTest::operatorLoop_bleedFixupAndRevalidate()
{
    const QString pdfPath = fixturePath(QStringLiteral("bleed-missing.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    QJsonObject initialReport;
    int initialExitCode = -1;
    QVERIFY(runPreflight(pdfPath, m_defaultProfilePath, &initialReport, &initialExitCode));
    QCOMPARE(initialExitCode, 1);
    QVERIFY(hasAddBleedFixup(initialReport));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(QStringLiteral("bleed-fixed.pdf"));

    int addBleedExitCode = -1;
    QVERIFY(runAddBleed(pdfPath, outputPath, &addBleedExitCode));
    QCOMPARE(addBleedExitCode, 0);
    QVERIFY(QFile::exists(outputPath));

    QJsonObject postReport;
    int postExitCode = -1;
    QVERIFY(runPreflight(outputPath, m_defaultProfilePath, &postReport, &postExitCode));
    QCOMPARE(postExitCode, 0);
    QVERIFY(postReport.value(QStringLiteral("pass")).toBool());
    QVERIFY(!checkIdsOf(postReport).contains(QStringLiteral("bleed")));
}

void OperatorAcceptanceTest::operatorLoop_preservesOriginalBytes()
{
    const QString pdfPath = fixturePath(QStringLiteral("bleed-missing.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    const QByteArray beforeHash = fileSha256(pdfPath);
    QVERIFY(!beforeHash.isEmpty());

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(QStringLiteral("bleed-fixed.pdf"));

    int addBleedExitCode = -1;
    QVERIFY(runAddBleed(pdfPath, outputPath, &addBleedExitCode));
    QCOMPARE(addBleedExitCode, 0);

    QCOMPARE(fileSha256(pdfPath), beforeHash);
    QVERIFY(!fileSha256(outputPath).isEmpty());
    QVERIFY(fileSha256(outputPath) != beforeHash);
}

void OperatorAcceptanceTest::unicodeAndSpacePaths_preflightSucceeds()
{
    const QString sourcePdf = fixturePath(QStringLiteral("bleed-adequate.pdf"));
    QVERIFY(QFile::exists(sourcePdf));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString nestedDir = temporaryDirectory.path() + QStringLiteral("/shop files");
    QVERIFY(QDir().mkpath(nestedDir));

    const QString targetPdf = nestedDir + QStringLiteral("/café poster.pdf");
    QVERIFY(QFile::copy(sourcePdf, targetPdf));
    QVERIFY(QFile::exists(targetPdf));

    QJsonObject report;
    int exitCode = -1;
    QVERIFY(runPreflight(targetPdf, m_defaultProfilePath, &report, &exitCode));
    QCOMPARE(exitCode, 0);
    QVERIFY(report.value(QStringLiteral("pass")).toBool());
}

void OperatorAcceptanceTest::malformedInput_failsWithoutCrash()
{
    const QString pdfPath = fixturePath(QStringLiteral("malformed-not-pdf.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    int exitCode = -1;
    QByteArray stdErr;
    QVERIFY(runPdfTool({ QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), m_defaultProfilePath },
                       nullptr,
                       &stdErr,
                       &exitCode));
    QVERIFY2(exitCode != 0, "Malformed input must not report a successful preflight run.");
    QVERIFY2(exitCode != 1, "Malformed input must not masquerade as a findings exit code.");
}

void OperatorAcceptanceTest::invalidProfile_returnsActionableError()
{
    const QString pdfPath = fixturePath(QStringLiteral("bleed-adequate.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString badProfilePath = temporaryDirectory.filePath(QStringLiteral("broken-profile.json"));

    QFile badProfileFile(badProfilePath);
    QVERIFY(badProfileFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    badProfileFile.write("{ not valid json");
    badProfileFile.close();

    int exitCode = -1;
    QByteArray stdErr;
    QVERIFY(runPdfTool({ QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), badProfilePath },
                       nullptr,
                       &stdErr,
                       &exitCode));
    QVERIFY(exitCode != 0);
    QVERIFY(exitCode != 1);
    QVERIFY(!stdErr.trimmed().isEmpty());
}

void OperatorAcceptanceTest::reportContract_rejectsUnsupportedSchema()
{
    QJsonObject report;
    report.insert(QStringLiteral("schema_version"), 99);
    report.insert(QStringLiteral("pass"), true);
    report.insert(QStringLiteral("profile"), QStringLiteral("Frisket Default"));
    report.insert(QStringLiteral("errors"), QJsonArray());
    report.insert(QStringLiteral("warnings"), QJsonArray());
    report.insert(QStringLiteral("fixups_available"), QJsonArray());

    QString validationError;
    QVERIFY(!pdfplugin::preflight::validateNormalizedReport(report, &validationError));
    QVERIFY(validationError.contains(QStringLiteral("schema_version")));

    report.insert(QStringLiteral("schema_version"), 2);
    QJsonObject documentFinding;
    documentFinding.insert(QStringLiteral("scope"), QStringLiteral("document"));
    documentFinding.insert(QStringLiteral("page"), 1);
    documentFinding.insert(QStringLiteral("type"), QStringLiteral("encrypted"));
    documentFinding.insert(QStringLiteral("severity"), QStringLiteral("error"));
    documentFinding.insert(QStringLiteral("message"), QStringLiteral("Document is password-protected"));
    documentFinding.insert(QStringLiteral("check_id"), QStringLiteral("document-access"));

    QJsonArray errors;
    errors.append(documentFinding);
    report.insert(QStringLiteral("errors"), errors);
    report.insert(QStringLiteral("pass"), false);

    validationError.clear();
    QVERIFY(!pdfplugin::preflight::validateNormalizedReport(report, &validationError));
    QVERIFY(validationError.contains(QStringLiteral("page")));
}

void OperatorAcceptanceTest::reportContract_classifiesVisualOverlays()
{
    const QString bleedPath = fixturePath(QStringLiteral("bleed-missing.pdf"));
    const QString fontPath = fixturePath(QStringLiteral("font-not-embedded.pdf"));
    QVERIFY(QFile::exists(bleedPath));
    QVERIFY(QFile::exists(fontPath));

    QJsonObject bleedReport;
    int bleedExitCode = -1;
    QVERIFY(runPreflight(bleedPath, m_defaultProfilePath, &bleedReport, &bleedExitCode));
    QCOMPARE(bleedExitCode, 1);

    const QJsonObject bleedFinding = bleedReport.value(QStringLiteral("errors")).toArray().first().toObject();
    QVERIFY(pdfplugin::preflight::findingHasVisualOverlay(bleedFinding, 2));

    QJsonObject fontReport;
    int fontExitCode = -1;
    QVERIFY(runPreflight(fontPath, m_defaultProfilePath, &fontReport, &fontExitCode));
    QCOMPARE(fontExitCode, 1);

    const QJsonObject fontFinding = fontReport.value(QStringLiteral("errors")).toArray().first().toObject();
    QVERIFY(!pdfplugin::preflight::findingHasVisualOverlay(fontFinding, 2));
    QCOMPARE(fontFinding.value(QStringLiteral("scope")).toString(), QStringLiteral("object"));
}

void OperatorAcceptanceTest::sidecarCancellation_terminatesCleanly()
{
    const QString pdfPath = fixturePath(QStringLiteral("bleed-adequate.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(PDFTOOL_EXECUTABLE_PATH),
                  { QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), m_defaultProfilePath });
    QVERIFY(process.waitForStarted(10000));

    process.kill();
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.state(), QProcess::NotRunning);
}

void OperatorAcceptanceTest::corpus_performanceBaseline()
{
    QElapsedTimer timer;
    timer.start();
    qint64 peakMemoryKb = currentPeakMemoryKb();

    int ranCases = 0;
    for (const OperatorCorpusEntry& entry : OPERATOR_CORPUS)
    {
        if (qstrcmp(entry.id, "malformed-input") == 0)
        {
            continue;
        }

        const QString pdfPath = fixturePath(QString::fromLatin1(entry.pdf));
        if (!QFile::exists(pdfPath))
        {
            continue;
        }

        QJsonObject report;
        int exitCode = -1;
        QVERIFY(runPreflight(pdfPath, m_defaultProfilePath, &report, &exitCode));
        QCOMPARE(report.value(QStringLiteral("pass")).toBool(), entry.expectPass);
        QCOMPARE(exitCode, entry.expectPass ? 0 : 1);
        ++ranCases;

        const qint64 samplePeak = currentPeakMemoryKb();
        if (samplePeak > peakMemoryKb)
        {
            peakMemoryKb = samplePeak;
        }
    }

    const qint64 elapsedMs = timer.elapsed();
    QVERIFY2(ranCases >= 4, "Expected at least four corpus cases to run.");

    qInfo("MIC-300 baseline: %d corpus preflights in %lld ms; peak memory ~%lld KiB (host process, informational only).",
          ranCases,
          static_cast<long long>(elapsedMs),
          static_cast<long long>(peakMemoryKb));
}

QTEST_APPLESS_MAIN(OperatorAcceptanceTest)

#include "tst_operatoracceptance.moc"
