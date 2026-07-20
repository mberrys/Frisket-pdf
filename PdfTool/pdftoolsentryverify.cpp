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

#include "pdftoolsentryverify.h"

#include "pdfsentry.h"

namespace pdftool
{

static PDFToolSentryVerify s_sentryVerifyApplication;

QString PDFToolSentryVerify::getStandardString(StandardString standardString) const
{
    switch (standardString)
    {
        case Command:
            return QStringLiteral("sentry-verify");

        case Name:
            return PDFToolTranslationContext::tr("Sentry Verify");

        case Description:
            return PDFToolTranslationContext::tr("Send a test event to Sentry to verify SDK configuration.");

        default:
            Q_ASSERT(false);
            break;
    }

    return QString();
}

int PDFToolSentryVerify::execute(const PDFToolOptions& options)
{
    if (!pdf::PDFSentrySession::isGloballyActive())
    {
        PDFConsole::writeError(PDFToolTranslationContext::tr("Sentry is not active. Set SENTRY_DSN (or PDF4QT_SENTRY_DSN at build time) and retry."),
                               options.outputCodec);
        return ErrorInvalidArguments;
    }

    pdf::PDFSentrySession::captureVerificationEvent();
    {
        const pdf::PDFSentryTransaction verificationTransaction(QStringLiteral("sentry-verify"));
        PDFConsole::writeText(QStringLiteral("Sentry verification event sent."), options.outputCodec);
    }
    return ExitSuccess;
}

PDFToolAbstractApplication::Options PDFToolSentryVerify::getOptionsFlags() const
{
    return ConsoleFormat;
}

}   // namespace pdftool
