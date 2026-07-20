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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace
{

constexpr char DEFAULT_PROFILE_REL[] = "profiles/frisket-default.json";
constexpr qreal POINTS_PER_MM = 72.0 / 25.4;

struct OperatorCorpusEntry
{
    const char* id;
    const char* pdf;
    bool expectPass;
    const char* expectCheckId;
    bool skipPreflightJson;
};

constexpr OperatorCorpusEntry OPERATOR_CORPUS[] = {
    { "clean-adequate-bleed", "bleed-adequate.pdf", true, nullptr, false },
    { "missing-bleed", "bleed-missing.pdf", false, "bleed", false },
    { "live-text-embedded", "font-embedded.pdf", true, nullptr, false },
    { "live-text-not-embedded", "font-not-embedded.pdf", false, "embedded-fonts", false },
    { "image-only-raster", "image-dpi-ok.pdf", true, nullptr, false },
    { "malformed-input", "malformed-not-pdf.pdf", false, nullptr, true },
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

QJsonObject findingWithCheckId(const QJsonObject& report, const QString& checkId)
{
    for (const QString& section : { QStringLiteral("errors"), QStringLiteral("warnings") })
    {
        for (const QJsonValue& findingValue : report.value(section).toArray())
        {
            const QJsonObject finding = findingValue.toObject();
            if (finding.value(QStringLiteral("check_id")).toString() == checkId)
            {
                return finding;
            }
        }
    }

    return QJsonObject();
}

bool hasAddBleedFixup(const QJsonObject& report, QJsonObject* fixupObject = nullptr)
{
    for (const QJsonValue& fixupValue : report.value(QStringLiteral("fixups_available")).toArray())
    {
        const QJsonObject fixup = fixupValue.toObject();
        if (fixup.value(QStringLiteral("id")).toString() == QStringLiteral("add-bleed"))
        {
            if (fixupObject)
            {
                *fixupObject = fixup;
            }
            return true;
        }
    }
    return false;
}

bool advertisedAddBleedParams(const QJsonObject& report, QString* mode, QString* bleedMm)
{
    QJsonObject fixup;
    if (!hasAddBleedFixup(report, &fixup))
    {
        return false;
    }

    const QJsonObject params = fixup.value(QStringLiteral("params")).toObject();
    const qreal amountPt = params.value(QStringLiteral("amount_pt")).toDouble(fixup.value(QStringLiteral("amount_pt")).toDouble(9.0));

    if (mode)
    {
        *mode = params.value(QStringLiteral("mode")).toString(QStringLiteral("mirror"));
    }

    if (bleedMm)
    {
        *bleedMm = QString::number(amountPt / POINTS_PER_MM, 'f', 3);
    }

    return amountPt > 0.0;
}

#ifdef Q_OS_LINUX
qint64 readProcessMemoryFieldKb(qint64 processId, const char* fieldName)
{
    QFile statusFile(QStringLiteral("/proc/%1/status").arg(processId));
    if (!statusFile.open(QIODevice::ReadOnly))
    {
        return -1;
    }

    const QList<QByteArray> lines = statusFile.readAll().split('\n');
    const QByteArray prefix = QByteArray(fieldName) + ':';
    for (const QByteArray& line : lines)
    {
        if (line.startsWith(prefix))
        {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2)
            {
                return parts.at(1).toLongLong();
            }
        }
    }

    return -1;
}
#endif

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

    void unicodeAndSpacePaths_preflightAndAddBleedSucceed();

    void malformedInput_failsWithoutCrash();
    void invalidProfile_returnsActionableError();
    void profileSemanticMismatch_returnsProfileFinding();

    void reportContract_rejectsUnsupportedSchema();
    void reportContract_classifiesVisualOverlays();

    void sidecarCancellation_terminatesCleanly();

    void corpus_performanceBaseline();

private:
    bool runPdfTool(const QStringList& arguments,
                    QByteArray* stdOut,
                    QByteArray* stdErr,
                    int* exitCode,
                    qint64* peakChildMemoryKb = nullptr) const;
    bool runPreflight(const QString& pdfPath,
                      const QString& profilePath,
                      QJsonObject* report,
                      int* exitCode,
                      qint64* peakChildMemoryKb = nullptr) const;
    bool runAddBleed(const QString& inputPath,
                     const QString& outputPath,
                     const QString& mode,
                     const QString& bleedMm,
                     int* exitCode) const;
    QString fixturePath(const QString& pdf) const;
    void assertMalformedPreflightFailure(const QString& pdfPath) const;

    QString m_defaultProfilePath;
    QString m_pdfToolPath;
};

