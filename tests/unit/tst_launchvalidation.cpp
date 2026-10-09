#include "nexora/emulator/EmulatorManager.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace nexora::emulator;
using nexora::shared::EmulatorConfig;

class TestLaunchValidation : public QObject
{
    Q_OBJECT

private:
    static QString createFile(const QTemporaryDir &dir, const QString &name, QFile::Permissions perms)
    {
        const QString path = dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write("#!/bin/sh\nexit 0\n");
        file.close();
        file.setPermissions(perms);
        return path;
    }

    static EmulatorConfig config(const QString &executable, bool enabled = true)
    {
        return {QStringLiteral("test"), QStringLiteral("Test Emulator"), executable, {}, enabled, {}};
    }

private slots:
    void validateExecutable()
    {
        QTemporaryDir dir;
        QString detail;
        QCOMPARE(EmulatorManager::validateExecutable(QString(), &detail), EmulatorError::InvalidConfig);
        QCOMPARE(EmulatorManager::validateExecutable(QStringLiteral("bin/emu"), &detail),
                 EmulatorError::InvalidConfig);
        QVERIFY(detail.contains(QStringLiteral("absolute")));
        QCOMPARE(EmulatorManager::validateExecutable(dir.filePath(QStringLiteral("missing")), &detail),
                 EmulatorError::ExecutableMissing);
        QCOMPARE(detail, dir.filePath(QStringLiteral("missing")));
        QCOMPARE(EmulatorManager::validateExecutable(dir.path()), EmulatorError::NotAFile);

        const QString plain = createFile(dir, QStringLiteral("plain"),
                                         QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup);
        QCOMPARE(EmulatorManager::validateExecutable(plain), EmulatorError::NotExecutable);

        const QString exec = createFile(dir, QStringLiteral("exec"),
                                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QCOMPARE(EmulatorManager::validateExecutable(exec), EmulatorError::None);
    }

    void invalidLaunchFailsSynchronously()
    {
        QTemporaryDir dir;
        EmulatorManager manager;
        QSignalSpy states(&manager, &EmulatorManager::stateChanged);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);

        const auto result = manager.launch(config(dir.filePath(QStringLiteral("missing"))));
        QVERIFY(!result.accepted);
        QCOMPARE(result.error, EmulatorError::ExecutableMissing);
        QVERIFY(result.message.startsWith(QStringLiteral("Emulator executable not found")));
        QCOMPARE(manager.state(), ProcessState::Failed);
        QCOMPARE(states.size(), 1); // Idle -> Failed, never Starting
        QCOMPARE(finished.size(), 1);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::ExecutableMissing);
        QVERIFY(!manager.isActive());
    }

    void disabledConfigIsRejected()
    {
        EmulatorManager manager;
        const auto result = manager.launch(config(QStringLiteral("/bin/true"), false));
        QVERIFY(!result.accepted);
        QCOMPARE(result.error, EmulatorError::Disabled);
        QCOMPARE(manager.state(), ProcessState::Failed);
    }

    void emptyIdIsRejected()
    {
        EmulatorManager manager;
        EmulatorConfig c = config(QStringLiteral("/bin/true"));
        c.id.clear();
        QCOMPARE(manager.launch(c).error, EmulatorError::InvalidConfig);
    }

    void stopWithoutProcessReturnsFalse()
    {
        EmulatorManager manager;
        QVERIFY(!manager.stop());
        QCOMPARE(manager.state(), ProcessState::Idle);
    }
};

QTEST_GUILESS_MAIN(TestLaunchValidation)
#include "tst_launchvalidation.moc"
