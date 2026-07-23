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

#include "pdfbleedfixup.h"
#include "pdfdocumentwriter.h"
#include "pdfdrawspacecontroller.h"
#include "pdfdrawwidget.h"
#include "pdfuitheme.h"
#include "pdfwidgetutils.h"
#include "pdfwidgetutils.h"

#include <QAction>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVBoxLayout>

#ifndef FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH
#define FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH "../share/frisket/profiles"
#endif

namespace pdfplugin
{

namespace
{

constexpr qreal POINTS_PER_MM = 72.0 / 25.4;

pdf::PDFBleedFixupMode bleedFixupModeFromString(const QString& text)
{
    if (text == QStringLiteral("pixel-repeat"))
    {
        return pdf::PDFBleedFixupMode::PixelRepeat;
    }
    if (text == QStringLiteral("stretch"))
    {
        return pdf::PDFBleedFixupMode::Stretch;
    }
    return pdf::PDFBleedFixupMode::Mirror;
}

QString defaultBleedOutputPath(const QString& sourcePath)
{
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.isFile())
    {
        return QString();
    }

    return sourceInfo.absolutePath() + QDir::separator()
        + sourceInfo.completeBaseName() + QStringLiteral("_bleed.") + sourceInfo.suffix();
}

}   // namespace

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

    m_widget->getDrawWidgetProxy()->registerDrawInterface(this);

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

    connect(m_reportDockWidget, &PreflightReportDockWidget::findingSelectionChanged,
            this, &FrisketPreflightPlugin::onFindingSelectionChanged);

    connect(m_reportDockWidget, &PreflightReportDockWidget::applyBleedFixupRequested,
            this, &FrisketPreflightPlugin::onApplyBleedFixupRequested);
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

bool FrisketPreflightPlugin::applyReportJson(const QJsonObject& report, QString* errorMessage, const QString& sourceLabel)
{
    const QJsonObject filteredReport = preflight::filterAdvertisedFixups(report);
    if (!preflight::validateNormalizedReport(filteredReport, errorMessage))
    {
        return false;
    }

    ensureDockWidget();
    m_reportDockWidget->setReport(filteredReport, sourceLabel);
    m_reportDocumentRevision = m_documentRevision;
    m_actionShowPanel->setChecked(true);
    m_reportDockWidget->show();
    return true;
}

bool FrisketPreflightPlugin::resolvePreflightPaths(QString* pdfToolPath, QString* profilePath) const
{
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    *pdfToolPath = preflight::resolveBundlePath(applicationDirectory, preflight::getPdfToolFileName());
    *profilePath = preflight::resolveBundlePath(applicationDirectory, QStringLiteral(FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH "/frisket-default.json"));

    if (!QFileInfo(*pdfToolPath).isExecutable())
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"),
                              tr("Could not find the PdfTool preflight sidecar at %1.").arg(QDir::toNativeSeparators(*pdfToolPath)));
        return false;
    }

    if (!QFileInfo(*profilePath).isFile())
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"),
                              tr("Could not find the bundled Frisket Default profile at %1.").arg(QDir::toNativeSeparators(*profilePath)));
        return false;
    }

    return true;
}