void OperatorAcceptanceTest::initTestCase()
{
    m_defaultProfilePath = defaultProfilePath();
    QVERIFY2(QFile::exists(m_defaultProfilePath),
             qPrintable(QStringLiteral("Missing default profile at %1").arg(m_defaultProfilePath)));

    m_pdfToolPath = QStringLiteral(PDFTOOL_EXECUTABLE_PATH);
    QVERIFY2(QFileInfo(m_pdfToolPath).isExecutable(),
             qPrintable(QStringLiteral("PdfTool not found or not executable at %1").arg(m_pdfToolPath)));
}

bool OperatorAcceptanceTest::runPdfTool(const QStringList& arguments,
                                        QByteArray* stdOut,
                                        QByteArray* stdErr,
                                        int* exitCode,
                                        qint64* peakChildMemoryKb) const
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(environment);
    process.start(m_pdfToolPath, arguments);
    if (!process.waitForStarted(10000))
    {
        return false;
    }

    qint64 peakMemoryKb = -1;
    QElapsedTimer runTimer;
    runTimer.start();
    while (!process.waitForFinished(250))
    {
        if (runTimer.elapsed() > 120000)
        {
            process.kill();
            process.waitForFinished(5000);
            return false;
        }

#ifdef Q_OS_LINUX
        const qint64 sample = readProcessMemoryFieldKb(process.processId(), "VmHWM");
        if (sample > peakMemoryKb)
        {
            peakMemoryKb = sample;
        }
#endif
    }

#ifdef Q_OS_LINUX
    const qint64 finalSample = readProcessMemoryFieldKb(process.processId(), "VmHWM");
    if (finalSample > peakMemoryKb)
    {
        peakMemoryKb = finalSample;
    }
#endif

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

    if (peakChildMemoryKb)
    {
        *peakChildMemoryKb = peakMemoryKb;
    }

    return process.exitStatus() == QProcess::NormalExit;
}

bool OperatorAcceptanceTest::runPreflight(const QString& pdfPath,
                                          const QString& profilePath,
                                          QJsonObject* report,
                                          int* exitCode,
                                          qint64* peakChildMemoryKb) const
{
    QByteArray stdOut;
    if (!runPdfTool({ QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), profilePath },
                    &stdOut,
                    nullptr,
                    exitCode,
                    peakChildMemoryKb))
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

