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

#include "pdfpagemasterexport.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentreader.h"
#include "pdfglobal.h"
#include "pdfprogress.h"

#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>

class PageMasterExportTest : public QObject
{
    Q_OBJECT

private slots:
    void pipelineOrder_geometryThenBleedThenWrite();
    void pipelineOrder_skipsDisabledStages();
    void multiOutput_writesAll();
    void failure_assembleError_writesNothing();
    void failure_geometryError_noPartialWrite();
    void failure_bleedError_noPartialWrite();
    void failure_overwriteDisabled_existingFile();
    void failure_writeError_reportsMessage();
    void multiOutput_midBatchFailure_keepsEarlierWrites();
    void progress_singleCombinedPhase();
    void multiOutput_benchmarkRecordsTiming();
};

namespace
{

pdf::PDFDocument buildFilledPage(const QRectF& mediaBox = QRectF(0, 0, 200, 200))
{
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(mediaBox);
    builder.setPageTrimBox(page, mediaBox.adjusted(10, 10, -10, -10));

    pdf::PDFPageContentStreamBuilder pageContentStreamBuilder(&builder,
                                                              pdf::PDFContentStreamBuilder::CoordinateSystem::PDF);
    if (QPainter* painter = pageContentStreamBuilder.begin(page))
    {
        painter->fillRect(mediaBox.adjusted(10, 10, -10, -10), Qt::black);
        pageContentStreamBuilder.end(painter);
    }

    return builder.build();
}

QSizeF pageSizeMM(const pdf::PDFDocument& document)
{
    const pdf::PDFPage* page = document.getCatalog()->getPage(0);
    const QRectF mediaBox = page->getMediaBox();
    return QSizeF(mediaBox.width() * pdf::PDF_POINT_TO_MM, mediaBox.height() * pdf::PDF_POINT_TO_MM);
}

pdf::PDFDocumentManipulator::AssembledPage documentPage(int documentIndex, const pdf::PDFDocument& document)
{
    return pdf::PDFDocumentManipulator::createDocumentPage(documentIndex, 0, pageSizeMM(document), pdf::PageRotation::None);
}

pdf::PDFDocument readDocument(const QString& fileName)
{
    pdf::PDFDocumentReader reader(nullptr, [](bool*) { return QString(); }, true, false);
    pdf::PDFDocument document = reader.readFromFile(fileName);
    Q_ASSERT(reader.getReadingResult() == pdf::PDFDocumentReader::Result::OK);
    return document;
}

bool anyOutputExists(const QStringList& paths)
{
    for (const QString& path : paths)
    {
        if (QFile::exists(path))
        {
            return true;
        }
    }
    return false;
}

/// Peak resident set from /proc (Linux); returns 0 when unavailable.
qint64 readVmHWMKilobytes()
{
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return 0;
    }

    const QString contents = QString::fromUtf8(status.readAll());
    for (const QString& rawLine : contents.split(QLatin1Char('\n')))
    {
        const QString line = rawLine.trimmed();
        if (!line.startsWith(QStringLiteral("VmHWM:")))
        {
            continue;
        }

        const QStringList parts = line.mid(6).simplified().split(QLatin1Char(' '));
        if (!parts.isEmpty())
        {
            bool ok = false;
            const qint64 value = parts.front().toLongLong(&ok);
            if (ok)
            {
                return value;
            }
        }
    }
    return 0;
}

} // namespace

void PageMasterExportTest::pipelineOrder_geometryThenBleedThenWrite()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    pdf::PDFDocument source = buildFilledPage();
    const qreal sourceMediaWidth = source.getCatalog()->getPage(0)->getMediaBox().width();

    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ documentPage(0, source) });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("geometry-bleed.pdf")));
    job.overwriteFiles = true;

    job.hasPageGeometrySettings = true;
    job.pageGeometrySettings.useTargetPageSize = true;
    job.pageGeometrySettings.targetPageSizeMM = QSizeF(100.0, 100.0);
    job.pageGeometrySettings.applyMediaBox = true;
    job.pageGeometrySettings.applyCropBox = true;
    job.pageGeometrySettings.applyBleedBox = true;
    job.pageGeometrySettings.applyTrimBox = true;

    job.hasBleedFixupSettings = true;
    job.bleedFixupSettings.force = true;
    job.bleedFixupSettings.skipIfAlreadyBleeding = false;
    job.bleedFixupSettings.dpi = 72;
    job.bleedFixupSettings.bleedMM = QMarginsF(3.0, 3.0, 3.0, 3.0);

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.writtenFiles.size(), 1);
    QVERIFY(QFile::exists(result.writtenFiles.front()));

    const pdf::PDFDocument written = readDocument(result.writtenFiles.front());
    const pdf::PDFPage* page = written.getCatalog()->getPage(0);
    QVERIFY(page);

    const qreal geometryWidthPt = 100.0 * pdf::PDF_MM_TO_POINT;
    const qreal bleedDepthPt = 3.0 * pdf::PDF_MM_TO_POINT;
    const qreal expectedMediaWidth = geometryWidthPt + (2.0 * bleedDepthPt);

    QVERIFY(sourceMediaWidth < geometryWidthPt);
    QVERIFY2(qAbs(page->getMediaBox().width() - expectedMediaWidth) < 1.0,
             qPrintable(QStringLiteral("media width %1 expected ~%2")
                            .arg(page->getMediaBox().width())
                            .arg(expectedMediaWidth)));
    QVERIFY(page->getTrimBox().width() < page->getMediaBox().width());
}