void FrisketPreflightPlugin::startPreflightOnFile(const QString& filePath,
                                                  const QString& profilePath,
                                                  quint64 revisionToMatch,
                                                  bool ignoreRevisionMatch,
                                                  const QString& reportSourceLabel)
{
    if (m_preflightProcess)
    {
        return;
    }

    m_preflightTemporaryDirectory = std::make_unique<QTemporaryDir>();
    if (!m_preflightTemporaryDirectory->isValid())
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"), tr("Could not create a temporary directory for preflight."));
        return;
    }

    const QString stagedPath = m_preflightTemporaryDirectory->filePath(QStringLiteral("preflight-input.pdf"));
    if (!QFile::copy(filePath, stagedPath))
    {
        QMessageBox::critical(m_widget, tr("Frisket Preflight"),
                              tr("Could not stage the preflight input file at %1.").arg(QDir::toNativeSeparators(filePath)));
        m_preflightTemporaryDirectory.reset();
        return;
    }

    QString pdfToolPath;
    QString bundledProfilePath;
    if (!resolvePreflightPaths(&pdfToolPath, &bundledProfilePath))
    {
        m_preflightTemporaryDirectory.reset();
        return;
    }

    Q_UNUSED(bundledProfilePath);

    m_preflightStdoutBuffer.clear();
    m_preflightStderrBuffer.clear();
    m_preflightRunRevision = revisionToMatch;
    m_preflightIgnoreRevision = ignoreRevisionMatch;
    m_preflightReportSourceLabel = reportSourceLabel;
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
    m_preflightProcess->start(pdfToolPath, { QStringLiteral("preflight"), stagedPath, QStringLiteral("--profile"), profilePath });
}

