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

#include "preflightengine.h"

#include "pdfbleedmarginprobe.h"
#include "pdfglobal.h"
#include "pdfcatalog.h"
#include "pdfdocument.h"
#include "pdfpage.h"
#include "pdfpreflightchecks.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace pdf
{

namespace
{

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

QString defaultFixupDescription(const QString& fixupId)
{
    if (fixupId == QStringLiteral("rgb-to-cmyk"))
    {
        return PDFTranslationContext::tr("Convert all RGB colors to CMYK");
    }
    if (fixupId == QStringLiteral("add-bleed"))
    {
        return PDFTranslationContext::tr("Extend page boxes / artwork to provide bleed");
    }
    if (fixupId == QStringLiteral("downsample-images"))
    {
        return PDFTranslationContext::tr("Downsample images above target DPI");
    }

    return PDFTranslationContext::tr("Apply fixup '%1'").arg(fixupId);
}

bool isBleedAdequate(const PDFPage* page, qreal amountPt, qreal tolerancePt, QRectF& bboxForReport)
{
    const QRectF media = page->getMediaBox().normalized();
    const QRectF trim = preflight::resolveEffectiveBox(page->getTrimBox(), page->getCropBox(), media);
    const QRectF bleed = page->getBleedBox().normalized();
    bboxForReport = bleed.isEmpty() ? media : bleed;

    return preflight::bleedAdequate(trim, bleed, amountPt, tolerancePt);
}

void runBleedCheck(PDFDocumentSession* session,
                   const PreflightCheckConfig& check,
                   QList<PreflightFinding>& errors,
                   QList<PreflightFinding>& warnings)
{
    const PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        QRectF bbox;
        const bool adequate = isBleedAdequate(page, check.amountPt, check.tolerancePt, bbox);
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
        finding.message = PDFTranslationContext::tr("BleedBox is missing or less than %1 pt on one or more edges").arg(check.amountPt);

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

enum class SizeCheckKind
{
    Trim,       ///< Measures the TrimBox (with trim -> crop -> media fallback).
    PageSize    ///< Measures the MediaBox (physical page size).
};

void runSizeCheck(SizeCheckKind kind,
                  PDFDocumentSession* session,
                  const PreflightCheckConfig& check,
                  QList<PreflightFinding>& errors,
                  QList<PreflightFinding>& warnings)
{
    if (!check.hasExpectedSize)
    {
        return;
    }

    const PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    const QString type = (kind == SizeCheckKind::Trim) ? QStringLiteral("trim") : QStringLiteral("page-size");
    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        const QRectF media = page->getMediaBox().normalized();
        const QRectF box = (kind == SizeCheckKind::Trim)
                               ? preflight::resolveEffectiveBox(page->getTrimBox(), page->getCropBox(), media)
                               : media;

        if (preflight::sizeWithinTolerance(box.width(), box.height(), check.expectedWidthPt, check.expectedHeightPt, check.tolerancePt))
        {
            continue;
        }

        PreflightFinding finding;
        finding.page = int(pageIndex + 1);
        finding.objectId = QString();
        finding.type = type;
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.bbox = box;
        finding.message = (kind == SizeCheckKind::Trim)
                              ? PDFTranslationContext::tr("TrimBox %1 x %2 pt does not match expected %3 x %4 pt (tolerance %5 pt)")
                                    .arg(box.width()).arg(box.height()).arg(check.expectedWidthPt).arg(check.expectedHeightPt).arg(check.tolerancePt)
                              : PDFTranslationContext::tr("Page size %1 x %2 pt does not match expected %3 x %4 pt (tolerance %5 pt)")
                                    .arg(box.width()).arg(box.height()).arg(check.expectedWidthPt).arg(check.expectedHeightPt).arg(check.tolerancePt);

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

QString sideNameForFinding(PDFBleedFixupSide side)
{
    switch (side)
    {
        case PDFBleedFixupSide::Left: return QStringLiteral("left");
        case PDFBleedFixupSide::Right: return QStringLiteral("right");
        case PDFBleedFixupSide::Top: return QStringLiteral("top");
        case PDFBleedFixupSide::Bottom: return QStringLiteral("bottom");
    }
    return QStringLiteral("unknown");
}

void runContentBleedCheck(PDFDocumentSession* session,
                           const PreflightCheckConfig& check,
                           QList<PreflightFinding>& errors,
                           QList<PreflightFinding>& warnings)
{
    if (!session)
    {
        return;
    }

    PDFDocument* document = session->getDocument();
    if (!document)
    {
        return;
    }

    PDFBleedMarginProbe probe(session);

    PDFBleedMarginProbeSettings probeSettings;
    const PDFReal bleedMm = convertPDFPointToMM(check.amountPt);
    probeSettings.bleedMM = QMarginsF(bleedMm, bleedMm, bleedMm, bleedMm);
    probeSettings.dpi = check.probeDpi;
    probeSettings.threshold = check.probeThreshold;
    probeSettings.fastOnly = !check.rasterConfirm;

    const PDFCatalog* catalog = document->getCatalog();
    const PDFInteger pageCount = catalog->getPageCount();

    for (PDFInteger pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        if (!page)
        {
            continue;
        }

        const PDFBleedMarginProbeResult result = probe.probe(page, static_cast<size_t>(pageIndex), probeSettings);

        if (result.allEdgesCovered())
        {
            continue;
        }

        const PDFBleedFixupSide sides[4] = {
            PDFBleedFixupSide::Left, PDFBleedFixupSide::Right,
            PDFBleedFixupSide::Top, PDFBleedFixupSide::Bottom
        };

        QStringList missingSides;
        QRectF unionMissingBbox;
        for (PDFBleedFixupSide side : sides)
        {
            bool hasContent = false;
            QRectF stripRect;
            switch (side)
            {
                case PDFBleedFixupSide::Left: hasContent = result.left.hasContent; stripRect = result.left.stripRect; break;
                case PDFBleedFixupSide::Right: hasContent = result.right.hasContent; stripRect = result.right.stripRect; break;
                case PDFBleedFixupSide::Top: hasContent = result.top.hasContent; stripRect = result.top.stripRect; break;
                case PDFBleedFixupSide::Bottom: hasContent = result.bottom.hasContent; stripRect = result.bottom.stripRect; break;
            }

            if (!hasContent)
            {
                missingSides.append(sideNameForFinding(side));
                if (stripRect.isValid())
                {
                    unionMissingBbox = unionMissingBbox.united(stripRect);
                }
            }
        }

        if (missingSides.isEmpty())
        {
            continue;
        }

        PreflightFinding finding;
        finding.page = int(pageIndex + 1);
        finding.objectId = QString();
        finding.type = QStringLiteral("content-bleed");
        finding.severity = check.severity;
        finding.checkId = check.id;
        finding.bbox = unionMissingBbox.isValid() ? unionMissingBbox : QRectF(0, 0, 0, 0);
        finding.message = PDFTranslationContext::tr("Artwork does not extend into bleed margin on %1").arg(missingSides.join(QStringLiteral(", ")));

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

bool hasBleedGapFinding(const QList<PreflightFinding>& findings)
{
    for (const PreflightFinding& finding : findings)
    {
        if (finding.checkId == QStringLiteral("bleed") || finding.checkId == QStringLiteral("content-bleed"))
        {
            return true;
        }
    }
    return false;
}

void adjustFixupsAvailable(QList<PreflightFixupConfig>& fixups, bool needsAddBleed, qreal addBleedAmountPt)
{
    PreflightFixupConfig addBleedConfig;
    bool hasProfileAddBleed = false;

    for (const PreflightFixupConfig& fixup : fixups)
    {
        if (fixup.id == QStringLiteral("add-bleed"))
        {
            addBleedConfig = fixup;
            hasProfileAddBleed = true;
            break;
        }
    }

    // Remove all add-bleed entries; they will be re-added below if needed.
    auto it = std::remove_if(fixups.begin(), fixups.end(), [](const PreflightFixupConfig& fixup)
    {
        return fixup.id == QStringLiteral("add-bleed");
    });
    fixups.erase(it, fixups.end());

    if (needsAddBleed)
    {
        if (!hasProfileAddBleed)
        {
            addBleedConfig.id = QStringLiteral("add-bleed");
            addBleedConfig.confirm = true;
        }
        if (addBleedConfig.description.isEmpty())
        {
            addBleedConfig.description = PDFTranslationContext::tr("Extend page boxes / artwork to provide bleed");
        }
        if (addBleedConfig.amountPt <= 0.0 && addBleedAmountPt > 0.0)
        {
            addBleedConfig.amountPt = addBleedAmountPt;
        }

        QJsonObject params = addBleedConfig.params;
        if (!params.contains(QStringLiteral("mode")))
        {
            params.insert(QStringLiteral("mode"), QStringLiteral("mirror"));
        }
        addBleedConfig.params = params;

        fixups.push_back(addBleedConfig);
    }
}

} // namespace

QJsonObject PreflightResult::toJson(const QString& pdfPath) const
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
    for (const PreflightFixupConfig& fixup : fixupsAvailable)
    {
        QJsonObject fixupObject;
        fixupObject.insert(QStringLiteral("id"), fixup.id);
        fixupObject.insert(QStringLiteral("safe"), false);
        fixupObject.insert(QStringLiteral("description"), fixup.description.isEmpty() ? defaultFixupDescription(fixup.id) : fixup.description);

        QJsonObject params = fixup.params;
        if (fixup.amountPt > 0.0 && !params.contains(QStringLiteral("amount_pt")))
        {
            params.insert(QStringLiteral("amount_pt"), fixup.amountPt);
        }
        if (!params.isEmpty())
        {
            fixupObject.insert(QStringLiteral("params"), params);
        }

        fixupsArray.append(fixupObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema_version"), 1);
    root.insert(QStringLiteral("pass"), errors.isEmpty());
    root.insert(QStringLiteral("profile"), profileName);
    root.insert(QStringLiteral("engine_version"), QCoreApplication::applicationVersion());
    if (!pdfPath.isEmpty())
    {
        root.insert(QStringLiteral("pdf"), pdfPath);
    }
    root.insert(QStringLiteral("errors"), errorsArray);
    root.insert(QStringLiteral("warnings"), warningsArray);
    root.insert(QStringLiteral("fixups_available"), fixupsArray);

    return root;
}

PreflightEngine::PreflightEngine(PDFDocumentSession* session) :
    m_session(session)
{
    registerBuiltInChecks();
}

void PreflightEngine::registerCheck(const QString& id, CheckRunner runner)
{
    m_checks[id] = std::move(runner);
}

bool PreflightEngine::hasCheck(const QString& id) const
{
    return m_checks.count(id) > 0;
}

PreflightResult PreflightEngine::run(const QJsonObject& profile)
{
    PreflightProfileData data;
    QString errorMessage;
    if (!parseProfile(profile, data, errorMessage))
    {
        // Profile parsing errors are treated as a failing preflight result so
        // callers can surface them in the same report shape.
        PreflightResult result;
        result.pass = false;
        result.profileName = profile.value(QStringLiteral("name")).toString();

        PreflightFinding finding;
        finding.page = 1;
        finding.type = QStringLiteral("profile");
        finding.severity = QStringLiteral("error");
        finding.message = errorMessage;
        finding.bbox = QRectF();
        result.errors.push_back(finding);

        return result;
    }

    return run(data);
}

PreflightResult PreflightEngine::run(const PreflightProfileData& profile)
{
    PreflightResult result;
    result.profileName = profile.name;

    for (const PreflightCheckConfig& check : profile.checks)
    {
        if (!check.enabled)
        {
            continue;
        }

        auto it = m_checks.find(check.id);
        if (it == m_checks.end())
        {
            // Unknown check ids are ignored (per profile schema note).
            continue;
        }

        it->second(m_session, check, result.errors, result.warnings);
    }

    result.pass = result.errors.isEmpty();
    result.fixupsAvailable = profile.fixups;

    qreal addBleedAmountPt = 0.0;
    for (const PreflightCheckConfig& check : profile.checks)
    {
        if (check.enabled && (check.id == QStringLiteral("bleed") || check.id == QStringLiteral("content-bleed")) && check.amountPt > 0.0)
        {
            addBleedAmountPt = check.amountPt;
            break;
        }
    }

    const bool needsAddBleed = hasBleedGapFinding(result.errors) || hasBleedGapFinding(result.warnings);
    adjustFixupsAvailable(result.fixupsAvailable, needsAddBleed, addBleedAmountPt);

    return result;
}

PDFDocumentSession* PreflightEngine::getSession() const
{
    return m_session;
}

bool PreflightEngine::loadProfile(const QString& profilePath, QJsonObject& profile, QString& errorMessage)
{
    QFile profileFile(profilePath);
    if (!profileFile.open(QIODevice::ReadOnly))
    {
        errorMessage = PDFTranslationContext::tr("Cannot open profile '%1'.").arg(profilePath);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(profileFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        errorMessage = PDFTranslationContext::tr("Invalid profile JSON in '%1': %2").arg(profilePath, parseError.errorString());
        return false;
    }

    profile = document.object();
    return true;
}

bool PreflightEngine::parseProfile(const QJsonObject& profileObject, PreflightProfileData& profile, QString& errorMessage)
{
    errorMessage.clear();
    profile = PreflightProfileData();

    profile.name = profileObject.value(QStringLiteral("name")).toString();
    if (profile.name.isEmpty())
    {
        errorMessage = PDFTranslationContext::tr("Profile is missing required field 'name'.");
        return false;
    }

    const QJsonArray checks = profileObject.value(QStringLiteral("checks")).toArray();
    if (checks.isEmpty())
    {
        errorMessage = PDFTranslationContext::tr("Profile must define at least one check.");
        return false;
    }

    for (const QJsonValue& checkValue : checks)
    {
        const QJsonObject checkObject = checkValue.toObject();
        PreflightCheckConfig check;
        check.id = checkObject.value(QStringLiteral("id")).toString();
        if (check.id.isEmpty())
        {
            errorMessage = PDFTranslationContext::tr("Profile contains a check without 'id'.");
            return false;
        }

        check.severity = checkObject.value(QStringLiteral("severity")).toString(check.severity);
        check.enabled = checkObject.value(QStringLiteral("enabled")).toBool(true);
        check.amountPt = checkObject.value(QStringLiteral("amount_pt")).toDouble(check.amountPt);
        check.required = checkObject.value(QStringLiteral("required")).toBool(check.required);
        check.expectedWidthPt = checkObject.value(QStringLiteral("expected_width_pt")).toDouble(check.expectedWidthPt);
        check.expectedHeightPt = checkObject.value(QStringLiteral("expected_height_pt")).toDouble(check.expectedHeightPt);
        check.tolerancePt = checkObject.value(QStringLiteral("tolerance_pt")).toDouble(check.tolerancePt);
        check.hasExpectedSize = check.expectedWidthPt > 0.0 && check.expectedHeightPt > 0.0;

        check.rasterConfirm = checkObject.value(QStringLiteral("raster_confirm")).toBool(false);
        check.probeDpi = checkObject.value(QStringLiteral("probe_dpi")).toInt(150);
        check.probeThreshold = checkObject.value(QStringLiteral("probe_threshold")).toInt(16);

        profile.checks.push_back(check);
    }

    const QJsonArray fixups = profileObject.value(QStringLiteral("fixups")).toArray();
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
        fixup.description = fixupObject.value(QStringLiteral("description")).toString();
        fixup.params = fixupObject.value(QStringLiteral("params")).toObject();

        profile.fixups.push_back(fixup);
    }

    return true;
}

void PreflightEngine::registerBuiltInChecks()
{
    m_checks[QStringLiteral("bleed")] = [](PDFDocumentSession* session,
                                            const PreflightCheckConfig& check,
                                            QList<PreflightFinding>& errors,
                                            QList<PreflightFinding>& warnings)
    {
        runBleedCheck(session, check, errors, warnings);
    };

    m_checks[QStringLiteral("trim")] = [](PDFDocumentSession* session,
                                           const PreflightCheckConfig& check,
                                           QList<PreflightFinding>& errors,
                                           QList<PreflightFinding>& warnings)
    {
        runSizeCheck(SizeCheckKind::Trim, session, check, errors, warnings);
    };

    m_checks[QStringLiteral("page-size")] = [](PDFDocumentSession* session,
                                                 const PreflightCheckConfig& check,
                                                 QList<PreflightFinding>& errors,
                                                 QList<PreflightFinding>& warnings)
    {
        runSizeCheck(SizeCheckKind::PageSize, session, check, errors, warnings);
    };

    m_checks[QStringLiteral("content-bleed")] = [](PDFDocumentSession* session,
                                                    const PreflightCheckConfig& check,
                                                    QList<PreflightFinding>& errors,
                                                    QList<PreflightFinding>& warnings)
    {
        runContentBleedCheck(session, check, errors, warnings);
    };
}

} // namespace pdf
