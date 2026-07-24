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
#include "pdfdocumentwriter.h"
#include "pdfprogress.h"
#include "preflightengine.h"
#include "pdfdocumentsession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <utility>

namespace pdf
{

namespace
{

constexpr int MANIFEST_SCHEMA_VERSION = 1;
constexpr QLatin1String MANIFEST_FILE_NAME(".frisket-batch.json");
constexpr QLatin1String OUTPUT_STATUS_PENDING("pending");
constexpr QLatin1String OUTPUT_STATUS_WRITTEN("written");
constexpr QLatin1String OUTPUT_STATUS_FAILED("failed");

bool isCancelRequested(const PDFPageMasterExportJob& job)
{
    return job.cancelFlag && job.cancelFlag->load(std::memory_order_acquire);
}

PDFProgress* activeProgress(const PDFPageMasterExportJob& job)
{
    if (job.progressAlive && !job.progressAlive->load(std::memory_order_acquire))
    {
        return nullptr;
    }
    return job.progress;
}

PDFPageMasterExportResult createExportError(QString message, QStringList writtenFiles = {}, QString manifestPath = {}, QJsonObject manifest = {})
{
    PDFPageMasterExportResult result;
    result.success = false;
    result.errorMessage = std::move(message);
    result.writtenFiles = std::move(writtenFiles);
    result.manifestPath = std::move(manifestPath);
    result.manifest = std::move(manifest);
    return result;
}

PDFPageMasterExportResult createExportCancelled(QStringList writtenFiles = {}, QString manifestPath = {}, QJsonObject manifest = {})
{
    PDFPageMasterExportResult result;
    result.success = false;
    result.cancelled = true;
    result.writtenFiles = std::move(writtenFiles);
    result.manifestPath = std::move(manifestPath);
    result.manifest = std::move(manifest);
    return result;
}

void finishProgressIfActive(PDFProgress* progress)
{
    if (progress)
    {
        progress->finish();
    }
}

QString resolveManifestPath(const PDFPageMasterExportJob& job)
{
    if (!job.manifestPath.isEmpty())
    {
        return job.manifestPath;
    }

    if (job.outputFileNames.empty())
    {
        return {};
    }

    const QFileInfo firstOutput(job.outputFileNames.front());
    return QDir(firstOutput.absolutePath()).filePath(QString(MANIFEST_FILE_NAME));
}

bool writeFileAtomically(const QString& finalPath, const QByteArray& payload)
{
    QSaveFile saveFile(finalPath);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }

    if (saveFile.write(payload) != payload.size())
    {
        saveFile.cancelWriting();
        return false;
    }

    return saveFile.commit();
}

QJsonObject createManifestObject(const QString& batchId, const QStringList& outputFileNames)
{
    QJsonArray outputs;
    for (const QString& path : outputFileNames)
    {
        outputs.append(QJsonObject{
            { QStringLiteral("path"), path },
            { QStringLiteral("status"), QString(OUTPUT_STATUS_PENDING) }
        });
    }

    return QJsonObject{
        { QStringLiteral("schema_version"), MANIFEST_SCHEMA_VERSION },
        { QStringLiteral("batch_id"), batchId },
        { QStringLiteral("outputs"), outputs }
    };
}

bool persistManifest(const QString& manifestPath, const QJsonObject& manifest)
{
    const QByteArray payload = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    return writeFileAtomically(manifestPath, payload);
}

QString outputStatusAt(const QJsonObject& manifest, int index)
{
    const QJsonArray outputs = manifest.value(QStringLiteral("outputs")).toArray();
    if (index < 0 || index >= outputs.size())
    {
        return {};
    }

    return outputs.at(index).toObject().value(QStringLiteral("status")).toString();
}

void setOutputStatus(QJsonObject& manifest, int index, const QString& status, const QString& error = {})
{
    QJsonArray outputs = manifest.value(QStringLiteral("outputs")).toArray();
    if (index < 0 || index >= outputs.size())
    {
        return;
    }

    QJsonObject entry = outputs.at(index).toObject();
    entry.insert(QStringLiteral("status"), status);
    if (error.isEmpty())
    {
        entry.remove(QStringLiteral("error"));
    }
    else
    {
        entry.insert(QStringLiteral("error"), error);
    }
    outputs.replace(index, entry);
    manifest.insert(QStringLiteral("outputs"), outputs);
}

QString bleedSideName(PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return QStringLiteral("left");
        case PDFBleedFixupSide::Right: return QStringLiteral("right");
        case PDFBleedFixupSide::Top: return QStringLiteral("top");
        case PDFBleedFixupSide::Bottom: return QStringLiteral("bottom");
    }
    return QStringLiteral("unknown");
}

