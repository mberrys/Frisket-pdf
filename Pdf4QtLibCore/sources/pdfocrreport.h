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

#ifndef PDFOCRREPORT_H
#define PDFOCRREPORT_H

#include "pdfglobal.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>

#include <vector>

namespace pdf
{

inline constexpr int OCR_REPORT_SCHEMA_VERSION = 1;

struct PDF4QTLIBCORESHARED_EXPORT PDFOcrLine
{
    QString text;
    double confidence = 0.0;
    QRectF bbox;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFOcrPageResult
{
    int page = 0;
    QString status;
    QString text;
    std::vector<PDFOcrLine> lines;
    QString error;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFOcrSkippedPage
{
    int page = 0;
    QString reason;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFOcrError
{
    QString message;
    int page = 0;
    bool hasPage = false;
};

struct PDF4QTLIBCORESHARED_EXPORT PDFOcrReport
{
    bool pass = true;
    QString pdfPath;
    std::vector<PDFOcrPageResult> pages;
    std::vector<PDFOcrSkippedPage> skippedPages;
    std::vector<PDFOcrError> errors;

    QJsonObject toJson() const;
};

}   // namespace pdf

#endif // PDFOCRREPORT_H
