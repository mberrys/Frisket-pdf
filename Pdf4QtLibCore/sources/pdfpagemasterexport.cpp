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

PDFPageMasterExportResult createExportError(QString message)
{
    PDFPageMasterExportResult result;
    result.success = false;
    result.errorMessage = std::move(message);
    return result;
}

} // namespace

PDFPageMasterExportResult PDFPageMasterExport::run(PDFPageMasterExportJob job)
{
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

    if (job.progress && !job.assembledDocuments.empty())
    {
        ProgressStartupInfo info;
        info.showDialog = true;
        info.text = QCoreApplication::translate("pdf::PDFPageMasterExport", "Assembling documents...");
        job.progress->start(job.assembledDocuments.size(), std::move(info));
    }

    for (size_t index = 0; index < job.assembledDocuments.size(); ++index)
    {
        PDFOperationResult currentResult = manipulator.assemble(job.assembledDocuments[index]);
        if (!currentResult)
        {
            if (job.progress)
            {
                job.progress->finish();
            }
            return createExportError(currentResult.getErrorMessage());
        }

        PDFDocument assembledDocument = manipulator.takeAssembledDocument();
        if (job.hasPageGeometrySettings)
        {
            const PDFOperationResult geometryResult = PDFPageGeometry::apply(&assembledDocument, job.pageGeometrySettings);
            if (!geometryResult)
            {
                if (job.progress)
                {
                    job.progress->finish();
                }
                return createExportError(geometryResult.getErrorMessage());
            }
        }

        if (job.hasBleedFixupSettings)
        {
            const PDFOperationResult bleedResult = PDFBleedFixup::apply(&assembledDocument, job.bleedFixupSettings);
            if (!bleedResult)
            {
                if (job.progress)
                {
                    job.progress->finish();
                }
                return createExportError(bleedResult.getErrorMessage());
            }
        }

        assembledDocumentStorage.emplace_back(job.outputFileNames[index], std::move(assembledDocument));
        if (job.progress)
        {
            job.progress->step();
        }
    }

    if (job.progress && !job.assembledDocuments.empty())
    {
        job.progress->finish();
    }

    if (job.optimizeImages)
    {
        PDFImageOptimizer imageOptimizer;
        for (auto& assembledDocumentItem : assembledDocumentStorage)
        {
            assembledDocumentItem.second = imageOptimizer.optimize(&assembledDocumentItem.second, job.imageOptimizationSettings, {}, job.progress);
        }
    }

    PDFPageMasterExportResult result;
    result.success = true;
    result.writtenFiles.reserve(int(assembledDocumentStorage.size()));

    if (job.progress && !assembledDocumentStorage.empty())
    {
        ProgressStartupInfo info;
        info.showDialog = true;
        info.text = QCoreApplication::translate("pdf::PDFPageMasterExport", "Writing documents...");
        job.progress->start(assembledDocumentStorage.size(), std::move(info));
    }

    for (const auto& assembledDocumentItem : assembledDocumentStorage)
    {
        const QString& fileName = assembledDocumentItem.first;
        const PDFDocument* document = &assembledDocumentItem.second;
        const bool isDocumentFileAlreadyExisting = QFile::exists(fileName);
        if (!job.overwriteFiles && isDocumentFileAlreadyExisting)
        {
            if (job.progress)
            {
                job.progress->finish();
            }
            return createExportError(QCoreApplication::translate("pdf::PDFPageMasterExport", "Document with filename '%1' already exists.").arg(fileName));
        }

        PDFDocumentWriter writer(nullptr);
        PDFOperationResult writeResult = writer.write(fileName, document, isDocumentFileAlreadyExisting);
        if (!writeResult)
        {
            if (job.progress)
            {
                job.progress->finish();
            }
            return createExportError(writeResult.getErrorMessage());
        }
        result.writtenFiles << fileName;
        if (job.progress)
        {
            job.progress->step();
        }
    }

    if (job.progress && !assembledDocumentStorage.empty())
    {
        job.progress->finish();
    }

    return result;
}

} // namespace pdf
