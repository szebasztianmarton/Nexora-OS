#pragma once

#include "nexora/shared/EmulatorConfig.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <optional>

class QLocalServer;
class QLocalSocket;
class QUdpSocket;

namespace nexora::emulator {

// Where a session's states live and what it starts from.
struct SaveStateLaunch
{
    QString sessionDir; // exists; the adapter may keep working files here
    QString resumeFile; // empty = fresh start; otherwise the state to start from
    QString gamePath; // the game file (RetroArch names state files after it)
};

// Asks a running emulator to write a save state and to quit, and adds the arguments that
// start it from a state (docs/emulator-integration.md#capabilities).
//
// One adapter belongs to one game session. Typical use:
//   prepareLaunch(launch)          -> extra launch arguments  (before EmulatorManager::launch)
//   save(dir)                      -> saveFinished(ok, file, error)
//   requestQuit()                  -> quitFinished(ok, error)
// Requests are asynchronous and time out after the capability's timeoutMs; only one request
// runs at a time (a second one fails at once with "busy"). Nothing passes through a shell.
class SaveStateAdapter : public QObject
{
    Q_OBJECT

public:
    explicit SaveStateAdapter(const shared::SaveStateCapability &capability, QObject *parent = nullptr);

    // Creates the adapter named by |capability|.
    static std::unique_ptr<SaveStateAdapter> create(const shared::SaveStateCapability &capability,
                                                    QObject *parent = nullptr);

    // Prepares a launch. Returns the extra arguments for LaunchRequest::extraArguments, or
    // nullopt with |error| set.
    virtual std::optional<QStringList> prepareLaunch(const SaveStateLaunch &launch, QString *error) = 0;
    // Asks the emulator to write a state into |directory| (exists). On success the file is a
    // new, non-empty regular file inside |directory|; existing files there are never
    // overwritten by the adapter.
    virtual void save(const QString &directory) = 0;
    // Asks the emulator to quit on its own (it may save its own data). quitFinished(true)
    // means the request was delivered and acknowledged where the protocol has an answer;
    // the caller still waits for the process to exit and falls back to SIGTERM.
    virtual void requestQuit() = 0;
    // Releases sockets after the process ended.
    virtual void close() = 0;
    // Whether save() can work now (control-socket: the emulator has connected).
    virtual bool isReady() const = 0;

    bool isBusy() const { return m_busy; }
    const shared::SaveStateCapability &capability() const { return m_capability; }
    static QString adapterName(shared::SaveStateCapability::Adapter adapter);

signals:
    void saveFinished(bool ok, const QString &file, const QString &error);
    void quitFinished(bool ok, const QString &error);
    void readyChanged(bool ready);

protected:
    // Starts the request timer; false (and the matching *Finished(false) emitted by the
    // caller) when another request is running.
    bool begin();
    void endSave(bool ok, const QString &file, const QString &error);
    void endQuit(bool ok, const QString &error);
    // Checks that |file| is a non-empty regular file directly or indirectly inside |directory|.
    static bool checkStateFile(const QString &file, const QString &directory, QString *error);

    QTimer m_timeout;

private:
    shared::SaveStateCapability m_capability;
    bool m_busy = false;
};

// Nexora's own line protocol over a Unix socket, one per session.
//
// prepareLaunch() listens on a fresh socket (owner-only permissions) and substitutes its path
// for "{control-socket}" in launchArguments, plus resumeArguments with "{state-file}" when
// resuming. The emulator (or a wrapper script) connects once and answers, one line each:
//   SAVE <directory>  ->  OK <state file>  |  ERR <message>
//   QUIT              ->  OK               (then exits)
// Lines are UTF-8, at most 4096 bytes. Further connections are refused.
class ControlSocketAdapter : public SaveStateAdapter
{
    Q_OBJECT

public:
    explicit ControlSocketAdapter(const shared::SaveStateCapability &capability, QObject *parent = nullptr);
    ~ControlSocketAdapter() override;

    std::optional<QStringList> prepareLaunch(const SaveStateLaunch &launch, QString *error) override;
    void save(const QString &directory) override;
    void requestQuit() override;
    void close() override;
    bool isReady() const override;
    QString socketPath() const;

    static constexpr int MaxLineBytes = 4096;

private:
    enum class Pending { None, Save, Quit };
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onTimeout();
    void fail(const QString &error);
    bool send(const QByteArray &line);

    QLocalServer *m_server = nullptr;
    QPointer<QLocalSocket> m_client;
    QByteArray m_buffer;
    Pending m_pending = Pending::None;
    QString m_saveDirectory;
};

// RetroArch network commands (UNVERIFIED: written from RetroArch's documentation and only
// tested against a stand-in UDP listener until RetroArch is integrated).
//
// prepareLaunch() uses <sessionDir>/retroarch as RetroArch's savestate_directory and writes
// <sessionDir>/nexora-retroarch.cfg (network commands on 127.0.0.1:port, slot 0, no automatic
// save/load, no quit confirmation), adding "--appendconfig <file>". Resuming copies the resume
// file to retroarch/<game base name>.state and adds "--entryslot 0". save() sends SAVE_STATE,
// waits until a *.state file in retroarch/ appears or changes and stops growing, then copies
// it to a new file in the requested directory (RetroArch overwrites its slot file in place, so
// the copy keeps earlier states intact). requestQuit() sends QUIT (RetroArch does not answer).
class RetroArchAdapter : public SaveStateAdapter
{
    Q_OBJECT

public:
    explicit RetroArchAdapter(const shared::SaveStateCapability &capability, QObject *parent = nullptr);
    ~RetroArchAdapter() override;

    std::optional<QStringList> prepareLaunch(const SaveStateLaunch &launch, QString *error) override;
    void save(const QString &directory) override;
    void requestQuit() override;
    void close() override;
    bool isReady() const override { return m_prepared; }

    static QString configFileName() { return QStringLiteral("nexora-retroarch.cfg"); }
    QString workDirectory() const { return m_stateDir; }

private:
    struct Snapshot
    {
        QString file;
        qint64 size = -1;
        QDateTime modified;
    };
    static Snapshot newestState(const QString &directory);
    bool sendCommand(const QByteArray &command, QString *error);
    void poll();

    QUdpSocket *m_socket = nullptr;
    QTimer m_poll;
    bool m_prepared = false;
    QString m_stateDir; // RetroArch's savestate_directory
    QString m_saveDirectory; // where save() copies the state
    Snapshot m_before;
    Snapshot m_last;
};

} // namespace nexora::emulator
