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

#ifndef PDFREDACTVERIFIER_H
#define PDFREDACTVERIFIER_H

#include "pdfdocument.h"
#include "pdfredact.h"

#include <QString>
#include <QVector>

namespace pdf
{

struct PDFRedactVerificationIssue
{
    QString checkId;
    QString message;
};

struct PDFRedactVerification
{
    QVector<PDFRedactVerificationIssue> issues;

    bool passed() const { return issues.isEmpty(); }
};

struct PDFRedactVerificationSettings
{
    PDFRedact::Options redactOptions = PDFRedact::None;
    bool checkIncrementalRewrite = true;
};

class PDF4QTLIBCORESHARED_EXPORT PDFRedactVerifier
{
public:
    static PDFRedactVerification verify(const PDFDocument* originalDocument,
                                        const PDFDocument* redactedDocument,
                                        const PDFRedactVerificationSettings& settings = {},
                                        const QString& redactedFilePath = QString());
};

}   // namespace pdf

#endif // PDFREDACTVERIFIER_H
