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

#ifndef OCRREPORTDOCKWIDGET_H
#define OCRREPORTDOCKWIDGET_H

#include <QDockWidget>
#include <QJsonObject>

class QLabel;
class QTableWidget;

namespace pdfplugin
{

class OcrReportDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit OcrReportDockWidget(QWidget* parent = nullptr);

    void setReport(const QJsonObject& report, const QString& sourceLabel = QString());
    void clearReport();
    bool hasReport() const { return m_hasReport; }

Q_SIGNALS:
    void pageActivated(int pageIndex);

private:
    void refreshHeader();

    bool m_hasReport = false;
    QLabel* m_headerLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QString m_reportSourceLabel;
};

}   // namespace pdfplugin

#endif // OCRREPORTDOCKWIDGET_H
