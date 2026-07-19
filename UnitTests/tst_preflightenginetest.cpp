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
#include "pdfdocumentbuilder.h"
#include "pdfdocumentsession.h"

#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>

class PreflightEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void parseProfile_rejectsMissingName();
    void parseProfile_rejectsEmptyChecks();
    void run_bleedCheckFailsWhenBoxMissing();
    void run_bleedCheckPassesWhenBoxAdequate();
    void run_unknownCheckIdIsIgnored();
    void run_includesProfileFixups();
    void run_synthesizesAddBleedWhenGapAndNoProfileFixup();
    void run_removesAddBleedWhenNoGap();
    void run_doesNotAdvertiseUnimplementedFixups();
    void run_invalidProfileEmitsDocumentScopeFinding();
    void run_contentBleedWithoutRaster_emitsContentBleedAndNeedsAutoBleed();
    void run_contentBleedRasterConfirm_emitsBleedMarginEmptyAndNeedsAutoBleed();
};

namespace
{

pdf::PDFDocument buildTieredBleedGapPage()
{
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 220, 220));
    builder.setPageTrimBox(page, QRectF(10, 10, 200, 200));

    pdf::PDFPageContentStreamBuilder pageContentStreamBuilder(&builder,
                                                              pdf::PDFContentStreamBuilder::CoordinateSystem::PDF);
    if (QPainter* painter = pageContentStreamBuilder.begin(page))
    {
        painter->fillRect(QRectF(10, 10, 200, 200), Qt::black);
        pageContentStreamBuilder.end(painter);
    }

    return builder.build();
}

QJsonObject tieredBleedProfile(bool rasterConfirm)
{
    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Tiered bleed test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("content-bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("warning") },
        { QStringLiteral("raster_confirm"), rasterConfirm }
    });
    profile.insert(QStringLiteral("checks"), checks);
    return profile;
}

} // namespace

void PreflightEngineTest::parseProfile_rejectsMissingName()
{
    pdf::PreflightEngine engine(nullptr);
    pdf::PreflightProfileData profile;
    QString errorMessage;

    QVERIFY(!engine.parseProfile(QJsonObject(), profile, errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void PreflightEngineTest::parseProfile_rejectsEmptyChecks()
{
    pdf::PreflightEngine engine(nullptr);
    pdf::PreflightProfileData profile;
    QString errorMessage;

    QJsonObject profileObject;
    profileObject.insert(QStringLiteral("name"), QStringLiteral("Test"));
    profileObject.insert(QStringLiteral("checks"), QJsonArray());

    QVERIFY(!engine.parseProfile(profileObject, profile, errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void PreflightEngineTest::run_bleedCheckFailsWhenBoxMissing()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 200, 200));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);

    pdf::PreflightResult result = engine.run(profile);
    QVERIFY(!result.pass);
    QCOMPARE(result.errors.size(), 1);
    QCOMPARE(result.errors.first().type, QStringLiteral("bleed"));
}

void PreflightEngineTest::run_bleedCheckPassesWhenBoxAdequate()
{
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 220, 220));
    builder.setPageTrimBox(page, QRectF(10, 10, 200, 200));
    builder.setPageBleedBox(page, QRectF(0, 0, 220, 220));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);

    pdf::PreflightResult result = engine.run(profile);
    QVERIFY(result.pass);
    QVERIFY(result.errors.isEmpty());
}

void PreflightEngineTest::run_unknownCheckIdIsIgnored()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 200, 200));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("not-a-real-check") },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);

    pdf::PreflightResult result = engine.run(profile);
    QVERIFY(result.pass);
    QVERIFY(result.errors.isEmpty());
}