QJsonObject bleedFixupReportToJson(const PDFBleedFixupReport& report)
{
    QJsonArray pages;
    for (const PDFBleedFixupPageReport& page : report.pages)
    {
        QStringList sidesApplied;
        for (PDFBleedFixupSide side : page.sidesApplied)
        {
            sidesApplied.append(bleedSideName(side));
        }

        pages.append(QJsonObject{
            { QStringLiteral("page"), int(page.pageIndex + 1) },
            { QStringLiteral("sides_applied"), QJsonArray::fromStringList(sidesApplied) },
            { QStringLiteral("skip_reasons"), QJsonArray::fromStringList(page.skipReasons) },
            { QStringLiteral("eligible"), sidesApplied.isEmpty() && page.skipReasons.isEmpty() }
        });
    }

    return QJsonObject{
        { QStringLiteral("pages"), pages }
    };
}

void setOutputBleedReport(QJsonObject& manifest, int index, const PDFBleedFixupReport& report)
{
    QJsonArray outputs = manifest.value(QStringLiteral("outputs")).toArray();
    if (index < 0 || index >= outputs.size())
    {
        return;
    }

    QJsonObject entry = outputs.at(index).toObject();
    entry.insert(QStringLiteral("bleed_report"), bleedFixupReportToJson(report));
    outputs.replace(index, entry);
    manifest.insert(QStringLiteral("outputs"), outputs);
}

bool writePreflightReport(const QString& outputPath, const QJsonObject& report)
{
    const QString reportPath = outputPath + QStringLiteral(".preflight.json");
    const QByteArray payload = QJsonDocument(report).toJson(QJsonDocument::Indented);
    return writeFileAtomically(reportPath, payload);
}

bool loadExistingManifest(const QString& manifestPath, QJsonObject* manifest, QString* errorMessage)
{
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                          "Could not read batch manifest at %1.").arg(manifestPath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                      "Batch manifest at %1 is not valid JSON.").arg(manifestPath);
        }
        return false;
    }

    *manifest = document.object();
    return true;
}

bool manifestCompatibleWithJob(const QJsonObject& manifest, const PDFPageMasterExportJob& job)
{
    const QJsonArray outputs = manifest.value(QStringLiteral("outputs")).toArray();
    if (outputs.size() != int(job.outputFileNames.size()))
    {
        return false;
    }

    for (int index = 0; index < outputs.size(); ++index)
    {
        const QString manifestPath = outputs.at(index).toObject().value(QStringLiteral("path")).toString();
        if (QFileInfo(manifestPath).absoluteFilePath() != QFileInfo(job.outputFileNames[size_t(index)]).absoluteFilePath())
        {
            return false;
        }
    }

    return true;
}

int findOutputIndexByPath(const QJsonObject& manifest, const QString& fileName)
{
    const QJsonArray outputs = manifest.value(QStringLiteral("outputs")).toArray();
    const QString absolutePath = QFileInfo(fileName).absoluteFilePath();
    for (int index = 0; index < outputs.size(); ++index)
    {
        const QString entryPath = outputs.at(index).toObject().value(QStringLiteral("path")).toString();
        if (QFileInfo(entryPath).absoluteFilePath() == absolutePath)
        {
            return index;
        }
    }

    return -1;
}

bool shouldSkipResumedOutput(const PDFPageMasterExportJob& job, const QJsonObject& manifest, const QString& fileName)
{
    if (!job.resume)
    {
        return false;
    }

    const int index = findOutputIndexByPath(manifest, fileName);
    if (index < 0)
    {
        return false;
    }

    const QString status = outputStatusAt(manifest, index);
    return status == OUTPUT_STATUS_WRITTEN && QFile::exists(fileName);
}

