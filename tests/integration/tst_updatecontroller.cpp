// UpdateController wraps nexora::update::UpdateService for the QML UI: supported() gates
// the desktop platform unless NEXORA_UPDATE_TEST=1 (used here to exercise the full
// check -> verify -> ready flow against a real signed bundle, built with the same fixture
// helper the Task 5 update tests use).
#include "UpdateController.h"

#include "UpdateBundleFixture.h"

#include "nexora/update/UpdateService.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::ui;
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

} // namespace

class TestUpdateController : public QObject
{
    Q_OBJECT

private slots:
    void supportedFalseOnDesktopWithoutTestEnv()
    {
        qunsetenv("NEXORA_UPDATE_TEST");
        QTemporaryDir keyRoot, stageRoot;
        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        UpdateController controller(&service, QStringLiteral("0.4.0"), QStringLiteral("v0.4.0-0-gcafebabe"),
                                    /*isDesktopPlatform=*/true);
        QVERIFY(!controller.supported());
        QCOMPARE(controller.message(), QStringLiteral("Updates are installed only on the Nexora OS image"));
        QVERIFY(!controller.checkBundle(QStringLiteral("/nonexistent")));
        QCOMPARE(controller.state(), QStringLiteral("idle")); // refused before touching the service
    }

    void supportedTrueOnDesktopWithTestEnv()
    {
        qputenv("NEXORA_UPDATE_TEST", "1");
        QTemporaryDir keyRoot, stageRoot;
        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        UpdateController controller(&service, QStringLiteral("0.4.0"), QStringLiteral("v0.4.0-0-gcafebabe"),
                                    /*isDesktopPlatform=*/true);
        QVERIFY(controller.supported());
        qunsetenv("NEXORA_UPDATE_TEST");
    }

    void supportedTrueOnEmbeddedRegardlessOfEnv()
    {
        qunsetenv("NEXORA_UPDATE_TEST");
        QTemporaryDir keyRoot, stageRoot;
        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        UpdateController controller(&service, QStringLiteral("0.4.0"), QStringLiteral("v0.4.0-0-gcafebabe"),
                                    /*isDesktopPlatform=*/false);
        QVERIFY(controller.supported());
    }

    void checkVerifyReachesReadyToInstall()
    {
        qputenv("NEXORA_UPDATE_TEST", "1");
        QTemporaryDir bundleRoot, keyRoot, stageRoot;
        const auto key = TestKeyPair::generate();
        writePublicKey(keyRoot.path(), QStringLiteral("release"), key);
        BundleSpec spec;
        spec.imageSize = 2 * 1024 * 1024;
        writeBundle(bundleRoot.path(), spec, key);

        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        UpdateController controller(&service, QStringLiteral("0.4.0"), QStringLiteral("v0.4.0-0-gcafebabe"),
                                    /*isDesktopPlatform=*/true);
        QCOMPARE(controller.currentVersion(), QStringLiteral("0.4.0"));
        QCOMPARE(controller.currentBuildId(), QStringLiteral("v0.4.0-0-gcafebabe"));
        QCOMPARE(controller.state(), QStringLiteral("idle"));

        QVERIFY(controller.checkBundle(bundleRoot.path()));
        QCOMPARE(controller.state(), QStringLiteral("available"));
        QCOMPARE(controller.availableVersion(), spec.version);
        QCOMPARE(controller.notes(), spec.notes);

        QVERIFY(controller.verify());
        QVERIFY(QTest::qWaitFor(
            [&] {
                return controller.state() == QStringLiteral("readytoinstall")
                    || controller.state() == QStringLiteral("failed");
            },
            5000));
        QCOMPARE(controller.state(), QStringLiteral("readytoinstall"));
        QCOMPARE(controller.error(), QString());
        qunsetenv("NEXORA_UPDATE_TEST");
    }

    void candidateBundlesFindsHomeDirectoryBundle()
    {
        // candidateBundles() scans the real home directory (via QDir::homePath()); point it
        // at a temporary one so the test is hermetic.
        const QByteArray previousHome = qgetenv("HOME");
        QTemporaryDir home;
        qputenv("HOME", home.path().toUtf8());
        QVERIFY(QDir().mkpath(home.filePath(QStringLiteral("nexora-update-20261011"))));
        QVERIFY(QDir().mkpath(home.filePath(QStringLiteral("not-a-bundle"))));

        QTemporaryDir keyRoot, stageRoot;
        UpdateService service(baseEnv(keyRoot.path(), stageRoot.path()));
        UpdateController controller(&service, QStringLiteral("0.4.0"), QStringLiteral("v0.4.0-0-gcafebabe"), false);
        const QStringList found = controller.candidateBundles();
        QVERIFY(found.contains(home.filePath(QStringLiteral("nexora-update-20261011"))));
        for (const QString &path : found)
            QVERIFY(!path.endsWith(QStringLiteral("not-a-bundle")));
        qputenv("HOME", previousHome);
    }
};

QTEST_GUILESS_MAIN(TestUpdateController)
#include "tst_updatecontroller.moc"
