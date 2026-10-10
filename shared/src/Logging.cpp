#include "nexora/shared/Logging.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>
#include <deque>
#include <memory>

Q_LOGGING_CATEGORY(lcApp, "nexora.app")
Q_LOGGING_CATEGORY(lcConfig, "nexora.config")
Q_LOGGING_CATEGORY(lcProcess, "nexora.process")
Q_LOGGING_CATEGORY(lcProcessOutput, "nexora.process.output")
Q_LOGGING_CATEGORY(lcInput, "nexora.input")
Q_LOGGING_CATEGORY(lcUi, "nexora.ui")
Q_LOGGING_CATEGORY(lcLibrary, "nexora.library")
Q_LOGGING_CATEGORY(lcSystem, "nexora.system")

namespace nexora::shared {

namespace {

struct LogState
{
    QMutex mutex;
    std::unique_ptr<QFile> file;
    std::deque<QString> ring;
    int ringCapacity = 500;
};

LogState &state()
{
    static LogState instance;
    return instance;
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "debug";
    case QtInfoMsg: return "info";
    case QtWarningMsg: return "warning";
    case QtCriticalMsg: return "critical";
    case QtFatalMsg: return "fatal";
    }
    return "unknown";
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString line = QStringLiteral("%1 [%2] %3: %4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)),
                                  QString::fromLatin1(context.category ? context.category : "default"),
                                  message);
    const QByteArray bytes = line.toUtf8() + '\n';

    LogState &s = state();
    {
        QMutexLocker locker(&s.mutex);
        std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr);
        std::fflush(stderr);
        if (s.file) {
            s.file->write(bytes);
            s.file->flush();
        }
        // Emulator output is captured separately; keep it from evicting application lines.
        if (qstrcmp(context.category, "nexora.process.output") != 0) {
            s.ring.push_back(line);
            while (int(s.ring.size()) > s.ringCapacity)
                s.ring.pop_front();
        }
    }

    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

bool installMessageHandler(const LoggingOptions &options)
{
    bool ok = true;
    {
        LogState &s = state();
        QMutexLocker locker(&s.mutex);
        s.ringCapacity = qMax(1, options.ringCapacity);
        s.file.reset();
        if (!options.logFilePath.isEmpty()) {
            auto file = std::make_unique<QFile>(options.logFilePath);
            if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
                s.file = std::move(file);
            else
                ok = false;
        }
    }
    qInstallMessageHandler(messageHandler);
    if (!ok)
        qCWarning(lcApp) << "Cannot open log file" << options.logFilePath << "- logging to stderr only";
    return ok;
}

QStringList recentLogLines()
{
    LogState &s = state();
    QMutexLocker locker(&s.mutex);
    return QStringList(s.ring.cbegin(), s.ring.cend());
}

} // namespace nexora::shared
