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

#include "preflightreportdockwidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace pdfplugin
{

PreflightReportDockWidget::PreflightReportDockWidget(QWidget* parent) :
    QDockWidget(tr("Frisket Preflight Report"), parent)
{
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);

    m_headerLabel = new QLabel(tr("No preflight report loaded."), container);
    m_headerLabel->setWordWrap(true);
    layout->addWidget(m_headerLabel);

    m_summaryLabel = new QLabel(container);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_contentStack = new QStackedWidget(container);

    m_emptyLabel = new QLabel(tr("Run preflight or load an example report to see findings here."), container);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_contentStack->addWidget(m_emptyLabel);

    QWidget* reportPage = new QWidget(container);
    QVBoxLayout* reportLayout = new QVBoxLayout(reportPage);
    reportLayout->setContentsMargins(0, 0, 0, 0);

    m_findingsView = new QTableView(reportPage);
    m_findingsView->setModel(&m_model);
    m_findingsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_findingsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_findingsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_findingsView->horizontalHeader()->setStretchLastSection(true);
    m_findingsView->verticalHeader()->setVisible(false);
    reportLayout->addWidget(m_findingsView, 1);

    connect(m_findingsView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex& previous)
    {
        Q_UNUSED(previous);
        Q_EMIT findingSelectionChanged(current.isValid() ? current.row() : -1);
    });

    m_fixupsList = new QListWidget(reportPage);
    m_fixupsList->setMaximumHeight(120);
    reportLayout->addWidget(m_fixupsList);

    m_applyFixupButton = new QPushButton(tr("Apply Bleed Fix..."), reportPage);
    m_applyFixupButton->setEnabled(false);
    connect(m_applyFixupButton, &QPushButton::clicked, this, &PreflightReportDockWidget::applyBleedFixupRequested);
    reportLayout->addWidget(m_applyFixupButton);

    m_contentStack->addWidget(reportPage);
    layout->addWidget(m_contentStack, 1);

    setWidget(container);
    refreshEmptyState();
}

void PreflightReportDockWidget::setReport(const QJsonObject& report, const QString& sourceLabel)
{
    clearFindingSelection();
    m_reportSourceLabel = sourceLabel;
    m_model.setReport(report);
    refreshHeader();
    refreshFixups();
    refreshApplyFixupButton();
    m_contentStack->setCurrentIndex(1);
}

void PreflightReportDockWidget::clearReport()
{
    clearFindingSelection();
    m_reportSourceLabel.clear();
    m_model.clear();
    refreshHeader();
    refreshFixups();
    refreshApplyFixupButton();
    refreshEmptyState();
}

void PreflightReportDockWidget::clearFindingSelection()
{
    if (m_findingsView && m_findingsView->selectionModel())
    {
        m_findingsView->selectionModel()->clearSelection();
    }
    Q_EMIT findingSelectionChanged(-1);
}

void PreflightReportDockWidget::refreshHeader()
{
    if (!m_model.hasReport())
    {
        m_headerLabel->setText(tr("No preflight report loaded."));
        m_summaryLabel->clear();
        return;
    }

    const QString statusText = m_model.pass() ? tr("Pass") : tr("Fail");
    if (m_reportSourceLabel.isEmpty())
    {
        m_headerLabel->setText(tr("%1 — profile: %2").arg(statusText, m_model.profileName()));
    }
    else
    {
        m_headerLabel->setText(tr("%1 — profile: %2 — %3").arg(statusText, m_model.profileName(), m_reportSourceLabel));
    }
    m_summaryLabel->setText(tr("%1 error(s), %2 warning(s).")
                                .arg(m_model.errorCount())
                                .arg(m_model.warningCount()));
}

void PreflightReportDockWidget::refreshFixups()
{
    m_fixupsList->clear();

    if (!m_model.hasReport())
    {
        return;
    }

    for (const PreflightFixupEntry& fixup : m_model.fixups())
    {
        const QString label = fixup.safe
                                  ? tr("%1 — %2 (safe)").arg(fixup.id, fixup.description)
                                  : tr("%1 — %2").arg(fixup.id, fixup.description);
        m_fixupsList->addItem(label);
    }

    if (m_fixupsList->count() == 0)
    {
        m_fixupsList->addItem(tr("No fixups available."));
    }
}

void PreflightReportDockWidget::refreshApplyFixupButton()
{
    if (!m_applyFixupButton)
    {
        return;
    }

    m_applyFixupButton->setEnabled(m_model.hasReport() && m_model.hasAddBleedFixup());
}

void PreflightReportDockWidget::refreshEmptyState()
{
    m_contentStack->setCurrentIndex(0);
}

}   // namespace pdfplugin
