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

#include "preflightsidecarutils.h"

#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>

class PreflightPluginTest : public QObject
{
    Q_OBJECT

private slots:
    void resolveBundlePath_combinesApplicationAndRelativePaths();
    void isExpectedPreflightExitCode_acceptsPassAndFindings();
    void isNormalizedReport_requiresTheSidecarContract();
    void isNormalizedReport_acceptsFixupParams();
    void isNormalizedReport_acceptsSchemaV2ScopeFixtures();
    void isNormalizedReport_acceptsSchemaV3InspectionIncompletePass();
    void isNormalizedReport_rejectsInvalidScopeCombinations();
    void findingHasVisualOverlay_respectsScopeAndBbox();
    void filterAdvertisedFixups_removesUnimplementedFixups();
    void isImplementedFixupId_onlyAdvertisesAddBleed();
    void sidecarStreamBuffer_spillsToDiskAboveWatermark();
    void sidecarStreamBuffer_rejectsOverflowBeyondMax();
    void sidecarStreamBuffer_spillRoundTripsContent();
};

namespace
{

QJsonObject documentScopeFinding()
{
    return QJsonObject{
        { QStringLiteral("scope"), QStringLiteral("document") },
        { QStringLiteral("type"), QStringLiteral("encrypted") },
        { QStringLiteral("severity"), QStringLiteral("error") },
        { QStringLiteral("message"), QStringLiteral("Document is password-protected and cannot be fully inspected") },
        { QStringLiteral("check_id"), QStringLiteral("document-access") }
    };
}

QJsonObject pageScopeFinding()
{
    return QJsonObject{
        { QStringLiteral("scope"), QStringLiteral("page") },
        { QStringLiteral("page"), 2 },
        { QStringLiteral("type"), QStringLiteral("color-mode") },
        { QStringLiteral("severity"), QStringLiteral("error") },
        { QStringLiteral("message"), QStringLiteral("Disallowed color space(s) found on page 2: DeviceRGB (allowed: CMYK, Grayscale)") },
        { QStringLiteral("check_id"), QStringLiteral("color-mode") }
    };
}

QJsonObject objectScopeFinding()
{
    return QJsonObject{
        { QStringLiteral("scope"), QStringLiteral("object") },
        { QStringLiteral("page"), 1 },
        { QStringLiteral("object_id"), QStringLiteral("87") },
        { QStringLiteral("type"), QStringLiteral("image-resolution") },
        { QStringLiteral("severity"), QStringLiteral("warning") },
        { QStringLiteral("message"), QStringLiteral("Image effective DPI 180 is below min_dpi 300") },
        { QStringLiteral("bbox"), QJsonArray{ 72.0, 400.0, 540.0, 720.0 } },
        { QStringLiteral("check_id"), QStringLiteral("image-resolution") }
    };
}

QJsonObject scopeFixtureReport(const QJsonObject& finding, bool pass)
{
    QJsonArray errors;
    if (!pass)
    {
        errors.append(finding);
    }

    QJsonArray warnings;
    if (pass)
    {
        warnings.append(finding);
    }

    return QJsonObject{
        { QStringLiteral("schema_version"), 2 },
        { QStringLiteral("pass"), pass },
        { QStringLiteral("profile"), QStringLiteral("Frisket Default") },
        { QStringLiteral("errors"), errors },
        { QStringLiteral("warnings"), warnings },
        { QStringLiteral("fixups_available"), QJsonArray() }
    };
}

} // namespace

void PreflightPluginTest::resolveBundlePath_combinesApplicationAndRelativePaths()
{
    QCOMPARE(pdfplugin::preflight::resolveBundlePath(QStringLiteral("/bundle/app"), QStringLiteral("../share/frisket/profiles/frisket-default.json")),
             QStringLiteral("/bundle/share/frisket/profiles/frisket-default.json"));
}

void PreflightPluginTest::isExpectedPreflightExitCode_acceptsPassAndFindings()
{
    QVERIFY(pdfplugin::preflight::isExpectedPreflightExitCode(0));
    QVERIFY(pdfplugin::preflight::isExpectedPreflightExitCode(1));
    QVERIFY(!pdfplugin::preflight::isExpectedPreflightExitCode(2));
}

