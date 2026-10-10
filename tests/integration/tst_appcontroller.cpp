// AppController wired to a temporary configuration that uses the fake emulator.
#include "AppController.h"
#include "input/GamepadNavigator.h"
#include "input/InputController.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using nexora::ui::AppController;
using nexora::ui::EmulatorListModel;

namespace {

constexpr int Timeout = 10000;

void writeConfig(const QTemporaryDir &dir, const QString &id, const QString &executable, const QString &arguments = {},
                 bool enabled = true)
{
    QFile file(dir.filePath(id + QStringLiteral(".json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        QStringLiteral("{\"id\":\"%1\",\"name\":\"Emu %1\",\"executable\":\"%2\",\"arguments\":[%3],\"enabled\":%4}")
            .arg(id, executable, arguments, enabled ? QStringLiteral("true") : QStringLiteral("false"))
            .toUtf8());
}

QString statusOf(AppController &app, const QString &id)
{
    EmulatorListModel *model = app.emulators();
    return model->index(model->indexOf(id)).data(EmulatorListModel::StatusRole).toString();
}

} // namespace

class TestAppController : public QObject
{
    Q_OBJECT

private slots:
    void loadsConfigurationAndReportsIssues()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("ok"), QStringLiteral(NEXORA_FAKE_EMULATOR));
        writeConfig(dir, QStringLiteral("gone"), dir.filePath(QStringLiteral("missing-binary")));
        writeConfig(dir, QStringLiteral("off"), QStringLiteral(NEXORA_FAKE_EMULATOR), {}, false);
        QFile broken(dir.filePath(QStringLiteral("broken.json")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{ nope");
        broken.close();

        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        QVERIFY(app.configDirectoryFound());
        QCOMPARE(app.emulators()->rowCount(), 2); // disabled entry hidden
        QCOMPARE(app.disabledEmulators(), QStringList{QStringLiteral("Emu off")});
        QCOMPARE(app.configIssues().size(), 1);
        QVERIFY(app.configIssues().first().startsWith(QStringLiteral("Error: broken.json")));
        QCOMPARE(statusOf(app, QStringLiteral("ok")), QStringLiteral("ready"));
        QCOMPARE(statusOf(app, QStringLiteral("gone")), QStringLiteral("unavailable"));
        QVERIFY(!app.statusIsError());
    }

    void reportsMissingDirectory()
    {
        QTemporaryDir dir;
        AppController app({dir.filePath(QStringLiteral("absent")), QStringLiteral("test"), {}, false});
        QVERIFY(!app.configDirectoryFound());
        QCOMPARE(app.emulators()->rowCount(), 0);
        QVERIFY(app.statusIsError());
    }

    void launchAndReturn()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("ok"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--stdout\",\"hi\",\"--sleep-ms\",\"200\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        QSignalSpy stateSpy(&app, &AppController::stateChanged);

        QVERIFY(app.launch(QStringLiteral("ok")));
        QVERIFY(app.isBusy());
        QCOMPARE(app.activeEmulatorName(), QStringLiteral("Emu ok"));
        QTRY_COMPARE_WITH_TIMEOUT(statusOf(app, QStringLiteral("ok")), QStringLiteral("running"), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(!app.isBusy(), Timeout);
        QCOMPARE(app.state(), QStringLiteral("exited"));
        QCOMPARE(statusOf(app, QStringLiteral("ok")), QStringLiteral("exited"));
        QVERIFY(app.statusMessage().contains(QStringLiteral("exited normally")));
        QVERIFY(!app.statusIsError());
        QVERIFY(app.lastOutput().contains(QStringLiteral("hi")));
        QVERIFY(stateSpy.size() >= 3);

        // And again: the UI can launch another process after the previous one finished.
        QVERIFY(app.launch(QStringLiteral("ok")));
        QTRY_VERIFY_WITH_TIMEOUT(!app.isBusy(), Timeout);
        QCOMPARE(app.state(), QStringLiteral("exited"));
    }

    void failureIsReportedAndRecoverable()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("bad"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--exit-code\",\"7\""));
        writeConfig(dir, QStringLiteral("gone"), dir.filePath(QStringLiteral("missing-binary")));
        writeConfig(dir, QStringLiteral("good"), QStringLiteral(NEXORA_FAKE_EMULATOR));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});

        QVERIFY(!app.launch(QStringLiteral("gone")));
        QCOMPARE(app.state(), QStringLiteral("failed"));
        QVERIFY(app.statusIsError());
        QVERIFY(app.statusMessage().contains(QStringLiteral("not found")));
        QCOMPARE(statusOf(app, QStringLiteral("gone")), QStringLiteral("failed"));

        QVERIFY(app.launch(QStringLiteral("bad")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("failed"), Timeout);
        QVERIFY(app.statusMessage().contains(QStringLiteral("exit code 7")));
        QCOMPARE(statusOf(app, QStringLiteral("bad")), QStringLiteral("failed"));
        QCOMPARE(statusOf(app, QStringLiteral("gone")), QStringLiteral("unavailable"));

        QVERIFY(app.launch(QStringLiteral("good")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(!app.statusIsError());
    }

    void suspendsControllerInputWhileRunning()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        nexora::input::InputController input;
        QVERIFY(input.start(QStringLiteral("none")));
        app.setInputController(&input);
        QVERIFY(!input.isSuspended());

        QVERIFY(app.launch(QStringLiteral("long")));
        QVERIFY(input.isSuspended());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        QVERIFY(input.isSuspended());
        QVERIFY(app.stop());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(!input.isSuspended());
        QCOMPARE(app.inputBackend(), QStringLiteral("none"));
    }

    void controllerChordReturnsToLauncher()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        nexora::input::InputController input;
        QVERIFY(input.start(QStringLiteral("none")));
        input.navigator()->setChordHoldDuration(std::chrono::milliseconds(30));
        app.setInputController(&input);

        QVERIFY(!app.returnToLauncher()); // nothing running
        QVERIFY(app.launch(QStringLiteral("long")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        input.navigator()->buttonEvent(nexora::input::GamepadButton::Select, true);
        input.navigator()->buttonEvent(nexora::input::GamepadButton::Start, true);
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(app.statusMessage().contains(QStringLiteral("stopped")));
        QVERIFY(!input.isSuspended());
    }

    void stopAndBusyGuards()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});

        QVERIFY(!app.stop());
        QVERIFY(app.launch(QStringLiteral("long")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        QVERIFY(!app.launch(QStringLiteral("long")));
        QVERIFY(app.statusIsError());
        QVERIFY(!app.reloadConfig());
        QVERIFY(!app.launch(QStringLiteral("unknown")));

        QVERIFY(app.stop());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(app.statusMessage().contains(QStringLiteral("stopped")));
        QVERIFY(app.reloadConfig());
    }
};

QTEST_GUILESS_MAIN(TestAppController)
#include "tst_appcontroller.moc"