void FrisketPreflightPlugin::onRunPreflightTriggered()
{
    if (!m_document || m_preflightProcess)
    {
        return;
    }

    QString pdfToolPath;
    QString profilePath;
    if (!resolvePreflightPaths(&pdfToolPath, &profilePath))
    {
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
    m_preflightIgnoreRevision = false;
    m_preflightReportSourceLabel.clear();
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
    const bool ignoreRevision = m_preflightIgnoreRevision;
    const QString reportSourceLabel = m_preflightReportSourceLabel;
    const QByteArray standardOutput = m_preflightStdoutBuffer.takeData();
    const QString standardError = QString::fromLocal8Bit(m_preflightStderrBuffer.takeData()).trimmed();
    finishPreflightRun();

    if (!ignoreRevision && runRevision != m_documentRevision)
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
    if (!applyReportJson(report, &validationError, reportSourceLabel))
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

    if (m_preflightStdoutBuffer.append(m_preflightProcess->readAllStandardOutput()) == preflight::PreflightSidecarStreamBuffer::AppendResult::Overflow)
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

    if (m_preflightStderrBuffer.append(m_preflightProcess->readAllStandardError()) == preflight::PreflightSidecarStreamBuffer::AppendResult::Overflow)
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
    m_preflightIgnoreRevision = false;
    m_preflightReportSourceLabel.clear();

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
    m_selectedFindingIndex = -1;

    if (m_reportDockWidget)
    {
        m_reportDockWidget->clearReport();
    }

    m_reportDocumentRevision = 0;
    updateOverlayGraphics();
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

void FrisketPreflightPlugin::drawPage(QPainter* painter,
                                      pdf::PDFInteger pageIndex,
                                      const pdf::PDFPrecompiledPage* compiledPage,
                                      pdf::PDFTextLayoutGetter& layoutGetter,
                                      const QTransform& pagePointToDevicePointMatrix,
                                      const pdf::PDFColorConvertor& convertor,
                                      QList<pdf::PDFRenderError>& errors) const
{
    Q_UNUSED(compiledPage);
    Q_UNUSED(layoutGetter);
    Q_UNUSED(convertor);
    Q_UNUSED(errors);

    if (!m_reportDockWidget || !m_reportDockWidget->hasReport())
    {
        return;
    }

    const QVector<PreflightFindingEntry>& findings = m_reportDockWidget->findings();
    if (findings.isEmpty())
    {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal lineWidth = pdf::PDFWidgetUtils::scaleDPI_x(painter->device(), 1.0);
    const qreal selectedLineWidth = pdf::PDFWidgetUtils::scaleDPI_x(painter->device(), 2.5);

    for (int findingIndex = 0; findingIndex < findings.size(); ++findingIndex)
    {
        const PreflightFindingEntry& finding = findings.at(findingIndex);
        if (!finding.hasVisualOverlay || finding.page <= 0 || finding.page - 1 != pageIndex)
        {
            continue;
        }

        const QRectF deviceRect = pagePointToDevicePointMatrix.mapRect(finding.bbox).normalized();
        if (deviceRect.isEmpty())
        {
            continue;
        }

        const bool isSelected = findingIndex == m_selectedFindingIndex;
        QColor borderColor;
        QColor fillColor;

        if (finding.severity == QStringLiteral("error"))
        {
            borderColor = pdf::PDFUITheme::severityErrorColor();
            fillColor = QColor(borderColor.red(), borderColor.green(), borderColor.blue(), isSelected ? 90 : 55);
        }
        else if (finding.severity == QStringLiteral("warning"))
        {
            borderColor = pdf::PDFUITheme::severityWarningColor();
            fillColor = QColor(borderColor.red(), borderColor.green(), borderColor.blue(), isSelected ? 85 : 50);
        }
        else
        {
            borderColor = pdf::PDFUITheme::severityInfoColor();
            fillColor = QColor(borderColor.red(), borderColor.green(), borderColor.blue(), isSelected ? 80 : 45);
        }

        painter->setPen(QPen(borderColor, isSelected ? selectedLineWidth : lineWidth));
        painter->setBrush(fillColor);
        painter->drawRect(deviceRect);

        if (finding.severity == QStringLiteral("error"))
        {
            QPen hatchPen(borderColor, lineWidth * 0.75, Qt::SolidLine);
            painter->setPen(hatchPen);
            painter->setBrush(Qt::NoBrush);

            const qreal spacing = pdf::PDFWidgetUtils::scaleDPI_x(painter->device(), 6.0);
            for (qreal offset = deviceRect.left() - deviceRect.height(); offset < deviceRect.right(); offset += spacing)
            {
                painter->drawLine(QPointF(offset, deviceRect.bottom()),
                                    QPointF(offset + deviceRect.height(), deviceRect.top()));
            }
        }
    }
}

void FrisketPreflightPlugin::updateOverlayGraphics()
{
    if (m_widget)
    {
        m_widget->getDrawWidget()->getWidget()->update();
    }
}

void FrisketPreflightPlugin::onFindingSelectionChanged(int row)
{
    m_selectedFindingIndex = row;

    if (!m_reportDockWidget || !m_widget || row < 0)
    {
        updateOverlayGraphics();
        return;
    }

    const QVector<PreflightFindingEntry>& findings = m_reportDockWidget->findings();
    if (row >= findings.size())
    {
        updateOverlayGraphics();
        return;
    }

    const PreflightFindingEntry& finding = findings.at(row);
    if ((finding.scope == QStringLiteral("page") || finding.scope == QStringLiteral("object"))
        && finding.page > 0)
    {
        m_widget->getDrawWidgetProxy()->goToPage(finding.page - 1);
    }

    updateOverlayGraphics();
}

void FrisketPreflightPlugin::onApplyBleedFixupRequested()
{
    if (!m_document || !m_reportDockWidget)
    {
        return;
    }

    const PreflightFixupEntry* addBleedFixup = m_reportDockWidget->addBleedFixup();
    if (!addBleedFixup)
    {
        return;
    }

    const QString originalPath = m_dataExchangeInterface->getOriginalFileName();
    const double defaultAmountPt = addBleedFixup->amountPt > 0.0 ? addBleedFixup->amountPt : 9.0;
    const qreal defaultBleedMm = defaultAmountPt / POINTS_PER_MM;
    const QString defaultMode = addBleedFixup->params.value(QStringLiteral("mode")).toString(QStringLiteral("mirror"));

    QDialog dialog(m_widget);
    dialog.setWindowTitle(tr("Apply Bleed Fix"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QFormLayout* form = new QFormLayout();

    QComboBox* modeCombo = new QComboBox(&dialog);
    modeCombo->addItem(tr("Mirror"), int(pdf::PDFBleedFixupMode::Mirror));
    modeCombo->addItem(tr("Pixel repeat"), int(pdf::PDFBleedFixupMode::PixelRepeat));
    modeCombo->addItem(tr("Stretch"), int(pdf::PDFBleedFixupMode::Stretch));
    const int defaultModeIndex = modeCombo->findData(int(bleedFixupModeFromString(defaultMode)));
    if (defaultModeIndex >= 0)
    {
        modeCombo->setCurrentIndex(defaultModeIndex);
    }
    form->addRow(tr("Mode"), modeCombo);

    QDoubleSpinBox* bleedSpin = new QDoubleSpinBox(&dialog);
    bleedSpin->setRange(0.1, 50.0);
    bleedSpin->setDecimals(2);
    bleedSpin->setSuffix(tr(" mm"));
    bleedSpin->setValue(defaultBleedMm);
    form->addRow(tr("Bleed amount"), bleedSpin);

    QLineEdit* outputPathEdit = new QLineEdit(defaultBleedOutputPath(originalPath), &dialog);
    QPushButton* browseButton = new QPushButton(tr("Browse..."), &dialog);
    QHBoxLayout* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(outputPathEdit, 1);
    outputLayout->addWidget(browseButton);
    form->addRow(tr("Output file"), outputLayout);

    QCheckBox* rerunCheckBox = new QCheckBox(tr("Re-run preflight after fixing"), &dialog);
    rerunCheckBox->setChecked(true);
    layout->addLayout(form);
    layout->addWidget(rerunCheckBox);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(browseButton, &QPushButton::clicked, &dialog, [&dialog, outputPathEdit, originalPath]()
    {
        const QString selectedPath = QFileDialog::getSaveFileName(&dialog,
                                                                  tr("Save bleed-fixed PDF"),
                                                                  outputPathEdit->text().isEmpty() ? defaultBleedOutputPath(originalPath) : outputPathEdit->text(),
                                                                  tr("PDF files (*.pdf)"));
        if (!selectedPath.isEmpty())
        {
            outputPathEdit->setText(selectedPath);
        }
    });

    pdf::PDFWidgetUtils::style(&dialog);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString outputPath = outputPathEdit->text().trimmed();
    if (outputPath.isEmpty())
    {
        QMessageBox::warning(m_widget, tr("Apply Bleed Fix"), tr("Choose an output file path."));
        return;
    }

    pdf::PDFDocument fixedDocument = *m_document;
    pdf::PDFBleedFixupSettings settings;
    settings.mode = pdf::PDFBleedFixupMode(modeCombo->currentData().toInt());
    const qreal bleedMm = bleedSpin->value();
    settings.bleedMM = QMarginsF(bleedMm, bleedMm, bleedMm, bleedMm);
    settings.force = true;

    const pdf::PDFOperationResult fixupResult = pdf::PDFBleedFixup::apply(&fixedDocument, settings);
    if (!fixupResult)
    {
        QMessageBox::critical(m_widget, tr("Apply Bleed Fix"), fixupResult.getErrorMessage());
        return;
    }

    if (QFile::exists(outputPath))
    {
        QFile::remove(outputPath);
    }

    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult = writer.write(outputPath, &fixedDocument, true);
    if (!writeResult)
    {
        QMessageBox::critical(m_widget, tr("Apply Bleed Fix"), writeResult.getErrorMessage());
        return;
    }

    QMessageBox::information(m_widget, tr("Apply Bleed Fix"),
                             tr("Bleed fixup saved to %1. The open document was not modified.")
                                 .arg(QDir::toNativeSeparators(outputPath)));

    if (!rerunCheckBox->isChecked() || m_preflightProcess)
    {
        return;
    }

    QString pdfToolPath;
    QString profilePath;
    if (!resolvePreflightPaths(&pdfToolPath, &profilePath))
    {
        return;
    }

    startPreflightOnFile(outputPath,
                         profilePath,
                         m_documentRevision,
                         true,
                         tr("Post-fix results for: %1").arg(QDir::toNativeSeparators(outputPath)));
}

}   // namespace pdfplugin
