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

#include "pdftoolabstractapplication.h"
#include "pdfconstants.h"
#include "pdfsentry.h"

#include <QDir>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QStringList>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace
{

QString executableDirectory(const char* argv0)
{
#if defined(Q_OS_WIN)
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        return QFileInfo(QString::fromWCharArray(modulePath, int(length))).absolutePath();
    }
#endif

    const QFileInfo argvInfo(QString::fromLocal8Bit(argv0));
    if (argvInfo.isAbsolute())
    {
        return argvInfo.absolutePath();
    }

    // Bare argv[0] (PATH lookup) is not the process CWD — fall back to CWD only
    // after absolute-path resolution fails so plugin dirs still work for local runs.
    return QDir::currentPath();
}

} // namespace

int main(int argc, char *argv[])
{
    // Prefer offscreen when requested; ensure the exe dir is searched for plugins
    // (platforms/qoffscreen.dll) before QGuiApplication constructs the QPA plugin.
    QCoreApplication::setLibraryPaths(QStringList{ executableDirectory(argv[0]) }
                                      + QCoreApplication::libraryPaths());

    QGuiApplication a(argc, argv);
    QCoreApplication::setOrganizationName("MelkaJ");
    QCoreApplication::setApplicationName("PdfTool");
    QCoreApplication::setApplicationVersion(pdf::PDF_LIBRARY_VERSION);

    const pdf::PDFSentrySession sentrySession(QStringLiteral("pdftool"));

    QStringList arguments = QCoreApplication::arguments();

    QCommandLineParser parser;
    parser.setApplicationDescription("PdfTool - work with pdf documents via command line");
    parser.addPositionalArgument("command", "Command to execute.");
    parser.parse(arguments);

    QStringList positionalArguments = parser.positionalArguments();
    QString command = !positionalArguments.isEmpty() ? positionalArguments.front() : QString();
    arguments.removeOne(command);

    pdftool::PDFToolAbstractApplication* application = pdftool::PDFToolApplicationStorage::getApplicationByCommand(command);
    if (!application)
    {
        application = pdftool::PDFToolApplicationStorage::getDefaultApplication();
    }
    else
    {
        parser.clearPositionalArguments();
    }

    application->initializeCommandLineParser(&parser);

    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(arguments);

    const QString sentryCommand = command.isEmpty() ? QStringLiteral("help") : command;
    const pdf::PDFSentryTransaction sentryTransaction(sentryCommand, "pdftool.command");

    return application->execute(application->getOptions(&parser));
}
