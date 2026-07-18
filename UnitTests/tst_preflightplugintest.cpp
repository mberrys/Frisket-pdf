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

class PreflightPluginTest : public QObject
{
    Q_OBJECT

private slots:
    void resolveBundlePath_combinesApplicationAndRelativePaths();
    void isExpectedPreflightExitCode_acceptsPassAndFindings();
    void isNormalizedReport_requiresTheSidecarContract();
    void isNormalizedReport_acceptsFixupParams();
};

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

QTEST_APPLESS_MAIN(PreflightPluginTest)

#include "tst_preflightplugintest.moc"
