#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRecursiveMutex>
#include <QMutexLocker>

#include <cstdio>

namespace nexora {

namespace {

constexpr qint64 MaxLogSize = 2 * 1024 * 1024;
constexpr int MaxRecentErrors = 50;

struct LoggerState
{
    QRecursiveMutex mutex; // recursive: a Qt warning raised while writing must not deadlock
    QString directory;
    QFile file;
    QStringList recent;
    QtMessageHandler previousHandler = nullptr;
    bool installed = false;
};

LoggerState &state()
{
    static LoggerState s;
    return s;
}

QString filePathFor(const QString &directory)
{
    return directory.isEmpty() ? QString() : directory + QStringLiteral("/nexora.log");
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "INFO";
}

// Caller holds the mutex.
void openLogFile(LoggerState &s)
{
    s.file.close();
    s.file.setFileName(filePathFor(s.directory));
    if (!s.file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        std::fprintf(stderr, "nexora: cannot open log file %s\n", qPrintable(s.file.fileName()));
}

// Caller holds the mutex.
void rotateIfNeeded(LoggerState &s)
{
    if (!s.file.isOpen() || s.file.size() < MaxLogSize)
        return;
    const QString path = s.file.fileName();
    s.file.close();
    QFile::remove(path + QStringLiteral(".1"));
    QFile::rename(path, path + QStringLiteral(".1"));
    openLogFile(s);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    LoggerState &s = state();
    const QString line = QStringLiteral("%1 [%2] %3%4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QLatin1String(levelName(type)),
                                  context.category && qstrcmp(context.category, "default") != 0
                                      ? QStringLiteral("%1: ").arg(QLatin1String(context.category))
                                      : QString(),
                                  message);
    {
        QMutexLocker lock(&s.mutex);
        const QByteArray bytes = line.toUtf8() + '\n';
        std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr);
        std::fflush(stderr);
        if (s.file.isOpen()) {
            rotateIfNeeded(s);
            s.file.write(bytes);
            s.file.flush();
        }
        if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
            s.recent.append(line);
            while (s.recent.size() > MaxRecentErrors)
                s.recent.removeFirst();
        }
    }
    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

void Logger::init(const QString &logDirectory)
{
    LoggerState &s = state();
    {
        QMutexLocker lock(&s.mutex);
        s.directory = QDir(logDirectory).absolutePath();
        QDir().mkpath(s.directory);
        openLogFile(s);
        rotateIfNeeded(s);
    }
    if (!s.installed) {
        s.previousHandler = qInstallMessageHandler(messageHandler);
        s.installed = true;
    }
}

void Logger::shutdown()
{
    LoggerState &s = state();
    if (s.installed) {
        qInstallMessageHandler(s.previousHandler);
        s.installed = false;
    }
    QMutexLocker lock(&s.mutex);
    s.file.close();
}

QString Logger::logFilePath()
{
    LoggerState &s = state();
    QMutexLocker lock(&s.mutex);
    return filePathFor(s.directory);
}

QString Logger::logDirectory()
{
    LoggerState &s = state();
    QMutexLocker lock(&s.mutex);
    return s.directory;
}

QStringList Logger::recentErrors()
{
    LoggerState &s = state();
    QMutexLocker lock(&s.mutex);
    return s.recent;
}

void Logger::clearRecentErrors()
{
    LoggerState &s = state();
    QMutexLocker lock(&s.mutex);
    s.recent.clear();
}

QStringList Logger::tail(int maxLines)
{
    if (maxLines <= 0)
        return {};
    QString path;
    {
        LoggerState &s = state();
        QMutexLocker lock(&s.mutex);
        if (s.file.isOpen())
            s.file.flush();
        path = filePathFor(s.directory);
    }
    if (path.isEmpty())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    // The file is capped at ~2 MiB by rotation, so reading it whole is fine.
    QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.constLast().isEmpty())
        lines.removeLast();
    if (lines.size() > maxLines)
        lines = lines.mid(lines.size() - maxLines);
    return lines;
}

} // namespace nexora
