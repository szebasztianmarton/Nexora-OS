#include "nexora/update/UpdateService.h"

#include "UpdateBundleFixture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace nexora::update;
using nexora::update::testsupport::BundleSpec;
using nexora::update::testsupport::TestKeyPair;
using nexora::update::testsupport::writeBundle;
using nexora::update::testsupport::writePublicKey;

namespace {

UpdateEnvironment baseEnv(const QString &keyDir, const QString &stagingDir)
{
    UpdateEnvironment env;
    env.keyDir = keyDir;
    env.stagingDir = stagingDir;
    env.currentVersion = *Version::parse(QStringLiteral("0.4.0"));
    env.currentBuildId = QStringLiteral("v0.4.0-0-gcafebabe");
    env.board = QStringLiteral("nexora-x86_64");
    return env;
}

void flipFirstByte(const QString &path)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadWrite));
    char byte = 0;
    f.getChar(&byte);
    f.seek(0);
    f.putChar(char(byte ^ 0xFF));
    f.close();
}

// Replaces |path| the way the privileged helpers do (write a temporary file, rename it over
// the old one), so every status update is a new inode.
void writeStatusAtomically(const QString &path, const QByteArray &line)
{
    const QString tmp = path + QStringLiteral(".tmp");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(line);
    f.close();
    ::rename(QFile::encodeName(tmp).constData(), QFile::encodeName(path).constData());
}

// Stands in for nexora-updated: waits for one request line on |fifoPath|, then writes each
// of |statusLines| to |statusPath| (atomically, |gapMs| apart).
class FakeHelper
{
public:
    FakeHelper(const QString &fifoPath, const QString &statusPath, const QList<QByteArray> &statusLines,
               int gapMs = 100)
    {
        QVERIFY(::mkfifo(QFile::encodeName(fifoPath).constData(), 0600) == 0);
        m_thread = std::thread([this, fifoPath, statusPath, statusLines, gapMs] {
            QFile fifo(fifoPath);
            fifo.open(QIODevice::ReadOnly); // blocks until a writer opens the FIFO
            m_request = QString::fromUtf8(fifo.readLine());
            for (const QByteArray &line : statusLines) {
                QThread::msleep(gapMs);
                writeStatusAtomically(statusPath, line);
            }
        });
        // Let the reader reach its blocking open() before install() opens the FIFO
        // non-blocking (see installWithFifoReaderReachesRestartRequired).
        QThread::msleep(200);
    }
    ~FakeHelper() { join(); }
    FakeHelper(const FakeHelper &) = delete;
    FakeHelper &operator=(const FakeHelper &) = delete;

    QString request()
    {
        join();
        return m_request;
    }

private:
    void join()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    std::thread m_thread;
    QString m_request;
};

// checkBundle() + verify() a fresh 1 MiB bundle of version 0.4.1 -> ReadyToInstall.
void makeReady(UpdateService &service, const QString &bundleDir, const TestKeyPair &key)
{
    BundleSpec spec;
    spec.imageSize = 1024 * 1024;
    spec.version = QStringLiteral("0.4.1");
    writeBundle(bundleDir, spec, key);
    QVERIFY(service.checkBundle(bundleDir));
    QVERIFY(service.verify());
    QVERIFY(QTest::qWaitFor([&] { return service.state() == UpdateState::ReadyToInstall; }, 5000));
}

// Runs the install flow against a FakeHelper answering with |statusLines| (after |stale|
// was left in the status file by an earlier request, if not empty).
struct InstallRig
{
    QTemporaryDir bundleRoot, keyRoot, stageRoot, runRoot;
    TestKeyPair key = TestKeyPair::generate();
    QString fifoPath = runRoot.filePath(QStringLiteral("update-request"));
    QString statusPath = runRoot.filePath(QStringLiteral("update-status"));

    UpdateEnvironment env()
    {
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        UpdateEnvironment e = baseEnv(keyRoot.path(), stageRoot.path());
        e.installRequestFifo = fifoPath;
        e.installStatusFile = statusPath;
        return e;
    }
};

} // namespace