bool writeDocumentAtomically(const QString& fileName, PDFDocument* document, bool allowOverwrite)
{
    const bool isDocumentFileAlreadyExisting = QFile::exists(fileName);
    if (!allowOverwrite && isDocumentFileAlreadyExisting)
    {
        return false;
    }

    // PDFDocumentWriter(safeWrite=true) uses QSaveFile: temp write then commit/rename
    // without deleting the previous final until the new bytes are durable.
    PDFDocumentWriter writer(nullptr);
    const PDFOperationResult writeResult = writer.write(fileName, document, true);
    if (!writeResult)
    {
        return false;
    }

    return true;
}

} // namespace

PDFPageMasterExportResult PDFPageMasterExport::run(PDFPageMasterExportJob job)
{
    if (isCancelRequested(job))
    {
        return createExportCancelled();
    }

    PDFDocumentManipulator manipulator;
    manipulator.setOutlineMode(job.outlineMode);

    for (const auto& documentItem : job.documents)
    {
        manipulator.addDocument(documentItem.first, &documentItem.second);
    }
    for (const auto& imageItem : job.images)
    {
        manipulator.addImage(imageItem.first, imageItem.second);
    }

    if (job.assembledDocuments.size() != job.outputFileNames.size())
    {
        return createExportError(QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                             "Export job has %1 assembled output(s) but %2 output filename(s).")
                                     .arg(job.assembledDocuments.size())
                                     .arg(job.outputFileNames.size()));
    }

    const QString manifestPath = resolveManifestPath(job);
    QJsonObject manifest;
    QString batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (job.resume && !manifestPath.isEmpty() && QFile::exists(manifestPath))
    {
        QString manifestError;
        if (!loadExistingManifest(manifestPath, &manifest, &manifestError))
        {
            return createExportError(std::move(manifestError));
        }

        if (!manifestCompatibleWithJob(manifest, job))
        {
            // Stale or mismatched batch — start a fresh manifest for this job.
            job.resume = false;
            manifest = createManifestObject(batchId, QStringList(job.outputFileNames.begin(), job.outputFileNames.end()));
            if (!persistManifest(manifestPath, manifest))
            {
                return createExportError(QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                                     "Could not write batch manifest at %1.").arg(manifestPath));
            }
        }
        else
        {
            batchId = manifest.value(QStringLiteral("batch_id")).toString(batchId);
        }
    }
    else
    {
        job.resume = false;
        manifest = createManifestObject(batchId, QStringList(job.outputFileNames.begin(), job.outputFileNames.end()));
        if (!manifestPath.isEmpty() && !persistManifest(manifestPath, manifest))
        {
            return createExportError(QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                                 "Could not write batch manifest at %1.").arg(manifestPath));
        }
    }

    PDFPageMasterExportResult result;
    result.manifestPath = manifestPath;
    result.manifest = manifest;
    result.success = true;
    result.writtenFiles.reserve(int(job.assembledDocuments.size()));

    PDFProgress* progress = activeProgress(job);
    const bool trackProgress = progress && !job.assembledDocuments.empty();
    if (trackProgress)
    {
        ProgressStartupInfo info;
        info.showDialog = true;
        info.text = QCoreApplication::translate("pdf::PDFPageMasterExport", "Exporting documents...");
        progress->start(job.assembledDocuments.size(), std::move(info));
    }

    for (size_t index = 0; index < job.assembledDocuments.size(); ++index)
    {
        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportCancelled(std::move(result.writtenFiles), manifestPath, manifest);
        }

        const QString& fileName = job.outputFileNames[index];

        if (shouldSkipResumedOutput(job, manifest, fileName))
        {
            result.writtenFiles << fileName;
            if (PDFProgress* stepProgress = activeProgress(job))
            {
                stepProgress->step();
            }
            continue;
        }

        PDFOperationResult currentResult = manipulator.assemble(job.assembledDocuments[index]);
        if (!currentResult)
        {
            setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, currentResult.getErrorMessage());
            persistManifest(manifestPath, manifest);
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportError(currentResult.getErrorMessage(), std::move(result.writtenFiles), manifestPath, manifest);
        }

        PDFDocument assembledDocument = manipulator.takeAssembledDocument();

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportCancelled(std::move(result.writtenFiles), manifestPath, manifest);
        }

        if (job.hasPreflightGate && !job.preflightProfilePath.isEmpty())
        {
            QJsonObject profileObject;
            QString profileError;
            if (!PreflightEngine::loadProfile(job.preflightProfilePath, profileObject, profileError))
            {
                setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, profileError);
                persistManifest(manifestPath, manifest);
                finishProgressIfActive(activeProgress(job));
                result.manifest = manifest;
                return createExportError(profileError, std::move(result.writtenFiles), manifestPath, manifest);
            }

            PDFDocumentSession session(&assembledDocument);
            PreflightEngine engine(&session);
            const PreflightResult preflightResult = engine.run(profileObject);
            writePreflightReport(fileName, preflightResult.toJson(fileName));

            if (!preflightResult.pass && !job.forcePreflight)
            {
                const QString message = QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                                    "Preflight failed for '%1'.").arg(fileName);
                setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, message);
                persistManifest(manifestPath, manifest);
                finishProgressIfActive(activeProgress(job));
                result.manifest = manifest;
                return createExportError(message, std::move(result.writtenFiles), manifestPath, manifest);
            }
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportCancelled(std::move(result.writtenFiles), manifestPath, manifest);
        }

        if (job.hasPageGeometrySettings)
        {
            const PDFOperationResult geometryResult = PDFPageGeometry::apply(&assembledDocument, job.pageGeometrySettings);
            if (!geometryResult)
            {
                setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, geometryResult.getErrorMessage());
                persistManifest(manifestPath, manifest);
                finishProgressIfActive(activeProgress(job));
                result.manifest = manifest;
                return createExportError(geometryResult.getErrorMessage(), std::move(result.writtenFiles), manifestPath, manifest);
            }
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportCancelled(std::move(result.writtenFiles), manifestPath, manifest);
        }

        if (job.hasBleedFixupSettings)
        {
            PDFBleedFixupReport bleedReport;
            const PDFOperationResult bleedResult = PDFBleedFixup::apply(&assembledDocument, job.bleedFixupSettings, &bleedReport);
            if (!bleedResult)
            {
                setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, bleedResult.getErrorMessage());
                persistManifest(manifestPath, manifest);
                finishProgressIfActive(activeProgress(job));
                result.manifest = manifest;
                return createExportError(bleedResult.getErrorMessage(), std::move(result.writtenFiles), manifestPath, manifest);
            }
            setOutputBleedReport(manifest, int(index), bleedReport);
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportCancelled(std::move(result.writtenFiles), manifestPath, manifest);
        }

        if (job.optimizeImages)
        {
            PDFImageOptimizer imageOptimizer;
            PDFImageOptimizer::Settings optimizeSettings = job.imageOptimizationSettings;
            optimizeSettings.enabled = true;
            assembledDocument = imageOptimizer.optimize(&assembledDocument, optimizeSettings, {}, nullptr);
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportCancelled(std::move(result.writtenFiles), manifestPath, manifest);
        }

        const bool isDocumentFileAlreadyExisting = QFile::exists(fileName);
        if (!job.overwriteFiles && isDocumentFileAlreadyExisting)
        {
            const QString message = QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                                  "Document with filename '%1' already exists.").arg(fileName);
            setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, message);
            persistManifest(manifestPath, manifest);
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportError(message, std::move(result.writtenFiles), manifestPath, manifest);
        }

        if (!writeDocumentAtomically(fileName, &assembledDocument, job.overwriteFiles))
        {
            const QString message = QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                                "Could not write document to '%1'.").arg(fileName);
            setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, message);
            persistManifest(manifestPath, manifest);
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportError(message, std::move(result.writtenFiles), manifestPath, manifest);
        }

        setOutputStatus(manifest, int(index), OUTPUT_STATUS_WRITTEN);
        if (!persistManifest(manifestPath, manifest))
        {
            QFile::remove(fileName);
            const QString message = QCoreApplication::translate("pdf::PDFPageMasterExport",
                                                                "Manifest update failed after writing '%1'; output was removed to keep batch state consistent.")
                                      .arg(fileName);
            setOutputStatus(manifest, int(index), OUTPUT_STATUS_FAILED, message);
            persistManifest(manifestPath, manifest);
            finishProgressIfActive(activeProgress(job));
            result.manifest = manifest;
            return createExportError(message, std::move(result.writtenFiles), manifestPath, manifest);
        }
        result.writtenFiles << fileName;

        if (PDFProgress* stepProgress = activeProgress(job))
        {
            stepProgress->step();
        }
    }

    if (trackProgress)
    {
        finishProgressIfActive(activeProgress(job));
    }

    result.manifest = manifest;
    return result;
}

} // namespace pdf
