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

#include "frisketpreflightplugin.h"
#include "preflightreportdockwidget.h"

#include <QAction>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMessageBox>

namespace pdfplugin
{

FrisketPreflightPlugin::FrisketPreflightPlugin() :
    pdf::PDFPlugin(nullptr)
{

}

void FrisketPreflightPlugin::setWidget(pdf::PDFWidget* widget)
{
    Q_ASSERT(!m_widget);

    BaseClass::setWidget(widget);

    m_actionRunPreflight = new QAction(QIcon(":/pdfplugins/frisketpreflight/preflight.svg"), tr("&Run Preflight"), this);
    m_actionRunPreflight->setObjectName("frisketpreflight_Run");
    m_actionRunPreflight->setEnabled(false);
    m_actionRunPreflight->setToolTip(tr("Runs PdfTool preflight via QProcess (MIC-136)."));
    connect(m_actionRunPreflight, &QAction::triggered, this, &FrisketPreflightPlugin::onRunPreflightTriggered);

    m_actionShowPanel = new QAction(QIcon(":/pdfplugins/frisketpreflight/preflight.svg"), tr("Show &Report Panel"), this);
    m_actionShowPanel->setObjectName("frisketpreflight_ShowPanel");
    m_actionShowPanel->setCheckable(true);
    connect(m_actionShowPanel, &QAction::toggled, this, &FrisketPreflightPlugin::onShowPanelTriggered);

    m_actionLoadExample = new QAction(tr("Load &Example Report"), this);
    m_actionLoadExample->setObjectName("frisketpreflight_LoadExample");
    connect(m_actionLoadExample, &QAction::triggered, this, &FrisketPreflightPlugin::onLoadExampleReportTriggered);

    updateActions();
}

void FrisketPreflightPlugin::setDocument(const pdf::PDFModifiedDocument& document)
{
    BaseClass::setDocument(document);

    if (document.hasReset())
    {
        if (m_reportDockWidget)
        {
            m_reportDockWidget->clearReport();
        }

        updateActions();
    }
}

std::vector<QAction*> FrisketPreflightPlugin::getActions() const
{
    return { m_actionRunPreflight, m_actionShowPanel, m_actionLoadExample };
}

QString FrisketPreflightPlugin::getPluginMenuName() const
{
    return tr("Frisket &Preflight");
}

void FrisketPreflightPlugin::ensureDockWidget()
{
    if (m_reportDockWidget)
    {
        return;
    }

    QMainWindow* mainWindow = m_dataExchangeInterface->getMainWindow();
    m_reportDockWidget = new PreflightReportDockWidget(mainWindow);
    m_reportDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_reportDockWidget, Qt::Vertical);
    m_reportDockWidget->setFloating(false);

    connect(m_reportDockWidget, &QDockWidget::visibilityChanged, this, [this](bool visible)
    {
        if (m_actionShowPanel && m_actionShowPanel->isChecked() != visible)
        {
            m_actionShowPanel->setChecked(visible);
        }
    });
}

void FrisketPreflightPlugin::updateActions()
{
    const bool hasDocument = m_widget && m_document;

    if (m_actionShowPanel)
    {
        m_actionShowPanel->setEnabled(hasDocument);
    }

    if (m_actionLoadExample)
    {
        m_actionLoadExample->setEnabled(hasDocument);
    }
}

void FrisketPreflightPlugin::applyReportJson(const QJsonObject& report)
{
    ensureDockWidget();
    m_reportDockWidget->setReport(report);
    m_actionShowPanel->setChecked(true);
    m_reportDockWidget->show();
}

void FrisketPreflightPlugin::onRunPreflightTriggered()
{
    // MIC-136: QProcess -> PdfTool preflight <pdf> --profile <profile.json>
}

void FrisketPreflightPlugin::onShowPanelTriggered(bool checked)
{
    if (!m_document)
    {
        return;
    }

    ensureDockWidget();

    if (checked)
    {
        m_reportDockWidget->show();
    }
    else
    {
        m_reportDockWidget->hide();
    }
}

void FrisketPreflightPlugin::onLoadExampleReportTriggered()
{
    QFile file(QStringLiteral(FRISKET_PREFLIGHT_EXAMPLE_REPORT));
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(m_widget, tr("Frisket Preflight"),
                             tr("Could not open the example report at %1.").arg(file.fileName()));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        QMessageBox::warning(m_widget, tr("Frisket Preflight"),
                             tr("Example report is not valid JSON: %1").arg(parseError.errorString()));
        return;
    }

    applyReportJson(document.object());
}

}   // namespace pdfplugin
