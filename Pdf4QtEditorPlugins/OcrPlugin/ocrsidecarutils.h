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

#ifndef OCRSIDECARUTILS_H
#define OCRSIDECARUTILS_H

#include <QByteArray>
#include <QDir>
#include <QJsonObject>

#ifndef FRISKET_OCR_SCHEMA_VERSION
#define FRISKET_OCR_SCHEMA_VERSION 1
#endif

namespace pdfplugin::ocr
{

inline QString resolveBundlePath(const QString& applicationDirectory, const QString& relativePath)
{
    return QDir::cleanPath(QDir(applicationDirectory).filePath(relativePath));
}

inline QString getPdfToolFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("PdfTool.exe");
#else
    return QStringLiteral("PdfTool");
#endif
}

inline bool isExpectedOcrExitCode(int exitCode)
{
    return exitCode == 0 || exitCode == 1;
}

inline bool validateOcrReport(const QJsonObject& report, QString* errorMessage = nullptr)
{
    if (!report.contains(QStringLiteral("schema_version")))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("OCR report missing schema_version.");
        }
        return false;
    }

    if (report.value(QStringLiteral("schema_version")).toInt() != FRISKET_OCR_SCHEMA_VERSION)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Unsupported OCR report schema_version.");
        }
        return false;
    }

    if (!report.contains(QStringLiteral("pages")) || !report.value(QStringLiteral("pages")).isArray())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("OCR report missing pages array.");
        }
        return false;
    }

    return true;
}

}   // namespace pdfplugin::ocr

#endif // OCRSIDECARUTILS_H
