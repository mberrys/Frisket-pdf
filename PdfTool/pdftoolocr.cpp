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

#include "pdftoolocr.h"

#include "pdftoolcancel.h"
#include "pdfcatalog.h"
#include "pdfdocumentsession.h"
#include "pdfocrpagegate.h"
#include "pdfocrreport.h"
#include "pdfconstants.h"
#include "pdfoptionalcontent.h"
#include "pdfpage.h"
#include "pdffont.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

namespace pdftool
{

namespace
{

static PDFToolOcrApplication s_ocrApplication;

QString bundledOcrSidecarPath(const QString& applicationDirectory)
{
#ifdef Q_OS_WIN
    return QDir(applicationDirectory).filePath(QStringLiteral("FrisketOcrService/FrisketOcrService.exe"));
#else
    return QDir(applicationDirectory).filePath(QStringLiteral("FrisketOcrService/FrisketOcrService"));
#endif
}

QString devOcrSidecarPath(const QString& applicationDirectory)
{
#ifdef Q_OS_WIN
    const QString relativePath = QStringLiteral("../../../../frisket-ocr/tools/dev_ocr_sidecar.cmd");
#else
    const QString relativePath = QStringLiteral("../../../../frisket-ocr/tools/dev_ocr_sidecar.sh");
#endif
    return QDir::cleanPath(QDir(applicationDirectory).filePath(relativePath));
}

bool sidecarIsRunnable(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists())
    {
        return false;
    }
#ifdef Q_OS_WIN
    if (info.isFile())
    {
        return true;
    }
    return info.suffix().compare(QStringLiteral("cmd"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("bat"), Qt::CaseInsensitive) == 0;
#else
    return info.isExecutable();
#endif
}

QString resolveOcrSidecarPath()
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();

    const QByteArray envSidecar = qgetenv("FRISKET_OCR_SIDECAR");
    if (!envSidecar.isEmpty())
    {
        const QString path = QString::fromUtf8(envSidecar);
        if (sidecarIsRunnable(path))
        {
            return path;
        }
    }

    const QString bundledPath = bundledOcrSidecarPath(applicationDirectory);
    if (sidecarIsRunnable(bundledPath))
    {
        return bundledPath;
    }

    const QString devPath = devOcrSidecarPath(applicationDirectory);
    if (sidecarIsRunnable(devPath))
    {
        return devPath;
    }

    return bundledPath;
}

QJsonObject mediaBoxObject(const pdf::PDFPage* page)
{
    const QRectF mediaBox = page->getMediaBox();
    QJsonObject object;
    object.insert(QStringLiteral("x"), mediaBox.x());
    object.insert(QStringLiteral("y"), mediaBox.y());
    object.insert(QStringLiteral("width"), mediaBox.width());
    object.insert(QStringLiteral("height"), mediaBox.height());
    return object;
}

QRectF bboxFromJson(const QJsonObject& object)
{
    return QRectF(object.value(QStringLiteral("x")).toDouble(),
                  object.value(QStringLiteral("y")).toDouble(),
                  object.value(QStringLiteral("width")).toDouble(),
                  object.value(QStringLiteral("height")).toDouble());
}

class OcrSidecarClient
{
public:
    bool start(const QString& sidecarPath, QString& errorMessage)
    {
        if (!sidecarIsRunnable(sidecarPath))
        {
            errorMessage = PDFToolTranslationContext::tr("OCR sidecar not found or not runnable: %1").arg(sidecarPath);
            return false;
        }

        m_process.setProgram(sidecarPath);
        m_process.setArguments({});
        m_process.setProcessChannelMode(QProcess::SeparateChannels);
        m_process.start();
        if (!m_process.waitForStarted(30000))
        {
            errorMessage = PDFToolTranslationContext::tr("Failed to start OCR sidecar: %1").arg(m_process.errorString());
            return false;
        }
        return true;
    }

    bool sendRequest(const QJsonObject& request, QJsonObject& response, QString& errorMessage)
    {
        const QByteArray line = QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
        if (m_process.write(line) != line.size())
        {
            errorMessage = PDFToolTranslationContext::tr("Failed to write OCR request to sidecar.");
            return false;
        }
        if (!m_process.waitForBytesWritten(30000))
        {
            errorMessage = PDFToolTranslationContext::tr("Timed out writing OCR request to sidecar.");
            return false;
        }

        // EasyOCR startup can take a while and native libs may emit blank
        // stdout lines; keep reading until a JSON object line arrives.
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 120000)
        {
            if (!m_process.canReadLine())
            {
                if (!m_process.waitForReadyRead(qMax(1, 120000 - int(timer.elapsed()))))
                {
                    break;
                }
                continue;
            }

            const QByteArray outputLine = m_process.readLine().trimmed();
            if (outputLine.isEmpty())
            {
                continue;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(outputLine, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                errorMessage = PDFToolTranslationContext::tr("Invalid OCR sidecar JSON: %1").arg(parseError.errorString());
                return false;
            }

            response = document.object();
            return true;
        }

        errorMessage = PDFToolTranslationContext::tr("Timed out waiting for OCR sidecar response.");
        return false;
    }

