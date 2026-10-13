#pragma once

#include "nexora/shared/EmulatorConfig.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace nexora::shared {

class SystemCatalog;

struct ConfigIssue
{
    enum class Severity { Warning, Error };

    Severity severity = Severity::Error;
    QString file;
    QString message;

    // "file: message" with the file reduced to its name, suitable for the UI.
    QString toDisplayString() const;
};

struct ConfigLoadResult
{
    QList<EmulatorConfig> emulators; // valid entries (enabled and disabled), sorted by file name
    QList<ConfigIssue> issues;
    bool directoryFound = false;

    bool hasErrors() const;
};

// Loads emulator definitions: one JSON object per *.json file in a directory.
//
// Schema:
//   id          string, required, ^[a-z0-9][a-z0-9_-]{0,63}$, unique across files
//   name        string, required, non-empty
//   executable  string, required; relative paths resolve against the config file's
//               directory (no PATH lookup)
//   arguments   array of strings, optional (default [])
//   enabled     bool, optional (default true)
//   workingDirectory  string, optional; relative to the config file's directory
//   extensions  array of ".ext" strings, optional; game files this emulator handles
//   stopTimeoutMs     integer 500..60000, optional; graceful-stop timeout before SIGKILL
//   version     string, optional, at most 64 characters; emulator version for display
//   systems     array of system ids, optional; ids missing from the catalog (when one is
//               given) or not shaped like a system id are dropped with a warning
//   pausable    bool, optional (default false); the emulator may be suspended (SIGSTOP/SIGCONT)
//   profiles    array of {id, name, emulator?, arguments?, workingDirectory?, enabled?},
//               optional; ids unique per emulator, "emulator" must equal the emulator id
// Unknown keys produce a warning and are otherwise ignored.
class ConfigLoader
{
public:
    static constexpr qint64 MaxFileSize = 256 * 1024;
    static constexpr int MinStopTimeoutMs = 500;
    static constexpr int MaxStopTimeoutMs = 60000;
    static constexpr int MaxProfiles = 32;
    static constexpr int MaxVersionLength = 64;

    // |catalog| (optional) is used to check "systems" ids; without it they are only
    // checked for syntax.
    static ConfigLoadResult loadDirectory(const QString &directory, const SystemCatalog *catalog = nullptr);

    // Parses and validates a single definition. Returns std::nullopt on error and
    // appends human-readable messages to |errors|; non-fatal notes go to |warnings|.
    static std::optional<EmulatorConfig> parse(const QByteArray &json, const QString &sourceFile, QStringList *errors,
                                               QStringList *warnings = nullptr, const SystemCatalog *catalog = nullptr);

    static bool isValidId(const QString &id);
};

} // namespace nexora::shared
