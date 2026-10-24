// The full backend object graph used by the UI (UiContext), with the fake emulator,
// temporary settings/database files and a temporary game directory.
#include "SystemListModel.h"
#include "UiContext.h"
#include "input/GamepadNavigator.h"
#include "input/InputBackend.h"
#include "input/InputController.h"

#include "nexora/shared/DataLayout.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora;
using nexora::shared::Settings;

namespace {

constexpr int Timeout = 10000;

void write(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

} // namespace

// Stands in for a gamepad backend so remap capture can be exercised without hardware.
class FakeBackend : public input::InputBackend
{
public:
    using InputBackend::InputBackend;
    bool start() override { return true; }
    QString name() const override { return QStringLiteral("fake"); }
    QString statusText() const override { return QStringLiteral("fake pad"); }
    QList<DeviceInfo> devices() const override { return {{QStringLiteral("fake0"), QStringLiteral("Fake Pad")}}; }
};

// A backend whose device list a test can change, to simulate plugging pads in and out.
class SwitchableBackend : public input::InputBackend
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

class TestUiContext : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString configDir() const { return m_dir.filePath(QStringLiteral("emulators")); }
    QString gamesDir() const { return m_dir.filePath(QStringLiteral("games")); }
    shared::DataLayout layout() const { return shared::DataLayout::underRoot(m_dir.filePath(QStringLiteral("state"))); }
    // BackupService always restores to <layout.configDir>/settings.json, so the settings
    // userFile has to be that exact path for backup/restore to round-trip anything real.
    QString settingsFile() const { return layout().configDir + QStringLiteral("/settings.json"); }

    ui::UiContext::Options options() const
    {
        ui::UiContext::Options o;
        o.app = {configDir(), QStringLiteral("test"), {}, false, {}};
        o.settings = {{m_dir.filePath(QStringLiteral("system-defaults.json"))}, settingsFile()};
        o.layout = layout();
        o.databasePath = layout().dataDir + QStringLiteral("/library.sqlite");
        o.platformOverride = QStringLiteral("desktop");
        return o;
    }

    void writeEmulator(const QByteArray &extra = {})
    {
        write(configDir() + QStringLiteral("/fake.json"),
              "{\"id\":\"fake\",\"name\":\"Fake\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
              "\"arguments\":[\"--echo-args\",\"{file}\"],\"extensions\":[\".rom\"],"
              "\"profiles\":[{\"id\":\"long\",\"name\":\"Long\",\"arguments\":[\"--sleep-ms\",\"30000\"]},"
              "{\"id\":\"off\",\"name\":\"Off\",\"enabled\":false}]"
                  + extra + "}");
    }

    static void waitForScan(ui::UiContext &ctx)
    {
        ctx.gameLibrary().waitForScan();
        QTRY_VERIFY_WITH_TIMEOUT(!ctx.library()->isScanning(), Timeout);
    }

private slots:
    void init()
    {
        QDir(m_dir.path()).removeRecursively();
        QDir().mkpath(gamesDir());
        writeEmulator();
        write(gamesDir() + QStringLiteral("/Space Quest.rom"), "rom");
        write(gamesDir() + QStringLiteral("/Alpha.rom"), "rom");
        write(gamesDir() + QStringLiteral("/readme.txt"), "not a game");
    }