    void stop()
    {
        if (m_process.state() != QProcess::NotRunning)
        {
            m_process.closeWriteChannel();
            m_process.waitForFinished(5000);
            if (m_process.state() != QProcess::NotRunning)
            {
                m_process.kill();
                m_process.waitForFinished(3000);
            }
        }
    }

private:
    QProcess m_process;
};

bool renderPageToPng(pdf::PDFDocument* document,
                     pdf::PDFInteger pageIndex,
                     int dpi,
                     const QString& outputPath,
                     QString& errorMessage)
{
    pdf::PDFOptionalContentActivity optionalContentActivity(document, pdf::OCUsage::Export, nullptr);
    pdf::PDFCMSManager cmsManager(nullptr);
    cmsManager.setDocument(document);
    pdf::PDFMeshQualitySettings meshQualitySettings;
    pdf::PDFFontCache fontCache(pdf::DEFAULT_FONT_CACHE_LIMIT, pdf::DEFAULT_REALIZED_FONT_CACHE_LIMIT);
    pdf::PDFModifiedDocument modifiedDocument(document, &optionalContentActivity);
    fontCache.setDocument(modifiedDocument);
    fontCache.setCacheShrinkEnabled(nullptr, false);

    const pdf::PDFPage* page = document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        errorMessage = PDFToolTranslationContext::tr("Page %1 does not exist.").arg(pageIndex + 1);
        return false;
    }

    const QSize imageSize = (page->getRotatedMediaBox().size() * pdf::PDF_POINT_TO_INCH * qreal(dpi)).toSize();
    if (imageSize.isEmpty())
    {
        errorMessage = PDFToolTranslationContext::tr("Invalid render size for page %1.").arg(pageIndex + 1);
        return false;
    }

    pdf::PDFRasterizerPool rasterizerPool(document,
                                          &fontCache,
                                          &cmsManager,
                                          &optionalContentActivity,
                                          pdf::PDFRenderer::None,
                                          meshQualitySettings,
                                          1,
                                          pdf::RendererEngine::QPainter,
                                          nullptr);

    std::vector<pdf::PDFInteger> pageIndices = { pageIndex };
    bool rendered = false;
    QString renderError;

    auto onRendered = [&](pdf::PDFRenderedPageImage& renderedPageImage)
    {
        QImageWriter writer(outputPath, "png");
        if (!writer.write(renderedPageImage.pageImage))
        {
            renderError = writer.errorString();
            return;
        }
        rendered = true;
    };

    rasterizerPool.render(pageIndices,
                          [&](const pdf::PDFPage* renderPage) -> QSize
                          {
                              Q_UNUSED(renderPage);
                              return imageSize;
                          },
                          onRendered,
                          nullptr);

    fontCache.setCacheShrinkEnabled(nullptr, true);

    if (!rendered)
    {
        errorMessage = renderError.isEmpty()
            ? PDFToolTranslationContext::tr("Failed to render page %1.").arg(pageIndex + 1)
            : renderError;
        return false;
    }

    return true;
}

}   // namespace

QString PDFToolOcrApplication::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
        case Command:
            return QStringLiteral("ocr");

        case Name:
            return PDFToolTranslationContext::tr("OCR");

        case Description:
            return PDFToolTranslationContext::tr("Run Frisket OCR on image-only pages and emit a JSON report.");

        default:
            Q_ASSERT(false);
            break;
    }

    return QString();
}

PDFToolAbstractApplication::Options PDFToolOcrApplication::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | PageSelector | OcrOptions;
}

