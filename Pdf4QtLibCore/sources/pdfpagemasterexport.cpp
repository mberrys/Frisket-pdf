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

#include <QCoreApplication>
#include <QFile>

#include <utility>

namespace pdf
{

namespace
{

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

PDFPageMasterExportResult createExportError(QString message, QStringList writtenFiles = {})
{
    PDFPageMasterExportResult result;
    result.success = false;
    result.errorMessage = std::move(message);
    result.writtenFiles = std::move(writtenFiles);
    return result;
}

PDFPageMasterExportResult createExportCancelled(QStringList writtenFiles = {})
{
    PDFPageMasterExportResult result;
    result.success = false;
    result.cancelled = true;
    result.writtenFiles = std::move(writtenFiles);
    return result;
}

void finishProgressIfActive(PDFProgress* progress)
{
    if (progress)
    {
        progress->finish();
    }
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

    PDFPageMasterExportResult result;
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
            return createExportCancelled(std::move(result.writtenFiles));
        }

        PDFOperationResult currentResult = manipulator.assemble(job.assembledDocuments[index]);
        if (!currentResult)
        {
            finishProgressIfActive(activeProgress(job));
            return createExportError(currentResult.getErrorMessage(), std::move(result.writtenFiles));
        }

        PDFDocument assembledDocument = manipulator.takeAssembledDocument();

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled(std::move(result.writtenFiles));
        }

        if (job.hasPageGeometrySettings)
        {
            const PDFOperationResult geometryResult = PDFPageGeometry::apply(&assembledDocument, job.pageGeometrySettings);
            if (!geometryResult)
            {
                finishProgressIfActive(activeProgress(job));
                return createExportError(geometryResult.getErrorMessage(), std::move(result.writtenFiles));
            }
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled(std::move(result.writtenFiles));
        }

        if (job.hasBleedFixupSettings)
        {
            const PDFOperationResult bleedResult = PDFBleedFixup::apply(&assembledDocument, job.bleedFixupSettings);
            if (!bleedResult)
            {
                finishProgressIfActive(activeProgress(job));
                return createExportError(bleedResult.getErrorMessage(), std::move(result.writtenFiles));
            }
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled(std::move(result.writtenFiles));
        }

        if (job.optimizeImages)
        {
            PDFImageOptimizer imageOptimizer;
            PDFImageOptimizer::Settings optimizeSettings = job.imageOptimizationSettings;
            // job.optimizeImages is authoritative (matches PageMaster UI checkbox wiring).
            optimizeSettings.enabled = true;
            // Pass nullptr so optimize does not nest a second progress phase.
            assembledDocument = imageOptimizer.optimize(&assembledDocument, optimizeSettings, {}, nullptr);
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled(std::move(result.writtenFiles));
        }

        const QString& fileName = job.outputFileNames[index];
        const bool isDocumentFileAlreadyExisting = QFile::exists(fileName);
        if (!job.overwriteFiles && isDocumentFileAlreadyExisting)
        {
            finishProgressIfActive(activeProgress(job));
            return createExportError(QCoreApplication::translate("pdf::PDFPageMasterExport", "Document with filename '%1' already exists.").arg(fileName),
                                    std::move(result.writtenFiles));
        }

        PDFDocumentWriter writer(nullptr);
        PDFOperationResult writeResult = writer.write(fileName, &assembledDocument, isDocumentFileAlreadyExisting);
        if (!writeResult)
        {
            finishProgressIfActive(activeProgress(job));
            return createExportError(writeResult.getErrorMessage(), std::move(result.writtenFiles));
        }

        result.writtenFiles << fileName;
        // assembledDocument leaves scope here — at most one live assembled doc.

        if (PDFProgress* stepProgress = activeProgress(job))
        {
            stepProgress->step();
        }
    }

    if (trackProgress)
    {
        finishProgressIfActive(activeProgress(job));
    }

    if (isCancelRequested(job))
    {
        // All writes already completed; treat as success rather than cancel.
        return result;
    }

    return result;
}

} // namespace pdf
