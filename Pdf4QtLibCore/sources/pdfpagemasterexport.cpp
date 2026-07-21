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

PDFPageMasterExportResult createExportError(QString message)
{
    PDFPageMasterExportResult result;
    result.success = false;
    result.errorMessage = std::move(message);
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

    std::vector<std::pair<QString, PDFDocument>> assembledDocumentStorage;
    assembledDocumentStorage.reserve(job.assembledDocuments.size());

    if (PDFProgress* progress = activeProgress(job); progress && !job.assembledDocuments.empty())
    {
        ProgressStartupInfo info;
        info.showDialog = true;
        info.text = QCoreApplication::translate("pdf::PDFPageMasterExport", "Assembling documents...");
        progress->start(job.assembledDocuments.size(), std::move(info));
    }

    for (size_t index = 0; index < job.assembledDocuments.size(); ++index)
    {
        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled();
        }

        PDFOperationResult currentResult = manipulator.assemble(job.assembledDocuments[index]);
        if (!currentResult)
        {
            finishProgressIfActive(activeProgress(job));
            return createExportError(currentResult.getErrorMessage());
        }

        PDFDocument assembledDocument = manipulator.takeAssembledDocument();

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled();
        }

        if (job.hasPageGeometrySettings)
        {
            const PDFOperationResult geometryResult = PDFPageGeometry::apply(&assembledDocument, job.pageGeometrySettings);
            if (!geometryResult)
            {
                finishProgressIfActive(activeProgress(job));
                return createExportError(geometryResult.getErrorMessage());
            }
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled();
        }

        if (job.hasBleedFixupSettings)
        {
            const PDFOperationResult bleedResult = PDFBleedFixup::apply(&assembledDocument, job.bleedFixupSettings);
            if (!bleedResult)
            {
                finishProgressIfActive(activeProgress(job));
                return createExportError(bleedResult.getErrorMessage());
            }
        }

        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled();
        }

        assembledDocumentStorage.emplace_back(job.outputFileNames[index], std::move(assembledDocument));
        if (PDFProgress* progress = activeProgress(job))
        {
            progress->step();
        }
    }

    if (PDFProgress* progress = activeProgress(job); progress && !job.assembledDocuments.empty())
    {
        progress->finish();
    }

    if (isCancelRequested(job))
    {
        return createExportCancelled();
    }

    if (job.optimizeImages)
    {
        PDFImageOptimizer imageOptimizer;
        for (auto& assembledDocumentItem : assembledDocumentStorage)
        {
            if (isCancelRequested(job))
            {
                return createExportCancelled();
            }

            assembledDocumentItem.second = imageOptimizer.optimize(&assembledDocumentItem.second,
                                                                   job.imageOptimizationSettings,
                                                                   {},
                                                                   activeProgress(job));
        }
    }

    if (isCancelRequested(job))
    {
        return createExportCancelled();
    }

    PDFPageMasterExportResult result;
    result.success = true;
    result.writtenFiles.reserve(int(assembledDocumentStorage.size()));

    if (PDFProgress* progress = activeProgress(job); progress && !assembledDocumentStorage.empty())
    {
        ProgressStartupInfo info;
        info.showDialog = true;
        info.text = QCoreApplication::translate("pdf::PDFPageMasterExport", "Writing documents...");
        progress->start(assembledDocumentStorage.size(), std::move(info));
    }

    for (const auto& assembledDocumentItem : assembledDocumentStorage)
    {
        if (isCancelRequested(job))
        {
            finishProgressIfActive(activeProgress(job));
            return createExportCancelled(std::move(result.writtenFiles));
        }

        const QString& fileName = assembledDocumentItem.first;
        const PDFDocument* document = &assembledDocumentItem.second;
        const bool isDocumentFileAlreadyExisting = QFile::exists(fileName);
        if (!job.overwriteFiles && isDocumentFileAlreadyExisting)
        {
            finishProgressIfActive(activeProgress(job));
            return createExportError(QCoreApplication::translate("pdf::PDFPageMasterExport", "Document with filename '%1' already exists.").arg(fileName));
        }

        PDFDocumentWriter writer(nullptr);
        PDFOperationResult writeResult = writer.write(fileName, document, isDocumentFileAlreadyExisting);
        if (!writeResult)
        {
            finishProgressIfActive(activeProgress(job));
            return createExportError(writeResult.getErrorMessage());
        }
        result.writtenFiles << fileName;
        if (PDFProgress* progress = activeProgress(job))
        {
            progress->step();
        }
    }

    if (PDFProgress* progress = activeProgress(job); progress && !assembledDocumentStorage.empty())
    {
        progress->finish();
    }

    if (isCancelRequested(job))
    {
        // All writes already completed; treat as success rather than cancel.
        return result;
    }

    return result;
}

} // namespace pdf
