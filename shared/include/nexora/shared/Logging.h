#pragma once

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcConfig)
Q_DECLARE_LOGGING_CATEGORY(lcProcess)
Q_DECLARE_LOGGING_CATEGORY(lcProcessOutput)
Q_DECLARE_LOGGING_CATEGORY(lcInput)
Q_DECLARE_LOGGING_CATEGORY(lcUi)
Q_DECLARE_LOGGING_CATEGORY(lcLibrary)
Q_DECLARE_LOGGING_CATEGORY(lcSystem)

namespace nexora::shared {

struct LoggingOptions
{
    QString logFilePath; // optional; empty = stderr only
    int ringCapacity = 500; // lines kept in memory for the diagnostics screen
    // The log file is rotated (file.1 .. file.N) when it would exceed this size, so it
    // never grows without bound. 0 disables rotation.
    qint64 maxFileBytes = 2 * 1024 * 1024;
    int keepRotated = 3;
};

// Installs a process-wide Qt message handler writing "time [level] category: message"
// to stderr, an optional log file and an in-memory ring buffer. Filtering follows the
// standard QT_LOGGING_RULES mechanism. Returns false if the log file could not be opened
// (logging to stderr continues in that case).
bool installMessageHandler(const LoggingOptions &options);

// Most recent formatted log lines (oldest first).
QStringList recentLogLines();

} // namespace nexora::shared