    void gameOptionsDefaultProfilesAndPurge()
    {
        // A second emulator for the same extension makes the emulator choice meaningful.
        write(configDir() + QStringLiteral("/fake2.json"),
              "{\"id\":\"fake2\",\"name\":\"Fake Two\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
              "\"extensions\":[\".rom\"]}");
        qint64 alpha = 0;
        {
            ui::UiContext ctx(options());
            ctx.start();
            QVERIFY(ctx.library()->addDirectory(gamesDir()));
            waitForScan(ctx);
            for (const auto &e : ctx.gameLibrary().entries()) {
                if (e.title == QStringLiteral("Alpha"))
                    alpha = e.id;
            }
            QVERIFY(alpha > 0);

            QVariantMap details = ctx.library()->gameDetails(alpha);
            QCOMPARE(details.value(QStringLiteral("title")).toString(), QStringLiteral("Alpha"));
            QCOMPARE(details.value(QStringLiteral("emulators")).toList().size(), 2);
            QVERIFY(details.value(QStringLiteral("automatic")).toBool());
            QCOMPARE(details.value(QStringLiteral("emulatorId")).toString(), QStringLiteral("fake"));
            // Only enabled profiles of the effective emulator are offered.
            const QVariantList profiles = details.value(QStringLiteral("profiles")).toList();
            QCOMPARE(profiles.size(), 1);
            QCOMPARE(profiles.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("long"));
            QVERIFY(!profiles.first().toMap().value(QStringLiteral("isDefault")).toBool());

            // Emulator choice: only candidates for the extension are accepted.
            QSignalSpy notices(ctx.library(), &ui::LibraryController::notice);
            QVERIFY(!ctx.library()->setGameEmulator(alpha, QStringLiteral("unknown")));
            QVERIFY(notices.last().at(1).toBool());
            QVERIFY(ctx.library()->setGameEmulator(alpha, QStringLiteral("fake2")));
            details = ctx.library()->gameDetails(alpha);
            QCOMPARE(details.value(QStringLiteral("emulatorId")).toString(), QStringLiteral("fake2"));
            QVERIFY(!details.value(QStringLiteral("automatic")).toBool());
            QCOMPARE(
                ctx.library()->games()->data(ctx.library()->games()->index(ctx.library()->games()->indexOfGame(alpha)),
                                             ui::GameListModel::EmulatorIdRole),
                QVariant(QStringLiteral("fake2")));
        }
        ui::UiContext ctx(options());
        ctx.start();
        // The choice survived the restart; back to automatic.
        QCOMPARE(ctx.library()->gameDetails(alpha).value(QStringLiteral("emulatorId")).toString(),
                 QStringLiteral("fake2"));
        QVERIFY(ctx.library()->setGameEmulator(alpha, QString()));
        QVERIFY(ctx.library()->gameDetails(alpha).value(QStringLiteral("automatic")).toBool());

        // Default profile: used by a plain launch, skipped by an explicit "no profile" launch.
        QVERIFY(ctx.settings()->setDefaultProfile(QStringLiteral("fake"), QStringLiteral("long")));
        QCOMPARE(ctx.settings()->defaultProfile(QStringLiteral("fake")), QStringLiteral("long"));
        QVERIFY(ctx.library()
                    ->gameDetails(alpha)
                    .value(QStringLiteral("profiles"))
                    .toList()
                    .first()
                    .toMap()
                    .value(QStringLiteral("isDefault"))
                    .toBool());
        auto *manager = ctx.app()->manager();
        QSignalSpy finished(manager, &emulator::EmulatorManager::processFinished);
        QVERIFY(ctx.library()->launch(alpha));
        QTRY_COMPARE_WITH_TIMEOUT(manager->state(), emulator::ProcessState::Running, Timeout);
        QTest::qWait(200);
        QCOMPARE(manager->state(), emulator::ProcessState::Running); // "long" sleeps
        QVERIFY(ctx.app()->stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);

        QVERIFY(ctx.library()->launchWithProfile(alpha, QString()));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, Timeout);
        QCOMPARE(manager->lastOutcome().finalState, emulator::ProcessState::Exited);
        QVERIFY2(
            manager->capturedStdout().contains(QStringLiteral("arg: ") + gamesDir() + QStringLiteral("/Alpha.rom")),
            qPrintable(manager->capturedStdout()));

        QVERIFY(ctx.settings()->setDefaultProfile(QStringLiteral("fake"), QString()));
        QCOMPARE(ctx.settings()->defaultProfile(QStringLiteral("fake")), QString());

