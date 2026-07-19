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
#include "preflightsidecarutils.h"

#include "pdfdocumentwriter.h"
#include "pdfdrawwidget.h"

#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QTemporaryDir>

#ifndef FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH
#define FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH "../share/frisket/profiles"
#endif

namespace pdfplugin
{

FrisketPreflightPlugin::FrisketPreflightPlugin() :
    pdf::PDFPlugin(nullptr)
{

}

FrisketPreflightPlugin::~FrisketPreflightPlugin()
{
    cancelPreflightRun(true);
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

    const bool documentChanged = documentModificationInvalidatesReport(document);
    if (documentChanged)
    {
        ++m_documentRevision;
    }

    if (document.hasReset())
    {
        cancelPreflightRun(true);
        invalidateReport();
    }
    else if (documentChanged)
    {
        if (m_preflightProcess)
        {
            cancelPreflightRun(true);
        }
        invalidateReportIfStale();
    }

    updateActions();
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

    if (m_actionRunPreflight)
    {
        m_actionRunPreflight->setEnabled(hasDocument && !m_preflightProcess);
    }
}

bool FrisketPreflightPlugin::applyReportJson(const QJsonObject& report, QString* errorMessage)
{
    const QJsonObject filteredReport = preflight::filterAdvertisedFixups(report);
    if (!preflight::validateNormalizedReport(filteredReport, errorMessage))
    {
        return false;
    }

    ensureDockWidget();
    m_reportDockWidget->setReport(filteredReport);
    m_reportDocumentRevision = m_documentRevision;
    m_actionShowPanel->setChecked(true);
    m_reportDockWidget->show();
    return true;
}

void FrisketPreflightPlugin::onRunPreflightTriggered()
{
    if (!m_document || m_preflightProcess)
    {
        return;
    }

    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QString pdfToolPath = preflight::resolveBundlePath(applicationDirectory, preflight::getPdfToolFileName());
    const QString profilePath = preflight::resolveBundlePath(applicationDirectory, QStringLiteral(FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH "/frisket-default.json"));

    if (!QFileInfo(pdfToolPath).isExecutable())
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"),
                              tr("Could not find the PdfTool preflight sidecar at %1.").arg(QDir::toNativeSeparators(pdfToolPath)));
        return;
    }

    if (!QFileInfo(profilePath).isFile())
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"),
                              tr("Could not find the bundled Frisket Default profile at %1.").arg(QDir::toNativeSeparators(profilePath)));
        return;
    }

    auto temporaryDirectory = std::make_unique<QTemporaryDir>();
    if (!temporaryDirectory->isValid())
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"), tr("Could not create a temporary directory for the preflight snapshot."));
        return;
    }

    const QString snapshotPath = temporaryDirectory->filePath(QStringLiteral("preflight-snapshot.pdf"));
    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult = writer.write(snapshotPath, m_document, false);
    if (!writeResult)
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"),
                              tr("Could not create a preflight snapshot: %1").arg(writeResult.getErrorMessage()));
        return;
    }

    m_preflightTemporaryDirectory = std::move(temporaryDirectory);
    m_preflightStdoutBuffer.clear();
    m_preflightStderrBuffer.clear();
    m_preflightRunRevision = m_documentRevision;
    m_preflightProcess = new QProcess(this);
    m_preflightProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_preflightProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) { onPreflightProcessFinished(exitCode, static_cast<int>(exitStatus)); });
    connect(m_preflightProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart)
        {
            onPreflightProcessErrorOccurred();
        }
    });
    connect(m_preflightProcess, &QProcess::readyReadStandardOutput, this, &FrisketPreflightPlugin::onPreflightStdoutReady);
    connect(m_preflightProcess, &QProcess::readyReadStandardError, this, &FrisketPreflightPlugin::onPreflightStderrReady);

    updateActions();
    m_preflightProcess->start(pdfToolPath, { QStringLiteral("preflight"), snapshotPath, QStringLiteral("--profile"), profilePath });
}

