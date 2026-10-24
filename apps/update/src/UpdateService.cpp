#include "nexora/update/UpdateService.h"

#include "nexora/shared/AtomicFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QStorageInfo>
#include <QThread>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nexora::update {

namespace {
constexpr const char *ExpectedProduct = "nexora-os";
constexpr qint64 MaxManifestFileBytes = 64 * 1024;
constexpr qint64 SignatureBytes = 64;
constexpr qint64 MaxLastUpdateBytes = 16 * 1024;
constexpr qint64 MaxInstallStatusBytes = 4 * 1024;
} // namespace

bool isValidTransition(UpdateState from, UpdateState to)
{
    using S = UpdateState;
    switch (from) {
    case S::Idle: return to == S::Checking || to == S::Completed || to == S::RecoveryRequired;
    case S::Checking: return to == S::Available || to == S::Failed;
    case S::Available: return to == S::Checking || to == S::Downloading || to == S::Verifying;
    case S::Downloading: return to == S::Verifying || to == S::Failed;
    case S::Verifying: return to == S::ReadyToInstall || to == S::Failed;
    case S::ReadyToInstall: return to == S::Checking || to == S::Installing;
    case S::Installing: return to == S::RestartRequired || to == S::Failed;
    case S::RestartRequired: return false; // terminal: a real install reboots the system
    case S::Completed: return to == S::Checking || to == S::Idle;
    case S::Failed: return to == S::Checking;
    case S::RecoveryRequired: return to == S::Checking || to == S::Idle;
    }
    return false;
}

QString nameOf(UpdateState state)
{
    switch (state) {
    case UpdateState::Idle: return QStringLiteral("Idle");
    case UpdateState::Checking: return QStringLiteral("Checking");
    case UpdateState::Available: return QStringLiteral("Available");
    case UpdateState::Downloading: return QStringLiteral("Downloading");
    case UpdateState::Verifying: return QStringLiteral("Verifying");
    case UpdateState::ReadyToInstall: return QStringLiteral("ReadyToInstall");
    case UpdateState::Installing: return QStringLiteral("Installing");
    case UpdateState::RestartRequired: return QStringLiteral("RestartRequired");
    case UpdateState::Completed: return QStringLiteral("Completed");
    case UpdateState::Failed: return QStringLiteral("Failed");
    case UpdateState::RecoveryRequired: return QStringLiteral("RecoveryRequired");
    }
    return QStringLiteral("Unknown");
}

QString nameOf(UpdateError error)
{
    switch (error) {
    case UpdateError::None: return QStringLiteral("None");
    case UpdateError::NoKeys: return QStringLiteral("NoKeys");
    case UpdateError::BadManifest: return QStringLiteral("BadManifest");
    case UpdateError::BadSignature: return QStringLiteral("BadSignature");
    case UpdateError::WrongProduct: return QStringLiteral("WrongProduct");
    case UpdateError::WrongBoard: return QStringLiteral("WrongBoard");
    case UpdateError::NotNewer: return QStringLiteral("NotNewer");
    case UpdateError::TooOld: return QStringLiteral("TooOld");
    case UpdateError::ImageMissing: return QStringLiteral("ImageMissing");
    case UpdateError::SizeMismatch: return QStringLiteral("SizeMismatch");
    case UpdateError::HashMismatch: return QStringLiteral("HashMismatch");
    case UpdateError::InsufficientStorage: return QStringLiteral("InsufficientStorage");
    case UpdateError::InstallRequestFailed: return QStringLiteral("InstallRequestFailed");
    case UpdateError::InstallFailed: return QStringLiteral("InstallFailed");
    case UpdateError::InstallTimedOut: return QStringLiteral("InstallTimedOut");
    case UpdateError::Cancelled: return QStringLiteral("Cancelled");
    case UpdateError::Busy: return QStringLiteral("Busy");
    case UpdateError::UnsafeBundlePath: return QStringLiteral("UnsafeBundlePath");
    }
    return QStringLiteral("Unknown");
}

