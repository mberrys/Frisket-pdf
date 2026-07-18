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

#ifndef PREFLIGHTENGINE_H
#define PREFLIGHTENGINE_H

#include "pdfglobal.h"
#include "pdfdocumentsession.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>

#include <functional>
#include <map>

namespace pdf
{

/// Report contract version emitted by PreflightResult::toJson().
inline constexpr int PREFLIGHT_REPORT_SCHEMA_VERSION = 2;

/// Finding location scope in normalized preflight reports.
inline constexpr QLatin1String PREFLIGHT_FINDING_SCOPE_DOCUMENT("document");
inline constexpr QLatin1String PREFLIGHT_FINDING_SCOPE_PAGE("page");
inline constexpr QLatin1String PREFLIGHT_FINDING_SCOPE_OBJECT("object");

/// Configuration for a single preflight check, parsed from a profile.
struct PDF4QTLIBCORESHARED_EXPORT PreflightCheckConfig
{
    QString id;
    QString severity = QStringLiteral("error");
    bool enabled = true;
    qreal amountPt = 9.0;
    bool required = true;
    qreal expectedWidthPt = 0.0;
    qreal expectedHeightPt = 0.0;
    qreal tolerancePt = 1.0;
    bool hasExpectedSize = false;

    // Tier-2 content-bleed parameters.
    bool rasterConfirm = false;
    int probeDpi = 150;
    int probeThreshold = 16;

    // image-resolution parameters.
    int minDpi = 0;

    // color-mode parameters (e.g. ["CMYK", "Grayscale"]).
    QStringList allowedColorModes;
};

/// Configuration for a single advertised fixup, parsed from a profile.
struct PDF4QTLIBCORESHARED_EXPORT PreflightFixupConfig
{
    QString id;
    bool confirm = true;
    qreal amountPt = 0.0;
    QString description;
    QJsonObject params;
};

/// A single preflight finding (error or warning).
struct PDF4QTLIBCORESHARED_EXPORT PreflightFinding
{
    QString scope = QString::fromLatin1(PREFLIGHT_FINDING_SCOPE_PAGE);
    int page = 1;
    QString objectId;
    QString type;
    QString severity;
    QString message;
    QRectF bbox;
    QString checkId;
};

/// Parsed preflight profile.
struct PDF4QTLIBCORESHARED_EXPORT PreflightProfileData
{
    QString name;
    QList<PreflightCheckConfig> checks;
    QList<PreflightFixupConfig> fixups;
};

/// Result of a preflight run.
struct PDF4QTLIBCORESHARED_EXPORT PreflightResult
{
    bool pass = true;
    QString profileName;
    QList<PreflightFinding> errors;
    QList<PreflightFinding> warnings;
    QList<PreflightFixupConfig> fixupsAvailable;

    QJsonObject toJson(const QString& pdfPath = QString()) const;
};

/// Orchestrator that runs a declarative preflight profile against a document
/// session and produces a normalized JSON report.
///
/// Checks register by string id. The engine ships with built-in Tier-1 checks
/// (bleed, trim, page-size). Additional checks (for example, content-bleed) can
/// be registered before calling run().
class PDF4QTLIBCORESHARED_EXPORT PreflightEngine
{
public:
    using CheckRunner = std::function<void(PDFDocumentSession* session,
                                           const PreflightCheckConfig& check,
                                           QList<PreflightFinding>& errors,
                                           QList<PreflightFinding>& warnings)>;

    explicit PreflightEngine(PDFDocumentSession* session);

    /// Registers a check implementation. Overwrites any previous registration
    /// for the same id.
    void registerCheck(const QString& id, CheckRunner runner);

    /// Returns true if a check has been registered for the given id.
    bool hasCheck(const QString& id) const;

    /// Runs the profile against the document session and returns findings plus
    /// the advertised fixups. Errors (severity "error") cause pass == false.
    PreflightResult run(const QJsonObject& profile);
    PreflightResult run(const PreflightProfileData& profile);

    PDFDocumentSession* getSession() const;

    /// Loads a profile JSON file. On error, returns false and writes a message
    /// to \p errorMessage.
    static bool loadProfile(const QString& profilePath, QJsonObject& profile, QString& errorMessage);

    /// Parses a profile JSON object into the engine's internal struct. On error,
    /// returns false and writes a message to \p errorMessage.
    static bool parseProfile(const QJsonObject& profileObject, PreflightProfileData& profile, QString& errorMessage);

private:
    void registerBuiltInChecks();

    PDFDocumentSession* m_session;
    std::map<QString, CheckRunner> m_checks;
};

} // namespace pdf

#endif // PREFLIGHTENGINE_H
