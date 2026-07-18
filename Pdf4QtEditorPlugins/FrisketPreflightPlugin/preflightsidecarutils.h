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

#ifndef PREFLIGHTSIDECARUTILS_H
#define PREFLIGHTSIDECARUTILS_H

#include <cmath>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace pdfplugin::preflight
{

inline QString resolveBundlePath(const QString& applicationDirectory, const QString& relativePath)
{
    return QDir::cleanPath(QDir(applicationDirectory).filePath(relativePath));
}

inline QString getPdfToolFileName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("PdfTool.exe");
#else
    return QStringLiteral("PdfTool");
#endif
}

inline bool isExpectedPreflightExitCode(int exitCode)
{
    return exitCode == 0 || exitCode == 1;
}

inline bool setValidationError(QString* errorMessage, const QString& message)
{
    if (errorMessage)
    {
        *errorMessage = message;
    }

    return false;
}

inline bool hasOnlyProperties(const QJsonObject& object, const QSet<QString>& allowedProperties, const QString& context, QString* errorMessage)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator)
    {
        if (!allowedProperties.contains(iterator.key()))
        {
            return setValidationError(errorMessage,
                                      QStringLiteral("%1 contains unsupported property '%2'.").arg(context, iterator.key()));
        }
    }

    return true;
}

inline bool isInteger(const QJsonValue& value)
{
    const double number = value.toDouble();
    return value.isDouble() && std::isfinite(number) && std::floor(number) == number;
}

inline bool isContractIdentifier(const QString& value)
{
    static const QRegularExpression expression(QStringLiteral("^[a-z][a-z0-9-]*$"));
    return expression.match(value).hasMatch();
}

inline bool isSupportedSchemaVersion(int schemaVersion)
{
    return schemaVersion == 1 || schemaVersion == 2;
}

inline bool validateBboxValue(const QJsonValue& bboxValue, const QString& context, QString* errorMessage)
{
    if (!bboxValue.isArray() || bboxValue.toArray().size() != 4)
    {
        return setValidationError(errorMessage, QStringLiteral("%1.bbox must contain four numbers.").arg(context));
    }

    const QJsonArray bbox = bboxValue.toArray();
    for (const QJsonValue& coordinate : bbox)
    {
        if (!coordinate.isDouble())
        {
            return setValidationError(errorMessage, QStringLiteral("%1.bbox must contain four numbers.").arg(context));
        }
    }

    if (bbox.at(0).toDouble() > bbox.at(2).toDouble() || bbox.at(1).toDouble() > bbox.at(3).toDouble())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.bbox coordinates are not ordered.").arg(context));
    }

    return true;
}

inline bool validateFindingCommonFields(const QJsonObject& finding, const QString& context, QString* errorMessage)
{
    const QString type = finding.value(QStringLiteral("type")).toString();
    if (!finding.value(QStringLiteral("type")).isString() || !isContractIdentifier(type))
    {
        return setValidationError(errorMessage, QStringLiteral("%1.type must be a non-empty kebab-case identifier.").arg(context));
    }

    const QString severity = finding.value(QStringLiteral("severity")).toString();
    if (severity != QStringLiteral("error") && severity != QStringLiteral("warning") && severity != QStringLiteral("info"))
    {
        return setValidationError(errorMessage, QStringLiteral("%1.severity is invalid.").arg(context));
    }

    if (!finding.value(QStringLiteral("message")).isString() || finding.value(QStringLiteral("message")).toString().isEmpty())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.message must be a non-empty string.").arg(context));
    }

    const QJsonValue objectId = finding.value(QStringLiteral("object_id"));
    if (!objectId.isUndefined() && !objectId.isNull() && !objectId.isString())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.object_id must be a string or null.").arg(context));
    }

    const QJsonValue checkId = finding.value(QStringLiteral("check_id"));
    if (!checkId.isUndefined() && !checkId.isString())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.check_id must be a string.").arg(context));
    }

    return true;
}