QString describe(UpdateError error, const QString &detail)
{
    auto withDetail
        = [&](const QString &text) { return detail.isEmpty() ? text : text + QStringLiteral(": ") + detail; };
    switch (error) {
    case UpdateError::None: return QStringLiteral("No error");
    case UpdateError::NoKeys: return withDetail(QStringLiteral("No trusted update keys are installed"));
    case UpdateError::BadManifest: return withDetail(QStringLiteral("Update manifest is missing or malformed"));
    case UpdateError::BadSignature: return withDetail(QStringLiteral("Update signature is invalid"));
    case UpdateError::WrongProduct: return withDetail(QStringLiteral("Update is for a different product"));
    case UpdateError::WrongBoard: return withDetail(QStringLiteral("Update is for a different board"));
    case UpdateError::NotNewer: return withDetail(QStringLiteral("Update is not newer than the running version"));
    case UpdateError::TooOld:
        return withDetail(QStringLiteral("Running version is too old to apply this update directly"));
    case UpdateError::ImageMissing: return withDetail(QStringLiteral("Update image file is missing"));
    case UpdateError::SizeMismatch: return withDetail(QStringLiteral("Update image size does not match the manifest"));
    case UpdateError::HashMismatch:
        return withDetail(QStringLiteral("Update image checksum does not match the manifest"));
    case UpdateError::InsufficientStorage:
        return withDetail(QStringLiteral("Not enough free storage to install the update"));
    case UpdateError::InstallRequestFailed: return withDetail(QStringLiteral("Could not reach the install helper"));
    case UpdateError::InstallFailed: return withDetail(QStringLiteral("The install helper reported a failure"));
    case UpdateError::InstallTimedOut:
        return withDetail(QStringLiteral("The install helper did not finish in time; the update was not confirmed"));
    case UpdateError::Cancelled: return withDetail(QStringLiteral("Cancelled"));
    case UpdateError::Busy: return withDetail(QStringLiteral("Another update operation is already in progress"));
    case UpdateError::UnsafeBundlePath:
        return withDetail(QStringLiteral("The install helper does not accept the bundle directory path"));
    }
    return withDetail(QStringLiteral("Unknown error"));
}

QString bundlePathProblem(const QString &bundleDir)
{
    // Mirrors handle_request() in nexora-updated, which answers all of these with a bare
    // "error invalid request". Checked on the bytes the request line carries.
    const QByteArray path = QFile::encodeName(bundleDir);
    for (const char c : path) {
        if (uchar(c) < 0x20 || uchar(c) == 0x7f)
            return QStringLiteral("it contains a control character (such as a line break)");
    }
    if (!path.startsWith('/'))
        return QStringLiteral("it is not an absolute path");
    if (path.size() < 2)
        return QStringLiteral("it is the root directory");
    if (path.size() > 256)
        return QStringLiteral("it is longer than 256 bytes (%1)").arg(path.size());
    if (path.contains(".."))
        return QStringLiteral("it contains \"..\"");
    for (const char c : path) {
        const bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.'
            || c == '_' || c == '/' || c == ' ' || c == '-';
        if (!allowed)
            return QStringLiteral("it may contain only letters, digits, spaces and . _ / - (rename or move the "
                                  "bundle directory)");
    }
    return {};
}

std::optional<VerifiedBundle> verifyBundle(const QString &bundleDir, const QString &keyDir,
                                           const std::optional<QString> &board,
                                           const std::optional<Version> &currentVersion, UpdateError *error,
                                           QString *detail)
{
    auto fail = [&](UpdateError e, const QString &d) {
        if (error)
            *error = e;
        if (detail)
            *detail = d;
        return std::optional<VerifiedBundle>();
    };

    const QString dir = QFileInfo(bundleDir).absoluteFilePath();

    QString readError;
    const auto manifestBytes
        = shared::readFileLimited(dir + QStringLiteral("/manifest.json"), MaxManifestFileBytes, &readError);
    if (!manifestBytes)
        return fail(UpdateError::BadManifest, readError);

    QString parseError;
    const auto manifest = UpdateManifest::parse(*manifestBytes, &parseError);
    if (!manifest)
        return fail(UpdateError::BadManifest, parseError);

    QString sigError;
    const auto signature
        = shared::readFileLimited(dir + QStringLiteral("/manifest.json.sig"), SignatureBytes, &sigError);
    if (!signature || signature->size() != SignatureBytes)
        return fail(UpdateError::BadSignature,
                    sigError.isEmpty() ? QStringLiteral("signature must be 64 bytes") : sigError);

    QStringList issues;
    const SignatureVerifier verifier = SignatureVerifier::fromDirectory(keyDir, &issues);
    if (verifier.keyCount() == 0)
        return fail(UpdateError::NoKeys, issues.join(QStringLiteral("; ")));
    if (!verifier.verify(*manifestBytes, *signature))
        return fail(UpdateError::BadSignature, {});

    if (manifest->product != QLatin1String(ExpectedProduct))
        return fail(UpdateError::WrongProduct, manifest->product);
    if (board && manifest->board != *board)
        return fail(UpdateError::WrongBoard, manifest->board);
    if (currentVersion) {
        if (!(*currentVersion < manifest->version))
            return fail(UpdateError::NotNewer, manifest->version.toString());
        if (*currentVersion < manifest->minimumVersion)
            return fail(UpdateError::TooOld, manifest->minimumVersion.toString());
    }

    const QString imagePath = dir + u'/' + manifest->image.file;
    const QFileInfo imageInfo(imagePath);
    if (!imageInfo.isFile())
        return fail(UpdateError::ImageMissing, manifest->image.file);
    if (imageInfo.size() != manifest->image.size)
        return fail(UpdateError::SizeMismatch,
                    QStringLiteral("expected %1 bytes, found %2").arg(manifest->image.size).arg(imageInfo.size()));

    if (error)
        *error = UpdateError::None;
    if (detail)
        detail->clear();
    return VerifiedBundle{*manifest, dir, imagePath};
}

