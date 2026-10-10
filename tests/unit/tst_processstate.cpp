#include "nexora/emulator/EmulatorProcessState.h"

#include <QTest>

using namespace nexora::emulator;
using S = ProcessState;

class TestProcessState : public QObject
{
    Q_OBJECT

private slots:
    void transitions_data()
    {
        QTest::addColumn<ProcessState>("from");
        QTest::addColumn<ProcessState>("to");
        QTest::addColumn<bool>("valid");

        const QList<S> all{S::Idle, S::Starting, S::Running, S::Stopping, S::Exited, S::Failed};
        const QList<QPair<S, S>> allowed{
            {S::Idle, S::Starting},   {S::Idle, S::Failed},      {S::Starting, S::Running}, {S::Starting, S::Stopping},
            {S::Starting, S::Failed}, {S::Running, S::Stopping}, {S::Running, S::Exited},   {S::Running, S::Failed},
            {S::Stopping, S::Exited}, {S::Exited, S::Starting},  {S::Exited, S::Failed},    {S::Failed, S::Starting},
            {S::Failed, S::Failed},
        };
        for (S from : all) {
            for (S to : all) {
                const QByteArray name = (nameOf(from) + QStringLiteral("->") + nameOf(to)).toLatin1();
                QTest::newRow(name.constData()) << from << to << allowed.contains({from, to});
            }
        }
    }

    void transitions()
    {
        QFETCH(ProcessState, from);
        QFETCH(ProcessState, to);
        QFETCH(bool, valid);
        QCOMPARE(isValidTransition(from, to), valid);
    }

    void activeStates()
    {
        QVERIFY(!isActive(S::Idle));
        QVERIFY(isActive(S::Starting));
        QVERIFY(isActive(S::Running));
        QVERIFY(isActive(S::Stopping));
        QVERIFY(!isActive(S::Exited));
        QVERIFY(!isActive(S::Failed));
    }

    void stateNames()
    {
        QCOMPARE(nameOf(S::Idle), QStringLiteral("Idle"));
        QCOMPARE(nameOf(S::Failed), QStringLiteral("Failed"));
        QCOMPARE(nameOf(EmulatorError::Crashed), QStringLiteral("Crashed"));
    }

    void errorMessages_data()
    {
        QTest::addColumn<EmulatorError>("error");
        QTest::addColumn<QString>("expected");
        QTest::newRow("missing") << EmulatorError::ExecutableMissing
                                 << QStringLiteral("Emulator executable not found: /x");
        QTest::newRow("not file") << EmulatorError::NotAFile
                                  << QStringLiteral("Emulator executable is not a regular file: /x");
        QTest::newRow("not exec") << EmulatorError::NotExecutable
                                  << QStringLiteral("Emulator file is not executable: /x");
        QTest::newRow("busy") << EmulatorError::AlreadyActive
                              << QStringLiteral("Another emulator is already running: /x");
        QTest::newRow("start") << EmulatorError::FailedToStart << QStringLiteral("Emulator failed to start: /x");
        QTest::newRow("code") << EmulatorError::NonZeroExit << QStringLiteral("Emulator exited with an error code: /x");
        QTest::newRow("crash") << EmulatorError::Crashed << QStringLiteral("Emulator crashed: /x");
        QTest::newRow("disabled") << EmulatorError::Disabled
                                  << QStringLiteral("Emulator is disabled in its configuration: /x");
        QTest::newRow("config") << EmulatorError::InvalidConfig << QStringLiteral("Invalid emulator configuration: /x");
        QTest::newRow("profile") << EmulatorError::ProfileNotFound << QStringLiteral("Launch profile not found: /x");
        QTest::newRow("profile off") << EmulatorError::ProfileDisabled
                                     << QStringLiteral("Launch profile is disabled: /x");
        QTest::newRow("workdir") << EmulatorError::InvalidWorkingDirectory
                                 << QStringLiteral("Working directory does not exist: /x");
        QTest::newRow("game") << EmulatorError::GameFileMissing
                              << QStringLiteral("Game file is missing or unreadable: /x");
    }

    void errorMessages()
    {
        QFETCH(EmulatorError, error);
        QFETCH(QString, expected);
        QCOMPARE(describe(error, QStringLiteral("/x")), expected);
        QVERIFY(!describe(error).contains(u':'));
    }
};

QTEST_GUILESS_MAIN(TestProcessState)
#include "tst_processstate.moc"
