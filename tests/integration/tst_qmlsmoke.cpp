// Loads the real Main.qml (offscreen) against a real UiContext and drives the dashboard
// with key events, the same way controller input arrives (InputController::keyForAction).
// Any QQmlEngine warning fails the test.
//
// objectName hooks used here (keep them stable):
//   mainWindow          ApplicationWindow; property currentTab (0 Home, 1 Games, 2 Emulators,
//                       3 Settings, 4 Diagnostics)
//   tabBar              NavTabBar (property currentIndex); tabs tab_home, tab_games, …
//   homeScreen, gamesScreen, emulatorsScreen, settingsScreen, diagnosticsScreen  (tab pages)
//   homeActions, recentRow, favoritesRow, homeEmptyState                         (Home)
//   searchField, filterChips, filter_all|favorites|recent, sortSelector, gameGrid,
//   gamesLoading, gamesEmpty, gamesEmptyAction, scanSummary, resultCount         (Games)
//   emulatorList, emptyState, statusLine, profilePicker, profileList             (Emulators)
//   settingLanguage, settingTheme, settingUiScale, settingReducedMotion, settingHints,
//   settingSort, settingBackend, settingDeadzone, mapping_<action>, testInputRow,
//   settingDisplayMode, settingResolution, settingVolume, settingMute, libraryDir_<n>,
//   addDirField, addDirButton, rescanRow, cancelScanRow, restoreRemovedRow,
//   defaultEmulator_<systemId>, softwareUpdateRow, exportDiagnosticsRow, createBackupRow,
//   restoreBackupRow, clearArtworkCacheRow, resetRow, rebootRow, powerOffRow,
//   settingsIssues, sectionTitle_<Name>, tutorialRow                              (Settings)
//   reloadButton                                                                  (Diagnostics)
//   runningOverlay, stopButton, runningTitle                                      (overlay)
//   gameBar, gameBarTabs, barTab_home|settings|audio|performance, gameBarStatus,
//   controllerStatus, networkStatus, batteryStatus, gameBarClock, gameBarPanel,
//   homePanel, gameBarArtwork, gameBarTitle, sessionTime, homeActions, tileLabel,
//   gameBarResume, gameBarStop, gameBarHome, gameBarGames, gameBarRecent_<n>,
//   settingsPanel, gameBarSettings, gameBarReboot, gameBarPowerOff, audioPanel,
//   gameBarVolume, gameBarVolumeValue, performancePanel, cpuValue, cpuGraph,
//   memoryMeter                                                                   (game bar)
//   gameOptions, gameOptionsList, gameOptionsTitle, gameOptionsSummary, purgeRow  (game options)
//   tutorialOverlay, tutorialTitle, tutorialProgress, tutorialSkip, tutorialBack,
//   tutorialNext                                                                  (tutorial)
//   stateChip, scanChip, rootChip, clock, hintBar, toast                          (chrome)
#include "AppController.h"
#include "LibraryController.h"
#include "SettingsController.h"
#include "UiBootstrap.h"
#include "UiContext.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Nexora_ConsolePlugin)

using nexora::ui::AppController;