inline bool validateFindingV1(const QJsonObject& finding, const QString& context, QString* errorMessage)
{
    static const QSet<QString> allowedProperties = {
        QStringLiteral("page"),
        QStringLiteral("object_id"),
        QStringLiteral("type"),
        QStringLiteral("severity"),
        QStringLiteral("message"),
        QStringLiteral("bbox"),
        QStringLiteral("check_id")
    };
    if (!hasOnlyProperties(finding, allowedProperties, context, errorMessage))
    {
        return false;
    }

    const QJsonValue page = finding.value(QStringLiteral("page"));
    if (!isInteger(page) || page.toInt() < 1)
    {
        return setValidationError(errorMessage, QStringLiteral("%1.page must be a positive integer.").arg(context));
    }

    if (!validateFindingCommonFields(finding, context, errorMessage))
    {
        return false;
    }

    return validateBboxValue(finding.value(QStringLiteral("bbox")), context, errorMessage);
}

inline bool validateFindingV2(const QJsonObject& finding, const QString& context, QString* errorMessage)
{
    static const QSet<QString> allowedProperties = {
        QStringLiteral("scope"),
        QStringLiteral("page"),
        QStringLiteral("object_id"),
        QStringLiteral("type"),
        QStringLiteral("severity"),
        QStringLiteral("message"),
        QStringLiteral("bbox"),
        QStringLiteral("check_id")
    };
    if (!hasOnlyProperties(finding, allowedProperties, context, errorMessage))
    {
        return false;
    }

    const QString scope = finding.value(QStringLiteral("scope")).toString();
    if (scope != QStringLiteral("document") && scope != QStringLiteral("page") && scope != QStringLiteral("object"))
    {
        return setValidationError(errorMessage, QStringLiteral("%1.scope must be document, page, or object.").arg(context));
    }

    if (!validateFindingCommonFields(finding, context, errorMessage))
    {
        return false;
    }

    const bool hasPage = finding.contains(QStringLiteral("page"));
    const bool hasBbox = finding.contains(QStringLiteral("bbox"));

    if (scope == QStringLiteral("document"))
    {
        if (hasPage)
        {
            return setValidationError(errorMessage, QStringLiteral("%1.page must be absent for document scope.").arg(context));
        }

        if (hasBbox)
        {
            return setValidationError(errorMessage, QStringLiteral("%1.bbox must be absent for document scope.").arg(context));
        }

        return true;
    }

    const QJsonValue page = finding.value(QStringLiteral("page"));
    if (!isInteger(page) || page.toInt() < 1)
    {
        return setValidationError(errorMessage, QStringLiteral("%1.page must be a positive integer for page/object scope.").arg(context));
    }

    if (hasBbox && !validateBboxValue(finding.value(QStringLiteral("bbox")), context, errorMessage))
    {
        return false;
    }

    return true;
}

inline bool findingHasVisualOverlay(const QJsonObject& finding, int schemaVersion)
{
    if (schemaVersion == 1)
    {
        return finding.contains(QStringLiteral("bbox"));
    }

    const QString scope = finding.value(QStringLiteral("scope")).toString();
    if (scope == QStringLiteral("document"))
    {
        return false;
    }

    return finding.contains(QStringLiteral("bbox"));
}

inline bool validateFinding(const QJsonValue& value, const QString& section, int index, int schemaVersion, QString* errorMessage)
{
    const QString context = QStringLiteral("%1[%2]").arg(section).arg(index);
    if (!value.isObject())
    {
        return setValidationError(errorMessage, QStringLiteral("%1 must be an object.").arg(context));
    }

    const QJsonObject finding = value.toObject();
    if (schemaVersion == 1)
    {
        return validateFindingV1(finding, context, errorMessage);
    }

    return validateFindingV2(finding, context, errorMessage);
}

