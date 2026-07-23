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

#include "pdfocrreport.h"

#include <QJsonArray>

namespace pdf
{

namespace
{

QJsonObject bboxToJson(const QRectF& bbox)
{
    QJsonObject object;
    object.insert(QStringLiteral("x"), bbox.x());
    object.insert(QStringLiteral("y"), bbox.y());
    object.insert(QStringLiteral("width"), bbox.width());
    object.insert(QStringLiteral("height"), bbox.height());
    return object;
}

}   // namespace

QJsonObject PDFOcrReport::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("schema_version"), OCR_REPORT_SCHEMA_VERSION);
    root.insert(QStringLiteral("pass"), pass);
    root.insert(QStringLiteral("pdf"), pdfPath);

    QJsonArray pagesArray;
    for (const PDFOcrPageResult& pageResult : pages)
    {
        QJsonObject pageObject;
        pageObject.insert(QStringLiteral("page"), pageResult.page);
        pageObject.insert(QStringLiteral("status"), pageResult.status);
        if (!pageResult.text.isEmpty())
        {
            pageObject.insert(QStringLiteral("text"), pageResult.text);
        }
        if (!pageResult.error.isEmpty())
        {
            pageObject.insert(QStringLiteral("error"), pageResult.error);
        }

        QJsonArray linesArray;
        for (const PDFOcrLine& line : pageResult.lines)
        {
            QJsonObject lineObject;
            lineObject.insert(QStringLiteral("text"), line.text);
            lineObject.insert(QStringLiteral("confidence"), line.confidence);
            lineObject.insert(QStringLiteral("bbox"), bboxToJson(line.bbox));
            linesArray.append(lineObject);
        }
        if (!linesArray.isEmpty())
        {
            pageObject.insert(QStringLiteral("lines"), linesArray);
        }
        pagesArray.append(pageObject);
    }
    root.insert(QStringLiteral("pages"), pagesArray);

    QJsonArray skippedArray;
    for (const PDFOcrSkippedPage& skipped : skippedPages)
    {
        QJsonObject skippedObject;
        skippedObject.insert(QStringLiteral("page"), skipped.page);
        skippedObject.insert(QStringLiteral("reason"), skipped.reason);
        skippedArray.append(skippedObject);
    }
    root.insert(QStringLiteral("skipped_pages"), skippedArray);

    QJsonArray errorsArray;
    for (const PDFOcrError& error : errors)
    {
        QJsonObject errorObject;
        errorObject.insert(QStringLiteral("message"), error.message);
        if (error.hasPage)
        {
            errorObject.insert(QStringLiteral("page"), error.page);
        }
        errorsArray.append(errorObject);
    }
    root.insert(QStringLiteral("errors"), errorsArray);

    return root;
}

}   // namespace pdf
