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

#include "pdftoolpreflight.h"

#include "pdfdocument.h"
#include "pdfpage.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace pdftool
{

namespace
{

static PDFToolPreflightApplication s_preflightApplication;

struct PreflightCheckConfig
{
    QString id;
    QString severity = QStringLiteral("error");
    bool enabled = true;
    qreal amountPt = 9.0;
    bool required = true;
};

struct PreflightFixupConfig
{
    QString id;
    bool confirm = true;
    qreal amountPt = 0.0;
    QString description;
};

struct PreflightProfileData
{
    QString name;
    QList<PreflightCheckConfig> checks;
    QList<PreflightFixupConfig> fixups;
};

struct PreflightFinding
{
    int page = 1;
    QString objectId;
    QString type;
    QString severity;
    QString message;
    QRectF bbox;
    QString checkId;
};

QJsonArray rectToBbox(const QRectF& rect)
{
    return QJsonArray{ rect.left(), rect.top(), rect.right(), rect.bottom() };
}

QJsonObject findingToJson(const PreflightFinding& finding)
{
    QJsonObject object;
    object.insert(QStringLiteral("page"), finding.page);
    if (finding.objectId.isNull())
    {
        object.insert(QStringLiteral("object_id"), QJsonValue::Null);
    }
    else
    {
        object.insert(QStringLiteral("object_id"), finding.objectId);
    }
    object.insert(QStringLiteral("type"), finding.type);
    object.insert(QStringLiteral("severity"), finding.severity);
    object.insert(QStringLiteral("message"), finding.message);
    object.insert(QStringLiteral("bbox"), rectToBbox(finding.bbox));
    if (!finding.checkId.isEmpty())
    {
        object.insert(QStringLiteral("check_id"), finding.checkId);
    }
    return object;
}

bool loadProfile(const QString& profilePath, PreflightProfileData& profile, QString& errorMessage)
{
    QFile profileFile(profilePath);
    if (!profileFile.open(QIODevice::ReadOnly))
    {
        errorMessage = PDFToolTranslationContext::tr("Cannot open profile '%1'.").arg(profilePath);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(profileFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        errorMessage = PDFToolTranslationContext::tr("Invalid profile JSON in '%1': %2").arg(profilePath, parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    profile.name = root.value(QStringLiteral("name")).toString();
    if (profile.name.isEmpty())
    {
        errorMessage = PDFToolTranslationContext::tr("Profile '%1' is missing required field 'name'.").arg(profilePath);
        return false;
    }

    const QJsonArray checks = root.value(QStringLiteral("checks")).toArray();
    if (checks.isEmpty())
    {
        errorMessage = PDFToolTranslationContext::tr("Profile '%1' must define at least one check.").arg(profilePath);
        return false;
    }

    for (const QJsonValue& checkValue : checks)
    {
        const QJsonObject checkObject = checkValue.toObject();
        PreflightCheckConfig check;
        check.id = checkObject.value(QStringLiteral("id")).toString();
        if (check.id.isEmpty())
        {
            errorMessage = PDFToolTranslationContext::tr("Profile '%1' contains a check without 'id'.").arg(profilePath);
            return false;
        }

        check.severity = checkObject.value(QStringLiteral("severity")).toString(check.severity);
        check.enabled = checkObject.value(QStringLiteral("enabled")).toBool(true);
        check.amountPt = checkObject.value(QStringLiteral("amount_pt")).toDouble(check.amountPt);
        check.required = checkObject.value(QStringLiteral("required")).toBool(check.required);
        profile.checks.push_back(check);
    }

    const QJsonArray fixups = root.value(QStringLiteral("fixups")).toArray();
    for (const QJsonValue& fixupValue : fixups)
    {
        const QJsonObject fixupObject = fixupValue.toObject();
        PreflightFixupConfig fixup;
        fixup.id = fixupObject.value(QStringLiteral("id")).toString();
        if (fixup.id.isEmpty())
        {
            continue;
        }

        fixup.confirm = fixupObject.value(QStringLiteral("confirm")).toBool(true);
        fixup.amountPt = fixupObject.value(QStringLiteral("amount_pt")).toDouble(0.0);
        profile.fixups.push_back(fixup);
    }

    return true;
}

bool isBleedAdequate(const pdf::PDFPage* page, qreal amountPt, QRectF& bboxForReport)
{
    const QRectF media = page->getMediaBox().normalized();
    QRectF trim = page->getTrimBox().normalized();
    if (trim.isEmpty())
    {
        trim = page->getCropBox().normalized();
    }
    if (trim.isEmpty())
    {
        trim = media;
    }

    QRectF bleed = page->getBleedBox().normalized();
    bboxForReport = bleed.isEmpty() ? media : bleed;

    if (bleed.isEmpty())
    {
        return false;
    }

    const qreal tolerance = 0.25;
    return (trim.left() - bleed.left() >= amountPt - tolerance)
        && (trim.top() - bleed.top() >= amountPt - tolerance)
        && (bleed.right() - trim.right() >= amountPt - tolerance)
        && (bleed.bottom() - trim.bottom() >= amountPt - tolerance);
}

void appendBleedFindings(const pdf::PDFDocument& document,
                         const PreflightCheckConfig& check,
                         QList<PreflightFinding>& errors,
                         QList<PreflightFinding>& warnings)
{
    const pdf::PDFCatalog* catalog = document.getCatalog();
    const pdf::PDFInteger pageCount = catalog->getPageCount();

    for (pdf::PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const pdf::PDFPage* page = catalog->getPage(pageIndex);
        QRectF bbox;
        const bool adequate = isBleedAdequate(page, check.amountPt, bbox);
        if (adequate)
        {
            continue;
        }

        PreflightFinding finding;
        finding.page = int(pageIndex + 1);
        finding.objectId = QString();
        finding.type = QStringLiteral("bleed");
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.bbox = bbox;
        finding.message = PDFToolTranslationContext::tr("BleedBox is missing or less than %1 pt on one or more edges").arg(check.amountPt);

        if (check.severity == QStringLiteral("warning") || check.severity == QStringLiteral("info"))
        {
            warnings.push_back(finding);
        }
        else
        {
            errors.push_back(finding);
        }
    }
}

QString defaultFixupDescription(const QString& fixupId)
{
    if (fixupId == QStringLiteral("rgb-to-cmyk"))
    {
        return PDFToolTranslationContext::tr("Convert all RGB colors to CMYK");
    }
    if (fixupId == QStringLiteral("add-bleed"))
    {
        return PDFToolTranslationContext::tr("Extend page boxes / artwork to provide bleed");
    }
    if (fixupId == QStringLiteral("downsample-images"))
    {
        return PDFToolTranslationContext::tr("Downsample images above target DPI");
    }

    return PDFToolTranslationContext::tr("Apply fixup '%1'").arg(fixupId);
}

QByteArray buildReportJson(const PreflightProfileData& profile,
                           const QString& pdfPath,
                           const QList<PreflightFinding>& errors,
                           const QList<PreflightFinding>& warnings)
{
    QJsonArray errorsArray;
    for (const PreflightFinding& finding : errors)
    {
        errorsArray.append(findingToJson(finding));
    }

    QJsonArray warningsArray;
    for (const PreflightFinding& finding : warnings)
    {
        warningsArray.append(findingToJson(finding));
    }

    QJsonArray fixupsArray;
    for (const PreflightFixupConfig& fixup : profile.fixups)
    {
        QJsonObject fixupObject;
        fixupObject.insert(QStringLiteral("id"), fixup.id);
        fixupObject.insert(QStringLiteral("safe"), false);
        fixupObject.insert(QStringLiteral("description"), fixup.description.isEmpty() ? defaultFixupDescription(fixup.id) : fixup.description);
        fixupsArray.append(fixupObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema_version"), 1);
    root.insert(QStringLiteral("pass"), errors.isEmpty());
    root.insert(QStringLiteral("profile"), profile.name);
    root.insert(QStringLiteral("engine_version"), QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("pdf"), pdfPath);
    root.insert(QStringLiteral("errors"), errorsArray);
    root.insert(QStringLiteral("warnings"), warningsArray);
    root.insert(QStringLiteral("fixups_available"), fixupsArray);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

}   // namespace

QString PDFToolPreflightApplication::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
        case Command:
            return QStringLiteral("preflight");

        case Name:
            return PDFToolTranslationContext::tr("Preflight");

        case Description:
            return PDFToolTranslationContext::tr("Run Frisket preflight checks and emit a normalized JSON report.");

        default:
            Q_ASSERT(false);
            break;
    }

    return QString();
}

int PDFToolPreflightApplication::execute(const PDFToolOptions& options)
{
    if (options.document.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("No document specified."), options.outputCodec);
        return ErrorNoDocumentSpecified;
    }

    if (options.preflightProfilePath.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("No profile specified. Use --profile <file.json>."), options.outputCodec);
        return ErrorInvalidArguments;
    }

    PreflightProfileData profile;
    QString profileError;
    if (!loadProfile(options.preflightProfilePath, profile, profileError))
    {
        PDFConsole::writeError(profileError, options.outputCodec);
        return ErrorInvalidArguments;
    }

    pdf::PDFDocument document;
    QByteArray sourceData;
    if (!readDocument(options, document, &sourceData, false))
    {
        return ErrorDocumentReading;
    }

    QList<PreflightFinding> errors;
    QList<PreflightFinding> warnings;

    for (const PreflightCheckConfig& check : profile.checks)
    {
        if (!check.enabled)
        {
            continue;
        }

        if (check.id == QStringLiteral("bleed"))
        {
            appendBleedFindings(document, check, errors, warnings);
        }
        // Additional checks (fonts, color mode, image DPI, trim, page-size) — MIC-134.
    }

    const QByteArray reportJson = buildReportJson(profile, options.document, errors, warnings);
    PDFConsole::writeData(reportJson);
    PDFConsole::writeData(QByteArray("\n"));

    return errors.isEmpty() ? ExitSuccess : ExitFailure;
}

PDFToolAbstractApplication::Options PDFToolPreflightApplication::getOptionsFlags() const
{
    return OpenDocument | PreflightProfile;
}

}   // namespace pdftool
