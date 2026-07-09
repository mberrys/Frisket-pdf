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

#include "pdftooladdbleed.h"

#include "pdfbleedfixup.h"
#include "pdfdocumentwriter.h"
#include "pdfoutputformatter.h"

namespace pdftool
{

static PDFToolAddBleed s_toolAddBleedApplication;

QString PDFToolAddBleed::getStandardString(PDFToolAbstractApplication::StandardString standardString) const
{
    switch (standardString)
    {
        case Command:
            return "add-bleed";
        case Name:
            return PDFToolTranslationContext::tr("Add Bleed");
        case Description:
            return PDFToolTranslationContext::tr("Expand page boxes and fill bleed margins by mirroring, repeating, or stretching edge pixels.");
        default:
            Q_ASSERT(false);
            break;
    }
    return QString();
}

static QString modeName(pdf::PDFBleedFixupMode mode)
{
    switch (mode)
    {
        case pdf::PDFBleedFixupMode::Mirror: return QStringLiteral("mirror");
        case pdf::PDFBleedFixupMode::PixelRepeat: return QStringLiteral("pixel-repeat");
        case pdf::PDFBleedFixupMode::Stretch: return QStringLiteral("stretch");
    }
    return QStringLiteral("unknown");
}

static QString sideName(pdf::PDFBleedFixupSide side)
{
    switch (side)
    {
        case pdf::PDFBleedFixupSide::Left: return QStringLiteral("left");
        case pdf::PDFBleedFixupSide::Right: return QStringLiteral("right");
        case pdf::PDFBleedFixupSide::Top: return QStringLiteral("top");
        case pdf::PDFBleedFixupSide::Bottom: return QStringLiteral("bottom");
    }
    return QStringLiteral("unknown");
}

static void writeReport(const PDFToolOptions& options,
                        const pdf::PDFBleedFixupSettings& settings,
                        const pdf::PDFBleedFixupReport& report)
{
    PDFOutputFormatter formatter(options.outputStyle);
    formatter.beginDocument("add-bleed", PDFToolTranslationContext::tr("Add bleed report"));
    formatter.endl();

    formatter.beginTable("settings", PDFToolTranslationContext::tr("Settings"));
    formatter.beginTableHeaderRow("header");
    formatter.writeTableHeaderColumn("key", PDFToolTranslationContext::tr("Key"), Qt::AlignLeft);
    formatter.writeTableHeaderColumn("value", PDFToolTranslationContext::tr("Value"), Qt::AlignLeft);
    formatter.endTableHeaderRow();

    auto writeSetting = [&](const QString& key, const QString& value)
    {
        formatter.beginTableRow("row");
        formatter.writeTableColumn("key", key, Qt::AlignLeft);
        formatter.writeTableColumn("value", value, Qt::AlignLeft);
        formatter.endTableRow();
    };

    writeSetting(QStringLiteral("mode"), modeName(settings.mode));
    writeSetting(QStringLiteral("dpi"), QString::number(settings.dpi));
    writeSetting(QStringLiteral("sample-pixels"), QString::number(settings.samplePixels));
    writeSetting(QStringLiteral("bleed-mm"), QStringLiteral("%1,%2,%3,%4")
                 .arg(settings.bleedMM.left()).arg(settings.bleedMM.top())
                 .arg(settings.bleedMM.right()).arg(settings.bleedMM.bottom()));
    writeSetting(QStringLiteral("force"), settings.force ? QStringLiteral("true") : QStringLiteral("false"));
    writeSetting(QStringLiteral("dry-run"), options.addBleedDryRun ? QStringLiteral("true") : QStringLiteral("false"));
    formatter.endTable();
    formatter.endl();

    formatter.beginTable("pages", PDFToolTranslationContext::tr("Pages"));
    formatter.beginTableHeaderRow("header");
    formatter.writeTableHeaderColumn("page", PDFToolTranslationContext::tr("Page"), Qt::AlignLeft);
    formatter.writeTableHeaderColumn("sides", PDFToolTranslationContext::tr("Sides applied"), Qt::AlignLeft);
    formatter.writeTableHeaderColumn("media", PDFToolTranslationContext::tr("MediaBox"), Qt::AlignLeft);
    formatter.writeTableHeaderColumn("bleed", PDFToolTranslationContext::tr("BleedBox"), Qt::AlignLeft);
    formatter.writeTableHeaderColumn("trim", PDFToolTranslationContext::tr("TrimBox"), Qt::AlignLeft);
    formatter.writeTableHeaderColumn("notes", PDFToolTranslationContext::tr("Notes"), Qt::AlignLeft);
    formatter.endTableHeaderRow();

    auto formatRect = [](const QRectF& rect)
    {
        return QStringLiteral("[%1 %2 %3 %4]")
                .arg(rect.left(), 0, 'f', 2)
                .arg(rect.bottom(), 0, 'f', 2)
                .arg(rect.right(), 0, 'f', 2)
                .arg(rect.top(), 0, 'f', 2);
    };

    for (const pdf::PDFBleedFixupPageReport& page : report.pages)
    {
        QStringList sides;
        for (pdf::PDFBleedFixupSide side : page.sidesApplied)
        {
            sides.append(sideName(side));
        }

        formatter.beginTableRow("page");
        formatter.writeTableColumn("page", QString::number(page.pageIndex + 1), Qt::AlignLeft);
        formatter.writeTableColumn("sides", sides.isEmpty() ? QStringLiteral("-") : sides.join(QStringLiteral(",")), Qt::AlignLeft);
        formatter.writeTableColumn("media", QStringLiteral("%1 -> %2").arg(formatRect(page.originalMediaBox), formatRect(page.newMediaBox)), Qt::AlignLeft);
        formatter.writeTableColumn("bleed", QStringLiteral("%1 -> %2").arg(formatRect(page.originalBleedBox), formatRect(page.newBleedBox)), Qt::AlignLeft);
        formatter.writeTableColumn("trim", QStringLiteral("%1 -> %2").arg(formatRect(page.originalTrimBox), formatRect(page.newTrimBox)), Qt::AlignLeft);
        formatter.writeTableColumn("notes", page.skipReasons.join(QStringLiteral("; ")), Qt::AlignLeft);
        formatter.endTableRow();
    }

    formatter.endTable();
    formatter.endDocument();
    PDFConsole::writeText(formatter.getString(), options.outputCodec);
}

int PDFToolAddBleed::execute(const PDFToolOptions& options)
{
    if (!options.addBleedDryRun && options.addBleedOutputDocument.isEmpty())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Output document file name is not set. Use -o/--output or --dry-run."), options.outputCodec);
        return ErrorInvalidArguments;
    }

