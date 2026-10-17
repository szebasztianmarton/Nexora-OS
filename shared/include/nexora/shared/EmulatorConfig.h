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

// How Nexora asks a running emulator to write a save state and starts it from one
// (docs/emulator-integration.md#capabilities). Arguments are explicit lists; placeholders
// are substituted into them, nothing passes through a shell.
struct SaveStateCapability
{
    enum class Adapter {
        ControlSocket, // Nexora line protocol over a per-session Unix socket
        RetroArch, // RetroArch network commands (unverified)
    };
    Adapter adapter = Adapter::ControlSocket;
    int timeoutMs = 10000; // save / quit request timeout
    // Added to the launch when the session uses save states; "{control-socket}" is replaced
    // by the socket path. control-socket only.
    QStringList launchArguments;
    // Added when the game is resumed from a state; "{state-file}" is replaced by its path.
    // control-socket only.
    QStringList resumeArguments;
    int port = 55355; // retroarch only: network command port on 127.0.0.1

    static constexpr int DefaultTimeoutMs = 10000;
    static constexpr int MinTimeoutMs = 1000;
    static constexpr int MaxTimeoutMs = 120000;

    bool operator==(const SaveStateCapability &) const = default;
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
    bool pausable = false; // may be suspended with SIGSTOP and resumed with SIGCONT ("capabilities.pause")
    std::optional<SaveStateCapability> saveState; // "capabilities.saveState"; none = no save states

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
