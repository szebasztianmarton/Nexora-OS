// The full backend object graph used by the UI (UiContext), with the fake emulator,
// temporary settings/database files and a temporary game directory.
#include "UiContext.h"
#include "input/GamepadNavigator.h"
#include "input/InputBackend.h"
#include "input/InputController.h"

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
    QStringList deviceNames() const override { return {QStringLiteral("Fake Pad")}; }
};

class TestUiContext : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString configDir() const { return m_dir.filePath(QStringLiteral("emulators")); }
    QString gamesDir() const { return m_dir.filePath(QStringLiteral("games")); }

    ui::UiContext::Options options() const
    {
        ui::UiContext::Options o;
        o.app = {configDir(), QStringLiteral("test"), {}, false};
        o.settings = {{m_dir.filePath(QStringLiteral("system-defaults.json"))},
                      m_dir.filePath(QStringLiteral("user/settings.json"))};
        o.databasePath = m_dir.filePath(QStringLiteral("data/library.sqlite"));
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

    void corruptedSettingsFallBackToDefaults()
    {
        write(m_dir.filePath(QStringLiteral("user/settings.json")), "{ definitely not json");
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
        write(m_dir.filePath(QStringLiteral("user/settings.json")),
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
        input.navigator()->buttonEvent(input::GamepadButton::L1, true);
        QVERIFY(ctx.settings()->capturingAction().isEmpty());
        QVERIFY(!input.isCapturing());
        QCOMPARE(input.mapping().buttonFor(input::NavAction::Favorite), std::optional(input::GamepadButton::L1));
        QCOMPARE(input.mapping().buttonFor(input::NavAction::PrevTab), std::optional(input::GamepadButton::North));
        QVERIFY(!ctx.settings()->beginRemap(QStringLiteral("fly")));
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

        // Search and filters drive the visible list.
        ctx.library()->setSearchText(QStringLiteral("space"));
        QTRY_COMPARE_WITH_TIMEOUT(games->rowCount(), 1, Timeout);
        ctx.library()->setSearchText({});
        ctx.library()->setFilter(QStringLiteral("favorites"));
        QTRY_COMPARE_WITH_TIMEOUT(games->rowCount(), 1, Timeout);
        ctx.library()->setFilter(QStringLiteral("all"));
        QTRY_COMPARE_WITH_TIMEOUT(games->rowCount(), 2, Timeout);
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
};

QTEST_MAIN(TestUiContext)
#include "tst_uicontext.moc"