void PreflightPluginTest::isNormalizedReport_requiresTheSidecarContract()
{
    QJsonObject report;
    report.insert(QStringLiteral("schema_version"), 1);
    report.insert(QStringLiteral("pass"), true);
    report.insert(QStringLiteral("profile"), QStringLiteral("Frisket Default"));
    report.insert(QStringLiteral("errors"), QJsonArray());
    report.insert(QStringLiteral("warnings"), QJsonArray());
    report.insert(QStringLiteral("fixups_available"), QJsonArray());

    QVERIFY(pdfplugin::preflight::isNormalizedReport(report));
    QVERIFY(!pdfplugin::preflight::isNormalizedReport(QJsonObject()));
    report.insert(QStringLiteral("warnings"), QStringLiteral("not-an-array"));
    QVERIFY(!pdfplugin::preflight::isNormalizedReport(report));
}

void PreflightPluginTest::isNormalizedReport_acceptsFixupParams()
{
    QJsonObject report;
    report.insert(QStringLiteral("schema_version"), 1);
    report.insert(QStringLiteral("pass"), true);
    report.insert(QStringLiteral("profile"), QStringLiteral("Frisket Default"));
    report.insert(QStringLiteral("errors"), QJsonArray());
    report.insert(QStringLiteral("warnings"), QJsonArray());

    QJsonObject params;
    params.insert(QStringLiteral("mode"), QStringLiteral("mirror"));
    params.insert(QStringLiteral("amount_pt"), 9.0);

    QJsonObject fixup;
    fixup.insert(QStringLiteral("id"), QStringLiteral("add-bleed"));
    fixup.insert(QStringLiteral("safe"), false);
    fixup.insert(QStringLiteral("description"), QStringLiteral("Extend page boxes / artwork to provide bleed"));
    fixup.insert(QStringLiteral("params"), params);

    QJsonArray fixups;
    fixups.append(fixup);
    report.insert(QStringLiteral("fixups_available"), fixups);

    QVERIFY(pdfplugin::preflight::isNormalizedReport(report));
}

void PreflightPluginTest::isNormalizedReport_acceptsSchemaV2ScopeFixtures()
{
    QVERIFY(pdfplugin::preflight::isNormalizedReport(scopeFixtureReport(documentScopeFinding(), false)));
    QVERIFY(pdfplugin::preflight::isNormalizedReport(scopeFixtureReport(pageScopeFinding(), false)));
    QVERIFY(pdfplugin::preflight::isNormalizedReport(scopeFixtureReport(objectScopeFinding(), true)));
}

void PreflightPluginTest::isNormalizedReport_acceptsSchemaV3InspectionIncompletePass()
{
    QJsonObject report;
    report.insert(QStringLiteral("schema_version"), 3);
    report.insert(QStringLiteral("inspection_complete"), false);
    report.insert(QStringLiteral("pass"), false);
    report.insert(QStringLiteral("profile"), QStringLiteral("Frisket Default"));
    report.insert(QStringLiteral("errors"), QJsonArray());
    report.insert(QStringLiteral("warnings"), QJsonArray());
    report.insert(QStringLiteral("fixups_available"), QJsonArray());
    report.insert(QStringLiteral("checks"), QJsonArray());

    QVERIFY(pdfplugin::preflight::isNormalizedReport(report));
}

void PreflightPluginTest::isNormalizedReport_rejectsInvalidScopeCombinations()
{
    QJsonObject finding = documentScopeFinding();
    finding.insert(QStringLiteral("page"), 1);
    QJsonObject report = scopeFixtureReport(finding, false);

    QVERIFY(!pdfplugin::preflight::isNormalizedReport(report));
}

void PreflightPluginTest::findingHasVisualOverlay_respectsScopeAndBbox()
{
    QVERIFY(!pdfplugin::preflight::findingHasVisualOverlay(documentScopeFinding(), 2));
    QVERIFY(!pdfplugin::preflight::findingHasVisualOverlay(pageScopeFinding(), 2));
    QVERIFY(pdfplugin::preflight::findingHasVisualOverlay(objectScopeFinding(), 2));
}

