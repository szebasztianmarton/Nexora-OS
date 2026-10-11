#pragma once

#include "nexora/shared/Redaction.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace nexora::emulator {

// One finished launch attempt. Deliberately holds the game *title* only: game file paths
// are never written to the history.
struct LaunchRecord
{
    QString emulatorId;
    QString profileId;
    QString gameTitle;
    QDateTime startedAt;
    QDateTime endedAt;
    qint64 pid = 0;
    QString finalState;
    QString error;
    std::optional<int> exitCode;
    bool startupFailure = false;
    QString stderrTail; // at most MaxStderrTail characters, redacted
};

// Bounded, versioned launch log: {"version":1,"records":[...]} (oldest first on disk).
class LaunchHistory
{
public:
    static constexpr int MaxStderrTail = 4096;

    explicit LaunchHistory(QString filePath, int maxRecords = 50,
                           shared::RedactionRules rules = shared::defaultRedactionRules());

    // Redacts and bounds the record, then rewrites the file atomically keeping the newest
    // maxRecords. A corrupt existing file is moved aside first.
    bool append(const LaunchRecord &record, QString *error);

    // Newest first. A corrupt file is moved aside and yields an empty list.
    QList<LaunchRecord> records() const;

private:
    QString m_path;
    int m_maxRecords;
    shared::RedactionRules m_rules;
};

} // namespace nexora::emulator