int PDFToolOcrApplication::execute(const PDFToolOptions& options)
{
    if (options.document.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("No document specified."), options.outputCodec);
        return OcrContractError;
    }

    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    QString parseError;
    const std::vector<pdf::PDFInteger> pageIndices =
        options.getPageRange(document.getCatalog()->getPageCount(), parseError, true);
    if (!parseError.isEmpty())
    {
        PDFConsole::writeError(parseError, options.outputCodec);
        return OcrContractError;
    }

    const QString sidecarPath = options.ocrSidecarPath.isEmpty() ? resolveOcrSidecarPath() : options.ocrSidecarPath;
    OcrSidecarClient sidecar;
    QString sidecarError;
    if (!sidecar.start(sidecarPath, sidecarError))
    {
        PDFConsole::writeError(sidecarError, options.outputCodec);
        return OcrSidecarUnavailable;
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Could not create temporary directory for OCR images."),
                               options.outputCodec);
        sidecar.stop();
        return OcrContractError;
    }

    pdf::PDFDocumentSession session(&document);
    pdf::PDFOcrPageGate::Settings gateSettings;
    gateSettings.minTextCharacters = options.ocrMinTextChars;

    pdf::PDFOcrReport report;
    report.pdfPath = options.document;
    report.pass = true;

    QStringList languages = options.ocrLanguages.split(',', Qt::SkipEmptyParts);
    if (languages.isEmpty())
    {
        languages = QStringList{ QStringLiteral("en") };
    }

    QJsonArray languagesArray;
    for (const QString& language : languages)
    {
        languagesArray.append(language.trimmed());
    }

    bool anyPageFailed = false;
    bool cancelled = false;

    for (pdf::PDFInteger pageIndex : pageIndices)
    {
        if (cancelRequested().load(std::memory_order_acquire))
        {
            cancelled = true;
            break;
        }

        const int oneBasedPage = int(pageIndex) + 1;
        const pdf::PDFOcrPageGate::PageOcrNeed need =
            pdf::PDFOcrPageGate::classifyPage(&session, pageIndex, gateSettings);

        if (need == pdf::PDFOcrPageGate::PageOcrNeed::Failed)
        {
            pdf::PDFOcrPageResult pageResult;
            pageResult.page = oneBasedPage;
            pageResult.status = QStringLiteral("failed");
            pageResult.error = PDFToolTranslationContext::tr("Page gate failed.");
            report.pages.push_back(pageResult);
            report.pass = false;
            anyPageFailed = true;
            continue;
        }

        if (need == pdf::PDFOcrPageGate::PageOcrNeed::SkipHasText)
        {
            pdf::PDFOcrSkippedPage skipped;
            skipped.page = oneBasedPage;
            skipped.reason = QStringLiteral("has_text");
            report.skippedPages.push_back(skipped);

            pdf::PDFOcrPageResult pageResult;
            pageResult.page = oneBasedPage;
            pageResult.status = QStringLiteral("skipped_has_text");
            report.pages.push_back(pageResult);
            continue;
        }

        if (need == pdf::PDFOcrPageGate::PageOcrNeed::SkipEmpty)
        {
            pdf::PDFOcrSkippedPage skipped;
            skipped.page = oneBasedPage;
            skipped.reason = QStringLiteral("empty");
            report.skippedPages.push_back(skipped);

            pdf::PDFOcrPageResult pageResult;
            pageResult.page = oneBasedPage;
            pageResult.status = QStringLiteral("skipped_empty");
            report.pages.push_back(pageResult);
            continue;
        }

        const pdf::PDFPage* page = document.getCatalog()->getPage(pageIndex);
        const QString imagePath = temporaryDirectory.filePath(QStringLiteral("page-%1.png").arg(oneBasedPage));
        QString renderError;
        if (!renderPageToPng(&document, pageIndex, options.ocrDpi, imagePath, renderError))
        {
            pdf::PDFOcrPageResult pageResult;
            pageResult.page = oneBasedPage;
            pageResult.status = QStringLiteral("failed");
            pageResult.error = renderError;
            report.pages.push_back(pageResult);
            report.pass = false;
            anyPageFailed = true;
            continue;
        }

        QJsonObject request;
        request.insert(QStringLiteral("image"), QDir::toNativeSeparators(imagePath));
        request.insert(QStringLiteral("page"), oneBasedPage);
        request.insert(QStringLiteral("dpi"), options.ocrDpi);
        request.insert(QStringLiteral("languages"), languagesArray);
        request.insert(QStringLiteral("media_box"), mediaBoxObject(page));

        QJsonObject response;
        QString requestError;
        if (!sidecar.sendRequest(request, response, requestError))
        {
            pdf::PDFOcrError error;
            error.message = requestError;
            error.page = oneBasedPage;
            error.hasPage = true;
            report.errors.push_back(error);

            pdf::PDFOcrPageResult pageResult;
            pageResult.page = oneBasedPage;
            pageResult.status = QStringLiteral("failed");
            pageResult.error = requestError;
            report.pages.push_back(pageResult);
            report.pass = false;
            anyPageFailed = true;
            continue;
        }

        if (!response.value(QStringLiteral("ok")).toBool())
        {
            const QString errorText = response.value(QStringLiteral("error")).toString();
            pdf::PDFOcrPageResult pageResult;
            pageResult.page = oneBasedPage;
            pageResult.status = QStringLiteral("failed");
            pageResult.error = errorText;
            report.pages.push_back(pageResult);
            report.pass = false;
            anyPageFailed = true;
            continue;
        }

        pdf::PDFOcrPageResult pageResult;
        pageResult.page = oneBasedPage;
        pageResult.status = QStringLiteral("ocr");
        pageResult.text = response.value(QStringLiteral("text")).toString();

        const QJsonArray lines = response.value(QStringLiteral("lines")).toArray();
        for (const QJsonValue& lineValue : lines)
        {
            const QJsonObject lineObject = lineValue.toObject();
            pdf::PDFOcrLine line;
            line.text = lineObject.value(QStringLiteral("text")).toString();
            line.confidence = lineObject.value(QStringLiteral("confidence")).toDouble();
            line.bbox = bboxFromJson(lineObject.value(QStringLiteral("bbox")).toObject());
            pageResult.lines.push_back(line);
        }
        report.pages.push_back(pageResult);
    }

    sidecar.stop();

    const QByteArray reportJson = QJsonDocument(report.toJson()).toJson(QJsonDocument::Compact);
    PDFConsole::writeData(reportJson);
    PDFConsole::writeData(QByteArray("\n"));

    if (cancelled)
    {
        return OcrCancelled;
    }

    if (anyPageFailed)
    {
        return OcrPartialFailure;
    }

    return OcrSuccess;
}

}   // namespace pdftool
