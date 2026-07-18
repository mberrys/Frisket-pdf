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

#ifndef PREFLIGHTREPORTMODEL_H
#define PREFLIGHTREPORTMODEL_H

#include <QAbstractTableModel>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVector>

namespace pdfplugin
{

struct PreflightFixupEntry
{
    QString id;
    QString description;
    bool safe = false;
};

struct PreflightFindingEntry
{
    int page = 0;
    QString severity;
    QString type;
    QString message;
    QString checkId;
};

class PreflightReportModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        Page,
        Severity,
        Type,
        Message,
        CheckId,
        ColumnCount
    };

    explicit PreflightReportModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setReport(const QJsonObject& report);
    void clear();

    bool hasReport() const { return m_hasReport; }
    bool pass() const { return m_pass; }
    QString profileName() const { return m_profileName; }
    int errorCount() const { return m_errorCount; }
    int warningCount() const { return m_warningCount; }
    const QVector<PreflightFixupEntry>& fixups() const { return m_fixups; }

private:
    void appendFindings(const QJsonArray& findings);

    QVector<PreflightFindingEntry> m_findings;
    QVector<PreflightFixupEntry> m_fixups;
    bool m_hasReport = false;
    bool m_pass = true;
    QString m_profileName;
    int m_errorCount = 0;
    int m_warningCount = 0;
};

}   // namespace pdfplugin

#endif // PREFLIGHTREPORTMODEL_H