namespace {

constexpr int Timeout = 10000;

enum Tab { Home = 0, Games = 1, Emulators = 2, Settings = 3, Diagnostics = 4 };

void writeConfig(const QTemporaryDir &dir, const QString &id, const QString &arguments = {}, const QString &extra = {})
{
    QFile file(dir.filePath(id + QStringLiteral(".json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QStringLiteral("{\"id\":\"%1\",\"name\":\"Emu %1\",\"executable\":\"%2\",\"arguments\":[%3]%4}")
                   .arg(id, QStringLiteral(NEXORA_FAKE_EMULATOR), arguments,
                        extra.isEmpty() ? QString() : QStringLiteral(",") + extra)
                   .toUtf8());
}

void touch(const QTemporaryDir &dir, const QString &name)
{
    QFile file(dir.filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("rom");
}

} // namespace

class TestQmlSmoke : public QObject
{
    Q_OBJECT

private:
    struct Ui
    {
        std::unique_ptr<QTemporaryDir> state;
        std::unique_ptr<nexora::ui::UiContext> context;
        AppController *app = nullptr;
        std::unique_ptr<QQmlApplicationEngine> engine;
        QQuickWindow *window = nullptr;
        QList<QQmlError> warnings;
    };

    // |windowSize| is the OS window size (not Theme.scale / the UI-scale accessibility
    // setting): every test runs at both 1280x720 and 1920x1080 to catch anything that only
    // breaks at a different physical resolution (responsive layout, column counts, ...).
    // |firstStart| keeps ui.tutorialCompleted unset so the tutorial overlay is shown; the
    // other tests start with it already finished.
    static void load(Ui &ui, const QString &configDir, const QSize &windowSize = {1280, 720}, bool firstStart = false)
    {
        ui.state = std::make_unique<QTemporaryDir>();
        nexora::ui::UiContext::Options options;
        options.app = {configDir, QStringLiteral("test"), {}, false, {}};
        options.settings = {{}, ui.state->filePath(QStringLiteral("settings.json"))};
        options.databasePath = ui.state->filePath(QStringLiteral("library.sqlite"));
        options.platformOverride = QStringLiteral("desktop");
        ui.context = std::make_unique<nexora::ui::UiContext>(options);
        ui.context->start();
        if (!firstStart)
            ui.context->settings()->setTutorialCompleted(true);
        ui.app = ui.context->app();
        ui.engine = std::make_unique<QQmlApplicationEngine>();
        QObject::connect(ui.engine.get(), &QQmlEngine::warnings, ui.engine.get(),
                         [&ui](const QList<QQmlError> &warnings) { ui.warnings.append(warnings); });
        ui.window = nexora::ui::loadMainWindow(*ui.engine, ui.context.get());
        QVERIFY(ui.window);
        ui.window->resize(windowSize);
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
    }

    // Shared [[1280x720], [1920x1080]] data table for every *_data() slot below.
    static void addWindowSizeRows()
    {
        QTest::addColumn<QSize>("windowSize");
        QTest::newRow("1280x720") << QSize(1280, 720);
        QTest::newRow("1920x1080") << QSize(1920, 1080);
    }

    // Adds |dir| as a library directory and waits for the resulting scan.
    static void addGames(Ui &ui, const QTemporaryDir &dir)
    {
        QVERIFY(ui.context->library()->addDirectory(dir.path()));
        ui.context->gameLibrary().waitForScan();
        QVERIFY(!ui.context->library()->isScanning());
    }

    static QString describe(const QList<QQmlError> &errors)
    {
        QStringList lines;
        for (const auto &e : errors)
            lines.append(e.toString());
        return lines.join(u'\n');
    }

    static QString focusName(Ui &ui)
    {
        QQuickItem *item = ui.window->activeFocusItem();
        return item
            ? item->objectName() + QStringLiteral(" (") + QString::fromLatin1(item->metaObject()->className()) + u')'
            : QStringLiteral("<none>");
    }

    // Searches the visual item tree too: Repeater/view delegates have no QObject parent.
    static QQuickItem *findItem(QQuickItem *root, const QString &name)
    {
        if (root->objectName() == name)
            return root;
        for (QQuickItem *child : root->childItems()) {
            if (QQuickItem *found = findItem(child, name))
                return found;
        }
        return nullptr;
    }

    static QQuickItem *find(Ui &ui, const char *name)
    {
        const QString objectName = QString::fromLatin1(name);
        if (auto *item = ui.window->findChild<QQuickItem *>(objectName))
            return item;
        return findItem(ui.window->contentItem(), objectName);
    }

    static int currentTab(Ui &ui) { return ui.window->property("currentTab").toInt(); }

    // NEXORA_SCREENSHOT_DIR=<dir>: saves <dir>/<name>.png (for reviewing layouts and docs).
    static void shot(Ui &ui, const char *name)
    {
        const QString dir = qEnvironmentVariable("NEXORA_SCREENSHOT_DIR");
        if (dir.isEmpty())
            return;
        QTest::qWait(250); // let animations settle
        ui.window->grabWindow().save(dir + u'/' + QString::fromLatin1(name) + QStringLiteral(".png"));
    }

    static void key(Ui &ui, Qt::Key k, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QTest::keyClick(ui.window, k, modifiers);
    }

    // QTest::keyClicks() only exists for widgets.
    static void typeText(Ui &ui, const QString &text)
    {
        for (const QChar c : text)
            QTest::keyClick(ui.window, c.toLatin1());
    }

    static QString mappedButton(Ui &ui, const QString &action)
    {
        for (const QVariant &row : ui.context->settings()->mappingRows()) {
            const QVariantMap map = row.toMap();
            if (map.value(QStringLiteral("action")).toString() == action)
                return map.value(QStringLiteral("button")).toString();
        }
        return {};
    }

#define CHECK_NO_WARNINGS() QVERIFY2(ui.warnings.isEmpty(), qPrintable(describe(ui.warnings)))

private slots:
    void initTestCase() { QQuickStyle::setStyle(QStringLiteral("Basic")); }

    void tutorialOnFirstStart()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("alpha"));
        Ui ui;
        load(ui, dir.path(), {1280, 720}, true);
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        auto *tutorial = find(ui, "tutorialOverlay");
        auto *title = find(ui, "tutorialTitle");
        auto *next = find(ui, "tutorialNext");
        QVERIFY(tutorial && title && next);
        QVERIFY(!settings->tutorialCompleted());
        QVERIFY(tutorial->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(next->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(title->property("text").toString(), QStringLiteral("Welcome to Nexora"));

        // Tab switches, Diagnostics and search do nothing underneath.
        key(ui, Qt::Key_PageDown);
        key(ui, Qt::Key_F1);
        key(ui, Qt::Key_Slash);
        key(ui, Qt::Key_Up);
        QCOMPARE(currentTab(ui), int(Home));
        QVERIFY(next->hasActiveFocus());
        // Back on the first step stays there.
        key(ui, Qt::Key_Escape);
        QCOMPARE(title->property("text").toString(), QStringLiteral("Welcome to Nexora"));

        // Accept moves forward, Back returns; Left reaches the Back button.
        key(ui, Qt::Key_Return);
        QCOMPARE(title->property("text").toString(), QStringLiteral("Moving around"));
        key(ui, Qt::Key_Return);
        QCOMPARE(title->property("text").toString(), QStringLiteral("Tabs"));
        key(ui, Qt::Key_Escape);
        QCOMPARE(title->property("text").toString(), QStringLiteral("Moving around"));
        key(ui, Qt::Key_Left);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "tutorialBack")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return); // "Back" to the first step: focus returns to Next
        QCOMPARE(title->property("text").toString(), QStringLiteral("Welcome to Nexora"));
        QTRY_VERIFY2_WITH_TIMEOUT(next->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Walk to the end; without a library folder "Get started" opens Settings.
        for (int i = 0; i < 10 && next->property("text").toString() != QStringLiteral("Get started"); ++i)
            key(ui, Qt::Key_Return);
        QCOMPARE(find(ui, "tutorialProgress")->property("text").toString(), QStringLiteral("STEP 6 OF 6"));
        QVERIFY(!find(ui, "tutorialSkip")->isVisible());
        key(ui, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(!tutorial->isVisible(), Timeout);
        QVERIFY(settings->tutorialCompleted());
        QCOMPARE(currentTab(ui), int(Settings));
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "addDirField")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        // Saved: the next start skips the tutorial.
        QFile file(ui.state->filePath(QStringLiteral("settings.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("\"ui.tutorialCompleted\": true"));

        // Settings → "Show tutorial" opens it again; Skip closes it and keeps the tab.
        auto *row = find(ui, "tutorialRow");
        QVERIFY(row);
        for (int i = 0; i < 40 && !row->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Up);
        QVERIFY2(row->hasActiveFocus(), qPrintable(focusName(ui)));
        key(ui, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(tutorial->isVisible(), Timeout);
        QVERIFY(!settings->tutorialCompleted());
        QCOMPARE(title->property("text").toString(), QStringLiteral("Welcome to Nexora"));
        QTRY_VERIFY2_WITH_TIMEOUT(next->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Left);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "tutorialSkip")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(!tutorial->isVisible(), Timeout);
        QVERIFY(settings->tutorialCompleted());
        QCOMPARE(currentTab(ui), int(Settings));
        QTRY_VERIFY2_WITH_TIMEOUT(row->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        CHECK_NO_WARNINGS();
    }

    void startsOnHomeAndSwitchesTabs()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("alpha"));
        writeConfig(dir, QStringLiteral("beta"));
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;

        auto *tabBar = find(ui, "tabBar");
        auto *home = find(ui, "homeScreen");
        QVERIFY(tabBar && home);
        QCOMPARE(currentTab(ui), int(Home));
        QCOMPARE(tabBar->property("currentIndex").toInt(), int(Home));
        QTRY_VERIFY2_WITH_TIMEOUT(home->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        // No library folders yet: Home explains how to add one.
        QVERIFY(find(ui, "homeEmptyState")->isVisible());

        // L1 / R1 (PageUp / PageDown) and Ctrl+Tab / Ctrl+Shift+Tab switch tabs.
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Games));
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "gamesScreen")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(!home->isVisible());
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Emulators));
        key(ui, Qt::Key_PageUp);
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(currentTab(ui), int(Emulators));
        key(ui, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_PageUp);
        key(ui, Qt::Key_PageUp); // already the first tab
        QCOMPARE(currentTab(ui), int(Home));

        // Each tab remembers its focused child.
        key(ui, Qt::Key_PageDown);
        key(ui, Qt::Key_PageDown);
        auto *list = find(ui, "emulatorList");
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Right);
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Settings));
        key(ui, Qt::Key_PageUp);
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(list->property("currentIndex").toInt(), 1);

        // Menu / F1 opens Diagnostics.
        key(ui, Qt::Key_F1);
        QCOMPARE(currentTab(ui), int(Diagnostics));
        auto *diagnostics = find(ui, "diagnosticsScreen");
        QVERIFY(diagnostics && diagnostics->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(diagnostics->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Home);
        key(ui, Qt::Key_PageUp);
        key(ui, Qt::Key_Menu);
        QCOMPARE(currentTab(ui), int(Diagnostics));

        // Back on a root tab is a no-op (a console UI never quits on Back).
        key(ui, Qt::Key_Escape);
        key(ui, Qt::Key_Back);
        QCOMPARE(currentTab(ui), int(Diagnostics));
        QVERIFY(ui.window->isVisible());
        QVERIFY(diagnostics->hasActiveFocus());

        // Up at the top of a tab moves to the tab bar; Left/Right there switch, Down enters.
        key(ui, Qt::Key_Up);
        QTRY_VERIFY2_WITH_TIMEOUT(tabBar->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Left);
        QCOMPARE(currentTab(ui), int(Settings));
        QVERIFY(tabBar->hasActiveFocus());
        key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "settingsScreen")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        CHECK_NO_WARNINGS();
    }

    void gamesSearchLaunchAndFavorite()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"--sleep-ms\",\"30000\",\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        touch(games, QStringLiteral("Beta Racer.rom"));
        touch(games, QStringLiteral("Gamma Saga.rom"));
        touch(games, QStringLiteral("readme.txt")); // not a game
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        auto *library = ui.context->library();

        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Games));
        auto *grid = find(ui, "gameGrid");
        QVERIFY(grid);
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 3, Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(!find(ui, "gamesEmpty")->isVisible());
        QVERIFY(!find(ui, "gamesLoading")->isVisible());

        // "/" focuses search; typing filters; Return goes back to the results.
        auto *search = find(ui, "searchField");
        key(ui, Qt::Key_Slash);
        QTRY_VERIFY2_WITH_TIMEOUT(search->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        typeText(ui, QStringLiteral("beta"));
        QCOMPARE(library->searchText(), QStringLiteral("beta"));
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 1, Timeout);
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("currentIndex").toInt(), 0, Timeout);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // No results → empty state; Back inside the field returns to the grid area.
        key(ui, Qt::Key_Search);
        QTRY_VERIFY(search->hasActiveFocus());
        typeText(ui, QStringLiteral("zzz")); // replaces the selected text
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 0, Timeout);
        QTRY_VERIFY(find(ui, "gamesEmpty")->isVisible());
        // PageUp/PageDown still switch tabs from inside the text field.
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Emulators));
        key(ui, Qt::Key_PageUp);
        QTRY_VERIFY2_WITH_TIMEOUT(search->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        // Ctrl+F also focuses search; clear it and leave with Back.
        key(ui, Qt::Key_F, Qt::ControlModifier);
        key(ui, Qt::Key_Backspace);
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 3, Timeout);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(currentTab(ui), int(Games));

        // Launch the second game; the overlay takes focus and focus comes back afterwards.
        // The game selected while searching ("Beta Racer") stays selected.
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("currentIndex").toInt(), 1, Timeout);
        key(ui, Qt::Key_Right);
        QCOMPARE(grid->property("currentIndex").toInt(), 2);
        const qint64 gameId = library->games()->gameIdAt(2);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        auto *overlay = find(ui, "runningOverlay");
        QTRY_VERIFY_WITH_TIMEOUT(overlay->isVisible(), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Escape); // Back does not stop or leave
        key(ui, Qt::Key_PageDown); // nor does a tab switch happen underneath
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_Return); // "Stop emulator"
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(!overlay->isVisible(), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(library->games()->gameIdAt(grid->property("currentIndex").toInt()), gameId);
        QTRY_COMPARE_WITH_TIMEOUT(library->recentGames()->rowCount(), 1, Timeout);

        // Favorite (gamepad north = Key_Favorites, keyboard F) toggles the focused game.
        key(ui, Qt::Key_Favorites);
        QTRY_COMPARE_WITH_TIMEOUT(library->favoriteGames()->rowCount(), 1, Timeout);
        QCOMPARE(library->favoriteGames()->gameIdAt(0), gameId);
        key(ui, Qt::Key_F);
        QTRY_COMPARE_WITH_TIMEOUT(library->favoriteGames()->rowCount(), 0, Timeout);
        QVERIFY(grid->hasActiveFocus());

        // Home now shows the game under "Continue playing".
        key(ui, Qt::Key_PageUp);
        QCOMPARE(currentTab(ui), int(Home));
        auto *recent = find(ui, "recentRow");
        QVERIFY(recent->isVisible());
        QCOMPARE(recent->property("count").toInt(), 1);
        // Home remembered its quick-action focus; Down moves into the row.
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "homeActions")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(recent->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return); // launches from Home too
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(recent->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(!find(ui, "homeEmptyState")->isVisible());

        CHECK_NO_WARNINGS();
    }

    void emulatorsEmptyConfiguration()
    {
        QTemporaryDir dir;
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;
        key(ui, Qt::Key_PageDown);
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Emulators));
        auto *empty = find(ui, "emptyState");
        QVERIFY(empty);
        QTRY_VERIFY(empty->isVisible());
        QVERIFY(!find(ui, "emulatorList")->isVisible());
        key(ui, Qt::Key_Return); // nothing to launch
        QCOMPARE(ui.app->state(), QStringLiteral("idle"));
        CHECK_NO_WARNINGS();
    }

    void emulatorsLaunchAndProfiles()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral("\"--sleep-ms\",\"30000\""));
        writeConfig(dir, QStringLiteral("profiled"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"profiles\":[{\"id\":\"fast\",\"name\":\"Fast\"},"
                                   "{\"id\":\"off\",\"name\":\"Off\",\"enabled\":false}]"));
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;
        key(ui, Qt::Key_PageDown);
        key(ui, Qt::Key_PageDown);
        auto *list = find(ui, "emulatorList");
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(!find(ui, "emptyState")->isVisible());
        QCOMPARE(list->property("count").toInt(), 2);

        // Without profiles: launches directly.
        QCOMPARE(ui.app->emulators()->emulatorIdAt(0), QStringLiteral("long"));
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // With enabled profiles: a picker ("Default" + enabled profiles); Back closes it.
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Return);
        auto *picker = find(ui, "profilePicker");
        QTRY_VERIFY(picker->isVisible());
        auto *profiles = find(ui, "profileList");
        QTRY_VERIFY2_WITH_TIMEOUT(profiles->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(profiles->property("count").toInt(), 2);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!picker->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(ui.app->state(), QStringLiteral("exited"));

        key(ui, Qt::Key_Return);
        QTRY_VERIFY(profiles->hasActiveFocus());
        key(ui, Qt::Key_Down);
        // Y / F makes the selected profile the emulator's default; the picker stays open.
        key(ui, Qt::Key_F);
        QCOMPARE(ui.context->settings()->defaultProfile(QStringLiteral("profiled")), QStringLiteral("fast"));
        QVERIFY(picker->isVisible());
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!picker->isVisible());
        key(ui, Qt::Key_Return); // reopens on the default profile
        QTRY_VERIFY(profiles->hasActiveFocus());
        QCOMPARE(profiles->property("currentIndex").toInt(), 1);
        shot(ui, "profile-picker-default");
        key(ui, Qt::Key_Return); // "Fast"
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QVERIFY(!picker->isVisible());
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(list->property("currentIndex").toInt(), 1);

        CHECK_NO_WARNINGS();
    }

    void gameOptionsDialog()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("one"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        writeConfig(config, QStringLiteral("two"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"extensions\":[\".rom\"],\"profiles\":[{\"id\":\"fast\",\"name\":\"Fast\"}]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        touch(games, QStringLiteral("Beta Racer.rom"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        auto *library = ui.context->library();
        key(ui, Qt::Key_PageDown);
        auto *grid = find(ui, "gameGrid");
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 2, Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        const qint64 alpha = library->games()->gameIdAt(0);

        // O (keyboard) opens the options of the focused game.
        key(ui, Qt::Key_O);
        auto *dialog = find(ui, "gameOptions");
        QTRY_VERIFY(dialog->isVisible());
        auto *options = find(ui, "gameOptionsList");
        QTRY_VERIFY2_WITH_TIMEOUT(options->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(find(ui, "gameOptionsTitle")->property("text").toString(), QStringLiteral("Alpha Quest"));
        // Play, Emulator: automatic, Emu one, Emu two, favorite, close.
        QCOMPARE(options->property("count").toInt(), 6);
        shot(ui, "game-options");
        // Tab switching keys do not leak through the modal dialog.
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Games));

        // Choose the second emulator; the dialog stays open and now offers its profile.
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Return);
        QCOMPARE(library->gameDetails(alpha).value(QStringLiteral("emulatorId")).toString(), QStringLiteral("two"));
        QVERIFY(dialog->isVisible());
        QTRY_COMPARE(options->property("count").toInt(), 8); // + "Play with Fast", "emulator defaults"
        shot(ui, "game-options-emulator-chosen");

        // Back closes it and focus returns to the game.
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(library->games()->gameIdAt(grid->property("currentIndex").toInt()), alpha);
        shot(ui, "games-grid");

        // The controller's Options button (Key_Context1) opens it too; "Play" launches with
        // the chosen emulator, and focus comes back to the game afterwards.
        // keyClick() aborts on keys without an ASCII mapping; send it like InputController does.
        QTest::sendKeyEvent(QTest::Click, ui.window, Qt::Key_Context1, char(0), Qt::NoModifier);
        QTRY_VERIFY2_WITH_TIMEOUT(options->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QCOMPARE(ui.app->manager()->activeEmulatorId(), QStringLiteral("two"));
        QVERIFY(!dialog->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // A missing game shows up as a removable entry in Settings.
        auto *purge = find(ui, "purgeRow");
        QVERIFY(purge);
        QVERIFY(!purge->isVisible());
        QVERIFY(QFile::remove(games.filePath(QStringLiteral("Beta Racer.rom"))));
        QVERIFY(library->refresh());
        ui.context->gameLibrary().waitForScan();
        QTRY_COMPARE(library->missingCount(), 1);
        key(ui, Qt::Key_PageDown);
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Settings));
        QTRY_VERIFY(purge->isVisible());
        shot(ui, "settings-purge");

        CHECK_NO_WARNINGS();
    }

    void settingsVolumeAndRemap()
    {
        QTemporaryDir dir;
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        for (int i = 0; i < 3; ++i)
            key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Settings));
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "settingLanguage")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Keyboard remap: Right on a mapping row cycles to the next button. Controllers
        // (with the mapping rows) comes before Display/Audio (with volume), so this is
        // reached first.
        const QString before = mappedButton(ui, QStringLiteral("favorite"));
        QCOMPARE(before, QStringLiteral("north"));
        auto *row = find(ui, "mapping_favorite");
        QVERIFY(row);
        for (int i = 0; i < 40 && !row->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(row->hasActiveFocus(), qPrintable(focusName(ui)));
        key(ui, Qt::Key_Right);
        QCOMPARE(mappedButton(ui, QStringLiteral("favorite")), QStringLiteral("start"));
        // The action that had "start" (menu) swapped onto "north".
        QCOMPARE(mappedButton(ui, QStringLiteral("menu")), QStringLiteral("north"));
        // The rows are rebuilt; focus stays on the same action.
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "mapping_favorite")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Left);
        QCOMPARE(mappedButton(ui, QStringLiteral("favorite")), QStringLiteral("north"));

        // Walk on down to the volume row (Display, then Audio); Right raises it by 5, Left
        // lowers it again.
        auto *volumeRow = find(ui, "settingVolume");
        QVERIFY(volumeRow);
        for (int i = 0; i < 40 && !volumeRow->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(volumeRow->hasActiveFocus(), qPrintable(focusName(ui)));
        const int volume = settings->volume();
        QVERIFY(volume <= 95);
        key(ui, Qt::Key_Right);
        QCOMPARE(settings->volume(), volume + 5);
        key(ui, Qt::Key_Left);
        QCOMPARE(settings->volume(), volume);

        // Show control hints toggles the hint bar.
        auto *hintBar = find(ui, "hintBar");
        QVERIFY(hintBar->isVisible());
        settings->setShowHints(false);
        QTRY_VERIFY(!hintBar->isVisible());
        settings->setShowHints(true);
        QTRY_VERIFY(hintBar->isVisible());

        // Up/Down never leave the screen downwards; the last row stays focused (Restart /
        // Power off are hidden on this "desktop" platform override, so Reset preferences is
        // the true last one).
        for (int i = 0; i < 100; ++i)
            key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "resetRow")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(currentTab(ui), int(Settings));

        // Add a library folder from the text field.
        QTemporaryDir games;
        auto *field = find(ui, "addDirField");
        for (int i = 0; i < 100 && !field->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Up);
        QTRY_VERIFY2_WITH_TIMEOUT(field->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        typeText(ui, games.path());
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.context->library()->directories(), QStringList{games.path()}, Timeout);
        ui.context->gameLibrary().waitForScan();
        QTRY_VERIFY(find(ui, "libraryDir_0") != nullptr);
        key(ui, Qt::Key_Escape); // leaves the field
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "addDirButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        CHECK_NO_WARNINGS();
    }
    void gameBarOverlay()
    {
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral("\"--sleep-ms\",\"30000\",\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        Ui ui;
        load(ui, dir.path());
        if (QTest::currentTestFailed())
            return;
        auto *bar = ui.context->gameBar();
        auto *layer = find(ui, "gameBar");
        QVERIFY(layer);
        QVERIFY(!layer->isVisible());
        QQuickItem *before = ui.window->activeFocusItem();
        QVERIFY(before);
        auto focused = [&ui](const char *name) {
            auto *item = find(ui, name);
            return item && item->hasActiveFocus();
        };
#define CHECK_FOCUS(name) QTRY_VERIFY2_WITH_TIMEOUT(focused(name), qPrintable(focusName(ui)), Timeout)

        // Ctrl+G opens it on the Home widget button; idle, Home offers the dashboard.
        key(ui, Qt::Key_G, Qt::ControlModifier);
        QVERIFY(bar->isOpen());
        QTRY_VERIFY(layer->isVisible());
        CHECK_FOCUS("barTab_home");
        QVERIFY(find(ui, "homePanel")->isVisible());
        QCOMPARE(find(ui, "gameBarTitle")->property("text").toString(), QStringLiteral("No game running"));
        QVERIFY(!find(ui, "gameBarResume")->isVisible());
        QVERIFY(find(ui, "gameBarClock")->isVisible());

        // Down enters the panel, Left/Right walk the tiles, Up returns to the buttons.
        key(ui, Qt::Key_Down);
        CHECK_FOCUS("gameBarHome");
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("gameBarGames");
        key(ui, Qt::Key_Right); // last tile: stays
        CHECK_FOCUS("gameBarGames");
        key(ui, Qt::Key_Up);
        CHECK_FOCUS("barTab_home");

        // The panel follows the selected widget button.
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("barTab_settings");
        QTRY_VERIFY(find(ui, "settingsPanel")->isVisible());
        QVERIFY(!find(ui, "homePanel")->isVisible());
        QVERIFY(!find(ui, "gameBarPowerOff")->isEnabled()); // desktop: no power control
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("barTab_audio");
        key(ui, Qt::Key_Down);
        CHECK_FOCUS("gameBarVolume");
        const int volume = ui.context->settings()->volume();
        QVERIFY(volume >= 5);
        key(ui, Qt::Key_Left);
        QCOMPARE(ui.context->settings()->volume(), volume - 5);
        QCOMPARE(find(ui, "gameBarVolumeValue")->property("text").toString(), QString::number(volume - 5) + u'%');
        key(ui, Qt::Key_Right);
        QCOMPARE(ui.context->settings()->volume(), volume);
        key(ui, Qt::Key_Up);
        CHECK_FOCUS("barTab_audio");
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("barTab_performance");
        QTRY_VERIFY(find(ui, "cpuValue")->isVisible());
        QVERIFY(find(ui, "memoryMeter")->isVisible());

        // Back closes it and gives focus back; nothing leaked to the dashboard.
        QCOMPARE(currentTab(ui), int(Home));
        key(ui, Qt::Key_Escape);
        QVERIFY(!bar->isOpen());
        QTRY_VERIFY(!layer->isVisible());
        QTRY_COMPARE(ui.window->activeFocusItem(), before);

        // The Guide button (gameBarRequested -> toggle) reopens it on Home; "Game library"
        // jumps to the Games tab.
        bar->toggle();
        CHECK_FOCUS("barTab_home");
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("gameBarGames");
        key(ui, Qt::Key_Return);
        QVERIFY(!bar->isOpen());
        QCOMPARE(currentTab(ui), int(Games));

        // A played game shows up as a tile on Home and launches from there.
        addGames(ui, games);
        auto *library = ui.context->library();
        QTRY_COMPARE_WITH_TIMEOUT(library->games()->rowCount(), 1, Timeout);
        QVERIFY(library->launch(library->games()->gameIdAt(0)));
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QVERIFY(ui.app->stop());
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_COMPARE_WITH_TIMEOUT(library->recentGames()->rowCount(), 1, Timeout);
        key(ui, Qt::Key_G, Qt::ControlModifier);
        CHECK_FOCUS("barTab_home");
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("gameBarRecent_0");
        QVERIFY(find(ui, "tileLabel")->property("text").toString().contains(QStringLiteral("Alpha Quest")));
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QVERIFY(!bar->isOpen());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Over a running game: "Back to game" first, the session timer runs, and keys do not
        // reach the running overlay underneath (Return resumes, it does not stop).
        key(ui, Qt::Key_G, Qt::ControlModifier);
        CHECK_FOCUS("barTab_home");
        QVERIFY(find(ui, "sessionTime")->isVisible());
        QVERIFY(find(ui, "gameBarTitle")->property("text").toString().contains(QStringLiteral("Alpha Quest")));
        key(ui, Qt::Key_Down);
        CHECK_FOCUS("gameBarResume");
        QVERIFY(!find(ui, "gameBarRecent_0")->isEnabled()); // one game at a time
        key(ui, Qt::Key_Return);
        QVERIFY(!bar->isOpen());
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Stop needs a confirmation; the panel then switches to its idle tiles.
        bar->toggle();
        CHECK_FOCUS("barTab_home");
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Right);
        CHECK_FOCUS("gameBarStop");
        key(ui, Qt::Key_Return);
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        QCOMPARE(find(ui, "tileLabel")->property("text").toString(), QStringLiteral("Press again to stop"));
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QVERIFY(bar->isOpen());
        CHECK_FOCUS("gameBarHome");
        key(ui, Qt::Key_Escape);
        QVERIFY(!bar->isOpen());
        QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem() && !find(ui, "runningOverlay")->isVisible(),
                                  qPrintable(focusName(ui)), Timeout);
#undef CHECK_FOCUS

        CHECK_NO_WARNINGS();
    }
};

QTEST_MAIN(TestQmlSmoke)
#include "tst_qmlsmoke.moc"
