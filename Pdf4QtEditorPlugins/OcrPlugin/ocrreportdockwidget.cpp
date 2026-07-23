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

#include "ocrreportdockwidget.h"

#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace pdfplugin
{

OcrReportDockWidget::OcrReportDockWidget(QWidget* parent) :
    QDockWidget(parent)
{
    setObjectName(QStringLiteral("OcrReportDockWidget"));
    setWindowTitle(tr("OCR Report"));

    QWidget* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);

    m_headerLabel = new QLabel(container);
    m_summaryLabel = new QLabel(container);
    m_table = new QTableWidget(container);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({ tr("Page"), tr("Status"), tr("Characters") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_headerLabel);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_table);
    setWidget(container);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int)
    {
        QTableWidgetItem* pageItem = m_table->item(row, 0);
        if (!pageItem)
        {
            return;
        }
        Q_EMIT pageActivated(pageItem->data(Qt::UserRole).toInt());
    });

    clearReport();
}

void OcrReportDockWidget::setReport(const QJsonObject& report, const QString& sourceLabel)
{
    m_hasReport = true;
    m_reportSourceLabel = sourceLabel;
    m_table->setRowCount(0);

    const QJsonArray pages = report.value(QStringLiteral("pages")).toArray();
    for (const QJsonValue& pageValue : pages)
    {
        const QJsonObject pageObject = pageValue.toObject();
        const int page = pageObject.value(QStringLiteral("page")).toInt();
        const QString status = pageObject.value(QStringLiteral("status")).toString();
        const QString text = pageObject.value(QStringLiteral("text")).toString();

        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* pageItem = new QTableWidgetItem(QString::number(page));
        pageItem->setData(Qt::UserRole, page - 1);
        m_table->setItem(row, 0, pageItem);
        m_table->setItem(row, 1, new QTableWidgetItem(status));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(text.size())));
    }

    refreshHeader();
}

void OcrReportDockWidget::clearReport()
{
    m_hasReport = false;
    m_reportSourceLabel.clear();
    m_table->setRowCount(0);
    refreshHeader();
}

void OcrReportDockWidget::refreshHeader()
{
    if (!m_hasReport)
    {
        m_headerLabel->setText(tr("No OCR report loaded."));
        m_summaryLabel->clear();
        return;
    }

    m_headerLabel->setText(m_reportSourceLabel.isEmpty() ? tr("OCR report") : m_reportSourceLabel);
    m_summaryLabel->setText(tr("%1 page(s) in report.").arg(m_table->rowCount()));
}

}   // namespace pdfplugin
