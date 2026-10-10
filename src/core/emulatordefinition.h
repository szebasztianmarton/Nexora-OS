#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace nexora {

struct LaunchProfile
{
    QString id;
    QString name;
    QStringList arguments;   // replaces the emulator's default arguments when chosen
};

enum class EmulatorStatus {
    Unknown = 0,
    Available,
    ExecutableMissing,       // path does not exist
    NotExecutable,           // exists but lacks execute permission / is a directory
    InvalidConfiguration,    // malformed entry (missing id/name/executable, bad types, ...)
    Disabled                 // "enabled": false
};

// One entry of emulators.json. Example:
// {
//   "id": "mock", "name": "Mock Emulator",
//   "executable": "/usr/lib/nexora/mock-emulator",
//   "arguments": ["--fullscreen", "{game}"],
//   "workingDirectory": "", "enabled": true,
//   "platforms": ["Test Platform"], "extensions": ["mock"],
//   "profiles": [{"id": "windowed", "name": "Windowed", "arguments": ["{game}"]}]
// }
// "{game}" in an argument is replaced by the absolute game path, either as a
// whole argument or embedded ("--rom={game}"); each argument stays ONE argv
// element and is never passed through a shell. If a game is launched and no
// argument contains "{game}", the path is appended as the final argument.
// When launching the emulator without a game, arguments containing "{game}"
// are dropped.
struct EmulatorDefinition
{
    QString id;
    QString name;
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    bool enabled = true;
    QStringList platforms;     // only what the config explicitly states
    QStringList extensions;    // lower-case, without dot
    QList<LaunchProfile> profiles;

    EmulatorStatus status = EmulatorStatus::Unknown;
    QStringList validationMessages; // human-readable problems found by validate()
    QStringList configurationMessages; // structural problems found by fromJson(), kept across validate()

    // Parses one JSON entry. Structural problems are reported via
    // validationMessages and status = InvalidConfiguration (the entry is still
    // returned so the UI can show it); `ok` is false only if the entry is so
    // broken it has no usable id.
    static EmulatorDefinition fromJson(const QJsonObject &object, bool *ok = nullptr);

    // Re-checks the filesystem state of `executable`/`workingDirectory` and
    // updates status + validationMessages. Cheap (stat calls only), never runs
    // the executable.
    void validate();

    // Builds the argv list for a launch. Empty profileId = default arguments.
    // Unknown profileId falls back to default arguments.
    QStringList buildArguments(const QString &gamePath, const QString &profileId = {}) const;

    bool canLaunch() const { return status == EmulatorStatus::Available; }
    bool handlesExtension(const QString &extension) const;

    static QString statusKey(EmulatorStatus status);   // "available", "missing", "not-executable", "invalid", "disabled", "unknown"
    static QString statusText(EmulatorStatus status);  // "Available", "Executable missing", ...

    QVariantMap toVariantMap() const; // keys documented in EmulatorRegistry roles
};

} // namespace nexora