class TestUpdateService : public QObject
{
    Q_OBJECT
private slots:
    void happyPathReachesReadyToInstall()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        QVERIFY(bundleRoot.isValid() && keyRoot.isValid() && stageRoot.isValid());
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 3 * 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        QCOMPARE(service.state(), UpdateState::Idle);
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QCOMPARE(service.state(), UpdateState::Available);
        QCOMPARE(service.error(), UpdateError::None);
        QVERIFY(service.pending().has_value());

        QVERIFY(service.verify());
        QVERIFY(QTest::qWaitFor(
            [&] { return service.state() == UpdateState::ReadyToInstall || service.state() == UpdateState::Failed; },
            5000));
        QCOMPARE(service.state(), UpdateState::ReadyToInstall);
    }

    void wrongBoardFails()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.board = QStringLiteral("some-other-board");
        writeBundle(bundleRoot.path(), spec, key);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::WrongBoard);
    }

    void sameVersionIsNotNewer()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.version = QStringLiteral("0.4.0"); // == env.currentVersion
        writeBundle(bundleRoot.path(), spec, key);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::NotNewer);
    }

    void currentBelowMinimumIsTooOld()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.version = QStringLiteral("0.5.0");
        spec.minimumVersion = QStringLiteral("0.4.5"); // > env.currentVersion (0.4.0)
        writeBundle(bundleRoot.path(), spec, key);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::TooOld);
    }

    void tamperedImageFailsDuringVerify()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 2 * 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);
        // Flip one byte without changing the file size, so checkBundle's cheap size check
        // still passes and only the full hash in verify() catches the tamper.
        flipFirstByte(bundleRoot.path() + QLatin1String("/") + spec.imageFileName);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QCOMPARE(service.state(), UpdateState::Available);
        QVERIFY(service.verify());
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Verifying; }, 5000));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::HashMismatch);
    }

    void insufficientStorageFailsVerify()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 2 * 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.path());
        // Nothing is staged (the helper writes the image straight to the inactive slot), so
        // only room for the small status/record files is required; 16 KiB is below that.
        env.freeBytes = [](const QString &) { return qint64(16) * 1024; };
        UpdateService service(env);
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QVERIFY(service.verify());
        // Synchronous: the free-space check runs before any worker thread is started.
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::InsufficientStorage);
    }

    void cancelDuringVerifyIsHonoured()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 100 * 1024 * 1024;
        spec.sparseImage = true; // fast to create and to hash; still ~100 one-MiB progress steps
        writeBundle(bundleRoot.path(), spec, key);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QSignalSpy progress(&service, &UpdateService::progressChanged);
        QVERIFY(service.verify());
        // Cancel as soon as the first progress update lands: at most one of ~100 chunks
        // has been hashed, so the worker thread has ~99 remaining chances to observe the
        // cancellation flag before it would otherwise finish.
        QVERIFY(QTest::qWaitFor([&] { return progress.size() > 0; }, 5000));
        service.cancel();
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Verifying; }, 5000));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::Cancelled);
    }

    void installWithoutAReaderFails()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot, runRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.path());
        env.installRequestFifo = runRoot.filePath(QStringLiteral("update-request")); // no FIFO created: open() fails
        UpdateService service(env);
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QVERIFY(service.verify());
        QVERIFY(QTest::qWaitFor([&] { return service.state() == UpdateState::ReadyToInstall; }, 5000));
        QVERIFY(service.install());
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::InstallRequestFailed);
    }

    void installWithFifoReaderReachesRestartRequired()
    {
        QTemporaryDir bundleRoot, keyRoot, stageRoot, runRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 1024 * 1024;
        spec.version = QStringLiteral("0.4.1");
        writeBundle(bundleRoot.path(), spec, key);

        const QString fifoPath = runRoot.filePath(QStringLiteral("update-request"));
        QCOMPARE(::mkfifo(fifoPath.toLocal8Bit().constData(), 0600), 0);
        const QString statusPath = runRoot.filePath(QStringLiteral("update-status"));

        QString receivedLine;
        std::thread reader([&] {
            QFile fifo(fifoPath);
            fifo.open(QIODevice::ReadOnly); // blocks until a writer opens the FIFO
            receivedLine = QString::fromUtf8(fifo.readLine());
            QFile status(statusPath);
            status.open(QIODevice::WriteOnly | QIODevice::Truncate);
            status.write("ok 0.4.1\n");
        });
        // Give the reader time to reach its blocking open() before the non-blocking
        // writer side (install()) tries to open the FIFO; otherwise the writer would see
        // ENXIO (no reader yet) exactly like installWithoutAReaderFails() above.
        QThread::msleep(200);

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.path());
        env.installRequestFifo = fifoPath;
        env.installStatusFile = statusPath;
        UpdateService service(env);
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QVERIFY(service.verify());
        QVERIFY(QTest::qWaitFor([&] { return service.state() == UpdateState::ReadyToInstall; }, 5000));
        QVERIFY(service.install());
        QCOMPARE(service.state(), UpdateState::Installing);
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Installing; }, 5000));
        QCOMPARE(service.state(), UpdateState::RestartRequired);
        reader.join();
        QCOMPARE(receivedLine, QStringLiteral("install %1\n").arg(bundleRoot.path()));
    }

    // nexora-updated refuses these with a bare "error invalid request"; the service refuses
    // them itself, with a reason, before anything reaches the FIFO.
    void installRefusesABundlePathTheHelperWouldReject_data()
    {
        QTest::addColumn<QString>("dirName");
        QTest::addColumn<QString>("reason");
        QTest::newRow("newline") << QStringLiteral("nexora-update\nx") << QStringLiteral("control character");
        QTest::newRow("dot dot") << QStringLiteral("nexora..update") << QStringLiteral("\"..\"");
        QTest::newRow("outside the set") << QStringLiteral("nexora$update") << QStringLiteral("only letters, digits");
        QTest::newRow("longer than 256 bytes")
            << QString(130, QLatin1Char('a')) + QLatin1Char('/') + QString(130, QLatin1Char('b'))
            << QStringLiteral("longer than 256 bytes");
    }

    void installRefusesABundlePathTheHelperWouldReject()
    {
        QFETCH(QString, dirName);
        QFETCH(QString, reason);
        InstallRig rig;
        UpdateService service(rig.env());
        const QString bundleDir = rig.bundleRoot.filePath(dirName);
        QVERIFY(QDir().mkpath(bundleDir));
        makeReady(service, bundleDir, rig.key);
        QCOMPARE(service.state(), UpdateState::ReadyToInstall);

        // A reader is waiting, so a request would get through.
        QCOMPARE(::mkfifo(QFile::encodeName(rig.fifoPath).constData(), 0600), 0);
        const int reader = ::open(QFile::encodeName(rig.fifoPath).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        QVERIFY(reader >= 0);
        auto closeReader = qScopeGuard([reader] { ::close(reader); });

        QVERIFY(service.install());
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::UnsafeBundlePath);
        QVERIFY2(service.message().contains(reason), qPrintable(service.message()));
        char byte = 0;
        QVERIFY(::read(reader, &byte, 1) <= 0); // nothing was written
    }

    void bundlePathRuleMatchesTheHelper()
    {
        QVERIFY(bundlePathProblem(QStringLiteral("/media/usb 1/nexora-update-0.4.1_rc.2")).isEmpty());
        QVERIFY(bundlePathProblem(QLatin1Char('/') + QString(255, QLatin1Char('a'))).isEmpty()); // 256 bytes
        QVERIFY(!bundlePathProblem(QLatin1Char('/') + QString(256, QLatin1Char('a'))).isEmpty());
        QVERIFY(!bundlePathProblem(QStringLiteral("/")).isEmpty());
        QVERIFY(!bundlePathProblem(QStringLiteral("relative/bundle")).isEmpty());
        QVERIFY(!bundlePathProblem(QStringLiteral("/media/usb/nexora\tupdate")).isEmpty());
        QVERIFY(!bundlePathProblem(QStringLiteral("/media/usb/nexora\rupdate")).isEmpty());
        QVERIFY(!bundlePathProblem(QStringLiteral("/media/usb/../update")).isEmpty());
        QVERIFY(!bundlePathProblem(QStringLiteral("/media/usb/frissítés")).isEmpty());
    }

    void verifyCreatesAMissingStateDirectory()
    {
        // First update on a fresh system: stateDir/update does not exist yet. The default
        // free-space probe (QStorageInfo) must not see a missing path as "no space".
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);

        const QString stagingDir = stageRoot.filePath(QStringLiteral("state/nexora/update"));
        QVERIFY(!QFileInfo::exists(stagingDir));
        UpdateService service(baseEnv(keyRoot.path(), stagingDir));
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QVERIFY(service.verify());
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Verifying; }, 5000));
        QCOMPARE(service.state(), UpdateState::ReadyToInstall);
        QVERIFY(QFileInfo(stagingDir).isDir());
    }

    void verifyDoesNotRequireRoomForTheImage()
    {
        // The image is never copied into the state directory (the 1 GiB data partition is
        // smaller than a 2 GiB root image), so an image larger than the free space there is
        // fine: the helper writes it straight to the inactive slot.
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 3 * 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.path());
        env.freeBytes = [](const QString &) { return qint64(2) * 1024 * 1024; }; // < image size
        UpdateService service(env);
        QVERIFY(service.checkBundle(bundleRoot.path()));
        QVERIFY(service.verify());
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Verifying; }, 5000));
        QCOMPARE(service.state(), UpdateState::ReadyToInstall);
    }

    void staleStatusIsIgnored_data()
    {
        QTest::addColumn<QByteArray>("stale");
        QTest::newRow("ok from nexora-bootok") << QByteArray("ok 0.4.1\n");
        QTest::newRow("earlier error") << QByteArray("error bundle directory not found\n");
    }

    void staleStatusIsIgnored()
    {
        // /run/nexora/update-status survives between requests (nexora-bootok writes
        // "ok <version>" there after confirming a boot); it must not decide this attempt.
        QFETCH(QByteArray, stale);
        InstallRig rig;
        writeStatusAtomically(rig.statusPath, stale);
        UpdateService service(rig.env());
        FakeHelper helper(rig.fifoPath, rig.statusPath, {}); // accepts the request, never answers
        makeReady(service, rig.bundleRoot.path(), rig.key);
        QVERIFY(service.install());
        QTest::qWait(1500); // three poll intervals
        QCOMPARE(service.state(), UpdateState::Installing);
        QCOMPARE(helper.request(), QStringLiteral("install %1\n").arg(rig.bundleRoot.path()));
    }

    void freshStatusWithTheSameTextIsAccepted()
    {
        // The helper may repeat the previous attempt's line verbatim; a rewritten file is
        // still a new answer.
        InstallRig rig;
        writeStatusAtomically(rig.statusPath, "error invalid request\n");
        UpdateService service(rig.env());
        FakeHelper helper(rig.fifoPath, rig.statusPath, {"error invalid request\n"});
        makeReady(service, rig.bundleRoot.path(), rig.key);
        QVERIFY(service.install());
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Installing; }, 5000));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::InstallFailed);
        QVERIFY(service.message().contains(QStringLiteral("invalid request")));
    }

    void installingThenOkForThePendingVersionSucceeds()
    {
        InstallRig rig;
        writeStatusAtomically(rig.statusPath, "ok 0.4.0\n"); // the running version's boot confirmation
        UpdateService service(rig.env());
        FakeHelper helper(rig.fifoPath, rig.statusPath, {"installing b\n", "ok 0.4.1\n"}, 1200);
        makeReady(service, rig.bundleRoot.path(), rig.key);
        QVERIFY(service.install());
        QVERIFY(QTest::qWaitFor([&] { return service.message().contains(QStringLiteral("slot b")); }, 5000));
        QCOMPARE(service.state(), UpdateState::Installing);
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Installing; }, 5000));
        QCOMPARE(service.state(), UpdateState::RestartRequired);
    }

    void okForAnotherVersionIsAFailure()
    {
        InstallRig rig;
        UpdateService service(rig.env());
        FakeHelper helper(rig.fifoPath, rig.statusPath, {"installing b\n", "ok 0.9.9\n"});
        makeReady(service, rig.bundleRoot.path(), rig.key);
        QVERIFY(service.install());
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Installing; }, 5000));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::InstallFailed);
    }

    void installTimesOut()
    {
        InstallRig rig;
        UpdateEnvironment env = rig.env();
        env.installTimeout = std::chrono::milliseconds(1500);
        UpdateService service(env);
        FakeHelper helper(rig.fifoPath, rig.statusPath, {"installing b\n"}); // then never finishes
        makeReady(service, rig.bundleRoot.path(), rig.key);
        QVERIFY(service.install());
        QVERIFY(QTest::qWaitFor([&] { return service.state() != UpdateState::Installing; }, 6000));
        QCOMPARE(service.state(), UpdateState::Failed);
        QCOMPARE(service.error(), UpdateError::InstallTimedOut);
        QVERIFY(!service.message().isEmpty());
    }

    void acknowledgeClearsARollbackRecord()
    {
        QTemporaryDir stageRoot, keyRoot;
        const QString stagingDir = stageRoot.filePath(QStringLiteral("update"));
        QVERIFY(QDir().mkpath(stagingDir));
        const QString path = stagingDir + QStringLiteral("/last-update.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        // Exactly what nexora-bootok writes.
        f.write(R"({"format": 1, "result": "rolled-back", "previousBuildId": "v0.4.0-0-gcafebabe", )"
                R"("newBuildId": "v0.4.1-0-gabcdef0", "previousVersion": "0.4.0", "newVersion": "0.4.1", "slot": "a"})"
                "\n");
        f.close();

        UpdateService service(baseEnv(keyRoot.path(), stagingDir));
        QVERIFY(!service.acknowledgeLastResult()); // nothing to acknowledge while Idle
        service.loadPersistedState();
        QCOMPARE(service.state(), UpdateState::RecoveryRequired);
        QVERIFY(service.message().contains(QStringLiteral("0.4.1")));
        QVERIFY(service.acknowledgeLastResult());
        QCOMPARE(service.state(), UpdateState::Idle);
        QVERIFY(!QFileInfo::exists(path));

        // A restart does not report it again.
        UpdateService again(baseEnv(keyRoot.path(), stagingDir));
        again.loadPersistedState();
        QCOMPARE(again.state(), UpdateState::Idle);
    }

    void acknowledgeReturnsCompletedToIdle()
    {
        QTemporaryDir stageRoot, keyRoot;
        const QString stagingDir = stageRoot.filePath(QStringLiteral("update"));
        QVERIFY(QDir().mkpath(stagingDir));
        QFile f(stagingDir + QStringLiteral("/last-update.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"format": 1, "result": "ok", "previousBuildId": "v0.4.0-0-gcafebabe", )"
                R"("newBuildId": "v0.4.1-0-gabcdef0"})");
        f.close();

        UpdateEnvironment env = baseEnv(keyRoot.path(), stagingDir);
        env.currentBuildId = QStringLiteral("v0.4.1-0-gabcdef0");
        UpdateService service(env);
        service.loadPersistedState();
        QCOMPARE(service.state(), UpdateState::Completed);
        QVERIFY(service.acknowledgeLastResult());
        QCOMPARE(service.state(), UpdateState::Idle);
    }

    void invalidTransitionsAreRejected()
    {
        using S = UpdateState;
        QVERIFY(isValidTransition(S::Idle, S::Checking));
        QVERIFY(isValidTransition(S::Idle, S::Completed));
        QVERIFY(isValidTransition(S::Idle, S::RecoveryRequired));
        QVERIFY(!isValidTransition(S::Idle, S::Verifying));
        QVERIFY(!isValidTransition(S::Idle, S::ReadyToInstall));
        QVERIFY(!isValidTransition(S::Idle, S::Installing));
        QVERIFY(isValidTransition(S::Checking, S::Available));
        QVERIFY(isValidTransition(S::Checking, S::Failed));
        QVERIFY(!isValidTransition(S::Checking, S::Checking));
        QVERIFY(isValidTransition(S::Available, S::Verifying));
        QVERIFY(isValidTransition(S::Available, S::Checking));
        QVERIFY(!isValidTransition(S::Available, S::Installing));
        QVERIFY(isValidTransition(S::Verifying, S::ReadyToInstall));
        QVERIFY(isValidTransition(S::Verifying, S::Failed));
        QVERIFY(!isValidTransition(S::Verifying, S::Verifying));
        QVERIFY(isValidTransition(S::ReadyToInstall, S::Installing));
        QVERIFY(isValidTransition(S::ReadyToInstall, S::Checking));
        QVERIFY(isValidTransition(S::Installing, S::RestartRequired));
        QVERIFY(isValidTransition(S::Installing, S::Failed));
        QVERIFY(!isValidTransition(S::RestartRequired, S::Idle));
        QVERIFY(!isValidTransition(S::RestartRequired, S::Checking));
        QVERIFY(isValidTransition(S::Failed, S::Checking));
        QVERIFY(isValidTransition(S::Completed, S::Checking));
        QVERIFY(isValidTransition(S::RecoveryRequired, S::Checking));
        QVERIFY(isValidTransition(S::Completed, S::Idle)); // acknowledged
        QVERIFY(isValidTransition(S::RecoveryRequired, S::Idle)); // acknowledged
        QVERIFY(!isValidTransition(S::Failed, S::Idle));
    }

    void loadPersistedStateDetectsCompletion()
    {
        QTemporaryDir stageRoot, keyRoot;
        QVERIFY(QDir().mkpath(stageRoot.filePath(QStringLiteral("update"))));
        const QString path = stageRoot.filePath(QStringLiteral("update/last-update.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"previousBuildId":"v0.4.0-0-gcafebabe","newBuildId":"v0.4.1-0-gabcdef0","result":"ok"})");
        f.close();

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.filePath(QStringLiteral("update")));
        env.currentBuildId = QStringLiteral("v0.4.1-0-gabcdef0");
        UpdateService service(env);
        service.loadPersistedState();
        QCOMPARE(service.state(), UpdateState::Completed);
        // Kept until the user acknowledges it, so a restart before then does not lose it.
        QVERIFY(QFileInfo::exists(path));
        UpdateService afterRestart(env);
        afterRestart.loadPersistedState();
        QCOMPARE(afterRestart.state(), UpdateState::Completed);
        QVERIFY(afterRestart.acknowledgeLastResult());
        QVERIFY(!QFileInfo::exists(path));
    }

    void loadPersistedStateDetectsRollback()
    {
        QTemporaryDir stageRoot, keyRoot;
        QVERIFY(QDir().mkpath(stageRoot.filePath(QStringLiteral("update"))));
        const QString path = stageRoot.filePath(QStringLiteral("update/last-update.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"previousBuildId":"v0.4.0-0-gcafebabe","newBuildId":"v0.4.1-0-gabcdef0","result":"rolled-back"})");
        f.close();

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.filePath(QStringLiteral("update")));
        UpdateService service(env);
        service.loadPersistedState();
        QCOMPARE(service.state(), UpdateState::RecoveryRequired);
        QVERIFY(QFileInfo::exists(path));
    }

    void loadPersistedStateIgnoresAnUnrelatedBuild()
    {
        QTemporaryDir stageRoot, keyRoot;
        QVERIFY(QDir().mkpath(stageRoot.filePath(QStringLiteral("update"))));
        const QString path = stageRoot.filePath(QStringLiteral("update/last-update.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"previousBuildId":"v0.3.0","newBuildId":"v0.9.9","result":"ok"})");
        f.close();

        UpdateEnvironment env = baseEnv(keyRoot.path(), stageRoot.filePath(QStringLiteral("update")));
        UpdateService service(env);
        service.loadPersistedState();
        QCOMPARE(service.state(), UpdateState::Idle);
    }

#ifdef NEXORA_UPDATE_FIXTURE_DIR
    // CTest FIXTURES_SETUP for test_update_tool.sh: writes a good and a tampered bundle
    // plus their trusted key into a stable location using the same reusable fixture
    // helper the tests above use. See tests/integration/CMakeLists.txt.
    void writesToolFixture()
    {
        const QString root = QString::fromLocal8Bit(NEXORA_UPDATE_FIXTURE_DIR);
        QVERIFY(QDir().mkpath(root));
        const auto key = TestKeyPair::generate();
        writePublicKey(root + QStringLiteral("/keys"), QStringLiteral("release"), key);

        BundleSpec spec;
        spec.imageSize = 2 * 1024 * 1024;
        writeBundle(root + QStringLiteral("/good-bundle"), spec, key);
        writeBundle(root + QStringLiteral("/bad-bundle"), spec, key);
        flipFirstByte(root + QStringLiteral("/bad-bundle/") + spec.imageFileName);
    }
#endif
};

QTEST_GUILESS_MAIN(TestUpdateService)
#include "tst_updateservice.moc"