    pdf::PDFDocument document;
    if (!readDocument(options, document, nullptr, false))
    {
        return ErrorDocumentReading;
    }

    pdf::PDFBleedFixupSettings settings = options.addBleedSettings;
    if (!options.pageSelectorSelection.isEmpty())
    {
        settings.pageRange = options.pageSelectorSelection;
    }
    else if (!options.pageSelectorFirstPage.isEmpty() || !options.pageSelectorLastPage.isEmpty())
    {
        const QString first = options.pageSelectorFirstPage.isEmpty() ? QStringLiteral("1") : options.pageSelectorFirstPage;
        const QString last = options.pageSelectorLastPage.isEmpty() ? QStringLiteral("-") : options.pageSelectorLastPage;
        settings.pageRange = QStringLiteral("%1-%2").arg(first, last);
    }

    pdf::PDFBleedFixupReport report;
    const pdf::PDFOperationResult result = pdf::PDFBleedFixup::apply(&document, settings, &report);
    if (!result)
    {
        PDFConsole::writeError(result.getErrorMessage(), options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    if (options.addBleedReport || options.addBleedDryRun)
    {
        writeReport(options, settings, report);
    }

    if (options.addBleedDryRun)
    {
        return ExitSuccess;
    }

    pdf::PDFDocumentWriter writer(nullptr);
    const pdf::PDFOperationResult writeResult = writer.write(options.addBleedOutputDocument, &document, true);
    if (!writeResult)
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Failed to write output document. %1").arg(writeResult.getErrorMessage()), options.outputCodec);
        return ErrorFailedWriteToFile;
    }

    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolAddBleed::getOptionsFlags() const
{
    return ConsoleFormat | OpenDocument | PageSelector | ColorManagementSystem | AddBleed;
}

} // namespace pdftool