void FrisketPreflightPlugin::onPreflightProcessFinished(int exitCode, int exitStatus)
{
    if (!m_preflightProcess)
    {
        return;
    }

    onPreflightStdoutReady();
    onPreflightStderrReady();

    const quint64 runRevision = m_preflightRunRevision;
    const QByteArray standardOutput = m_preflightStdoutBuffer;
    const QString standardError = QString::fromLocal8Bit(m_preflightStderrBuffer).trimmed();
    finishPreflightRun();

    if (runRevision != m_documentRevision)
    {
        return;
    }

    if (exitStatus != QProcess::NormalExit || !preflight::isExpectedPreflightExitCode(exitCode))
    {
        QString detail = standardError;
        if (detail.isEmpty())
        {
            detail = tr("PdfTool exited with code %1.").arg(exitCode);
        }

        QMessageBox::critical(m_widget, tr("Frisket Preflight"), tr("Preflight did not complete successfully: %1").arg(detail));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument reportDocument = QJsonDocument::fromJson(standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !reportDocument.isObject())
    {
        QString detail = parseError.error != QJsonParseError::NoError ? parseError.errorString() : tr("The output is not a JSON object.");
        QMessageBox::critical(m_widget, tr("Frisket Preflight"), tr("PdfTool returned an invalid preflight report: %1").arg(detail));
        return;
    }

    const QJsonObject report = reportDocument.object();
    QString validationError;
    if (!applyReportJson(report, &validationError))
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"), tr("PdfTool returned an invalid preflight report: %1").arg(validationError));
    }
}

void FrisketPreflightPlugin::onPreflightProcessErrorOccurred()
{
    if (!m_preflightProcess)
    {
        return;
    }

    const QString error = m_preflightProcess->errorString();
    finishPreflightRun();
    QMessageBox::critical(m_widget, tr("Frisket Preflight"), tr("Could not start PdfTool: %1").arg(error));
}

void FrisketPreflightPlugin::onPreflightStdoutReady()
{
    if (!m_preflightProcess)
    {
        return;
    }

    m_preflightStdoutBuffer.append(m_preflightProcess->readAllStandardOutput());
    if (m_preflightStdoutBuffer.size() > preflight::PREFLIGHT_SIDECAR_STDOUT_MAX_BYTES)
    {
        abortPreflightRun(tr("Preflight output exceeded the maximum allowed size."));
    }
}

void FrisketPreflightPlugin::onPreflightStderrReady()
{
    if (!m_preflightProcess)
    {
        return;
    }

    m_preflightStderrBuffer.append(m_preflightProcess->readAllStandardError());
    if (m_preflightStderrBuffer.size() > preflight::PREFLIGHT_SIDECAR_STDERR_MAX_BYTES)
    {
        abortPreflightRun(tr("Preflight diagnostic output exceeded the maximum allowed size."));
    }
}

void FrisketPreflightPlugin::finishPreflightRun()
{
    cancelPreflightRun(true);
    updateActions();
}

void FrisketPreflightPlugin::cancelPreflightRun(bool silent)
{
    if (m_preflightProcess)
    {
        disconnect(m_preflightProcess, nullptr, this, nullptr);
        if (m_preflightProcess->state() != QProcess::NotRunning)
        {
            m_preflightProcess->kill();
            m_preflightProcess->waitForFinished(3000);
        }
        m_preflightProcess->deleteLater();
        m_preflightProcess = nullptr;
    }

    m_preflightTemporaryDirectory.reset();
    m_preflightStdoutBuffer.clear();
    m_preflightStderrBuffer.clear();
    m_preflightRunRevision = 0;

    if (!silent)
    {
        updateActions();
    }
}

void FrisketPreflightPlugin::abortPreflightRun(const QString& message)
{
    finishPreflightRun();
    QMessageBox::critical(m_widget, tr("Frisket Preflight"), message);
}

void FrisketPreflightPlugin::invalidateReport()
{
    if (m_reportDockWidget)
    {
        m_reportDockWidget->clearReport();
    }

    m_reportDocumentRevision = 0;
}

void FrisketPreflightPlugin::invalidateReportIfStale()
{
    if (m_reportDocumentRevision != 0 && m_reportDocumentRevision != m_documentRevision)
    {
        invalidateReport();
    }
}

bool FrisketPreflightPlugin::documentModificationInvalidatesReport(const pdf::PDFModifiedDocument& document) const
{
    return document.hasReset()
        || document.hasPageContentsChanged()
        || document.hasFlag(pdf::PDFModifiedDocument::Annotation)
        || document.hasFlag(pdf::PDFModifiedDocument::FormField);
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
    QFile file(QStringLiteral(":/pdfplugins/frisketpreflight/report.example.json"));
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

    QString validationError;
    if (!applyReportJson(document.object(), &validationError))
    {
        QMessageBox::warning(m_widget, tr("Frisket Preflight"),
                             tr("Example report does not match the Frisket report contract: %1").arg(validationError));
    }
}

}   // namespace pdfplugin
