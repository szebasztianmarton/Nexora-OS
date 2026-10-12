// AppController wired to a temporary configuration that uses the fake emulator.
#include "AppController.h"
#include "GameBarController.h"
#include "input/GamepadNavigator.h"
#include "input/InputController.h"

#include <QDir>
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

void write(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
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

        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});
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
        AppController app({dir.filePath(QStringLiteral("absent")), QStringLiteral("test"), {}, false, {}});
        QVERIFY(!app.configDirectoryFound());
        QCOMPARE(app.emulators()->rowCount(), 0);
        QVERIFY(app.statusIsError());
    }

    void launchAndReturn()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("ok"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--stdout\",\"hi\",\"--sleep-ms\",\"200\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});
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
        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});

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
        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});
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
        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});
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

    void gameBarOverRunningEmulator()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        QDir(dir.path()).mkpath(QStringLiteral("proc"));
        nexora::system::SystemEnvironment env;
        env.procRoot = dir.filePath(QStringLiteral("proc"));
        nexora::system::DesktopSystemIntegration system(env);
        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});
        nexora::input::InputController input;
        QVERIFY(input.start(QStringLiteral("none")));
        app.setInputController(&input);
        nexora::ui::GameBarController bar(&app, &system, &input);
        QSignalSpy openSpy(&bar, &nexora::ui::GameBarController::openChanged);
        QCOMPARE(bar.sessionSeconds(), -1);

        QVERIFY(app.launch(QStringLiteral("long")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        QVERIFY(input.isSuspended());
        QVERIFY(bar.sessionSeconds() >= 0);

        // Guide opens the bar on top of the game and hands the pad back to the launcher.
        input.navigator()->buttonEvent(nexora::input::GamepadButton::Guide, true);
        QVERIFY(bar.isOpen());
        QCOMPARE(openSpy.size(), 1);
        QVERIFY(!input.isSuspended());

        // Performance figures: a CPU percentage needs two samples.
        write(env.procRoot + QStringLiteral("/stat"), "cpu  100 0 100 800 0 0 0 0 0 0\n");
        write(env.procRoot + QStringLiteral("/meminfo"), "MemTotal: 4194304 kB\nMemAvailable: 1048576 kB\n");
        bar.refresh();
        QCOMPARE(bar.cpuPercent(), -1);
        write(env.procRoot + QStringLiteral("/stat"), "cpu  150 0 150 900 0 0 0 0 0 0\n");
        bar.refresh();
        QCOMPARE(bar.cpuPercent(), 50);
        QCOMPARE(bar.cpuHistory(), QVariantList{50});
        QCOMPARE(bar.memoryPercent(), 75);
        QCOMPARE(bar.memoryUsedMiB(), 3072);
        QCOMPARE(bar.memoryTotalMiB(), 4096);
        for (int i = 0; i < nexora::ui::GameBarController::kHistoryLength + 5; ++i) {
            write(env.procRoot + QStringLiteral("/stat"),
                  QStringLiteral("cpu  %1 0 0 %1 0 0 0 0 0 0\n").arg(1000 + i * 10).toLatin1());
            bar.refresh();
        }
        QCOMPARE(bar.cpuHistory().size(), nexora::ui::GameBarController::kHistoryLength);

        // Guide again closes it: the game gets the pad back.
        input.navigator()->buttonEvent(nexora::input::GamepadButton::Guide, false);
        input.navigator()->buttonEvent(nexora::input::GamepadButton::Guide, true);
        QVERIFY(!bar.isOpen());
        QVERIFY(input.isSuspended());

        // Stopping the game from the open bar leaves the launcher with the pad.
        bar.toggle();
        QVERIFY(app.stop());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(!input.isSuspended());
        QCOMPARE(bar.sessionSeconds(), -1);
        bar.close();
        QVERIFY(!input.isSuspended());
    }

    void stopAndBusyGuards()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false, {}});

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
