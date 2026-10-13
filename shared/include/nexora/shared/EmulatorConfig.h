#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace nexora::shared {

// A named variant of how an emulator is started (e.g. "Fullscreen", "Debug").
struct LaunchProfile
{
    QString id;
    QString name;
    QString emulatorId;
    QStringList arguments; // replaces the emulator's arguments when hasArguments is set
    bool hasArguments = false;
    QString workingDirectory; // absolute; empty = emulator's working directory
    bool enabled = true;

    bool operator==(const LaunchProfile &) const = default;
};

// One validated emulator definition, loaded from a single JSON file.
struct EmulatorConfig
{
    QString id;
    QString name;
    QString executable; // absolute, cleaned path; never resolved through PATH
    QStringList arguments;
    bool enabled = true;
    QString sourceFile; // JSON file this entry was loaded from (diagnostics only)
    QString workingDirectory; // absolute; empty = the executable's directory
    QStringList extensions; // lower-case game file extensions handled, e.g. ".sfc"
    std::optional<int> stopTimeoutMs; // graceful-stop timeout before SIGKILL
    QList<LaunchProfile> profiles;
    QString version; // emulator version for display; free text, at most 64 characters
    QStringList systems; // ids of the game systems this emulator can run
    bool pausable = false; // may be suspended with SIGSTOP and resumed with SIGCONT

    static EmulatorConfig make(const QString &id, const QString &name, const QString &executable,
                               const QStringList &arguments = {}, bool enabled = true)
    {
        EmulatorConfig c;
        c.id = id;
        c.name = name;
        c.executable = executable;
        c.arguments = arguments;
        c.enabled = enabled;
        return c;
    }

    const LaunchProfile *profile(const QString &profileId) const
    {
        for (const auto &p : profiles) {
            if (p.id == profileId)
                return &p;
        }
        return nullptr;
    }

    bool operator==(const EmulatorConfig &) const = default;
};

} // namespace nexora::shared