inline bool validateFixup(const QJsonValue& value, int index, QString* errorMessage)
{
    const QString context = QStringLiteral("fixups_available[%1]").arg(index);
    if (!value.isObject())
    {
        return setValidationError(errorMessage, QStringLiteral("%1 must be an object.").arg(context));
    }

    const QJsonObject fixup = value.toObject();
    static const QSet<QString> allowedProperties = {
        QStringLiteral("id"),
        QStringLiteral("safe"),
        QStringLiteral("description"),
        QStringLiteral("params")
    };
    if (!hasOnlyProperties(fixup, allowedProperties, context, errorMessage))
    {
        return false;
    }

    const QString id = fixup.value(QStringLiteral("id")).toString();
    if (!fixup.value(QStringLiteral("id")).isString() || !isContractIdentifier(id))
    {
        return setValidationError(errorMessage, QStringLiteral("%1.id must be a non-empty kebab-case identifier.").arg(context));
    }

    if (!fixup.value(QStringLiteral("safe")).isBool())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.safe must be a boolean.").arg(context));
    }

    const QJsonValue params = fixup.value(QStringLiteral("params"));
    if (!params.isUndefined() && !params.isNull() && !params.isObject())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.params must be an object.").arg(context));
    }

    if (!fixup.value(QStringLiteral("description")).isString() || fixup.value(QStringLiteral("description")).toString().isEmpty())
    {
        return setValidationError(errorMessage, QStringLiteral("%1.description must be a non-empty string.").arg(context));
    }

    return true;
}

inline bool validateNormalizedReport(const QJsonObject& report, QString* errorMessage = nullptr)
{
    if (errorMessage)
    {
        errorMessage->clear();
    }

    static const QSet<QString> allowedProperties = {
        QStringLiteral("schema_version"),
        QStringLiteral("pass"),
        QStringLiteral("profile"),
        QStringLiteral("engine_version"),
        QStringLiteral("pdf"),
        QStringLiteral("errors"),
        QStringLiteral("warnings"),
        QStringLiteral("fixups_available")
    };
    if (!hasOnlyProperties(report, allowedProperties, QStringLiteral("report"), errorMessage))
    {
        return false;
    }

    const QJsonValue schemaVersion = report.value(QStringLiteral("schema_version"));
    if (!isInteger(schemaVersion) || !isSupportedSchemaVersion(schemaVersion.toInt()))
    {
        return setValidationError(errorMessage, QStringLiteral("schema_version must be 1 or 2."));
    }

    const int schemaVersionValue = schemaVersion.toInt();

    if (!report.value(QStringLiteral("pass")).isBool())
    {
        return setValidationError(errorMessage, QStringLiteral("pass must be a boolean."));
    }

    if (!report.value(QStringLiteral("profile")).isString() || report.value(QStringLiteral("profile")).toString().isEmpty())
    {
        return setValidationError(errorMessage, QStringLiteral("profile must be a non-empty string."));
    }

    for (const QString& optionalString : { QStringLiteral("engine_version"), QStringLiteral("pdf") })
    {
        const QJsonValue value = report.value(optionalString);
        if (!value.isUndefined() && !value.isString())
        {
            return setValidationError(errorMessage, QStringLiteral("%1 must be a string.").arg(optionalString));
        }
    }

    for (const QString& section : { QStringLiteral("errors"), QStringLiteral("warnings") })
    {
        const QJsonValue sectionValue = report.value(section);
        if (!sectionValue.isArray())
        {
            return setValidationError(errorMessage, QStringLiteral("%1 must be an array.").arg(section));
        }

        const QJsonArray findings = sectionValue.toArray();
        for (int i = 0; i < findings.size(); ++i)
        {
            if (!validateFinding(findings.at(i), section, i, schemaVersionValue, errorMessage))
            {
                return false;
            }
        }
    }

    const QJsonValue fixupsValue = report.value(QStringLiteral("fixups_available"));
    if (!fixupsValue.isArray())
    {
        return setValidationError(errorMessage, QStringLiteral("fixups_available must be an array."));
    }

    const QJsonArray fixups = fixupsValue.toArray();
    for (int i = 0; i < fixups.size(); ++i)
    {
        if (!validateFixup(fixups.at(i), i, errorMessage))
        {
            return false;
        }
    }

    const bool expectedPass = report.value(QStringLiteral("errors")).toArray().isEmpty();
    if (report.value(QStringLiteral("pass")).toBool() != expectedPass)
    {
        return setValidationError(errorMessage, QStringLiteral("pass must be true exactly when errors is empty."));
    }

    return true;
}

inline bool isNormalizedReport(const QJsonObject& report)
{
    return validateNormalizedReport(report);
}

}   // namespace pdfplugin::preflight

#endif // PREFLIGHTSIDECARUTILS_H
