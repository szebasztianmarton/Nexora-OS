#pragma once

#include "NotificationService.h"

#include "nexora/emulator/EmulatorManager.h"
#include "nexora/emulator/SaveStateAdapter.h"
#include "nexora/library/GameEntry.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <memory>
#include <optional>

namespace nexora::shared {
class Settings;
}

namespace nexora::ui {

class AppController;

// Quick Resume: game sessions on top of EmulatorManager (docs/quick-resume.md, ADR-001).
//
// At most one emulator process runs. "Suspend" asks the emulator, through its save-state
// adapter, to write a state, verifies it (non-empty, SHA-256 recorded), then lets the
// emulator quit; "resume" starts the game again from that state. Every session with a state
// is a directory <statesDir>/<session id>/ holding the state files and meta.json; there is
// no other index, so a lost or damaged meta.json only affects that one session. One session
// per game: starting a game that has a state resumes it.
//
// Safety rules: a failed save never stops the game; a state is only removed by an explicit
// removeSession() (the UI asks first) or when a newer state of the same session has been
// verified and recorded (the newest two are kept); limits refuse a suspend, they never
// delete anything.
class GameSessionManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    // Saved sessions, newest first: {id, gameId, title, emulatorName, savedAt (local time
    // text), bytes, note, problem ("" = resumable), old (older than sessions.maxAgeDays)}.
    Q_PROPERTY(QVariantList suspended READ suspended NOTIFY sessionsChanged)
    // The running session: {} or {id, gameId, title, state, canSuspend, reason}.
    Q_PROPERTY(QVariantMap active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool canSuspend READ canSuspend NOTIFY activeChanged)
    // Why the running game cannot be suspended ("" when it can or nothing runs).
    Q_PROPERTY(QString suspendUnavailableReason READ suspendUnavailableReason NOTIFY activeChanged)
    // Saving, closing or resuming: the UI shows progress and offers nothing else.
    Q_PROPERTY(bool working READ working NOTIFY activeChanged)

public:
    enum class State {
        Starting,
        Running,
        SuspendedInMemory, // paused with SIGSTOP from the quick menu
        SavingState,
        Closing, // state saved, waiting for the emulator to quit
        Resuming, // starting from a state
        Failed, // the process ended while a save was being answered
    };
    Q_ENUM(State)

    struct SavedState
    {
        QString file; // file name inside the session directory
        qint64 bytes = 0;
        QString sha256; // hex
        QDateTime savedAt; // UTC
    };

    // One saved session (meta.json).
    struct Record
    {
        QString id; // UUID without braces; also the directory name
        qint64 gameId = 0;
        QString gameTitle;
        QString gamePath;
        qint64 gameSize = 0;
        QDateTime gameModified; // UTC
        QString emulatorId;
        QString emulatorVersion;
        QString profileId;
        QString adapter;
        QDateTime createdAt; // UTC
        QString note; // e.g. the game was played after this state was saved
        QList<SavedState> states; // newest first, at most MaxStatesPerSession
    };

    struct Dependencies
    {
        AppController *app = nullptr;
        shared::Settings *settings = nullptr;
        NotificationService *notifications = nullptr; // optional
        QString statesDir;
    };

    static constexpr int MetaVersion = 1;
    static constexpr int MaxStatesPerSession = 2;
    static constexpr qint64 MaxMetaBytes = 64 * 1024;

    explicit GameSessionManager(const Dependencies &deps, QObject *parent = nullptr);
    ~GameSessionManager() override;

    // Reads every <statesDir>/*/meta.json. Unreadable ones are moved aside (never deleted)
    // and reported; their state files stay where they are.
    void load();

    // Starts |entry| with |emulatorId|. With |preferResume| and a resumable state for the
    // game, resumes it instead; with a state that cannot be resumed, emits resumeFailed and
    // starts nothing. Returns whether a launch was accepted.
    bool startGame(const library::GameEntry &entry, const QString &emulatorId, const QString &profileId,
                   bool applyDefaultProfile, bool preferResume);

    Q_INVOKABLE bool suspendToHome();
    Q_INVOKABLE bool resume(const QString &sessionId);
    // Starts the session's game without its state; the state is kept.
    Q_INVOKABLE bool startFresh(const QString &sessionId);
    // Deletes the session's directory (state files and meta.json). The UI confirms first.
    Q_INVOKABLE bool removeSession(const QString &sessionId);
    Q_INVOKABLE bool isQuickResumeEnabled(qint64 gameId) const;
    Q_INVOKABLE bool setQuickResumeEnabled(qint64 gameId, bool enabled);

    // For a power request from the UI: suspends the running game when possible, otherwise
    // stops it; calls |done| with true once no emulator runs, or false (power request must be
    // dropped) if the state could not be saved; the game then keeps running.
    void closeForPower(std::function<void(bool)> done);
    // Application exit: saves the running game if possible (bounded wait), then stops it.
    void shutdown();

    QVariantList suspended() const;
    QVariantMap active() const;
    bool canSuspend() const { return suspendUnavailableReason().isEmpty() && m_active.has_value(); }
    QString suspendUnavailableReason() const;
    bool working() const;
    bool hasActiveGame() const { return m_active.has_value(); }
    std::optional<State> activeState() const;
    const QList<Record> &records() const { return m_records; }
    QString sessionDirectory(const QString &sessionId) const;
    // Full check before a resume; "" when the session can be resumed.
    QString validateForResume(const Record &record) const;

    static QString stateName(State state);
    static QString sha256Of(const QString &file, QString *error);

signals:
    void sessionsChanged();
    void activeChanged();
    // A resume was refused or the emulator could not start from the state; the state is kept.
    void resumeFailed(const QString &sessionId, const QString &title, const QString &reason);
    // A launch was accepted for |gameId| (the library counts it as played once it runs).
    void launchAccepted(qint64 gameId);

private:
    struct Active
    {
        Record record;
        State state = State::Starting;
        std::unique_ptr<emulator::SaveStateAdapter> adapter;
        QString adapterError; // why save states are unavailable for this run
        bool fromState = false;
        bool saved = false; // a state was written during this run
    };

    Record *findRecord(const QString &sessionId);
    Record *findRecordForGame(qint64 gameId);
    std::optional<shared::EmulatorConfig> emulator(const QString &emulatorId) const;
    QString quickProblem(const Record &record) const;
    bool launch(Record record, const QString &resumeFile, bool applyDefaultProfile);
    void setState(State state);
    void onManagerState(emulator::ProcessState state);
    void onProcessFinished(const emulator::ProcessOutcome &outcome);
    void onSaveFinished(bool ok, const QString &file, const QString &error);
    void onCloseTimeout();
    void finishPower(bool ok);
    bool writeMeta(const Record &record, QString *error) const;
    void storeRecord(const Record &record);
    QString limitProblem() const;
    qint64 totalStateBytes(const QString &exceptSessionId) const;
    void notify(NotificationService::Category category, NotificationService::Severity severity, const QString &text);

    Dependencies m_deps;
    QList<Record> m_records; // newest state first
    std::optional<Active> m_active;
    QTimer m_closeTimer;
    std::function<void(bool)> m_powerDone;
    bool m_shutDown = false;
};

} // namespace nexora::ui
