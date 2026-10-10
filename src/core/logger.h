#pragma once

#include <QString>
#include <QStringList>

namespace nexora {

// Process-wide logging. Installs a Qt message handler that writes every message
// to <logDir>/nexora.log (rotated to nexora.log.1 when it exceeds 2 MiB) and to
// stderr, and keeps the most recent warning/critical messages in memory so the
// Diagnostics screen can show them without re-reading the file.
class Logger
{
public:
    static void init(const QString &logDirectory);
    static void shutdown();

    static QString logFilePath();
    static QString logDirectory();

    // Most recent warnings/errors, newest last, at most `maxEntries` (default cap 50).
    static QStringList recentErrors();
    static void clearRecentErrors();

    // Reads the last `maxLines` lines from the current log file. Never throws;
    // returns an empty list if the file cannot be read.
    static QStringList tail(int maxLines);
};

} // namespace nexora