UpdateService::UpdateService(UpdateEnvironment env, QObject *parent)
    : QObject(parent)
    , m_env(std::move(env))
{
    m_installPoll.setInterval(500);
    connect(&m_installPoll, &QTimer::timeout, this, &UpdateService::pollInstallStatus);
}

UpdateService::~UpdateService()
{
    if (m_verifyThread) {
        if (m_cancel)
            m_cancel->store(true);
        m_verifyThread->wait();
        delete m_verifyThread; // finished() handler never ran; avoid touching *this from it
    }
}

void UpdateService::setState(UpdateState state, UpdateError error, const QString &message)
{
    m_state = state;
    m_error = error;
    m_message = message;
    emit stateChanged();
}

bool UpdateService::checkBundle(const QString &bundleDir)
{
    if (!isValidTransition(m_state, UpdateState::Checking))
        return false;

    m_pending.reset();
    m_bundleDir.clear();
    setState(UpdateState::Checking);

    UpdateError error = UpdateError::None;
    QString detail;
    const auto verified = verifyBundle(bundleDir, m_env.keyDir, m_env.board, m_env.currentVersion, &error, &detail);
    if (!verified) {
        setState(UpdateState::Failed, error, describe(error, detail));
        return true; // the check ran; see state()/error() for the outcome
    }

    m_bundleDir = verified->bundleDir;
    m_pending = verified->manifest;
    setState(UpdateState::Available, UpdateError::None,
             QStringLiteral("update %1 available").arg(verified->manifest.version.toString()));
    return true;
}

