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

#ifndef FRISKETPREFLIGHTPLUGIN_H
#define FRISKETPREFLIGHTPLUGIN_H

#include "pdfplugin.h"

#include <memory>

class QJsonObject;
class QProcess;
class QTemporaryDir;

namespace pdfplugin
{

class PreflightReportDockWidget;

class FrisketPreflightPlugin : public pdf::PDFPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "PDF4QT.FrisketPreflightPlugin" FILE "FrisketPreflightPlugin.json")

private:
    using BaseClass = pdf::PDFPlugin;

public:
    FrisketPreflightPlugin();
    virtual ~FrisketPreflightPlugin();

    virtual void setWidget(pdf::PDFWidget* widget) override;
    virtual void setDocument(const pdf::PDFModifiedDocument& document) override;
    virtual std::vector<QAction*> getActions() const override;
    virtual QString getPluginMenuName() const override;

private:
    void ensureDockWidget();
    void updateActions();
    bool applyReportJson(const QJsonObject& report, QString* errorMessage = nullptr);
    void finishPreflightRun();

    void onRunPreflightTriggered();
    void onShowPanelTriggered(bool checked);
    void onLoadExampleReportTriggered();
    void onPreflightProcessFinished(int exitCode, int exitStatus);
    void onPreflightProcessErrorOccurred();

    QAction* m_actionRunPreflight = nullptr;
    QAction* m_actionShowPanel = nullptr;
    QAction* m_actionLoadExample = nullptr;
    PreflightReportDockWidget* m_reportDockWidget = nullptr;
    QProcess* m_preflightProcess = nullptr;
    std::unique_ptr<QTemporaryDir> m_preflightTemporaryDirectory;
};

}   // namespace pdfplugin

#endif // FRISKETPREFLIGHTPLUGIN_H
