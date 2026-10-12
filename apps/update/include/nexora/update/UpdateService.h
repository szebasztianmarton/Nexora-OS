#pragma once

#include "nexora/update/SignatureVerifier.h"
#include "nexora/update/UpdateManifest.h"
#include "nexora/update/Version.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

class QThread;

namespace nexora::update {
Q_NAMESPACE

// Lifecycle of one update attempt.
//
//   Idle ──checkBundle (ok)────────────► Checking ──compatible──────► Available
//   Idle ──checkBundle (bad/incompat)──► Checking ──bad/incompatible─► Failed
//   Available ──verify()──► Verifying ──hash ok──────────► ReadyToInstall
//                            Verifying ──cancel/mismatch/low space──► Failed
//   ReadyToInstall ──install()──► Installing ──helper "ok"────► RestartRequired
//                                  Installing ──helper "error"/no FIFO reader──► Failed
//   Available / ReadyToInstall / Failed / Completed / RecoveryRequired ──checkBundle──► Checking
//   Idle ──loadPersistedState (new build == running build)──► Completed
//   Idle ──loadPersistedState (helper reported "rolled-back")──► RecoveryRequired
//   Completed / Failed / RecoveryRequired ──checkBundle──► Checking
//
// "Downloading" is reserved for a future network-delivered bundle; this task only
// supports a bundle that is already present as a local directory (e.g. on a USB stick),
// so no code path produces it yet. isValidTransition() still accepts Available <->
// Downloading <-> Verifying/Failed so a later task can wire it in without widening the
// table.
enum class UpdateState {
    Idle,
    Checking,
    Available,
    Downloading,
    Verifying,
    ReadyToInstall,
    Installing,
    RestartRequired,
    Completed,
    Failed,
    RecoveryRequired
};
Q_ENUM_NS(UpdateState)

enum class UpdateError {
    None,
    NoKeys,
    BadManifest,
    BadSignature,
    WrongProduct,
    WrongBoard,
    NotNewer,
    TooOld,
    ImageMissing,
    SizeMismatch,
    HashMismatch,
    InsufficientStorage,
    InstallRequestFailed,
    InstallFailed,
    Cancelled,
    Busy
};
Q_ENUM_NS(UpdateError)

// The explicit transition table the diagram above documents.
bool isValidTransition(UpdateState from, UpdateState to);

QString nameOf(UpdateState state);
QString nameOf(UpdateError error);

// User-facing message for |error|; |detail| (a version, a path, a byte count, …) is
// appended when relevant.
QString describe(UpdateError error, const QString &detail = {});

// Everything UpdateService needs that is not part of the bundle itself.
struct UpdateEnvironment
{
    QString keyDir; // directory of trusted *.pem public keys; see docs/update-system.md
    QString stagingDir; // stateDir/update; holds last-update.json
    Version currentVersion;
    QString currentBuildId;
    QString board;
    QString installRequestFifo = QStringLiteral("/run/nexora/update-request");
    QString installStatusFile = QStringLiteral("/run/nexora/update-status");
    // Free space available at |path|'s filesystem. Default: QStorageInfo(path).bytesAvailable().
    std::function<qint64(const QString &path)> freeBytes;
};

// Verifies and stages a signed update bundle. The actual privileged install (A/B slot
// switch, GRUB boot counting — Task 10) happens out-of-process, requested over
// installRequestFifo; this class never runs anything with elevated privilege itself and
// never writes outside |stagingDir|.
class UpdateService : public QObject
{
    Q_OBJECT

public:
    explicit UpdateService(UpdateEnvironment env, QObject *parent = nullptr);
    ~UpdateService() override;

    UpdateState state() const { return m_state; }
    UpdateError error() const { return m_error; }
    QString message() const { return m_message; }
    double progress() const { return m_progress; }
    std::optional<UpdateManifest> pending() const { return m_pending; }

    // Idle/Available/Failed/Completed/RecoveryRequired -> Checking: reads and verifies
    // manifest.json and its signature, and checks product/board/version compatibility,
    // synchronously (the bundle's small files only) -> Available or Failed. Returns false
    // (state unchanged) only if called while Checking/Verifying/Installing is in progress.
    bool checkBundle(const QString &bundleDir);

    // Available -> Verifying: hashes the image on a worker thread and compares it against
    // the manifest, after checking that stagingDir's filesystem has at least
    // image size + 64 MiB free -> ReadyToInstall. Cancellable with cancel()
    // (-> Failed/Cancelled). Returns false (state unchanged) if not currently Available.
    bool verify();
    void cancel();

    // ReadyToInstall -> Installing: writes one line "install <absolute bundle dir>\n" to
    // installRequestFifo (a non-blocking open; no reader => Failed/InstallRequestFailed).
    // The privileged helper re-verifies everything as root and is polled for the outcome
    // via installStatusFile every 500 ms: "ok <version>" -> RestartRequired,
    // "error <text>" -> Failed/InstallFailed. Returns false (state unchanged) if not
    // currently ReadyToInstall.
    bool install();

    // Reads stagingDir/last-update.json, written by the privileged helper after an install
    // attempt ({"previousBuildId","newBuildId","result"}). If its newBuildId matches the
    // running build, moves to Completed and removes the file, so this is reported only
    // once. If the helper reported result "rolled-back", moves to RecoveryRequired with a
    // message and leaves the file in place. A no-op otherwise (including a missing file).
    // Only has an effect while Idle.
    void loadPersistedState();

signals:
    void stateChanged();
    void progressChanged();

private:
    void setState(UpdateState state, UpdateError error = UpdateError::None, const QString &message = {});
    void startVerifyWorker();
    void onVerifyDone(const std::optional<QByteArray> &sha256, const QString &error, bool cancelled);
    void pollInstallStatus();

    UpdateEnvironment m_env;
    UpdateState m_state = UpdateState::Idle;
    UpdateError m_error = UpdateError::None;
    QString m_message;
    double m_progress = 0.0;
    std::optional<UpdateManifest> m_pending;
    QString m_bundleDir; // absolute; set once checkBundle() succeeds

    QPointer<QThread> m_verifyThread;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QTimer m_installPoll;
};

} // namespace nexora::update
