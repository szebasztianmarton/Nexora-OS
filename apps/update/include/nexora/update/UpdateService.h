#pragma once

#include "nexora/update/SignatureVerifier.h"
#include "nexora/update/UpdateManifest.h"
#include "nexora/update/Version.h"

#include <QDeadlineTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <atomic>
#include <chrono>
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
//                            Verifying ──cancel/mismatch/state dir unusable──► Failed
//   ReadyToInstall ──install()──► Installing ──helper "ok <pending version>"──► RestartRequired
//                                  Installing ──helper "error"/other version/no FIFO reader/timeout──► Failed
//   Available / ReadyToInstall / Failed / Completed / RecoveryRequired ──checkBundle──► Checking
//   Idle ──loadPersistedState (new build == running build)──► Completed
//   Idle ──loadPersistedState (helper reported "rolled-back")──► RecoveryRequired
//   Completed / Failed / RecoveryRequired ──checkBundle──► Checking
//   Completed / RecoveryRequired ──acknowledgeLastResult──► Idle
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
    InstallTimedOut,
    Cancelled,
    Busy,
    UnsafeBundlePath
};
Q_ENUM_NS(UpdateError)

// The explicit transition table the diagram above documents.
bool isValidTransition(UpdateState from, UpdateState to);

QString nameOf(UpdateState state);
QString nameOf(UpdateError error);

// User-facing message for |error|; |detail| (a version, a path, a byte count, …) is
// appended when relevant.
QString describe(UpdateError error, const QString &detail = {});

// Why the root helper (nexora-updated) would refuse |bundleDir| in its request line, or an
// empty string if it accepts it: the same rule (absolute, 2-256 bytes, only
// [A-Za-z0-9._/ -], no "..") plus an explicit check for control characters.
QString bundlePathProblem(const QString &bundleDir);

// Result of verifyBundle(): a parsed and signature-verified manifest, the absolute bundle
// directory it came from, and the image file's absolute path.
struct VerifiedBundle
{
    UpdateManifest manifest;
    QString bundleDir;
    QString imagePath;
};

// The one place that reads a bundle's manifest, verifies its signature, and checks
// product/board/version compatibility and image presence/size. Shared by
// UpdateService::checkBundle() and nexora-update-tool's "verify" command, so a future
// change to this sequence cannot drift between the two.
//
// Reads manifest.json and manifest.json.sig from |bundleDir|, verifies the signature using
// *.pem keys loaded from |keyDir| (fails closed with zero keys), and checks the manifest's
// product against the fixed "nexora-os" product id. |board| and |currentVersion|, when
// given, additionally check board compatibility and that the manifest's version is newer
// than |currentVersion| and not older than its minimumVersion; when either is unset, that
// specific check is skipped (used by the CLI tool, where both flags are optional). Finally
// checks that the image file named in the manifest exists and is the expected size. Does
// NOT hash the image's contents — callers that need the full hash (UpdateService::verify(),
// nexora-update-tool) do that separately via sha256File().
//
// On success returns the parsed manifest, the absolute bundle directory, and the image's
// absolute path. On failure returns std::nullopt and sets *error and *detail (suitable for
// describe()).
std::optional<VerifiedBundle> verifyBundle(const QString &bundleDir, const QString &keyDir,
                                           const std::optional<QString> &board,
                                           const std::optional<Version> &currentVersion, UpdateError *error,
                                           QString *detail);

// Everything UpdateService needs that is not part of the bundle itself.
struct UpdateEnvironment
{
    QString keyDir; // directory of trusted *.pem public keys; see docs/update-system.md
    // stateDir/update; holds last-update.json (written by nexora-bootok). Nothing is staged
    // here: the privileged helper writes the image straight to the inactive slot. Created
    // on demand.
    QString stagingDir;
    Version currentVersion;
    QString currentBuildId;
    QString board;
    QString installRequestFifo = QStringLiteral("/run/nexora/update-request");
    QString installStatusFile = QStringLiteral("/run/nexora/update-status");
    // Installing -> Failed/InstallTimedOut when the helper has not finished by then. Writing
    // a 2 GiB image from slow USB media to the slot takes minutes, not hours.
    std::chrono::milliseconds installTimeout = std::chrono::minutes(15);
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
    // the manifest -> ReadyToInstall. First creates stagingDir if needed and checks that its
    // filesystem can hold the small status/record files (MinimumStateBytes); the image
    // itself is never copied there. Cancellable with cancel()
    // (-> Failed/Cancelled). Returns false (state unchanged) if not currently Available.
    bool verify();
    void cancel();

    // ReadyToInstall -> Installing: writes one line "install <absolute bundle dir>\n" to
    // installRequestFifo (a non-blocking open; no reader => Failed/InstallRequestFailed).
    // A bundle path the helper would refuse (see bundlePathProblem()) fails first with
    // Failed/UnsafeBundlePath, and nothing is written.
    // The privileged helper re-verifies everything as root and is polled for the outcome
    // via installStatusFile every 500 ms. The status file is root-owned and outlives a
    // request (nexora-bootok writes "ok <version>" into it after a confirmed boot), so the
    // UI cannot delete it: install() records its identity (inode, size, timestamps,
    // content) before sending the request, and only a status written after that counts:
    // "installing <slot>" -> progress message, "ok <pending version>" -> RestartRequired,
    // "ok <other version>" or "error <text>" -> Failed/InstallFailed. No answer within
    // installTimeout -> Failed/InstallTimedOut. Returns false (state unchanged) if not
    // currently ReadyToInstall.
    bool install();

    // Reads stagingDir/last-update.json, written by nexora-bootok after a trial boot
    // ({"format","result","previousBuildId","newBuildId","previousVersion","newVersion",
    // "slot"}). If its newBuildId matches the running build, moves to Completed; if the helper
    // reported result "rolled-back", moves to RecoveryRequired with a message. Either way the
    // file stays in place until acknowledgeLastResult(), so a restart does not lose it. A no-op otherwise (including a
    // missing file). Only has an effect while Idle. UiContext calls it once at startup.
    void loadPersistedState();

    // Completed/RecoveryRequired -> Idle: the user has seen the result. Removes
    // last-update.json (if still present) so a restart does not report it again. Returns
    // false (state unchanged) in any other state or if the record cannot be removed.
    bool acknowledgeLastResult();

    // Free space verify() requires in stagingDir's filesystem (status/record files only).
    static constexpr qint64 MinimumStateBytes = qint64(1024) * 1024;

signals:
    void stateChanged();
    void progressChanged();

private:
    void setState(UpdateState state, UpdateError error = UpdateError::None, const QString &message = {});
    void startVerifyWorker();
    void onVerifyDone(const std::optional<QByteArray> &sha256, const QString &error, bool cancelled);
    void pollInstallStatus();
    QString lastUpdatePath() const;

    // Identity of installStatusFile at one point in time; see install().
    struct StatusSnapshot
    {
        bool exists = false;
        quint64 device = 0;
        quint64 inode = 0;
        qint64 size = -1;
        qint64 mtimeNs = 0;
        qint64 ctimeNs = 0;
        QByteArray content;
        bool operator==(const StatusSnapshot &) const = default;
    };
    StatusSnapshot statusSnapshot() const;

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
    StatusSnapshot m_statusBefore; // installStatusFile as it was before the request
    QDeadlineTimer m_installDeadline;
};

} // namespace nexora::update