void PreflightEngineTest::run_includesProfileFixups()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 200, 200));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);
    QJsonArray fixups;
    fixups.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("add-bleed") },
        { QStringLiteral("amount_pt"), 9 }
    });
    profile.insert(QStringLiteral("fixups"), fixups);

    pdf::PreflightResult result = engine.run(profile);
    QCOMPARE(result.fixupsAvailable.size(), 1);
    QCOMPARE(result.fixupsAvailable.first().id, QStringLiteral("add-bleed"));
    QCOMPARE(result.fixupsAvailable.first().amountPt, 9.0);

    const QJsonObject report = result.toJson();
    const QJsonArray reportFixups = report.value(QStringLiteral("fixups_available")).toArray();
    QCOMPARE(reportFixups.size(), 1);
    const QJsonObject params = reportFixups.first().toObject().value(QStringLiteral("params")).toObject();
    QCOMPARE(params.value(QStringLiteral("amount_pt")).toDouble(), 9.0);
    QCOMPARE(params.value(QStringLiteral("mode")).toString(), QStringLiteral("mirror"));
}

void PreflightEngineTest::run_synthesizesAddBleedWhenGapAndNoProfileFixup()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 200, 200));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);

    pdf::PreflightResult result = engine.run(profile);
    QCOMPARE(result.fixupsAvailable.size(), 1);
    QCOMPARE(result.fixupsAvailable.first().id, QStringLiteral("add-bleed"));
    QCOMPARE(result.fixupsAvailable.first().amountPt, 9.0);

    const QJsonObject report = result.toJson();
    const QJsonArray reportFixups = report.value(QStringLiteral("fixups_available")).toArray();
    QCOMPARE(reportFixups.size(), 1);
    const QJsonObject params = reportFixups.first().toObject().value(QStringLiteral("params")).toObject();
    QCOMPARE(params.value(QStringLiteral("amount_pt")).toDouble(), 9.0);
    QCOMPARE(params.value(QStringLiteral("mode")).toString(), QStringLiteral("mirror"));
}

void PreflightEngineTest::run_removesAddBleedWhenNoGap()
{
    pdf::PDFDocumentBuilder builder;
    const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 220, 220));
    builder.setPageTrimBox(page, QRectF(10, 10, 200, 200));
    builder.setPageBleedBox(page, QRectF(0, 0, 220, 220));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);
    QJsonArray fixups;
    fixups.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("add-bleed") },
        { QStringLiteral("amount_pt"), 9 }
    });
    profile.insert(QStringLiteral("fixups"), fixups);

    pdf::PreflightResult result = engine.run(profile);
    QVERIFY(result.pass);
    for (const pdf::PreflightFixupConfig& fixup : result.fixupsAvailable)
    {
        QVERIFY(fixup.id != QStringLiteral("add-bleed"));
    }
}

void PreflightEngineTest::run_doesNotAdvertiseUnimplementedFixups()
{
    pdf::PDFDocumentBuilder builder;
    builder.appendPage(QRectF(0, 0, 200, 200));
    pdf::PDFDocument document = builder.build();

    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    QJsonObject profile;
    profile.insert(QStringLiteral("name"), QStringLiteral("Test"));
    QJsonArray checks;
    checks.append(QJsonObject{
        { QStringLiteral("id"), QStringLiteral("bleed") },
        { QStringLiteral("amount_pt"), 9 },
        { QStringLiteral("severity"), QStringLiteral("error") }
    });
    profile.insert(QStringLiteral("checks"), checks);
    QJsonArray fixups;
    fixups.append(QJsonObject{ { QStringLiteral("id"), QStringLiteral("rgb-to-cmyk") } });
    fixups.append(QJsonObject{ { QStringLiteral("id"), QStringLiteral("add-bleed") }, { QStringLiteral("amount_pt"), 9 } });
    fixups.append(QJsonObject{ { QStringLiteral("id"), QStringLiteral("downsample-images") }, { QStringLiteral("target_dpi"), 300 } });
    profile.insert(QStringLiteral("fixups"), fixups);

    pdf::PreflightResult result = engine.run(profile);
    QCOMPARE(result.fixupsAvailable.size(), 1);
    QCOMPARE(result.fixupsAvailable.first().id, QStringLiteral("add-bleed"));

    const QJsonArray reportFixups = result.toJson().value(QStringLiteral("fixups_available")).toArray();
    QCOMPARE(reportFixups.size(), 1);
    QCOMPARE(reportFixups.first().toObject().value(QStringLiteral("id")).toString(), QStringLiteral("add-bleed"));
}

