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

#ifndef PDFPAGEMASTEREXPORT_H
#define PDFPAGEMASTEREXPORT_H

#include "pdfglobal.h"
#include "pdfbleedfixup.h"
#include "pdfdocument.h"
#include "pdfdocumentmanipulator.h"
#include "pdfimageoptimizer.h"
#include "pdfpagegeometry.h"

#include <QImage>
#include <QString>
#include <QStringList>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace pdf
{

class PDFProgress;

/// Shared cancel / progress-lifetime flags for a PageMaster export run (MIC-308).
/// UI owns the shared_ptrs; the worker borrows raw pointers via the job.
struct PDF4QTLIBCORESHARED_EXPORT PDFPageMasterExportCancelToken
{
    std::shared_ptr<std::atomic_bool> cancel = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic_bool> progressAlive = std::make_shared<std::atomic_bool>(true);

    void requestCancel() const
    {
        cancel->store(true, std::memory_order_release);
    }

    void invalidateProgress() const
    {
        progressAlive->store(false, std::memory_order_release);
    }

    void requestCancelAndInvalidateProgress() const
    {
        requestCancel();
        invalidateProgress();
    }
};

/// Settings and inputs for a headless PageMaster-style assemble/export batch.
/// Documents and images are owned copies. progress / cancelFlag / progressAlive are borrowed (optional).
struct PDF4QTLIBCORESHARED_EXPORT PDFPageMasterExportJob
{
    std::map<int, PDFDocument> documents;
    std::map<int, QImage> images;
    std::vector<PDFDocumentManipulator::AssembledPages> assembledDocuments;
    std::vector<QString> outputFileNames;
    bool overwriteFiles = false;
    PDFDocumentManipulator::OutlineMode outlineMode = PDFDocumentManipulator::OutlineMode::DocumentParts;
    bool optimizeImages = false;
    PDFImageOptimizer::Settings imageOptimizationSettings;
    bool hasPageGeometrySettings = false;
    PDFPageGeometrySettings pageGeometrySettings;
    bool hasBleedFixupSettings = false;
    PDFBleedFixupSettings bleedFixupSettings;
    PDFProgress* progress = nullptr;
    std::atomic_bool* cancelFlag = nullptr;
    std::atomic_bool* progressAlive = nullptr;
};

/// Result of PDFPageMasterExport::run().
struct PDF4QTLIBCORESHARED_EXPORT PDFPageMasterExportResult
{
    bool success = false;
    bool cancelled = false;
    QString errorMessage;
    QStringList writtenFiles;
};

/// Headless PageMaster export orchestrator (ADR-003).
/// Locked stage order: assemble → page geometry → bleed fixup → image optimize → write.
/// Synchronous and not thread-safe; callers may invoke run() from a worker thread.
class PDF4QTLIBCORESHARED_EXPORT PDFPageMasterExport
{
public:
    static constexpr int DefaultCancelWaitMs = 3000;

    static PDFPageMasterExportResult run(PDFPageMasterExportJob job);
};

} // namespace pdf

#endif // PDFPAGEMASTEREXPORT_H