bool UpdateService::verify()
{
    if (!isValidTransition(m_state, UpdateState::Verifying) || !m_pending)
        return false;

    setState(UpdateState::Verifying, UpdateError::None, QStringLiteral("verifying update image"));
    m_progress = 0.0;
    emit progressChanged();

    // Nothing is staged: the helper writes the image straight from the bundle to the
    // inactive slot. The state directory only has to exist (QStorageInfo reports no space
    // for a missing path) and hold the small status/record files.
    if (m_env.stagingDir.isEmpty() || !QDir().mkpath(m_env.stagingDir)) {
        setState(UpdateState::Failed, UpdateError::InsufficientStorage,
                 describe(UpdateError::InsufficientStorage,
                          QStringLiteral("cannot create the update state directory %1").arg(m_env.stagingDir)));
        return true;
    }
    QFile::setPermissions(m_env.stagingDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const qint64 available
        = m_env.freeBytes ? m_env.freeBytes(m_env.stagingDir) : QStorageInfo(m_env.stagingDir).bytesAvailable();
    if (available < MinimumStateBytes) {
        setState(UpdateState::Failed, UpdateError::InsufficientStorage,
                 describe(UpdateError::InsufficientStorage,
                          QStringLiteral("the data partition is full (%1 bytes free)").arg(available)));
        return true;
    }

    startVerifyWorker();
    return true;
}

void UpdateService::startVerifyWorker()
{
    const QString imagePath = m_bundleDir + u'/' + m_pending->image.file;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_cancel;
    auto result = std::make_shared<std::optional<QByteArray>>();
    auto errorText = std::make_shared<QString>();

    QThread *thread = QThread::create([this, imagePath, cancel, result, errorText] {
        // Queued to the owner's thread; dropped if the service is destroyed meanwhile.
        const auto progress = [this](qint64 done, qint64 total) {
            QMetaObject::invokeMethod(
                this,
                [this, done, total] {
                    m_progress = total > 0 ? double(done) / double(total) : 0.0;
                    emit progressChanged();
                },
                Qt::QueuedConnection);
        };
        QString err;
        *result = sha256File(imagePath, progress, cancel.get(), &err);
        *errorText = err;
    });
    thread->setObjectName(QStringLiteral("nexora-update-verify"));
    connect(thread, &QThread::finished, this, [this, thread, result, errorText, cancel] {
        if (m_verifyThread == thread)
            m_verifyThread = nullptr;
        thread->deleteLater();
        onVerifyDone(*result, *errorText, cancel->load());
    });
    m_verifyThread = thread;
    thread->start(QThread::LowPriority);
}

void UpdateService::onVerifyDone(const std::optional<QByteArray> &sha256, const QString &error, bool cancelled)
{
    if (!m_pending) // defensive: cancel()/checkBundle() raced the worker (should not happen)
        return;
    if (!sha256) {
        if (cancelled)
            setState(UpdateState::Failed, UpdateError::Cancelled, describe(UpdateError::Cancelled));
        else
            setState(UpdateState::Failed, UpdateError::ImageMissing, describe(UpdateError::ImageMissing, error));
        return;
    }
    if (*sha256 != m_pending->image.sha256Hex) {
        setState(UpdateState::Failed, UpdateError::HashMismatch, describe(UpdateError::HashMismatch));
        return;
    }
    m_progress = 1.0;
    emit progressChanged();
    setState(UpdateState::ReadyToInstall, UpdateError::None,
             QStringLiteral("update %1 ready to install").arg(m_pending->version.toString()));
}

void UpdateService::cancel()
{
    if (m_cancel)
        m_cancel->store(true);
}

bool UpdateService::install()
{
    if (!isValidTransition(m_state, UpdateState::Installing) || m_bundleDir.isEmpty())
        return false;

    // The helper would refuse it without saying why; refuse it here, with the reason.
    const QString pathProblem = bundlePathProblem(m_bundleDir);
    if (!pathProblem.isEmpty()) {
        setState(UpdateState::Failed, UpdateError::UnsafeBundlePath,
                 describe(UpdateError::UnsafeBundlePath, pathProblem));
        return true;
    }

    // Before the request: whatever is in the status file now is an earlier answer.
    m_statusBefore = statusSnapshot();
    const int fd = ::open(m_env.installRequestFifo.toLocal8Bit().constData(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        setState(UpdateState::Failed, UpdateError::InstallRequestFailed,
                 describe(UpdateError::InstallRequestFailed, QString::fromLocal8Bit(std::strerror(errno))));
        return true;
    }
    const QByteArray line = "install " + QFile::encodeName(m_bundleDir) + '\n';
    const qint64 written = ::write(fd, line.constData(), size_t(line.size()));
    ::close(fd);
    if (written != line.size()) {
        setState(UpdateState::Failed, UpdateError::InstallRequestFailed,
                 describe(UpdateError::InstallRequestFailed, QStringLiteral("short write to the install FIFO")));
        return true;
    }

    setState(UpdateState::Installing, UpdateError::None,
             QStringLiteral("installing %1").arg(m_pending ? m_pending->version.toString() : QString()));
    m_installDeadline = QDeadlineTimer(m_env.installTimeout);
    m_installPoll.start();
    return true;
}

UpdateService::StatusSnapshot UpdateService::statusSnapshot() const
{
    StatusSnapshot snapshot;
    struct ::stat st
    { };
    if (::stat(QFile::encodeName(m_env.installStatusFile).constData(), &st) != 0)
        return snapshot;
    snapshot.exists = true;
    snapshot.device = quint64(st.st_dev);
    snapshot.inode = quint64(st.st_ino);
    snapshot.size = qint64(st.st_size);
    snapshot.mtimeNs = qint64(st.st_mtim.tv_sec) * 1000000000 + qint64(st.st_mtim.tv_nsec);
    snapshot.ctimeNs = qint64(st.st_ctim.tv_sec) * 1000000000 + qint64(st.st_ctim.tv_nsec);
    QString readError;
    snapshot.content
        = shared::readFileLimited(m_env.installStatusFile, MaxInstallStatusBytes, &readError).value_or(QByteArray());
    return snapshot;
}

void UpdateService::pollInstallStatus()
{
    if (m_state != UpdateState::Installing) {
        m_installPoll.stop();
        return;
    }
    // Only a status written after the request answers it (see install()). The helpers
    // replace the file atomically, so a new answer is a new inode even with the same text.
    const StatusSnapshot now = statusSnapshot();
    if (now.exists && !(now == m_statusBefore)) {
        const QString text = QString::fromUtf8(now.content).trimmed();
        const QString expected = m_pending ? m_pending->version.toString() : QString();
        if (text.startsWith(QStringLiteral("installing "))) {
            const QString message = QStringLiteral("installing %1 to slot %2").arg(expected, text.mid(11).trimmed());
            if (message != m_message)
                setState(UpdateState::Installing, UpdateError::None, message);
        } else if (text.startsWith(QStringLiteral("ok "))) {
            m_installPoll.stop();
            const QString reported = text.mid(3).trimmed();
            const auto reportedVersion = Version::parse(reported);
            if (m_pending && reportedVersion && *reportedVersion == m_pending->version) {
                setState(UpdateState::RestartRequired, UpdateError::None,
                         QStringLiteral("update installed; restart required (%1)").arg(reported));
            } else {
                setState(UpdateState::Failed, UpdateError::InstallFailed,
                         describe(UpdateError::InstallFailed,
                                  QStringLiteral("it reported version %1, expected %2").arg(reported, expected)));
            }
            return;
        } else if (text.startsWith(QStringLiteral("error "))) {
            m_installPoll.stop();
            setState(UpdateState::Failed, UpdateError::InstallFailed,
                     describe(UpdateError::InstallFailed, text.mid(6)));
            return;
        }
        // anything else: not a recognised status line (yet); keep polling
    }
    if (m_installDeadline.hasExpired()) {
        m_installPoll.stop();
        const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(m_env.installTimeout).count();
        setState(UpdateState::Failed, UpdateError::InstallTimedOut,
                 describe(UpdateError::InstallTimedOut,
                          minutes > 0
                              ? QStringLiteral("no result after %1 minutes; restart before trying again").arg(minutes)
                              : QStringLiteral("no result; restart before trying again")));
    }
}

QString UpdateService::lastUpdatePath() const
{
    return m_env.stagingDir + QStringLiteral("/last-update.json");
}

void UpdateService::loadPersistedState()
{
    if (m_state != UpdateState::Idle || m_env.stagingDir.isEmpty())
        return;

    const QString path = lastUpdatePath();
    QString readError;
    const auto bytes = shared::readFileLimited(path, MaxLastUpdateBytes, &readError);
    if (!bytes)
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(*bytes);
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    const QString newBuildId = root.value(QStringLiteral("newBuildId")).toString();
    const QString previousBuildId = root.value(QStringLiteral("previousBuildId")).toString();
    const QString result = root.value(QStringLiteral("result")).toString();
    // Versions are friendlier than build ids; older records only carry the build ids.
    const QString newVersion = root.value(QStringLiteral("newVersion")).toString();
    const QString previousVersion = root.value(QStringLiteral("previousVersion")).toString();
    const QString newName = newVersion.isEmpty() ? newBuildId : newVersion;
    const QString previousName = previousVersion.isEmpty() ? previousBuildId : previousVersion;

    if (result == QStringLiteral("rolled-back")) {
        setState(UpdateState::RecoveryRequired, UpdateError::None,
                 QStringLiteral("The update to %1 did not start correctly, so Nexora OS went back to %2. "
                                "Nothing was lost; you can try the update again.")
                     .arg(newName, previousName));
        return;
    }
    if (!newBuildId.isEmpty() && newBuildId == m_env.currentBuildId) {
        setState(UpdateState::Completed, UpdateError::None,
                 QStringLiteral("updated from %1 to %2").arg(previousName, newName));
        // The record stays until acknowledgeLastResult(), so a restart before the
        // user has seen the message does not lose it.
    }
}

bool UpdateService::acknowledgeLastResult()
{
    if (m_state != UpdateState::Completed && m_state != UpdateState::RecoveryRequired)
        return false;
    const QString path = lastUpdatePath();
    // unlink() is atomic: the record is either still there (and shown again) or gone.
    if (!m_env.stagingDir.isEmpty() && QFileInfo::exists(path) && !QFile::remove(path))
        return false;
    setState(UpdateState::Idle);
    return true;
}

} // namespace nexora::update