        // Missing games are kept until removed explicitly.
        QCOMPARE(ctx.library()->missingCount(), 0);
        QVERIFY(QFile::remove(gamesDir() + QStringLiteral("/Space Quest.rom")));
        QVERIFY(ctx.library()->refresh());
        waitForScan(ctx);
        QCOMPARE(ctx.library()->missingCount(), 1);
        QCOMPARE(ctx.library()->purgeMissing(), 1);
        QCOMPARE(ctx.library()->missingCount(), 0);
        QCOMPARE(ctx.library()->totalCount(), 1);
    }

    void settingsPersistAcrossRestarts()
    {
        {
            ui::UiContext ctx(options());
            ctx.start();
            QCOMPARE(ctx.settings()->volume(), 80);
            ctx.settings()->setVolume(35);
            ctx.settings()->cycle(Settings::DisplayMode, 1);
            QVERIFY(ctx.settings()->remap(QStringLiteral("accept"), QStringLiteral("east")));
            QVERIFY(ctx.library()->addDirectory(gamesDir()));
            waitForScan(ctx);
        }
        ui::UiContext again(options());
        again.start();
        QCOMPARE(again.settings()->volume(), 35);
        QCOMPARE(again.settings()->displayMode(), QStringLiteral("windowed"));
        QCOMPARE(again.library()->directories(), QStringList{gamesDir()});
        const auto rows = again.settings()->mappingRows();
        bool acceptOnEast = false;
        bool backOnSouth = false;
        for (const auto &r : rows) {
            const auto m = r.toMap();
            acceptOnEast |= m.value(QStringLiteral("action")) == QStringLiteral("accept")
                && m.value(QStringLiteral("button")) == QStringLiteral("east");
            backOnSouth |= m.value(QStringLiteral("action")) == QStringLiteral("back")
                && m.value(QStringLiteral("button")) == QStringLiteral("south");
        }
        QVERIFY(acceptOnEast);
        QVERIFY(backOnSouth); // swapped, still reachable
        // Library content came back from SQLite without needing a rescan.
        QVERIFY(!again.gameLibrary().needsRescan());
        QCOMPARE(again.library()->games()->rowCount(), 2);
    }

    void removedGamePersistsAcrossRestart()
    {
        qint64 id = 0;
        {
            ui::UiContext ctx(options());
            ctx.start();
            QVERIFY(ctx.library()->addDirectory(gamesDir()));
            waitForScan(ctx);
            id = ctx.library()->games()->gameIdAt(0);
            QVERIFY(ctx.library()->removeGame(id));
            QCOMPARE(ctx.library()->games()->rowCount(), 1);
        }
        ui::UiContext again(options());
        again.start();
        waitForScan(again);
        QCOMPARE(again.library()->games()->rowCount(), 1);
        for (int i = 0; i < again.library()->games()->rowCount(); ++i)
            QVERIFY(again.library()->games()->gameIdAt(i) != id);
        // Restoring it brings it back.
        QCOMPARE(again.library()->restoreRemovedGames(), 1);
        QCOMPARE(again.library()->games()->rowCount(), 2);
    }

    void gameDetailsHasExpectedKeysAndRedactsThePath()
    {
        const QByteArray previousHome = qgetenv("HOME");
        qputenv("HOME", m_dir.path().toUtf8()); // so defaultRedactionRules() matches gamesDir()

        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        const qint64 id = ctx.library()->games()->gameIdAt(0);

        const QVariantMap details = ctx.library()->gameDetails(id);
        for (const char *key : {"id", "title", "originalTitle", "path", "systemId", "systemName", "systemCandidates",
                                "emulatorId", "emulatorName", "emulatorCandidates", "favorite", "playCount",
                                "lastPlayed", "addedAt", "present", "notes"}) {
            QVERIFY2(details.contains(QLatin1String(key)), key);
        }
        QCOMPARE(details.value(QStringLiteral("id")).toLongLong(), id);
        QVERIFY(details.value(QStringLiteral("present")).toBool());
        QVERIFY2(details.value(QStringLiteral("path")).toString().startsWith(QLatin1Char('~')),
                 qPrintable(details.value(QStringLiteral("path")).toString()));
        QVERIFY(!details.value(QStringLiteral("path")).toString().contains(m_dir.path()));
        QCOMPARE(ctx.library()->gameDetails(-1), QVariantMap());

        qputenv("HOME", previousHome);
    }

    void corruptedSettingsFallBackToDefaults()
    {
        write(settingsFile(), "{ definitely not json");
        write(m_dir.filePath(QStringLiteral("system-defaults.json")),
              "{\"version\":1,\"settings\":{\"audio.volume\":55,\"library.directories\":[\"" + gamesDir().toUtf8()
                  + "\"]}}");
        ui::UiContext ctx(options());
        ctx.start();
        QCOMPARE(ctx.settings()->volume(), 55); // system default still applies
        QVERIFY(!ctx.settings()->issues().isEmpty());
        waitForScan(ctx);
        QCOMPARE(ctx.library()->games()->rowCount(), 2); // directory from system defaults
    }

    void invalidMappingIsReportedAndIgnored()
    {
        write(settingsFile(),
              "{\"version\":1,\"settings\":{\"input.mapping\":{\"accept\":\"south\",\"back\":\"south\"}}}");
        input::InputController input;
        ui::UiContext ctx(options(), &input);
        ctx.start();
        QVERIFY(ctx.settings()->issues().join(u'\n').contains(QStringLiteral("Controller mapping ignored")));
        QVERIFY(input.mapping() == input::ButtonMapping::defaults());
        // Remapping from the UI repairs it.
        QVERIFY(ctx.settings()->remap(QStringLiteral("back"), QStringLiteral("east")));
        QVERIFY(ctx.settings()->issues().isEmpty());
    }

    // Controller plug/unplug reach the notification service; its settings apply live and
    // warnings survive a restart.
    void controllerEventsBecomeNotifications()
    {
        input::InputController input;
        auto backend = std::make_unique<SwitchableBackend>(input.navigator());
        SwitchableBackend *pads = backend.get();
        QVERIFY(input.start(std::move(backend)));
        {
            ui::UiContext ctx(options(), &input);
            ctx.start();
            auto *notifications = ctx.notifications();
            pads->setDevices({{QStringLiteral("pad0"), QStringLiteral("Test Pad")}});
            QTRY_COMPARE_WITH_TIMEOUT(notifications->current().value(QStringLiteral("text")).toString(),
                                      QStringLiteral("Controller connected: Test Pad"), Timeout);
            notifications->dismiss();
            pads->setDevices({});
            QTRY_COMPARE_WITH_TIMEOUT(notifications->current().value(QStringLiteral("text")).toString(),
                                      QStringLiteral("Controller disconnected: Test Pad"), Timeout);
            QCOMPARE(notifications->current().value(QStringLiteral("severity")).toString(), QStringLiteral("warning"));
            notifications->dismiss();

            QVERIFY(ctx.settingsStore().setValue(shared::Settings::NotificationsHiddenCategories,
                                                 QStringList{QStringLiteral("controller")}));
            pads->setDevices({{QStringLiteral("pad1"), QStringLiteral("Other Pad")}});
            QTRY_VERIFY_WITH_TIMEOUT(notifications->history().size() == 3, Timeout);
            QVERIFY(notifications->current().isEmpty()); // hidden category: history only
            QVERIFY(ctx.settingsStore().setValue(shared::Settings::NotificationsQuiet, true));
            QVERIFY(notifications->quiet());
        }
        ui::UiContext again(options(), &input);
        again.start();
        bool found = false;
        for (const QVariant &v : again.notifications()->history()) {
            const QVariantMap n = v.toMap();
            if (n.value(QStringLiteral("text")).toString() == QStringLiteral("Controller disconnected: Test Pad"))
                found = n.value(QStringLiteral("fromPreviousRun")).toBool();
        }
        QVERIFY(found);
    }

    void deadzoneIsAppliedLive()
    {
        input::InputController input;
        ui::UiContext ctx(options(), &input);
        ctx.start();
        // The stored default (25%) reaches the navigator at start-up.
        QCOMPARE(ctx.settings()->deadzone(), 25);
        QCOMPARE(input.navigator()->deadzone(), 0.25);

        ctx.settings()->setDeadzone(40);
        QCOMPARE(ctx.settings()->deadzone(), 40);
        QCOMPARE(input.navigator()->deadzone(), 0.4);
        QCOMPARE(input.navigator()->axisPressThreshold(), 0.64);

        ctx.settings()->setDeadzone(99); // bounded to the 5-60 range the setting allows
        QCOMPARE(ctx.settings()->deadzone(), 60);
        QCOMPARE(input.navigator()->deadzone(), 0.6);

        // And it survives a restart.
        ui::UiContext again(options(), &input);
        again.start();
        QCOMPARE(again.settings()->deadzone(), 60);
        QCOMPARE(input.navigator()->deadzone(), 0.6);
    }

    void remapCapturesNextButton()
    {
        input::InputController input;
        ui::UiContext noPad(options(), &input);
        QVERIFY(!noPad.settings()->controllerAvailable()); // backend "none": capture not offered
        QVERIFY(!noPad.settings()->beginRemap(QStringLiteral("favorite")));
        QVERIFY(input.start(std::make_unique<FakeBackend>(input.navigator())));
        ui::UiContext ctx(options(), &input);
        ctx.start();
        QVERIFY(ctx.settings()->beginRemap(QStringLiteral("favorite")));
        QCOMPARE(ctx.settings()->capturingAction(), QStringLiteral("favorite"));
        QVERIFY(input.isCapturing());
        QSignalSpy delivered(&input, &input::InputController::actionDelivered);
        input.navigator()->buttonEvent(input::GamepadButton::L1, true);
        QTRY_VERIFY(ctx.settings()->capturingAction().isEmpty());
        QVERIFY(!input.isCapturing());
        QCOMPARE(input.mapping().buttonFor(input::NavAction::Favorite), std::optional(input::GamepadButton::L1));
        QCOMPARE(input.mapping().buttonFor(input::NavAction::PrevTab), std::optional(input::GamepadButton::North));
        QVERIFY(!ctx.settings()->beginRemap(QStringLiteral("fly")));
        QCOMPARE(delivered.size(), 0); // L1 (old prevTab, new favorite) was not also acted on

        // Remapping Confirm: the completing press must not act as Confirm right away.
        QVERIFY(ctx.settings()->beginRemap(QStringLiteral("accept")));
        input.navigator()->buttonEvent(input::GamepadButton::West, true);
        QTRY_VERIFY(ctx.settings()->capturingAction().isEmpty());
        QCoreApplication::processEvents();
        QCOMPARE(delivered.size(), 0);
        QCOMPARE(input.mapping().buttonFor(input::NavAction::Accept), std::optional(input::GamepadButton::West));
    }

    void launchGameFromLibrary()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        auto *games = ctx.library()->games();
        QCOMPARE(games->rowCount(), 2);
        QCOMPARE(games->index(0).data(ui::GameListModel::TitleRole).toString(), QStringLiteral("Alpha"));
        QVERIFY(games->index(0).data(ui::GameListModel::AvailableRole).toBool());
        QCOMPARE(games->index(0).data(ui::GameListModel::EmulatorNameRole).toString(), QStringLiteral("Fake"));
        const qint64 id = games->gameIdAt(1);

        QVERIFY(ctx.library()->toggleFavorite(id));
        QCOMPARE(ctx.library()->favoriteGames()->rowCount(), 1);

        QVERIFY(ctx.library()->launch(id));
        QCOMPARE(ctx.app()->activeEmulatorName(), QStringLiteral("Space Quest (Fake)"));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("exited"), Timeout);
        QVERIFY2(ctx.app()->lastOutput().contains(
                     QStringLiteral("arg: ")
                     + QFileInfo(gamesDir() + QStringLiteral("/Space Quest.rom")).canonicalFilePath()),
                 qPrintable(ctx.app()->lastOutput()));
        QCOMPARE(ctx.library()->recentGames()->rowCount(), 1);
        QCOMPARE(ctx.library()->recentGames()->gameIdAt(0), id);
        QCOMPARE(ctx.app()->launchHistory().size(), 1);

        // Search and filters drive the visible list.
        ctx.library()->setSearchText(QStringLiteral("space"));
        QTRY_COMPARE_WITH_TIMEOUT(games->rowCount(), 1, Timeout);
        ctx.library()->setSearchText({});
        ctx.library()->setFilter(QStringLiteral("favorites"));
        QTRY_COMPARE_WITH_TIMEOUT(games->rowCount(), 1, Timeout);
        ctx.library()->setFilter(QStringLiteral("all"));
        QTRY_COMPARE_WITH_TIMEOUT(games->rowCount(), 2, Timeout);
    }

    // Guide opens the quick menu (not the game bar) while an emulator runs, toggles the bar
    // otherwise; holding Start opens the menu too; both keep input flowing to the shell.
    void quickMenuOpensOnlyWhileAnEmulatorRuns()
    {
        input::InputController input;
        QVERIFY(input.start(std::make_unique<FakeBackend>(input.navigator())));
        input.navigator()->setMenuHoldDuration(std::chrono::milliseconds(50));
        ui::UiContext ctx(options(), &input);
        ctx.start();
        auto *bar = ctx.gameBar();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        const qint64 id = ctx.library()->games()->gameIdAt(0);

        // Idle: no quick menu; Guide toggles the bar.
        QVERIFY(!bar->openQuickMenu());
        QVERIFY(!bar->isQuickMenuOpen());
        input.injectAction(input::NavAction::GameBar);
        QVERIFY(bar->isOpen());
        input.injectAction(input::NavAction::GameBar);
        QVERIFY(!bar->isOpen());

        QVERIFY(ctx.library()->launchWithProfile(id, QStringLiteral("long")));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("running"), Timeout);
        QVERIFY(input.isSuspended());

        // Running: Guide opens the quick menu; input resumes; Guide again closes it.
        QSignalSpy menuChanged(bar, &ui::GameBarController::quickMenuOpenChanged);
        input.injectAction(input::NavAction::GameBar);
        QVERIFY(bar->isQuickMenuOpen());
        QVERIFY(!bar->isOpen());
        QVERIFY(!input.isSuspended());
        input.injectAction(input::NavAction::GameBar);
        QVERIFY(!bar->isQuickMenuOpen());
        QVERIFY(input.isSuspended());
        QCOMPARE(menuChanged.size(), 2);

        // Holding Start (while suspended) opens it as well.
        input.navigator()->buttonEvent(input::GamepadButton::Start, true);
        QTRY_VERIFY_WITH_TIMEOUT(bar->isQuickMenuOpen(), Timeout);
        input.navigator()->buttonEvent(input::GamepadButton::Start, false);
        QVERIFY(!input.isSuspended());

        // Ctrl+G's toggle() still reaches the bar, which replaces the menu.
        bar->toggle();
        QVERIFY(bar->isOpen());
        bar->close();
        QVERIFY(bar->openQuickMenu());

        // The menu closes by itself once the emulator is gone.
        QVERIFY(ctx.app()->stop());
        QTRY_VERIFY_WITH_TIMEOUT(!ctx.app()->isBusy(), Timeout);
        QVERIFY(!bar->isQuickMenuOpen());
    }

    void presentRoleAndSystemNames()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        QCOMPARE(ctx.library()->games()->rowCount(), 2);
        QVERIFY(ctx.library()->games()->index(0).data(ui::GameListModel::PresentRole).toBool());
        QVERIFY(ctx.library()->games()->roleNames().values().contains("present"));

        QVERIFY(QFile::remove(gamesDir() + QStringLiteral("/Alpha.rom")));
        QVERIFY(ctx.library()->refresh());
        waitForScan(ctx);
        ctx.library()->setFilter(QStringLiteral("missing"));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.library()->games()->rowCount(), 1, Timeout);
        QVERIFY(!ctx.library()->games()->index(0).data(ui::GameListModel::PresentRole).toBool());

        QCOMPARE(ctx.library()->systemName({}), QString());
        QCOMPARE(ctx.library()->systemName(QStringLiteral("no-such-system")), QString());
        QVERIFY(ctx.library()->systems()->rowCount() > 0);
        const QModelIndex first = ctx.library()->systems()->index(0);
        QCOMPARE(ctx.library()->systemName(first.data(ui::SystemListModel::SystemIdRole).toString()),
                 first.data(ui::SystemListModel::NameRole).toString());
    }

    void failedStartIsNotPlayed()
    {
        // Executable bit set but not a program: passes validation, then fails to start.
        const QString broken = m_dir.filePath(QStringLiteral("broken-emulator"));
        write(broken, QByteArray(64, '\x01'));
        QFile::setPermissions(broken, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        write(configDir() + QStringLiteral("/fake.json"),
              "{\"id\":\"fake\",\"name\":\"Fake\",\"executable\":\"" + broken.toUtf8()
                  + "\",\"extensions\":[\".rom\"]}");
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        QVERIFY(ctx.library()->launch(ctx.library()->games()->gameIdAt(0)));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("failed"), Timeout);
        QCOMPARE(ctx.library()->recentGames()->rowCount(), 0);
        QCOMPARE(ctx.gameLibrary().entries().first().playCount, 0);
    }

    void missingGameFileAndRecovery()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        const qint64 id = ctx.library()->games()->gameIdAt(0);
        QVERIFY(QFile::remove(gamesDir() + QStringLiteral("/Alpha.rom")));
        // Deleted after the scan: the launch is refused cleanly, the UI keeps working.
        QVERIFY(!ctx.library()->launch(id));
        QCOMPARE(ctx.app()->state(), QStringLiteral("failed"));
        QVERIFY(ctx.app()->statusMessage().contains(QStringLiteral("Game file is missing")));
        QVERIFY(ctx.library()->refresh());
        waitForScan(ctx);
        QCOMPARE(ctx.library()->games()->rowCount(), 1);
        QVERIFY(ctx.library()->launch(ctx.library()->games()->gameIdAt(0)));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("exited"), Timeout);
    }

    void profilesAndDefaults()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(!ctx.app()->launchProfile(QStringLiteral("fake"), QStringLiteral("off")));
        QVERIFY(ctx.app()->statusMessage().contains(QStringLiteral("Launch profile is disabled")));
        QVERIFY(!ctx.app()->launchProfile(QStringLiteral("fake"), QStringLiteral("nope")));
        QVERIFY(ctx.app()->statusMessage().contains(QStringLiteral("Launch profile not found")));

        // Default profile from settings is used by launch().
        QVERIFY(ctx.settingsStore().setValue(Settings::EmulatorDefaultProfiles,
                                             QVariantMap{{QStringLiteral("fake"), QStringLiteral("long")}}));
        QVERIFY(ctx.app()->launch(QStringLiteral("fake")));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("running"), Timeout);
        QVERIFY(ctx.app()->stop());
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("exited"), Timeout);

        // A stale default (disabled profile) falls back to the emulator's arguments.
        QVERIFY(ctx.settingsStore().setValue(Settings::EmulatorDefaultProfiles,
                                             QVariantMap{{QStringLiteral("fake"), QStringLiteral("off")}}));
        QVERIFY(ctx.app()->launch(QStringLiteral("fake")));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("exited"), Timeout);
    }

    void recoversAfterConfigurationErrors()
    {
        write(configDir() + QStringLiteral("/fake.json"), "{ broken");
        ui::UiContext ctx(options());
        ctx.start();
        QCOMPARE(ctx.app()->emulators()->rowCount(), 0);
        QVERIFY(!ctx.app()->configIssues().isEmpty());
        writeEmulator();
        QVERIFY(ctx.app()->reloadConfig());
        QCOMPARE(ctx.app()->emulators()->rowCount(), 1);
        QVERIFY(ctx.app()->configIssues().isEmpty());
        QVERIFY(ctx.app()->launch(QStringLiteral("fake")));
        QTRY_COMPARE_WITH_TIMEOUT(ctx.app()->state(), QStringLiteral("exited"), Timeout);
    }

    void desktopSystemIsSafe()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QCOMPARE(ctx.system()->platform(), QStringLiteral("desktop"));
        QVERIFY(!ctx.system()->powerSupported());
        QVERIFY(!ctx.system()->powerOff());
        QVERIFY(!ctx.system()->lastError().isEmpty());
        QVERIFY(!ctx.system()->statusLines().isEmpty());
        QVERIFY(!ctx.settings()->volumeSupported());
    }

    void restoreBackupRoundTripsThroughTheController()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        ctx.settings()->setVolume(42);
        QCOMPARE(ctx.library()->games()->rowCount(), 2);

        const QString backupPath = ctx.system()->createBackup(false);
        QVERIFY2(!backupPath.isEmpty(), qPrintable(ctx.system()->lastError()));

        // Change things, then restore: settings and the library content come back.
        ctx.settings()->setVolume(7);
        const qint64 removedId = ctx.library()->games()->gameIdAt(0);
        QVERIFY(ctx.library()->removeGame(removedId));
        QCOMPARE(ctx.library()->games()->rowCount(), 1);

        QVERIFY2(ctx.system()->restoreBackup(backupPath), qPrintable(ctx.system()->lastError()));
        QCOMPARE(ctx.settings()->volume(), 42);
        QCOMPARE(ctx.library()->games()->rowCount(), 2);
    }

    // Reloading the Settings store is not enough: a restore must also push the restored
    // values back out to the live subsystems (volume/mute, controller mapping, deadzone,
    // log level), or the UI keeps running on stale state until something else re-triggers
    // it. Deadzone is a good probe: unlike volume it applies unconditionally (no platform
    // capability check), straight onto the live GamepadNavigator.
    void restoreBackupReappliesLiveSettingsNotJustTheStore()
    {
        input::InputController input;
        ui::UiContext ctx(options(), &input);
        ctx.start();
        ctx.settings()->setDeadzone(40);
        QCOMPARE(input.navigator()->deadzone(), 0.4);

        const QString backupPath = ctx.system()->createBackup(false);
        QVERIFY2(!backupPath.isEmpty(), qPrintable(ctx.system()->lastError()));

        // Diverge the live state from the backup without going through another backup.
        ctx.settings()->setDeadzone(10);
        QCOMPARE(input.navigator()->deadzone(), 0.1);

        QVERIFY2(ctx.system()->restoreBackup(backupPath), qPrintable(ctx.system()->lastError()));
        QCOMPARE(ctx.settings()->deadzone(), 40); // the store came back...
        QCOMPARE(input.navigator()->deadzone(), 0.4); // ...and so did the live navigator state
    }

    // Settings::valueChanged listeners (library directories, default profiles, notification
    // options) must see the restored values too: a restore that only emits reloaded()
    // leaves the LibraryController scanning the pre-restore directories.
    void restoreBackupNotifiesSettingsListeners()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(gamesDir()));
        waitForScan(ctx);
        QCOMPARE(ctx.library()->games()->rowCount(), 2);

        const QString backupPath = ctx.system()->createBackup(false);
        QVERIFY2(!backupPath.isEmpty(), qPrintable(ctx.system()->lastError()));

        QVERIFY(ctx.library()->removeDirectory(gamesDir()));
        QVERIFY(ctx.library()->directories().isEmpty());

        QSignalSpy directories(ctx.library(), &ui::LibraryController::directoriesChanged);
        QVERIFY2(ctx.system()->restoreBackup(backupPath), qPrintable(ctx.system()->lastError()));
        QCOMPARE(ctx.library()->directories(), QStringList{gamesDir()});
        QCOMPARE(directories.size(), 1);
    }

    // Exercises LibraryController::rebuildSystems() (the real production path, with a real
    // GameLibrary, system catalog and emulator set) rather than SystemListModel in
    // isolation: the "Other" row (unrecognised systemId) must appear exactly when there is
    // at least one such game, and disappear again once there is none.
    void updateRecordFromBootIsShownAtStartup()
    {
        // nexora-bootok records a rolled-back trial boot in stateDir/update/last-update.json;
        // the UI must report it from the moment it starts.
        ui::UiContext::Options o = options();
        o.update.stagingDir = o.layout.stateDir + QStringLiteral("/update");
        o.update.currentBuildId = QStringLiteral("v0.4.0-0-gcafebabe");
        const QString record = o.update.stagingDir + QStringLiteral("/last-update.json");
        write(record,
              R"({"format": 1, "result": "rolled-back", "previousBuildId": "v0.4.0-0-gcafebabe", )"
              R"("newBuildId": "v0.4.1-0-gabcdef0", "previousVersion": "0.4.0", "newVersion": "0.4.1", )"
              R"("slot": "a"})");
        {
            ui::UiContext ctx(o);
            ctx.start();
            QCOMPARE(ctx.updates()->state(), QStringLiteral("recoveryrequired"));
            QVERIFY(ctx.updates()->message().contains(QStringLiteral("0.4.1")));
        }
        {
            // A completed update (the running build is the new one) is reported too.
            write(record,
                  R"({"format": 1, "result": "ok", "previousBuildId": "v0.4.0-0-gcafebabe", )"
                  R"("newBuildId": "v0.4.1-0-gabcdef0", "previousVersion": "0.4.0", "newVersion": "0.4.1"})");
            o.update.currentBuildId = QStringLiteral("v0.4.1-0-gabcdef0");
            ui::UiContext ctx(o);
            ctx.start();
            QCOMPARE(ctx.updates()->state(), QStringLiteral("completed"));
        }
    }

    void startCreatesTheUpdateStateDirectory()
    {
        ui::UiContext::Options o = options();
        o.update.stagingDir = o.layout.stateDir + QStringLiteral("/update");
        QVERIFY(!QFileInfo::exists(o.update.stagingDir));
        ui::UiContext ctx(o);
        ctx.start();
        QVERIFY(QFileInfo(o.update.stagingDir).isDir());
    }

    void systemsModelShowsOtherBucketOnlyWhenUnrecognizedGamesExist()
    {
        // .sfc is uniquely "snes" in the built-in catalog; .rom is not in the catalog at
        // all, so a .rom game ends up with no recognised system ("Other").
        write(configDir() + QStringLiteral("/fake2.json"),
              "{\"id\":\"fake2\",\"name\":\"Fake2\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
              "\"extensions\":[\".sfc\"]}");
        QTemporaryDir mixedGames;
        write(mixedGames.filePath(QStringLiteral("Mario.sfc")), "rom");
        write(mixedGames.filePath(QStringLiteral("Alpha.rom")), "rom");

        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(mixedGames.path()));
        waitForScan(ctx);

        auto rowOf = [&](const QString &systemId) -> int {
            auto *model = ctx.library()->systems();
            for (int i = 0; i < model->rowCount(); ++i) {
                if (model->index(i).data(ui::SystemListModel::SystemIdRole).toString() == systemId)
                    return i;
            }
            return -1;
        };

        const int snesRow = rowOf(QStringLiteral("snes"));
        QVERIFY(snesRow >= 0);
        QCOMPARE(ctx.library()->systems()->index(snesRow).data(ui::SystemListModel::GameCountRole).toInt(), 1);
        const int otherRow = rowOf(QString());
        QVERIFY2(otherRow >= 0, "Other should be present: Alpha.rom has no recognised system");
        QCOMPARE(ctx.library()->systems()->index(otherRow).data(ui::SystemListModel::GameCountRole).toInt(), 1);

        // Hide the one unrecognised-system game: Other must disappear entirely, not shrink
        // to a zero-count row.
        qint64 romId = 0;
        for (int i = 0; i < ctx.library()->games()->rowCount() && romId == 0; ++i) {
            const qint64 id = ctx.library()->games()->gameIdAt(i);
            if (ctx.library()->gameDetails(id).value(QStringLiteral("systemId")).toString().isEmpty())
                romId = id;
        }
        QVERIFY(romId != 0);
        QVERIFY(ctx.library()->removeGame(romId));
        QCOMPARE(rowOf(QString()), -1);
        QVERIFY(rowOf(QStringLiteral("snes")) >= 0); // the recognised bucket is unaffected
    }

    // Settings › Emulators › Default emulator: only an enabled emulator that declares the
    // system may become its default; "Other" (no system id) and unknown systems have none.
    // Invoked by name so the test also documents the QML-facing API.
    void defaultEmulatorIsValidated()
    {
        write(configDir() + QStringLiteral("/snesemu.json"),
              "{\"id\":\"snesemu\",\"name\":\"Snes Emu\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
              "\"extensions\":[\".sfc\"],\"systems\":[\"snes\"]}");
        write(configDir() + QStringLiteral("/gbaemu.json"),
              "{\"id\":\"gbaemu\",\"name\":\"Gba Emu\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
              "\"extensions\":[\".gba\"],\"systems\":[\"gba\"]}");
        ui::UiContext ctx(options());
        ctx.start();
        auto *library = ctx.library();
        auto set = [library](const QString &systemId, const QString &emulatorId) {
            bool ok = false;
            const bool invoked = QMetaObject::invokeMethod(library, "setDefaultEmulator", Q_RETURN_ARG(bool, ok),
                                                           Q_ARG(QString, systemId), Q_ARG(QString, emulatorId));
            return invoked && ok;
        };
        auto stored
            = [&ctx](const QString &systemId) { return ctx.settings()->defaultEmulators().value(systemId).toString(); };

        QVERIFY(set(QStringLiteral("snes"), QStringLiteral("snesemu")));
        QCOMPARE(stored(QStringLiteral("snes")), QStringLiteral("snesemu"));
        // Incompatible emulator, unknown emulator, "Other" and unknown systems are refused
        // and leave the stored value alone.
        QVERIFY(!set(QStringLiteral("snes"), QStringLiteral("gbaemu")));
        QVERIFY(!set(QStringLiteral("snes"), QStringLiteral("nope")));
        QVERIFY(!set(QString(), QStringLiteral("snesemu")));
        QVERIFY(!set(QStringLiteral("no-such-system"), QStringLiteral("snesemu")));
        QCOMPARE(stored(QStringLiteral("snes")), QStringLiteral("snesemu"));
        QVERIFY(!ctx.settings()->defaultEmulators().contains(QString()));
        // "" = back to automatic.
        QVERIFY(set(QStringLiteral("snes"), QString()));
        QVERIFY(!ctx.settings()->defaultEmulators().contains(QStringLiteral("snes")));

        // The choices offered for a system: Automatic + compatible emulators only.
        QVariantList choices;
        QVERIFY(QMetaObject::invokeMethod(library, "defaultEmulatorChoices", Q_RETURN_ARG(QVariantList, choices),
                                          Q_ARG(QString, QStringLiteral("snes"))));
        QCOMPARE(choices.size(), 2);
        QCOMPARE(choices.at(0).toMap().value(QStringLiteral("id")).toString(), QString());
        QCOMPARE(choices.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("snesemu"));
        QVERIFY(QMetaObject::invokeMethod(library, "defaultEmulatorChoices", Q_RETURN_ARG(QVariantList, choices),
                                          Q_ARG(QString, QString())));
        QVERIFY(choices.isEmpty());
    }

    // The tiles name the emulator that launch() would use: a system default set in
    // Settings › Emulators moves the game off the (broken) first candidate without a rescan.
    void gameListFollowsTheDefaultEmulator()
    {
        write(configDir() + QStringLiteral("/aemu.json"),
              "{\"id\":\"aemu\",\"name\":\"A Emu\",\"executable\":\"/nonexistent/a-emu\","
              "\"extensions\":[\".sfc\"],\"systems\":[\"snes\"]}");
        write(configDir() + QStringLiteral("/bemu.json"),
              "{\"id\":\"bemu\",\"name\":\"B Emu\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
              "\"extensions\":[\".sfc\"],\"systems\":[\"snes\"]}");
        QTemporaryDir snesGames;
        write(snesGames.filePath(QStringLiteral("Mario.sfc")), "rom");

        ui::UiContext ctx(options());
        ctx.start();
        QVERIFY(ctx.library()->addDirectory(snesGames.path()));
        waitForScan(ctx);
        auto *games = ctx.library()->games();
        QCOMPARE(games->rowCount(), 1);
        const QModelIndex row = games->index(0);
        QCOMPARE(row.data(ui::GameListModel::EmulatorIdRole).toString(), QStringLiteral("aemu"));
        QCOMPARE(row.data(ui::GameListModel::EmulatorNameRole).toString(), QStringLiteral("A Emu"));
        QVERIFY(!row.data(ui::GameListModel::AvailableRole).toBool());

        QSignalSpy changes(games, &QAbstractItemModel::dataChanged);
        QVERIFY(ctx.library()->setDefaultEmulator(QStringLiteral("snes"), QStringLiteral("bemu")));
        QVERIFY(changes.size() >= 1);
        QCOMPARE(row.data(ui::GameListModel::EmulatorIdRole).toString(), QStringLiteral("bemu"));
        QCOMPARE(row.data(ui::GameListModel::EmulatorNameRole).toString(), QStringLiteral("B Emu"));
        QVERIFY(row.data(ui::GameListModel::AvailableRole).toBool());
        QCOMPARE(row.data(ui::GameListModel::UnavailableReasonRole).toString(), QString());
        // The details screen and the launch path agree with the tile.
        QCOMPARE(ctx.library()->gameDetails(games->gameIdAt(0)).value(QStringLiteral("emulatorId")).toString(),
                 QStringLiteral("bemu"));
        QVERIFY(ctx.library()->launch(games->gameIdAt(0)));
        QCOMPARE(ctx.app()->activeEmulatorName(), QStringLiteral("Mario (B Emu)"));
    }

    // Edits that fail (unknown/removed game) say so through notice(..., true) instead of
    // failing silently.
    void failedEditsEmitNotices()
    {
        ui::UiContext ctx(options());
        ctx.start();
        QSignalSpy notices(ctx.library(), &ui::LibraryController::notice);
        const qint64 unknown = 987654;
        QVERIFY(!ctx.library()->setGameMetadata(unknown, QStringLiteral("Title"), QString()));
        QVERIFY(!ctx.library()->setGameSystem(unknown, QStringLiteral("snes")));
        QVERIFY(!ctx.library()->removeGame(unknown));
        QCOMPARE(notices.size(), 3);
        for (const auto &args : notices) {
            QVERIFY(args.at(1).toBool()); // isError
            QVERIFY(!args.at(0).toString().isEmpty());
        }
    }
};

QTEST_MAIN(TestUiContext)
#include "tst_uicontext.moc"
