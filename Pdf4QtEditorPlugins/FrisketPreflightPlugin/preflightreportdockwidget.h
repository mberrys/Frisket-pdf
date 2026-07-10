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

#ifndef PREFLIGHTREPORTDOCKWIDGET_H
#define PREFLIGHTREPORTDOCKWIDGET_H

#include "preflightreportmodel.h"

#include <QDockWidget>
#include <QJsonObject>

class QLabel;
class QListWidget;
class QStackedWidget;
class QTableView;

namespace pdfplugin
{

class PreflightReportDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit PreflightReportDockWidget(QWidget* parent = nullptr);

    void setReport(const QJsonObject& report);
    void clearReport();

private:
    void refreshHeader();
    void refreshFixups();
    void refreshEmptyState();

    PreflightReportModel m_model;
    QStackedWidget* m_contentStack = nullptr;
    QLabel* m_headerLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableView* m_findingsView = nullptr;
    QListWidget* m_fixupsList = nullptr;
    QLabel* m_emptyLabel = nullptr;
};

}   // namespace pdfplugin

#endif // PREFLIGHTREPORTDOCKWIDGET_H