void PreflightPluginTest::filterAdvertisedFixups_removesUnimplementedFixups()
{
    QJsonArray fixups;
    fixups.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("rgb-to-cmyk") },
        { QStringLiteral("safe"), false },
        { QStringLiteral("description"), QStringLiteral("Convert all RGB colors to CMYK") }
    });
    fixups.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("add-bleed") },
        { QStringLiteral("safe"), false },
        { QStringLiteral("description"), QStringLiteral("Extend page boxes / artwork to provide bleed") }
    });
    fixups.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("downsample-images") },
        { QStringLiteral("safe"), false },
        { QStringLiteral("description"), QStringLiteral("Downsample images above target DPI") }
    });

    QJsonObject report = scopeFixtureReport(pageScopeFinding(), false);
    report.insert(QStringLiteral("fixups_available"), fixups);

    const QJsonObject filtered = pdfplugin::preflight::filterAdvertisedFixups(report);
    const QJsonArray filteredFixups = filtered.value(QStringLiteral("fixups_available")).toArray();
    QCOMPARE(filteredFixups.size(), 1);
    QCOMPARE(filteredFixups.first().toObject().value(QStringLiteral("id")).toString(), QStringLiteral("add-bleed"));
}

void PreflightPluginTest::isImplementedFixupId_onlyAdvertisesAddBleed()
{
    QVERIFY(pdfplugin::preflight::isImplementedFixupId(QStringLiteral("add-bleed")));
    QVERIFY(!pdfplugin::preflight::isImplementedFixupId(QStringLiteral("rgb-to-cmyk")));
    QVERIFY(!pdfplugin::preflight::isImplementedFixupId(QStringLiteral("downsample-images")));
}

void PreflightPluginTest::sidecarStreamBuffer_spillsToDiskAboveWatermark()
{
    pdfplugin::preflight::PreflightSidecarStreamBuffer buffer(8 * 1024 * 1024);
    const QByteArray chunk(512 * 1024, 'a');
    for (int i = 0; i < 4; ++i)
    {
        QCOMPARE(buffer.append(chunk), pdfplugin::preflight::PreflightSidecarStreamBuffer::AppendResult::Ok);
    }

    QVERIFY(buffer.spilledBytes() >= pdfplugin::preflight::PREFLIGHT_SIDECAR_SPILL_WATERMARK_BYTES);
    QCOMPARE(buffer.takeData().size(), 4 * chunk.size());
}

void PreflightPluginTest::sidecarStreamBuffer_rejectsOverflowBeyondMax()
{
    pdfplugin::preflight::PreflightSidecarStreamBuffer buffer(1024);
    const QByteArray chunk(600, 'b');
    QCOMPARE(buffer.append(chunk), pdfplugin::preflight::PreflightSidecarStreamBuffer::AppendResult::Ok);
    QCOMPARE(buffer.append(chunk), pdfplugin::preflight::PreflightSidecarStreamBuffer::AppendResult::Overflow);
    QCOMPARE(buffer.totalSize(), chunk.size());
}

void PreflightPluginTest::sidecarStreamBuffer_spillRoundTripsContent()
{
    pdfplugin::preflight::PreflightSidecarStreamBuffer buffer(8 * 1024 * 1024);
    QByteArray expected;
    expected.reserve(3 * 1024 * 1024);
    for (int i = 0; i < 3; ++i)
    {
        const QByteArray chunk(1024 * 1024, char('A' + i));
        QCOMPARE(buffer.append(chunk), pdfplugin::preflight::PreflightSidecarStreamBuffer::AppendResult::Ok);
        expected.append(chunk);
    }

    QVERIFY(buffer.spilledBytes() > 0);
    QCOMPARE(buffer.takeData(), expected);
}

QTEST_APPLESS_MAIN(PreflightPluginTest)

#include "tst_preflightplugintest.moc"
