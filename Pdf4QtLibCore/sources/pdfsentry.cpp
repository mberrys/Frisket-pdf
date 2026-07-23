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

#include "pdfsentry.h"

#include "pdfconstants.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef PDF4QT_ENABLE_SENTRY_IMPL
#include <sentry.h>
#endif

namespace pdf
{

namespace
{

#ifdef PDF4QT_ENABLE_SENTRY_IMPL
int g_activeSentrySessions = 0;
#endif

#ifdef PDF4QT_ENABLE_SENTRY_IMPL
QString resolveDsn()
{
    const QByteArray fromEnvironment = qgetenv("SENTRY_DSN");
    if (!fromEnvironment.isEmpty())
    {
        return QString::fromUtf8(fromEnvironment);
    }

#ifdef PDF4QT_SENTRY_DSN
    return QStringLiteral(PDF4QT_SENTRY_DSN);
#else
    return QString();
#endif
}

QString databasePath()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (basePath.isEmpty())
    {
        basePath = QDir::tempPath();
    }

    QDir databaseDirectory(basePath);
    databaseDirectory.mkpath(QStringLiteral("sentry-native"));
    return databaseDirectory.filePath(QStringLiteral("sentry-native"));
}

QString releaseName(const QString& applicationId)
{
    return QStringLiteral("frisket-%1@%2").arg(applicationId, QString::fromUtf8(PDF4QT_PROJECT_VERSION));
}

double tracesSampleRate()
{
    const QByteArray fromEnvironment = qgetenv("SENTRY_TRACES_SAMPLE_RATE");
    if (!fromEnvironment.isEmpty())
    {
        bool ok = false;
        const double rate = fromEnvironment.toDouble(&ok);
        if (ok && rate >= 0.0 && rate <= 1.0)
        {
            return rate;
        }
    }

    return 0.2;
}

void configureCrashpadHandler(sentry_options_t* options)
{
#ifdef Q_OS_WIN
    const QString handlerPath = QCoreApplication::applicationDirPath() + QStringLiteral("/crashpad_handler.exe");
#else
    const QString handlerPath = QCoreApplication::applicationDirPath() + QStringLiteral("/crashpad_handler");
#endif
    if (QFileInfo::exists(handlerPath))
    {
#ifdef Q_OS_WIN
        sentry_options_set_handler_pathw(options, reinterpret_cast<const wchar_t*>(handlerPath.utf16()));
#else
        sentry_options_set_handler_path(options, handlerPath.toUtf8().constData());
#endif
    }
}
#endif

}   // namespace

PDFSentrySession::PDFSentrySession(const QString& applicationId)
{
#ifdef PDF4QT_ENABLE_SENTRY_IMPL
    const QString dsn = resolveDsn();
    if (dsn.isEmpty())
    {
        return;
    }

    sentry_options_t* options = sentry_options_new();
    sentry_options_set_dsn(options, dsn.toUtf8().constData());
#ifdef Q_OS_WIN
    const QString dbPath = databasePath();
    sentry_options_set_database_pathw(options, reinterpret_cast<const wchar_t*>(dbPath.utf16()));
#else
    sentry_options_set_database_path(options, databasePath().toUtf8().constData());
#endif
    sentry_options_set_release(options, releaseName(applicationId).toUtf8().constData());

    const QByteArray environment = qgetenv("SENTRY_ENVIRONMENT");
    if (!environment.isEmpty())
    {
        sentry_options_set_environment(options, environment.constData());
    }

    const bool debugEnabled = !qEnvironmentVariableIsEmpty("SENTRY_DEBUG");
    sentry_options_set_debug(options, debugEnabled ? 1 : 0);

    configureCrashpadHandler(options);

    sentry_options_set_send_default_pii(options, 0);

    const double tracesSampleRateValue = tracesSampleRate();
    if (tracesSampleRateValue > 0.0)
    {
        sentry_options_set_traces_sample_rate(options, tracesSampleRateValue);
    }

    if (sentry_init(options) == 0)
    {
        m_active = true;
        ++g_activeSentrySessions;
    }
#endif
}

PDFSentrySession::~PDFSentrySession()
{
#ifdef PDF4QT_ENABLE_SENTRY_IMPL
    if (m_active)
    {
        sentry_close();
        m_active = false;
        --g_activeSentrySessions;
    }
#endif
}

bool PDFSentrySession::isGloballyActive()
{
#ifdef PDF4QT_ENABLE_SENTRY_IMPL
    return g_activeSentrySessions > 0;
#else
    return false;
#endif
}

void PDFSentrySession::captureVerificationEvent()
{
#ifdef PDF4QT_ENABLE_SENTRY_IMPL
    sentry_capture_event(sentry_value_new_message_event(
        SENTRY_LEVEL_INFO,
        "custom",
        "It works!"));
    sentry_flush(2000);
#endif
}

PDFSentryTransaction::PDFSentryTransaction(const QString& name, const char* operation)
{
#ifdef PDF4QT_ENABLE_SENTRY_IMPL
    if (!PDFSentrySession::isGloballyActive() || name.isEmpty())
    {
        return;
    }

    sentry_transaction_context_t* transactionContext = sentry_transaction_context_new(
        name.toUtf8().constData(),
        operation);
    sentry_transaction_t* transaction = sentry_transaction_start(transactionContext, sentry_value_new_null());
    if (transaction)
    {
        m_transaction = transaction;
    }
#endif
}

PDFSentryTransaction::~PDFSentryTransaction()
{
#ifdef PDF4QT_ENABLE_SENTRY_IMPL
    if (m_transaction)
    {
        sentry_transaction_finish(static_cast<sentry_transaction_t*>(m_transaction));
        m_transaction = nullptr;
    }
#endif
}

}   // namespace pdf