void PageMasterExportTest::pipelineOrder_skipsDisabledStages()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    pdf::PDFDocument source = buildFilledPage();
    const qreal sourceMediaWidth = source.getCatalog()->getPage(0)->getMediaBox().width();

    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ documentPage(0, source) });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("plain.pdf")));
    job.overwriteFiles = true;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY2(result.success, qPrintable(result.errorMessage));

    const pdf::PDFDocument written = readDocument(result.writtenFiles.front());
    const pdf::PDFPage* page = written.getCatalog()->getPage(0);
    QVERIFY(page);
    QVERIFY2(qAbs(page->getMediaBox().width() - sourceMediaWidth) < 1.0,
             qPrintable(QStringLiteral("media width changed without geometry/bleed: %1 vs %2")
                            .arg(page->getMediaBox().width())
                            .arg(sourceMediaWidth)));
}

void PageMasterExportTest::multiOutput_writesAll()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    pdf::PDFDocument source = buildFilledPage();
    const auto page = documentPage(0, source);

    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ page });
    job.assembledDocuments.push_back({ page });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("out-a.pdf")));
    job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("out-b.pdf")));
    job.overwriteFiles = true;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.writtenFiles.size(), 2);
    for (const QString& path : result.writtenFiles)
    {
        QVERIFY(QFile::exists(path));
    }
}

void PageMasterExportTest::failure_assembleError_writesNothing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString outputPath = tempDir.filePath(QStringLiteral("assemble-fail.pdf"));

    pdf::PDFDocument source = buildFilledPage();
    pdf::PDFPageMasterExportJob job;
    // Point at a non-existent document index so assemble fails.
    job.assembledDocuments.push_back({
        pdf::PDFDocumentManipulator::createDocumentPage(99, 0, QSizeF(70.0, 70.0), pdf::PageRotation::None)
    });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(outputPath);
    job.overwriteFiles = true;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.writtenFiles.isEmpty());
    QVERIFY(!QFile::exists(outputPath));
}

void PageMasterExportTest::failure_geometryError_noPartialWrite()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString outputPath = tempDir.filePath(QStringLiteral("geometry-fail.pdf"));

    pdf::PDFDocument source = buildFilledPage();
    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ documentPage(0, source) });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(outputPath);
    job.overwriteFiles = true;
    job.hasPageGeometrySettings = true;
    job.pageGeometrySettings.applyMediaBox = false;
    job.pageGeometrySettings.applyCropBox = false;
    job.pageGeometrySettings.applyBleedBox = false;
    job.pageGeometrySettings.applyTrimBox = false;
    job.pageGeometrySettings.applyArtBox = false;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("No target page box"), Qt::CaseInsensitive));
    QVERIFY(result.writtenFiles.isEmpty());
    QVERIFY(!QFile::exists(outputPath));
}

void PageMasterExportTest::failure_bleedError_noPartialWrite()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString outputPath = tempDir.filePath(QStringLiteral("bleed-fail.pdf"));

    pdf::PDFDocument source = buildFilledPage();
    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ documentPage(0, source) });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(outputPath);
    job.overwriteFiles = true;
    job.hasBleedFixupSettings = true;
    job.bleedFixupSettings.dpi = 0;
    job.bleedFixupSettings.force = true;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("DPI"), Qt::CaseInsensitive));
    QVERIFY(result.writtenFiles.isEmpty());
    QVERIFY(!QFile::exists(outputPath));
}

void PageMasterExportTest::failure_overwriteDisabled_existingFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString outputPath = tempDir.filePath(QStringLiteral("exists.pdf"));
    {
        QFile existing(outputPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("placeholder");
    }
    const qint64 existingSize = QFileInfo(outputPath).size();

    pdf::PDFDocument source = buildFilledPage();
    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ documentPage(0, source) });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(outputPath);
    job.overwriteFiles = false;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("already exists"), Qt::CaseInsensitive));
    QVERIFY(result.writtenFiles.isEmpty());
    QCOMPARE(QFileInfo(outputPath).size(), existingSize);
}

void PageMasterExportTest::failure_writeError_reportsMessage()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString missingDir = tempDir.filePath(QStringLiteral("missing-subdir"));
    const QString outputPath = QDir(missingDir).filePath(QStringLiteral("out.pdf"));
    QVERIFY(!QDir(missingDir).exists());

    pdf::PDFDocument source = buildFilledPage();
    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ documentPage(0, source) });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(outputPath);
    job.overwriteFiles = true;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.writtenFiles.isEmpty());
    QVERIFY(!anyOutputExists({ outputPath }));
}

