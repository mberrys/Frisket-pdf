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

#ifndef OCRPLUGIN_H
#define OCRPLUGIN_H

#include "pdfplugin.h"
#include "ocrsidecarutils.h"

#include <QByteArray>
#include <QJsonObject>
#include <memory>

class QProcess;
class QTemporaryDir;

namespace pdfplugin
{

class OcrReportDockWidget;

class OcrPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.OcrPlugin" FILE "OcrPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    OcrPlugin();
    virtual ~OcrPlugin();

    virtual void setWidget(pdf::PDFWidget* widget) override;
    virtual void setDocument(const pdf::PDFModifiedDocument& document) override;
    virtual std::vector<QAction*> getActions() const override;
    virtual QString getPluginMenuName() const override;

private:
    void ensureDockWidget();
    void updateActions();
    bool resolvePdfToolPath(QString* pdfToolPath) const;
    void startOcrRun();
    void finishOcrRun();
    void cancelOcrRun(bool silent = false);
    bool applyReportJson(const QJsonObject& report, QString* errorMessage = nullptr);

    void onRunOcrTriggered();
    void onShowPanelTriggered(bool checked);
    void onOcrProcessFinished(int exitCode, int exitStatus);
    void onOcrProcessErrorOccurred();
    void onOcrStdoutReady();
    void onOcrStderrReady();
    void onPageActivated(int pageIndex);

    QAction* m_actionRunOcr = nullptr;
    QAction* m_actionShowPanel = nullptr;
    OcrReportDockWidget* m_reportDockWidget = nullptr;
    QProcess* m_ocrProcess = nullptr;
    std::unique_ptr<QTemporaryDir> m_ocrTemporaryDirectory;
    QByteArray m_ocrStdoutBuffer;
    QByteArray m_ocrStderrBuffer;
    quint64 m_documentRevision = 0;
};

}   // namespace pdfplugin

#endif // OCRPLUGIN_H