void PreflightEngineTest::run_invalidProfileEmitsDocumentScopeFinding()
{
    pdf::PreflightEngine engine(nullptr);
    const pdf::PreflightResult result = engine.run(QJsonObject{
        { QStringLiteral("name"), QStringLiteral("Broken") }
    });

    QVERIFY(!result.pass);
    QCOMPARE(result.errors.size(), 1);
    QCOMPARE(result.errors.first().scope, QString::fromLatin1(pdf::PREFLIGHT_FINDING_SCOPE_DOCUMENT));
    QVERIFY(!result.errors.first().bbox.isValid());

    const QJsonObject report = result.toJson();
    QCOMPARE(report.value(QStringLiteral("schema_version")).toInt(), pdf::PREFLIGHT_REPORT_SCHEMA_VERSION);
    const QJsonObject finding = report.value(QStringLiteral("errors")).toArray().at(0).toObject();
    QCOMPARE(finding.value(QStringLiteral("scope")).toString(), QStringLiteral("document"));
    QVERIFY(!finding.contains(QStringLiteral("page")));
    QVERIFY(!finding.contains(QStringLiteral("bbox")));
}

void PreflightEngineTest::run_contentBleedWithoutRaster_emitsContentBleedAndNeedsAutoBleed()
{
    pdf::PDFDocument document = buildTieredBleedGapPage();
    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    const pdf::PreflightResult result = engine.run(tieredBleedProfile(false));
    QVERIFY(result.pass);
    QCOMPARE(result.errors.size(), 0);
    QCOMPARE(result.warnings.size(), 2);

    QCOMPARE(result.warnings.at(0).type, QStringLiteral("content-bleed"));
    QCOMPARE(result.warnings.at(0).checkId, QStringLiteral("content-bleed"));
    QCOMPARE(result.warnings.at(1).type, QStringLiteral("needs-auto-bleed"));
    QCOMPARE(result.fixupsAvailable.size(), 1);
    QCOMPARE(result.fixupsAvailable.first().id, QStringLiteral("add-bleed"));
}

void PreflightEngineTest::run_contentBleedRasterConfirm_emitsBleedMarginEmptyAndNeedsAutoBleed()
{
    pdf::PDFDocument document = buildTieredBleedGapPage();
    pdf::PDFDocumentSession session(&document);
    pdf::PreflightEngine engine(&session);

    const pdf::PreflightResult result = engine.run(tieredBleedProfile(true));
    QVERIFY(result.pass);
    QCOMPARE(result.errors.size(), 0);
    QVERIFY(result.warnings.size() >= 5);

    int bleedMarginEmptyCount = 0;
    bool hasNeedsAutoBleed = false;
    for (const pdf::PreflightFinding& finding : result.warnings)
    {
        if (finding.type == QStringLiteral("bleed-margin-empty"))
        {
            ++bleedMarginEmptyCount;
            QCOMPARE(finding.checkId, QStringLiteral("content-bleed"));
        }
        if (finding.type == QStringLiteral("needs-auto-bleed"))
        {
            hasNeedsAutoBleed = true;
        }
        QVERIFY(finding.type != QStringLiteral("content-bleed"));
    }

    QCOMPARE(bleedMarginEmptyCount, 4);
    QVERIFY(hasNeedsAutoBleed);
    QCOMPARE(result.fixupsAvailable.size(), 1);
    QCOMPARE(result.fixupsAvailable.first().id, QStringLiteral("add-bleed"));
}

QTEST_GUILESS_MAIN(PreflightEngineTest)

#include "tst_preflightenginetest.moc"