void PageMasterExportTest::multiOutput_midBatchFailure_keepsEarlierWrites()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString firstPath = tempDir.filePath(QStringLiteral("ok.pdf"));
    const QString missingDir = tempDir.filePath(QStringLiteral("missing-subdir"));
    const QString secondPath = QDir(missingDir).filePath(QStringLiteral("fail.pdf"));
    QVERIFY(!QDir(missingDir).exists());

    pdf::PDFDocument source = buildFilledPage();
    const auto page = documentPage(0, source);

    pdf::PDFPageMasterExportJob job;
    job.assembledDocuments.push_back({ page });
    job.assembledDocuments.push_back({ page });
    job.documents.emplace(0, std::move(source));
    job.outputFileNames.push_back(firstPath);
    job.outputFileNames.push_back(secondPath);
    job.overwriteFiles = true;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
    QCOMPARE(result.writtenFiles.size(), 1);
    QCOMPARE(result.writtenFiles.front(), firstPath);
    QVERIFY(QFile::exists(firstPath));
    QVERIFY(!QFile::exists(secondPath));
}

void PageMasterExportTest::progress_singleCombinedPhase()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    pdf::PDFProgress progress(nullptr);
    QSignalSpy startedSpy(&progress, &pdf::PDFProgress::progressStarted);
    QSignalSpy stepSpy(&progress, &pdf::PDFProgress::progressStep);
    QSignalSpy finishedSpy(&progress, &pdf::PDFProgress::progressFinished);

    // Use image pages so optimize actually finds images. If progress were
    // wrongly forwarded to PDFImageOptimizer, it would start nested
    // "Optimizing images..." phases and startedSpy.count() would exceed 1.
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(Qt::red);
    const auto page = pdf::PDFDocumentManipulator::createImagePage(0, QSizeF(50.0, 50.0), pdf::PageRotation::None);

    pdf::PDFPageMasterExportJob job;
    job.images.emplace(0, std::move(image));
    job.assembledDocuments.push_back({ page });
    job.assembledDocuments.push_back({ page });
    job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("prog-a.pdf")));
    job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("prog-b.pdf")));
    job.overwriteFiles = true;
    job.optimizeImages = true;
    // Leave settings.enabled at default (false) — service must treat
    // optimizeImages as authoritative so optimize still runs.
    job.imageOptimizationSettings = pdf::PDFImageOptimizer::Settings::createDefault();
    QVERIFY(!job.imageOptimizationSettings.enabled);
    job.progress = &progress;

    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.writtenFiles.size(), 2);

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(stepSpy.count(), 2);

    const auto startedArgs = startedSpy.takeFirst();
    QCOMPARE(startedArgs.size(), 1);
    const pdf::ProgressStartupInfo info = qvariant_cast<pdf::ProgressStartupInfo>(startedArgs.at(0));
    QVERIFY(info.text.contains(QStringLiteral("Exporting"), Qt::CaseInsensitive));
    QVERIFY(!info.text.contains(QStringLiteral("Assembling"), Qt::CaseInsensitive));
    QVERIFY(!info.text.contains(QStringLiteral("Writing"), Qt::CaseInsensitive));
    QVERIFY(!info.text.contains(QStringLiteral("Optimizing"), Qt::CaseInsensitive));

    const int lastPercentage = stepSpy.last().at(0).toInt();
    QCOMPARE(lastPercentage, 100);
}

void PageMasterExportTest::multiOutput_benchmarkRecordsTiming()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    constexpr int outputCount = 8;
    pdf::PDFDocument source = buildFilledPage();
    const auto page = documentPage(0, source);

    pdf::PDFPageMasterExportJob job;
    job.documents.emplace(0, std::move(source));
    job.overwriteFiles = true;
    for (int i = 0; i < outputCount; ++i)
    {
        job.assembledDocuments.push_back({ page });
        job.outputFileNames.push_back(tempDir.filePath(QStringLiteral("bench-%1.pdf").arg(i)));
    }

    const qint64 vmBefore = readVmHWMKilobytes();
    QElapsedTimer timer;
    timer.start();
    const pdf::PDFPageMasterExportResult result = pdf::PDFPageMasterExport::run(std::move(job));
    const qint64 elapsedMs = timer.elapsed();
    const qint64 vmAfter = readVmHWMKilobytes();

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.writtenFiles.size(), outputCount);

    qInfo("MIC-307 multi-output benchmark: outputs=%d wall_ms=%lld VmHWM_before_kB=%lld VmHWM_after_kB=%lld",
          outputCount,
          static_cast<long long>(elapsedMs),
          static_cast<long long>(vmBefore),
          static_cast<long long>(vmAfter));

    QVERIFY(elapsedMs >= 0);
}

QTEST_APPLESS_MAIN(PageMasterExportTest)
#include "tst_pagemasterexporttest.moc"
