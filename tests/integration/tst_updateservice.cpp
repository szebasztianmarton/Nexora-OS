#include "nexora/update/UpdateService.h"

#include "UpdateBundleFixture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <thread>

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
        env.freeBytes = [](const QString &) { return qint64(1) * 1024 * 1024; }; // 1 MiB
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
        QVERIFY(!QFileInfo::exists(path)); // reported once
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
