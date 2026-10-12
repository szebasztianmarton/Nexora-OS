// AppController wired to a temporary configuration that uses the fake emulator.
#include "AppController.h"
#include "input/GamepadNavigator.h"
#include "input/InputBackend.h"
#include "input/InputController.h"

#include "nexora/emulator/LaunchHistory.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using nexora::ui::AppController;
using nexora::ui::EmulatorListModel;

namespace {

constexpr int Timeout = 10000;

void writeConfig(const QTemporaryDir &dir, const QString &id, const QString &executable, const QString &arguments = {},
                 bool enabled = true, const QString &extraJson = {})
{
    QFile file(dir.filePath(id + QStringLiteral(".json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(
        QStringLiteral("{\"id\":\"%1\",\"name\":\"Emu %1\",\"executable\":\"%2\",\"arguments\":[%3],\"enabled\":%4%5}")
            .arg(id, executable, arguments, enabled ? QStringLiteral("true") : QStringLiteral("false"),
                 extraJson.isEmpty() ? QString() : QStringLiteral(",") + extraJson)
            .toUtf8());
}

QString statusOf(AppController &app, const QString &id)
{
    EmulatorListModel *model = app.emulators();
    return model->index(model->indexOf(id)).data(EmulatorListModel::StatusRole).toString();
}

} // namespace

// A gamepad backend whose device list the test controls directly, so a "disconnect" can be
// simulated without real hardware: change deviceList() then emit statusChanged().
class ControllableBackend : public nexora::input::InputBackend
{
public:
    using InputBackend::InputBackend;
    bool start() override { return true; }
    QString name() const override { return QStringLiteral("fake"); }
    QString statusText() const override { return QStringLiteral("fake pad"); }
    QList<DeviceInfo> devices() const override { return m_devices; }
    void setDevices(QList<DeviceInfo> devices)
    {
        m_devices = std::move(devices);
        emit statusChanged();
    }

private:
    QList<DeviceInfo> m_devices;
};

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
        QVERIFY(app.statusMessage().contains(QStringLiteral("failed during startup (exit 7)")));
        QCOMPARE(statusOf(app, QStringLiteral("bad")), QStringLiteral("failed"));
        QCOMPARE(statusOf(app, QStringLiteral("gone")), QStringLiteral("unavailable"));

        QVERIFY(app.launch(QStringLiteral("good")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(!app.statusIsError());
    }

    void launchHistoryRecordsAfterARun()
    {
        QTemporaryDir dir, stateDir;
        writeConfig(dir, QStringLiteral("ok"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--exit-code\",\"3\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        nexora::emulator::LaunchHistory history(stateDir.filePath(QStringLiteral("launch-history.json")));
        app.setLaunchHistory(&history);
        QSignalSpy historyChanged(&app, &AppController::launchHistoryChanged);

        QVERIFY(app.launch(QStringLiteral("ok")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("failed"), Timeout);
        QTRY_COMPARE_WITH_TIMEOUT(historyChanged.size(), 1, Timeout);

        const QVariantList recorded = app.launchHistory();
        QCOMPARE(recorded.size(), 1);
        const QVariantMap record = recorded.first().toMap();
        QCOMPARE(record.value(QStringLiteral("emulatorId")).toString(), QStringLiteral("ok"));
        QCOMPARE(record.value(QStringLiteral("gameTitle")).toString(), QStringLiteral("Emu ok"));
        QCOMPARE(record.value(QStringLiteral("exitCode")).toInt(), 3);
        QVERIFY(!record.value(QStringLiteral("startedAt")).toString().isEmpty());

        QCOMPARE(history.records().size(), 1);
    }

    void pauseAndResumeWithPausableConfig()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("pausable"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""), true, QStringLiteral("\"pausable\":true"));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        QSignalSpy pausedSpy(&app, &AppController::pausedChanged);

        QVERIFY(!app.canPause()); // nothing running yet
        QVERIFY(!app.pause());
        QVERIFY(app.launch(QStringLiteral("pausable")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        QVERIFY(app.canPause());
        QVERIFY(!app.paused());

        QVERIFY(app.pause());
        QTRY_VERIFY_WITH_TIMEOUT(app.paused(), Timeout);
        QVERIFY(app.canPause());
        QCOMPARE(pausedSpy.size(), 1);

        QVERIFY(app.resume());
        QTRY_VERIFY_WITH_TIMEOUT(!app.paused(), Timeout);
        QCOMPARE(pausedSpy.size(), 2);

        QVERIFY(app.stop());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
        QVERIFY(!app.canPause());
    }

    void pauseRejectedWhenNotPausable()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("plain"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        QVERIFY(app.launch(QStringLiteral("plain")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        QVERIFY(!app.canPause());
        QVERIFY(!app.pause());
        QVERIFY(app.stop());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
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

    void controllerDisconnectNoticeWhileRunning()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral(NEXORA_FAKE_EMULATOR),
                    QStringLiteral("\"--sleep-ms\",\"30000\""));
        AppController app({dir.path(), QStringLiteral("test"), {}, false});
        nexora::input::InputController input;
        auto backend = std::make_unique<ControllableBackend>(input.navigator());
        ControllableBackend *rawBackend = backend.get();
        rawBackend->setDevices({{QStringLiteral("pad0"), QStringLiteral("Test Pad")}});
        QVERIFY(input.start(std::move(backend)));
        app.setInputController(&input);

        QVERIFY(app.launch(QStringLiteral("long")));
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("running"), Timeout);
        QSignalSpy statusSpy(&app, &AppController::statusMessageChanged);
        rawBackend->setDevices({}); // simulate the pad going away mid-run
        QTRY_VERIFY_WITH_TIMEOUT(statusSpy.size() > 0, Timeout);
        QVERIFY(app.statusMessage().contains(QStringLiteral("Controller disconnected")));
        QVERIFY(app.statusMessage().contains(QStringLiteral("Test Pad")));
        QVERIFY(app.statusIsError());

        QVERIFY(app.stop());
        QTRY_COMPARE_WITH_TIMEOUT(app.state(), QStringLiteral("exited"), Timeout);
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
