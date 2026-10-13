#pragma once

#include "nexora/shared/Redaction.h"
#include "nexora/system/HardwareInfo.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace nexora::system {

struct DiagnosticInputs
{
    QString appVersion;
    QString buildId;
    QString platform;
    HardwareSnapshot hardware;
    QStringList recentLog;
    QStringList configIssues;
    QStringList settingsIssues;
    QStringList libraryIssues;
    QStringList launchSummaries; // one plain-text line per recent launch
};

// Plain text, at most 512 KiB; the oldest log lines are dropped first. Every line goes
// through shared::redact().
QString buildDiagnosticReport(const DiagnosticInputs &in, const shared::RedactionRules &rules);

// Writes <diagnosticsDir>/diagnostics-<timestamp>.txt (0600, created atomically) and keeps
// only the newest 10 reports. Returns the path, or nullopt with |error| set.
std::optional<QString> exportDiagnosticReport(const QString &text, const QString &diagnosticsDir, QString *error);

} // namespace nexora::system
