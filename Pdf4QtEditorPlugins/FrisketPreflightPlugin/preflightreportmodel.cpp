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

#include "preflightreportmodel.h"

namespace pdfplugin
{

PreflightReportModel::PreflightReportModel(QObject* parent) :
    QAbstractTableModel(parent)
{

}

int PreflightReportModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_findings.size();
}

int PreflightReportModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return ColumnCount;
}

QVariant PreflightReportModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_findings.size())
    {
        return QVariant();
    }

    const PreflightFindingEntry& finding = m_findings.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case Page:
                return finding.page;

            case Severity:
                return finding.severity;

            case Type:
                return finding.type;

            case Message:
                return finding.message;

            case CheckId:
                return finding.checkId;

            default:
                break;
        }
    }

    return QVariant();
}

QVariant PreflightReportModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return QVariant();
    }

    switch (section)
    {
        case Page:
            return tr("Page");

        case Severity:
            return tr("Severity");

        case Type:
            return tr("Type");

        case Message:
            return tr("Message");

        case CheckId:
            return tr("Check");

        default:
            break;
    }

    return QVariant();
}

void PreflightReportModel::setReport(const QJsonObject& report)
{
    beginResetModel();
    m_findings.clear();
    m_fixups.clear();
    m_hasReport = true;
    m_pass = report.value(QStringLiteral("pass")).toBool(true);
    m_profileName = report.value(QStringLiteral("profile")).toString();
    m_errorCount = 0;
    m_warningCount = 0;

    appendFindings(report.value(QStringLiteral("errors")).toArray());
    m_errorCount = report.value(QStringLiteral("errors")).toArray().size();
    appendFindings(report.value(QStringLiteral("warnings")).toArray());
    m_warningCount = report.value(QStringLiteral("warnings")).toArray().size();

    const QJsonArray fixups = report.value(QStringLiteral("fixups_available")).toArray();
    for (const QJsonValue& fixupValue : fixups)
    {
        const QJsonObject fixupObject = fixupValue.toObject();
        PreflightFixupEntry fixup;
        fixup.id = fixupObject.value(QStringLiteral("id")).toString();
        fixup.description = fixupObject.value(QStringLiteral("description")).toString();
        fixup.safe = fixupObject.value(QStringLiteral("safe")).toBool(false);
        m_fixups.push_back(fixup);
    }

    endResetModel();
}

void PreflightReportModel::clear()
{
    beginResetModel();
    m_findings.clear();
    m_fixups.clear();
    m_hasReport = false;
    m_pass = true;
    m_profileName.clear();
    m_errorCount = 0;
    m_warningCount = 0;
    endResetModel();
}

void PreflightReportModel::appendFindings(const QJsonArray& findings)
{
    for (const QJsonValue& findingValue : findings)
    {
        const QJsonObject findingObject = findingValue.toObject();
        PreflightFindingEntry finding;
        finding.page = findingObject.value(QStringLiteral("page")).toInt();
        finding.severity = findingObject.value(QStringLiteral("severity")).toString();
        finding.type = findingObject.value(QStringLiteral("type")).toString();
        finding.message = findingObject.value(QStringLiteral("message")).toString();
        finding.checkId = findingObject.value(QStringLiteral("check_id")).toString();
        m_findings.push_back(finding);
    }
}

}   // namespace pdfplugin