bool OperatorAcceptanceTest::runAddBleed(const QString& inputPath,
                                         const QString& outputPath,
                                         const QString& mode,
                                         const QString& bleedMm,
                                         int* exitCode) const
{
    return runPdfTool({
                          QStringLiteral("add-bleed"),
                          inputPath,
                          QStringLiteral("--output"),
                          outputPath,
                          QStringLiteral("--mode"),
                          mode,
                          QStringLiteral("--bleed-mm"),
                          bleedMm,
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

void OperatorAcceptanceTest::assertMalformedPreflightFailure(const QString& pdfPath) const
{
    int exitCode = -1;
    QByteArray stdErr;
    QVERIFY(runPdfTool({ QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), m_defaultProfilePath },
                       nullptr,
                       &stdErr,
                       &exitCode));
    QVERIFY2(exitCode != 0, "Malformed input must not report a successful preflight run.");
    QVERIFY2(exitCode != 1, "Malformed input must not masquerade as a findings exit code.");
}

void OperatorAcceptanceTest::representativeCorpus_preflight_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("pdf");
    QTest::addColumn<bool>("expectPass");
    QTest::addColumn<QString>("expectCheckId");
    QTest::addColumn<bool>("skipPreflightJson");

    for (const OperatorCorpusEntry& entry : OPERATOR_CORPUS)
    {
        QTest::newRow(entry.id) << QString::fromLatin1(entry.id)
                                << QString::fromLatin1(entry.pdf)
                                << entry.expectPass
                                << (entry.expectCheckId ? QString::fromLatin1(entry.expectCheckId) : QString())
                                << entry.skipPreflightJson;
    }
}

void OperatorAcceptanceTest::representativeCorpus_preflight()
{
    QFETCH(QString, pdf);
    QFETCH(bool, expectPass);
    QFETCH(QString, expectCheckId);
    QFETCH(bool, skipPreflightJson);

    const QString pdfPath = fixturePath(pdf);
    QVERIFY2(QFile::exists(pdfPath), qPrintable(QStringLiteral("Missing fixture %1").arg(pdfPath)));

    if (skipPreflightJson)
    {
        assertMalformedPreflightFailure(pdfPath);
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
    QVERIFY(pdfplugin::preflight::validateNormalizedReport(initialReport));

    QString mode;
    QString bleedMm;
    QVERIFY(advertisedAddBleedParams(initialReport, &mode, &bleedMm));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(QStringLiteral("bleed-fixed.pdf"));

    int addBleedExitCode = -1;
    QVERIFY(runAddBleed(pdfPath, outputPath, mode, bleedMm, &addBleedExitCode));
    QCOMPARE(addBleedExitCode, 0);
    QVERIFY(QFile::exists(outputPath));

    QJsonObject postReport;
    int postExitCode = -1;
    QVERIFY(runPreflight(outputPath, m_defaultProfilePath, &postReport, &postExitCode));
    QCOMPARE(postExitCode, 0);

    const QJsonObject filteredPostReport = pdfplugin::preflight::filterAdvertisedFixups(postReport);
    QVERIFY(pdfplugin::preflight::validateNormalizedReport(filteredPostReport));
    QVERIFY(filteredPostReport.value(QStringLiteral("pass")).toBool());
    QVERIFY(!checkIdsOf(filteredPostReport).contains(QStringLiteral("bleed")));
}

void OperatorAcceptanceTest::operatorLoop_preservesOriginalBytes()
{
    const QString pdfPath = fixturePath(QStringLiteral("bleed-missing.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    QJsonObject initialReport;
    int initialExitCode = -1;
    QVERIFY(runPreflight(pdfPath, m_defaultProfilePath, &initialReport, &initialExitCode));

    QString mode;
    QString bleedMm;
    QVERIFY(advertisedAddBleedParams(initialReport, &mode, &bleedMm));

    const QByteArray beforeHash = fileSha256(pdfPath);
    QVERIFY(!beforeHash.isEmpty());

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString outputPath = temporaryDirectory.filePath(QStringLiteral("bleed-fixed.pdf"));

    int addBleedExitCode = -1;
    QVERIFY(runAddBleed(pdfPath, outputPath, mode, bleedMm, &addBleedExitCode));
    QCOMPARE(addBleedExitCode, 0);

    QCOMPARE(fileSha256(pdfPath), beforeHash);
    QVERIFY(!fileSha256(outputPath).isEmpty());
    QVERIFY(fileSha256(outputPath) != beforeHash);
}

void OperatorAcceptanceTest::unicodeAndSpacePaths_preflightAndAddBleedSucceed()
{
    const QString sourcePdf = fixturePath(QStringLiteral("bleed-missing.pdf"));
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
    QCOMPARE(exitCode, 1);

    QString mode;
    QString bleedMm;
    QVERIFY(advertisedAddBleedParams(report, &mode, &bleedMm));

    const QString outputPdf = nestedDir + QStringLiteral("/café poster_bleed.pdf");
    int addBleedExitCode = -1;
    QVERIFY(runAddBleed(targetPdf, outputPdf, mode, bleedMm, &addBleedExitCode));
    QCOMPARE(addBleedExitCode, 0);
    QVERIFY(QFile::exists(outputPdf));

    QJsonObject postReport;
    int postExitCode = -1;
    QVERIFY(runPreflight(outputPdf, m_defaultProfilePath, &postReport, &postExitCode));
    QCOMPARE(postExitCode, 0);
    QVERIFY(postReport.value(QStringLiteral("pass")).toBool());
}

void OperatorAcceptanceTest::malformedInput_failsWithoutCrash()
{
    assertMalformedPreflightFailure(fixturePath(QStringLiteral("malformed-not-pdf.pdf")));
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

void OperatorAcceptanceTest::profileSemanticMismatch_returnsProfileFinding()
{
    const QString pdfPath = fixturePath(QStringLiteral("bleed-adequate.pdf"));
    QVERIFY(QFile::exists(pdfPath));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString profilePath = temporaryDirectory.filePath(QStringLiteral("empty-checks-profile.json"));

    const QJsonObject profile{
        { QStringLiteral("name"), QStringLiteral("Broken profile") },
        { QStringLiteral("checks"), QJsonArray() },
    };

    QFile profileFile(profilePath);
    QVERIFY(profileFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    profileFile.write(QJsonDocument(profile).toJson(QJsonDocument::Compact));
    profileFile.close();

    QJsonObject report;
    int exitCode = -1;
    QVERIFY(runPreflight(pdfPath, profilePath, &report, &exitCode));
    QCOMPARE(exitCode, 1);
    QVERIFY(!report.value(QStringLiteral("pass")).toBool());

    const QJsonArray errors = report.value(QStringLiteral("errors")).toArray();
    QVERIFY(!errors.isEmpty());
    const QJsonObject profileFinding = errors.first().toObject();
    QCOMPARE(profileFinding.value(QStringLiteral("type")).toString(), QStringLiteral("profile"));
    QCOMPARE(profileFinding.value(QStringLiteral("scope")).toString(), QStringLiteral("document"));
    QVERIFY(!profileFinding.value(QStringLiteral("message")).toString().isEmpty());

    const QJsonObject filteredReport = pdfplugin::preflight::filterAdvertisedFixups(report);
    QVERIFY(pdfplugin::preflight::validateNormalizedReport(filteredReport));
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

    const QJsonObject bleedFinding = findingWithCheckId(bleedReport, QStringLiteral("bleed"));
    QVERIFY(!bleedFinding.isEmpty());
    QVERIFY(pdfplugin::preflight::findingHasVisualOverlay(bleedFinding, 2));

    QJsonObject fontReport;
    int fontExitCode = -1;
    QVERIFY(runPreflight(fontPath, m_defaultProfilePath, &fontReport, &fontExitCode));
    QCOMPARE(fontExitCode, 1);

    const QJsonObject fontFinding = findingWithCheckId(fontReport, QStringLiteral("embedded-fonts"));
    QVERIFY(!fontFinding.isEmpty());
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
    process.start(m_pdfToolPath,
                  { QStringLiteral("preflight"), pdfPath, QStringLiteral("--profile"), m_defaultProfilePath });
    QVERIFY(process.waitForStarted(10000));
    QVERIFY(process.waitForReadyRead(5000) || process.state() == QProcess::Running);

    process.kill();
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.state(), QProcess::NotRunning);
    QVERIFY(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0);
}

void OperatorAcceptanceTest::corpus_performanceBaseline()
{
    QElapsedTimer timer;
    timer.start();
    qint64 peakChildMemoryKb = -1;

    int ranCases = 0;
    for (const OperatorCorpusEntry& entry : OPERATOR_CORPUS)
    {
        if (entry.skipPreflightJson)
        {
            continue;
        }

        const QString pdfPath = fixturePath(QString::fromLatin1(entry.pdf));
        QVERIFY2(QFile::exists(pdfPath),
                 qPrintable(QStringLiteral("Missing corpus fixture %1").arg(QString::fromLatin1(entry.pdf))));

        QJsonObject report;
        int exitCode = -1;
        qint64 casePeakMemoryKb = -1;
        QVERIFY(runPreflight(pdfPath, m_defaultProfilePath, &report, &exitCode, &casePeakMemoryKb));
        QCOMPARE(report.value(QStringLiteral("pass")).toBool(), entry.expectPass);
        QCOMPARE(exitCode, entry.expectPass ? 0 : 1);
        ++ranCases;

        if (casePeakMemoryKb > peakChildMemoryKb)
        {
            peakChildMemoryKb = casePeakMemoryKb;
        }
    }

    const qint64 elapsedMs = timer.elapsed();
    QCOMPARE(ranCases, 5);

#ifdef Q_OS_LINUX
    qInfo("MIC-300 baseline: %d corpus preflights in %lld ms; peak PdfTool VmHWM ~%lld KiB (Linux child process, informational only).",
          ranCases,
          static_cast<long long>(elapsedMs),
          static_cast<long long>(peakChildMemoryKb));
#else
    qInfo("MIC-300 baseline: %d corpus preflights in %lld ms; child peak memory not sampled on this platform (informational wall time only).",
          ranCases,
          static_cast<long long>(elapsedMs));
    Q_UNUSED(peakChildMemoryKb);
#endif
}

QTEST_APPLESS_MAIN(OperatorAcceptanceTest)

#include "tst_operatoracceptance.moc"
