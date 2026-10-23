// Loads the real Main.qml (offscreen) against a real UiContext and drives the dashboard
// with key events, the same way controller input arrives (InputController::keyForAction).
// Any QQmlEngine warning fails the test.
//
// objectName hooks used here (keep them stable):
//   mainWindow          ApplicationWindow; property currentTab (0 Home, 1 Games, 2 Systems,
//                       3 Favorites, 4 Recent, 5 Emulators, 6 Settings, 7 System)
//   tabBar              NavTabBar (property currentIndex); tabs tab_home, tab_games, …
//   homeScreen, gamesScreen, systemsScreen, favoritesScreen, recentScreen, emulatorsScreen,
//   settingsScreen, systemScreen                                                  (tab pages)
//   homeActions, statusCards, statusCard_<key>, recentRow, favoritesRow, recentlyAddedRow,
//   homeSystemsRow, homeEmptyState                                                (Home)
//   searchField, filterChips, filter_all|favorites|recent|missing, viewToggle, systemChips,
//   systemChip_<id>, scanProgress, cancelScanButton, sortSelector, gameGrid,
//   gameTileTitle (every GameTile's title text, Home rows and grids alike),
//   gamesLoading, gamesEmpty, gamesEmptyAction, scanSummary, resultCount         (Games)
//   systemGrid, systemCard_<id>, systemsEmpty, systemsEmptyAction                (Systems)
//   favoritesScreenGrid, favoritesScreenEmpty, recentScreenGrid, recentScreenEmpty
//   gameDetails, gameDetailsTitle, detailsLaunch, detailsFavorite, detailsEmulator,
//   detailsOptions, detailsEdit, detailsSystem, detailsRemove, confirmRemoveGame (details)
//   quickMenu, quickReturn, quickPause, quickVolume, quickMute, quickDisplay,
//   quickSystemInfo, quickExit, confirmExitGame, quickSuspend, quickMenuState   (quick menu)
//   quickResumeRow, quickResumeList, quickResumeCard_<n>, confirmRemoveQuickResume,
//   resumeFailedDialog                                                           (Quick Resume)
//   updatePanel, updateState, updateMessage, updateUnsupported, updateAcknowledge,
//   updateClose, statusValue_updates                                            (updates)
//   systemActions, systemUpdateButton, exportDiagnosticsButton, systemDetails     (System)
//   emulatorList, emptyState, statusLine, profilePicker, profileList             (Emulators)
//   settingLanguage, settingTheme, settingUiScale, settingReducedMotion, settingHints,
//   settingSort, settingBackend, settingDeadzone, mapping_<action>, testInputRow,
//   settingDisplayMode, settingResolution, settingVolume, settingMute, libraryDir_<n>,
//   addDirField, addDirButton, rescanRow, cancelScanRow, restoreRemovedRow,
//   defaultEmulator_<systemId>, softwareUpdateRow, exportDiagnosticsRow, createBackupRow,
//   restoreBackupRow, clearArtworkCacheRow, resetRow, rebootRow, powerOffRow,
//   settingsIssues, sectionTitle_<Name>, tutorialRow                              (Settings)
//   reloadButton, systemVersion, systemHardware, systemLaunchHistory               (System)
//   runningOverlay, stopButton, runningTitle, confirmStopGame                     (overlay)
//   metadataEditor, metadataPanel, metadataTitle, metadataCancel, onScreenKeyboard,
//   oskPanel, oskKeys                                                             (editor)
//   inputTestPanel, inputTestHelp, confirmRestoreBackup, confirmGameBarReboot,
//   confirmGameBarPowerOff                                                        (modals)
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
#include "GameBarController.h"
#include "LibraryController.h"
#include "SettingsController.h"
#include "SystemController.h"
#include "UiBootstrap.h"
#include "UiContext.h"
#include "input/InputController.h"

#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
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

enum Tab { Home = 0, Games = 1, Systems = 2, Favorites = 3, Recent = 4, Emulators = 5, Settings = 6, System = 7 };
constexpr int TabCount = 8;

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
    // |lastUpdateRecord|: contents of stateDir/update/last-update.json (as written by
    // nexora-bootok) before the UI starts; empty = no record.
    static void load(Ui &ui, const QString &configDir, const QSize &windowSize = {1280, 720}, bool firstStart = false,
                     const QByteArray &lastUpdateRecord = {})
    {
        ui.state = std::make_unique<QTemporaryDir>();
        nexora::ui::UiContext::Options options;
        options.app = {configDir, QStringLiteral("test"), {}, false, {}};
        options.settings = {{}, ui.state->filePath(QStringLiteral("settings.json"))};
        // Everything (launch history, backups, logs) stays inside the temporary directory.
        options.layout = nexora::shared::DataLayout::underRoot(ui.state->filePath(QStringLiteral("data")));
        options.databasePath = ui.state->filePath(QStringLiteral("library.sqlite"));
        options.platformOverride = QStringLiteral("desktop");
        options.update.stagingDir = options.layout.stateDir + QStringLiteral("/update");
        options.update.currentBuildId = QStringLiteral("v0.4.0-0-gcafebabe");
        if (!lastUpdateRecord.isEmpty()) {
            QVERIFY(QDir().mkpath(options.update.stagingDir));
            QFile record(options.update.stagingDir + QStringLiteral("/last-update.json"));
            QVERIFY(record.open(QIODevice::WriteOnly));
            record.write(lastUpdateRecord);
        }
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

    // The Theme singleton of the loaded UI (Main.qml binds its inputs), read through a small
    // component in the same engine (the singleton is a QML file, one instance per engine).
    static QObject *theme(Ui &ui)
    {
        QQmlComponent component(ui.engine.get());
        component.setData("import QtQml; import Nexora.Console; QtObject { property QtObject theme: Theme }", QUrl());
        std::unique_ptr<QObject> probe(component.create());
        if (!probe)
            return nullptr;
        return qvariant_cast<QObject *>(probe->property("theme"));
    }

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

    // L1 / R1 (PageUp / PageDown) until |tab| is current.
    static void goTo(Ui &ui, Tab tab)
    {
        for (int i = 0; i < TabCount && currentTab(ui) != int(tab); ++i)
            key(ui, currentTab(ui) < int(tab) ? Qt::Key_PageDown : Qt::Key_PageUp);
        QCOMPARE(currentTab(ui), int(tab));
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

    // The running overlay's Stop asks first (spec §6.7): Accept opens the confirmation,
    // which defaults to Cancel; Right + Accept confirms.
    static void stopViaOverlay(Ui &ui)
    {
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        auto *confirm = find(ui, "confirmStopGame");
        QVERIFY(confirm);
        QTRY_VERIFY(confirm->isVisible());
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Return);
    }

    // Rect of |item| in window coordinates.
    static QRectF sceneRect(QQuickItem *item)
    {
        return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    }

    // Visible items under |root| whose objectName starts with |prefix|.
    static QList<QQuickItem *> visibleWithPrefix(QQuickItem *root, const QString &prefix)
    {
        QList<QQuickItem *> list;
        if (root->objectName().startsWith(prefix) && root->isVisible())
            list.append(root);
        for (QQuickItem *child : root->childItems())
            list.append(visibleWithPrefix(child, prefix));
        return list;
    }

    // The first QQuickText below |root| (depth first).
    static QQuickItem *firstText(QQuickItem *root)
    {
        for (QQuickItem *child : root->childItems()) {
            if (QString::fromLatin1(child->metaObject()->className()).startsWith(QStringLiteral("QQuickText")))
                return child;
            if (QQuickItem *found = firstText(child))
                return found;
        }
        return nullptr;
    }

    static bool focusInside(Ui &ui, QQuickItem *container)
    {
        QQuickItem *item = ui.window->activeFocusItem();
        return item && container && container->isAncestorOf(item);
    }

    static QVariant callQml(QObject *object, const char *method, const QVariant &arg = {})
    {
        QVariant ret;
        if (arg.isValid())
            QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, ret), Q_ARG(QVariant, arg));
        else
            QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, ret));
        return ret;
    }

    static QStringList hintActions(Ui &ui)
    {
        QStringList actions;
        for (const QVariant &hint : find(ui, "hintBar")->property("hints").toList())
            actions.append(hint.toMap().value(QStringLiteral("action")).toString());
        return actions;
    }

#define CHECK_NO_WARNINGS() QVERIFY2(ui.warnings.isEmpty(), qPrintable(describe(ui.warnings)))

private slots:
    void initTestCase() { QQuickStyle::setStyle(QStringLiteral("Basic")); }

    void tutorialOnFirstStart_data() { addWindowSizeRows(); }
    void tutorialOnFirstStart()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("alpha"));
        Ui ui;
        load(ui, dir.path(), windowSize, true);
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

    void startsOnHomeAndSwitchesTabs_data() { addWindowSizeRows(); }
    void startsOnHomeAndSwitchesTabs()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("alpha"));
        writeConfig(dir, QStringLiteral("beta"));
        Ui ui;
        load(ui, dir.path(), windowSize);
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
        QCOMPARE(currentTab(ui), int(Systems));
        key(ui, Qt::Key_PageUp);
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(currentTab(ui), int(Systems));
        key(ui, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_PageUp);
        key(ui, Qt::Key_PageUp); // already the first tab
        QCOMPARE(currentTab(ui), int(Home));

        // Each tab remembers its focused child.
        goTo(ui, Emulators);
        auto *list = find(ui, "emulatorList");
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Right);
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        key(ui, Qt::Key_PageDown);
        QCOMPARE(currentTab(ui), int(Settings));
        key(ui, Qt::Key_PageUp);
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(list->property("currentIndex").toInt(), 1);

        // Menu / F1 opens System (formerly Diagnostics).
        key(ui, Qt::Key_F1);
        QCOMPARE(currentTab(ui), int(System));
        auto *diagnostics = find(ui, "systemScreen");
        QVERIFY(diagnostics && diagnostics->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(diagnostics->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Home);
        key(ui, Qt::Key_PageUp);
        key(ui, Qt::Key_Menu);
        QCOMPARE(currentTab(ui), int(System));

        // Back on a root tab is a no-op (a console UI never quits on Back).
        key(ui, Qt::Key_Escape);
        key(ui, Qt::Key_Back);
        QCOMPARE(currentTab(ui), int(System));
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

    void gamesSearchLaunchAndFavorite_data() { addWindowSizeRows(); }
    void gamesSearchLaunchAndFavorite()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"--sleep-ms\",\"30000\",\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        touch(games, QStringLiteral("Beta Racer.rom"));
        touch(games, QStringLiteral("Gamma Saga.rom"));
        touch(games, QStringLiteral("readme.txt")); // not a game
        Ui ui;
        load(ui, config.path(), windowSize);
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
        QCOMPARE(currentTab(ui), int(Systems));
        key(ui, Qt::Key_PageUp);
        QTRY_VERIFY2_WITH_TIMEOUT(search->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        // Ctrl+F also focuses search; clear it and leave with Back.
        key(ui, Qt::Key_F, Qt::ControlModifier);
        key(ui, Qt::Key_Backspace);
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 3, Timeout);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(currentTab(ui), int(Games));

        // Accept opens the details of the third game; Play (the default action) launches it,
        // the overlay takes focus and focus comes back to Play afterwards; Back then returns
        // to the same tile. The game selected while searching ("Beta Racer") stays selected.
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("currentIndex").toInt(), 1, Timeout);
        key(ui, Qt::Key_Right);
        QCOMPARE(grid->property("currentIndex").toInt(), 2);
        const qint64 gameId = library->games()->gameIdAt(2);
        key(ui, Qt::Key_Return);
        auto *details = find(ui, "gameDetails");
        QTRY_VERIFY_WITH_TIMEOUT(details->isVisible(), Timeout);
        QCOMPARE(find(ui, "gameDetailsTitle")->property("text").toString(), QStringLiteral("Gamma Saga"));
        auto *play = find(ui, "detailsLaunch");
        QTRY_VERIFY2_WITH_TIMEOUT(play->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_PageDown); // modal: no tab switch underneath
        QCOMPARE(currentTab(ui), int(Games));
        shot(ui, "game-details");
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        auto *overlay = find(ui, "runningOverlay");
        QTRY_VERIFY_WITH_TIMEOUT(overlay->isVisible(), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Escape); // Back does not stop or leave
        key(ui, Qt::Key_PageDown); // nor does a tab switch happen underneath
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        QCOMPARE(currentTab(ui), int(Games));
        stopViaOverlay(ui); // "Stop emulator" (confirmed)
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(!overlay->isVisible(), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(play->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Escape); // Back: details close, the same tile has focus again
        QTRY_VERIFY_WITH_TIMEOUT(!details->isVisible(), Timeout);
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
        // Home remembered its quick-action focus; Down passes the status cards into the row.
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "homeActions")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "statusCards")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(recent->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return); // details from Home too; Play launches
        QTRY_VERIFY2_WITH_TIMEOUT(play->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        stopViaOverlay(ui);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(play->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(recent->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(!find(ui, "homeEmptyState")->isVisible());

        CHECK_NO_WARNINGS();
    }

    void emulatorsEmptyConfiguration_data() { addWindowSizeRows(); }
    void emulatorsEmptyConfiguration()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        Ui ui;
        load(ui, dir.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        goTo(ui, Emulators);
        auto *empty = find(ui, "emptyState");
        QVERIFY(empty);
        QTRY_VERIFY(empty->isVisible());
        QVERIFY(!find(ui, "emulatorList")->isVisible());
        key(ui, Qt::Key_Return); // nothing to launch
        QCOMPARE(ui.app->state(), QStringLiteral("idle"));
        CHECK_NO_WARNINGS();
    }

    void emulatorsLaunchAndProfiles_data() { addWindowSizeRows(); }
    void emulatorsLaunchAndProfiles()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral("\"--sleep-ms\",\"30000\""));
        writeConfig(dir, QStringLiteral("profiled"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"profiles\":[{\"id\":\"fast\",\"name\":\"Fast\"},"
                                   "{\"id\":\"off\",\"name\":\"Off\",\"enabled\":false}]"));
        Ui ui;
        load(ui, dir.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        goTo(ui, Emulators);
        auto *list = find(ui, "emulatorList");
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(!find(ui, "emptyState")->isVisible());
        QCOMPARE(list->property("count").toInt(), 2);

        // Without profiles: launches directly.
        QObject *themeObject = theme(ui);
        QVERIFY(themeObject);
        QCOMPARE(themeObject->property("sessionActive").toBool(), false);
        QCOMPARE(ui.app->emulators()->emulatorIdAt(0), QStringLiteral("long"));
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        // No blur behind a running emulator: Main.qml's state match drives Theme.sessionActive.
        QCOMPARE(themeObject->property("sessionActive").toBool(), true);
        QCOMPARE(themeObject->property("glassBlur").toBool(), false);
        stopViaOverlay(ui);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QCOMPARE(themeObject->property("sessionActive").toBool(), false);
        // Settings › Reduce transparency reaches Theme.
        QCOMPARE(themeObject->property("reduceTransparency").toBool(), false);
        ui.context->settings()->setReduceTransparency(true);
        QTRY_COMPARE(themeObject->property("reduceTransparency").toBool(), true);
        ui.context->settings()->setReduceTransparency(false);
        QTRY_COMPARE(themeObject->property("reduceTransparency").toBool(), false);
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
        stopViaOverlay(ui);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(list->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(list->property("currentIndex").toInt(), 1);

        CHECK_NO_WARNINGS();
    }

    void gameOptionsDialog_data() { addWindowSizeRows(); }
    void gameOptionsDialog()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("one"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        writeConfig(config, QStringLiteral("two"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"extensions\":[\".rom\"],\"profiles\":[{\"id\":\"fast\",\"name\":\"Fast\"}]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        touch(games, QStringLiteral("Beta Racer.rom"));
        Ui ui;
        load(ui, config.path(), windowSize);
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
        // Play, Emulator: automatic, Emu one, Emu two, favorite, Quick Resume, close.
        QCOMPARE(options->property("count").toInt(), 7);
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
        QTRY_COMPARE(options->property("count").toInt(), 9); // + "Play with Fast", "emulator defaults"
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
        stopViaOverlay(ui);
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
        goTo(ui, Settings);
        QTRY_VERIFY(purge->isVisible());
        shot(ui, "settings-purge");

        CHECK_NO_WARNINGS();
    }

    void settingsVolumeAndRemap_data() { addWindowSizeRows(); }
    void settingsVolumeAndRemap()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        Ui ui;
        load(ui, dir.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        goTo(ui, Settings);
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
    void gameBarOverlay_data() { addWindowSizeRows(); }
    void gameBarOverlay()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("long"), QStringLiteral("\"--sleep-ms\",\"30000\",\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        Ui ui;
        load(ui, dir.path(), windowSize);
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

    // Every one of the eight sections is reachable with L1/R1 alone and with the tab bar's
    // Left/Right, shows its page and gives it focus; Systems -> Games filtered by system.
    void allSectionsWithKeysOnly_data() { addWindowSizeRows(); }
    void allSectionsWithKeysOnly()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"--sleep-ms\",\"30000\",\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        Ui ui;
        load(ui, config.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;

        const char *pages[TabCount] = {"homeScreen",   "gamesScreen",     "systemsScreen",  "favoritesScreen",
                                       "recentScreen", "emulatorsScreen", "settingsScreen", "systemScreen"};
        const char *tabNames[TabCount] = {"tab_home",   "tab_games",     "tab_systems",  "tab_favorites",
                                          "tab_recent", "tab_emulators", "tab_settings", "tab_system"};
        for (int i = 0; i < TabCount; ++i) {
            QVERIFY2(find(ui, tabNames[i]), tabNames[i]);
            if (i > 0)
                key(ui, Qt::Key_PageDown);
            QCOMPARE(currentTab(ui), i);
            auto *page = find(ui, pages[i]);
            QVERIFY2(page && page->isVisible(), pages[i]);
            QTRY_VERIFY2_WITH_TIMEOUT(page->hasActiveFocus(),
                                      qPrintable(QString::fromLatin1(pages[i]) + u' ' + focusName(ui)), Timeout);
        }
        QVERIFY(!find(ui, "tab_downloads")); // not implemented, so not offered

        // Back along the tab bar.
        auto *tabBar = find(ui, "tabBar");
        for (int i = 0; i < 10 && !tabBar->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Up);
        QTRY_VERIFY2_WITH_TIMEOUT(tabBar->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        for (int i = TabCount - 2; i >= 0; --i) {
            key(ui, Qt::Key_Left);
            QCOMPARE(currentTab(ui), i);
            QVERIFY(find(ui, pages[i])->isVisible());
        }

        // Systems: the game's system card opens Games filtered to it.
        goTo(ui, Systems);
        key(ui, Qt::Key_Down); // from the tab bar into the page
        auto *systemGrid = find(ui, "systemGrid");
        QTRY_VERIFY(systemGrid->property("count").toInt() > 0);
        QTRY_VERIFY2_WITH_TIMEOUT(systemGrid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QCOMPARE(currentTab(ui), int(Games));
        QTRY_COMPARE_WITH_TIMEOUT(find(ui, "gameGrid")->property("count").toInt(), 1, Timeout);

        // Favorites / Recent start empty and explain themselves.
        goTo(ui, Favorites);
        QVERIFY(find(ui, "favoritesScreenEmpty")->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "favoritesScreenBrowse")->hasActiveFocus(), qPrintable(focusName(ui)),
                                  Timeout);
        key(ui, Qt::Key_Return); // "Browse games"
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_Favorites);
        QTRY_COMPARE_WITH_TIMEOUT(ui.context->library()->favoriteGames()->rowCount(), 1, Timeout);
        goTo(ui, Favorites);
        QTRY_VERIFY(find(ui, "favoritesScreenGrid")->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "favoritesScreenGrid")->hasActiveFocus(), qPrintable(focusName(ui)),
                                  Timeout);
        key(ui, Qt::Key_Return); // details
        QTRY_VERIFY(find(ui, "gameDetails")->isVisible());
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "favoritesScreenGrid")->hasActiveFocus(), qPrintable(focusName(ui)),
                                  Timeout);

        CHECK_NO_WARNINGS();
    }

    // High contrast + reduced motion at both TV resolutions: every section, the game details,
    // the update panel (honest "not on this build" on desktop) and the in-game quick menu with
    // its Exit confirmation render without a single QML warning.
    void rolledBackUpdateIsReportedAndDismissed_data() { addWindowSizeRows(); }
    void rolledBackUpdateIsReportedAndDismissed()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir dir;
        writeConfig(dir, QStringLiteral("fake"));
        Ui ui;
        load(ui, dir.path(), windowSize, false,
             R"({"format": 1, "result": "rolled-back", "previousBuildId": "v0.4.0-0-gcafebabe", )"
             R"("newBuildId": "v0.4.1-0-gabcdef0", "previousVersion": "0.4.0", "newVersion": "0.4.1", "slot": "a"})");

        // Home says so honestly instead of "Not on this build" / "No update selected".
        // The cards are a JS array model whose delegates are recreated when it changes, so the
        // text item is looked up again every time.
        auto updatesValue = [&ui] {
            QQuickItem *value = find(ui, "statusValue_updates");
            return value ? value->property("text").toString() : QString();
        };
        QTRY_COMPARE(updatesValue(), QStringLiteral("Update rolled back"));

        // The panel explains it and offers a dismiss action; afterwards it is gone for good.
        QMetaObject::invokeMethod(ui.window, "openUpdatePanel");
        auto *panel = find(ui, "updatePanel");
        QTRY_VERIFY(panel->isVisible());
        QCOMPARE(find(ui, "updateState")->property("text").toString(),
                 QStringLiteral("The last update was rolled back"));
        QVERIFY(find(ui, "updateMessage")->property("text").toString().contains(QStringLiteral("0.4.1")));
        auto *ack = find(ui, "updateAcknowledge");
        QVERIFY(ack->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(ack->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE(ui.context->updates()->state(), QStringLiteral("idle"));
        QTRY_VERIFY(!ack->isVisible());
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!panel->isVisible());
        QTRY_COMPARE(updatesValue(), QStringLiteral("Not on this build"));
        CHECK_NO_WARNINGS();
    }

    void accessibilityModes_data() { addWindowSizeRows(); }
    void accessibilityModes()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"--sleep-ms\",\"30000\",\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"],\"pausable\":true"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        touch(games, QStringLiteral("Beta Racer.rom"));
        Ui ui;
        load(ui, config.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        settings->setTheme(QStringLiteral("highContrast"));
        settings->setReducedMotion(true);
        settings->setUiScale(125);
        QTRY_COMPARE(ui.window->color(), QColor(QStringLiteral("#000000")));

        for (int i = 1; i < TabCount; ++i) {
            key(ui, Qt::Key_PageDown);
            QCOMPARE(currentTab(ui), i);
            QTest::qWait(20);
        }
        shot(ui, "high-contrast-system");

        // Details of the first game.
        goTo(ui, Games);
        auto *grid = find(ui, "gameGrid");
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(find(ui, "gameDetails")->isVisible());
        shot(ui, "high-contrast-details");

        // Remove from library: the confirmation defaults to Cancel.
        auto *remove = find(ui, "detailsRemove");
        for (int i = 0; i < 12 && !remove->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(remove->hasActiveFocus(), qPrintable(focusName(ui)));
        key(ui, Qt::Key_Return);
        auto *confirm = find(ui, "confirmRemoveGame");
        QTRY_VERIFY(confirm->isVisible());
        // Several ConfirmDialogs exist; the focused button must be this one's Cancel.
        QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem()
                                      && ui.window->activeFocusItem()->objectName()
                                          == QStringLiteral("confirmDialogCancel")
                                      && confirm->isAncestorOf(ui.window->activeFocusItem()),
                                  qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!confirm->isVisible());
        QCOMPARE(ui.context->library()->games()->rowCount(), 2);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Update panel from the System tab: unsupported on the desktop platform, and says so.
        goTo(ui, System);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "systemUpdateButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        auto *panel = find(ui, "updatePanel");
        QTRY_VERIFY(panel->isVisible());
        QVERIFY(find(ui, "updateUnsupported")->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "updateClose")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_PageDown); // modal
        QCOMPARE(currentTab(ui), int(System));
        shot(ui, "high-contrast-updates");
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!panel->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "systemUpdateButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Quick menu: hidden without a game; Menu / F1 on the running overlay opens it.
        auto *menu = find(ui, "quickMenu");
        QVERIFY(!menu->isVisible());
        QVERIFY(ui.context->library()->launch(ui.context->library()->games()->gameIdAt(0)));
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_F1);
        QTRY_VERIFY(menu->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "quickReturn")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(find(ui, "quickPause")->isVisible());
        shot(ui, "high-contrast-quick-menu");
        key(ui, Qt::Key_Escape); // back to the game
        QTRY_VERIFY(!menu->isVisible());
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Exit game asks first; Cancel keeps it running, Confirm stops it.
        key(ui, Qt::Key_F1);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "quickReturn")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        auto *exitRow = find(ui, "quickExit");
        for (int i = 0; i < 12 && !exitRow->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(exitRow->hasActiveFocus(), qPrintable(focusName(ui)));
        key(ui, Qt::Key_Return);
        auto *confirmExit = find(ui, "confirmExitGame");
        QTRY_VERIFY(confirmExit->isVisible());
        key(ui, Qt::Key_Return); // default focus is Cancel
        QTRY_VERIFY(!confirmExit->isVisible());
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(confirmExit->isVisible());
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY(!menu->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem() && !find(ui, "runningOverlay")->isVisible(),
                                  qPrintable(focusName(ui)), Timeout);

        CHECK_NO_WARNINGS();
    }

    // Quick Resume end to end through the UI: Quick Menu › Suspend and go Home saves the game
    // (control-socket adapter, fake emulator), Home shows it in the Quick Resume row, Accept
    // resumes from the state, a state that cannot be resumed explains why, Select / O removes it.
    void quickResumeFromQuickMenuAndHome_data() { addWindowSizeRows(); }
    void quickResumeFromQuickMenuAndHome()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        QTemporaryDir games;
        writeConfig(config, QStringLiteral("qr"), QStringLiteral("\"--sleep-ms\",\"30000\""),
                    QStringLiteral("\"extensions\":[\".sfc\"],\"capabilities\":{\"saveState\":{\"adapter\":"
                                   "\"control-socket\",\"timeoutMs\":3000,\"launchArguments\":[\"--control-socket\","
                                   "\"{control-socket}\"],\"resumeArguments\":[\"--load-state\",\"{state-file}\"]}}"));
        touch(games, QStringLiteral("Zelda.sfc"));
        Ui ui;
        load(ui, config.path(), windowSize);
        addGames(ui, games);
        auto *sessions = ui.context->sessions();
        auto *row = find(ui, "quickResumeRow");
        QVERIFY(row);
        QVERIFY(!row->isVisible());

        QVERIFY(ui.context->library()->launch(ui.context->library()->games()->gameIdAt(0)));
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(sessions->canSuspend(), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        // The chrome names the game (not "Zelda (Emu qr)"), the header with a capitalized state.
        const QString title = sessions->active().value(QStringLiteral("title")).toString();
        QVERIFY(!title.isEmpty());
        QCOMPARE(find(ui, "runningTitle")->property("text").toString(), title);
        QCOMPARE(find(ui, "stateChip")->property("label").toString(), title + QStringLiteral(" · Running"));
        ui.context->gameBar()->toggle();
        QTRY_VERIFY(find(ui, "gameBar")->isVisible());
        QCOMPARE(find(ui, "gameBarTitle")->property("text").toString(), title);
        ui.context->gameBar()->toggle();
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "stopButton")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_F1);
        auto *menu = find(ui, "quickMenu");
        QTRY_VERIFY(menu->isVisible());
        QCOMPARE(find(ui, "quickMenuTitle")->property("text").toString(), title);
        auto *suspendRow = find(ui, "quickSuspend");
        QVERIFY(suspendRow->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "quickReturn")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        for (int i = 0; i < 6 && !suspendRow->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(suspendRow->hasActiveFocus(), qPrintable(focusName(ui)));
        shot(ui, "quick-menu-suspend");
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY(!menu->isVisible());
        QCOMPARE(sessions->records().size(), 1);
        QTRY_VERIFY(row->isVisible());
        shot(ui, "home-quick-resume");

        // Accept on the card resumes from the saved state.
        auto *list = find(ui, "quickResumeList");
        list->forceActiveFocus();
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(
            ui.app->manager()->capturedStdout().contains(QStringLiteral("loaded-state: fake-state 1")), Timeout);
        QVERIFY(!row->isVisible() || sessions->suspended().isEmpty()); // the running game is not offered
        ui.app->stop();
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);
        QTRY_VERIFY(row->isVisible());

        // A state whose game file is gone cannot be resumed: the dialog says why, keeps it.
        QVERIFY(QFile::remove(games.filePath(QStringLiteral("Zelda.sfc"))));
        list->forceActiveFocus();
        key(ui, Qt::Key_Return);
        auto *failed = find(ui, "resumeFailedDialog");
        QTRY_VERIFY(failed->isVisible());
        QVERIFY(failed->property("title").toString().startsWith(QStringLiteral("Can't resume")));
        shot(ui, "quick-resume-failed");
        key(ui, Qt::Key_Return); // default focus is "Not now"
        QTRY_VERIFY(!failed->isVisible());
        QCOMPARE(sessions->records().size(), 1);

        // Select / O asks before removing the state.
        list->forceActiveFocus();
        key(ui, Qt::Key_O);
        auto *confirm = find(ui, "confirmRemoveQuickResume");
        QTRY_VERIFY(confirm->isVisible());
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(sessions->records().isEmpty());
        QTRY_VERIFY(!row->isVisible());

        CHECK_NO_WARNINGS();
    }

    // --- UI review regressions ---------------------------------------------------------------

    // Settings › Emulators: one default-emulator row per system that has games or a
    // compatible emulator (none for "Other"); each row only offers Automatic + emulators that
    // declare that system; changing the value keeps focus on the row.
    void defaultEmulatorRowsKeepFocus()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("snesemu"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".sfc\"],\"systems\":[\"snes\"]"));
        writeConfig(config, QStringLiteral("gbaemu"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".gba\"],\"systems\":[\"gba\"]"));
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.sfc"));
        touch(games, QStringLiteral("Gamma Unknown.rom")); // no recognised system: "Other"
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        goTo(ui, Settings);
        auto *screen = find(ui, "settingsScreen");

        QStringList rows;
        for (QQuickItem *row : visibleWithPrefix(screen, QStringLiteral("defaultEmulator_")))
            rows.append(row->objectName());
        rows.sort();
        QCOMPARE(rows, (QStringList{QStringLiteral("defaultEmulator_gba"), QStringLiteral("defaultEmulator_snes")}));

        auto *row = find(ui, "defaultEmulator_snes");
        for (int i = 0; i < 80 && !row->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(row->hasActiveFocus(), qPrintable(focusName(ui)));
        QCOMPARE(row->property("value").toString(), QStringLiteral("Automatic"));

        key(ui, Qt::Key_Right);
        QCOMPARE(settings->defaultEmulatorForSystem(QStringLiteral("snes")), QStringLiteral("snesemu"));
        QTest::qWait(100); // a model reset would drop focus within the event loop
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "defaultEmulator_snes")->hasActiveFocus(), qPrintable(focusName(ui)),
                                  Timeout);
        QCOMPARE(find(ui, "defaultEmulator_snes")->property("value").toString(), QStringLiteral("Emu snesemu"));
        // Only Automatic and the one compatible emulator: Right wraps back to Automatic.
        key(ui, Qt::Key_Right);
        QCOMPARE(settings->defaultEmulatorForSystem(QStringLiteral("snes")), QString());
        key(ui, Qt::Key_Left);
        QCOMPARE(settings->defaultEmulatorForSystem(QStringLiteral("snes")), QStringLiteral("snesemu"));
        QTest::qWait(100);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "defaultEmulator_snes")->hasActiveFocus(), qPrintable(focusName(ui)),
                                  Timeout);
        // Down still moves on from the row.
        key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(!find(ui, "defaultEmulator_snes")->hasActiveFocus() && focusInside(ui, screen),
                                  qPrintable(focusName(ui)), Timeout);

        CHECK_NO_WARNINGS();
    }

    // Modal dialogs and the input test panel keep every key (L1/R1, Menu, Search, "/");
    // hiding them (tab switch from code) closes them instead of leaving them armed.
    void modalKeysDoNotLeak()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("alpha"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        QVERIFY(!ui.context->system()->createBackup(false).isEmpty());
        goTo(ui, Settings);
        auto *screen = find(ui, "settingsScreen");
        callQml(screen, "refreshBackups");
        auto *restoreRow = find(ui, "restoreBackupRow");
        restoreRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem()
                                      && ui.window->activeFocusItem()->objectName()
                                          == QStringLiteral("confirmDialogCancel"),
                                  qPrintable(focusName(ui)), Timeout);
        QQuickItem *cancel = ui.window->activeFocusItem();
        const auto leakKeys
            = {Qt::Key_PageDown, Qt::Key_PageUp, Qt::Key_F1, Qt::Key_Menu, Qt::Key_Slash, Qt::Key_Up, Qt::Key_Down};
        for (Qt::Key k : leakKeys) {
            key(ui, k);
            QCOMPARE(currentTab(ui), int(Settings));
            QCOMPARE(ui.window->activeFocusItem(), cancel);
        }
        QTest::sendKeyEvent(QTest::Click, ui.window, Qt::Key_Search, char(0), Qt::NoModifier);
        key(ui, Qt::Key_F, Qt::ControlModifier);
        key(ui, Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(currentTab(ui), int(Settings));
        QCOMPARE(ui.window->activeFocusItem(), cancel);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(restoreRow->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Hidden by a tab switch from code: the dialog is cancelled, not left armed.
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.window->activeFocusItem(), cancel, Timeout);
        callQml(ui.window, "openTab", QStringLiteral("home"));
        QCOMPARE(currentTab(ui), int(Home));
        goTo(ui, Settings);
        QTest::qWait(100);
        QVERIFY(!cancel->isVisible());
        QVERIFY2(ui.window->activeFocusItem() != cancel, qPrintable(focusName(ui)));

        // Input test panel: same key trap, honest hints and the real Back button name.
        QVERIFY(ui.context->settings()->remap(QStringLiteral("back"), QStringLiteral("west")));
        auto *testRow = find(ui, "testInputRow");
        testRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        auto *panel = find(ui, "inputTestPanel");
        QTRY_VERIFY(panel->isVisible());
        for (Qt::Key k : leakKeys) {
            key(ui, k);
            QCOMPARE(currentTab(ui), int(Settings));
            QVERIFY(panel->isVisible());
        }
        QVERIFY2(!hintActions(ui).contains(QStringLiteral("Confirm")), qPrintable(hintActions(ui).join(u',')));
        QVERIFY2(hintActions(ui).contains(QStringLiteral("Close")), qPrintable(hintActions(ui).join(u',')));
        auto *help = find(ui, "inputTestHelp");
        QVERIFY(help);
        if (ui.app->input())
            QVERIFY2(help->property("text").toString().contains(QStringLiteral("West (X / Square)")),
                     qPrintable(help->property("text").toString()));
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!panel->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(testRow->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Hidden by a tab switch from code: closed, and test mode is off again.
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(panel->isVisible());
        callQml(ui.window, "openTab", QStringLiteral("home"));
        goTo(ui, Settings);
        QTest::qWait(100);
        QVERIFY(!panel->isVisible());
        if (ui.app->input())
            QVERIFY(!ui.app->input()->isTestMode());

        CHECK_NO_WARNINGS();
    }

    // Settings › System actions report failure in place (danger colour) and success with
    // the path; Restore backup and Clear artwork cache report their outcome with a notice.
    void settingsActionFeedback()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("alpha"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        const QString root = ui.state->filePath(QStringLiteral("data"));
        const QStringList dirs{root + QStringLiteral("/state/diagnostics"), root + QStringLiteral("/data/backups")};
        for (const QString &dir : dirs) {
            QDir(dir).removeRecursively();
            QDir().mkpath(QFileInfo(dir).absolutePath());
            QFile f(dir);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }
        goTo(ui, Settings);
        const QColor danger(QStringLiteral("#ff6372"));

        auto *exportRow = find(ui, "exportDiagnosticsRow");
        exportRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        QString description = exportRow->property("description").toString();
        QVERIFY2(description.startsWith(QStringLiteral("Export failed")), qPrintable(description));
        QVERIFY2(description.contains(ui.context->system()->lastError()), qPrintable(description));
        QCOMPARE(exportRow->property("descriptionColor").value<QColor>(), danger);

        auto *backupRow = find(ui, "createBackupRow");
        backupRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        description = backupRow->property("description").toString();
        QVERIFY2(description.startsWith(QStringLiteral("Backup failed")), qPrintable(description));
        QCOMPARE(backupRow->property("descriptionColor").value<QColor>(), danger);
        shot(ui, "settings-action-failed");

        // Success: the path, in the normal colour.
        for (const QString &dir : dirs) {
            QFile::remove(dir);
            QVERIFY(QDir().mkpath(dir));
        }
        exportRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        description = exportRow->property("description").toString();
        QVERIFY2(description.startsWith(QStringLiteral("Saved to ")), qPrintable(description));
        QVERIFY(exportRow->property("descriptionColor").value<QColor>() != danger);
        backupRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        QVERIFY2(backupRow->property("description").toString().startsWith(QStringLiteral("Saved to ")),
                 qPrintable(backupRow->property("description").toString()));

        // Restore (after its confirmation) and Clear artwork cache show a notice.
        auto *toast = find(ui, "toast");
        auto *restoreRow = find(ui, "restoreBackupRow");
        restoreRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem()
                                      && ui.window->activeFocusItem()->objectName()
                                          == QStringLiteral("confirmDialogCancel"),
                                  qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Return);
        // This fixture keeps settings and the database outside the backed-up data layout, so
        // the backup is empty and the restore fails: the notice says so (success is covered by
        // tst_uicontext's restoreBackupRoundTripsThroughTheController).
        QTRY_VERIFY2_WITH_TIMEOUT(
            toast->property("message").toString().startsWith(QStringLiteral("Restoring the backup failed: ")),
            qPrintable(toast->property("message").toString()), Timeout);
        QVERIFY(toast->property("isError").toBool());

        auto *clearRow = find(ui, "clearArtworkCacheRow");
        clearRow->forceActiveFocus();
        key(ui, Qt::Key_Return);
        key(ui, Qt::Key_Return); // requireConfirm
        QTRY_VERIFY_WITH_TIMEOUT(toast->property("message").toString() == QStringLiteral("Artwork cache cleared"),
                                 Timeout);
        QVERIFY(!toast->property("isError").toBool());

        CHECK_NO_WARNINGS();
    }

    // Stopping from the running overlay asks first (default Cancel); the scrim is opaque in
    // high contrast.
    void runningOverlayAsksBeforeStopping()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("long"), QStringLiteral("\"--sleep-ms\",\"30000\""));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        ui.context->settings()->setTheme(QStringLiteral("highContrast"));
        goTo(ui, Emulators);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "emulatorList")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("running"), Timeout);
        auto *overlay = find(ui, "runningOverlay");
        QTRY_VERIFY(overlay->isVisible());
        QVERIFY(!overlay->childItems().isEmpty());
        QCOMPARE(overlay->childItems().first()->opacity(), 1.0);
        auto *stop = find(ui, "stopButton");
        QTRY_VERIFY2_WITH_TIMEOUT(stop->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        key(ui, Qt::Key_Return);
        auto *confirm = find(ui, "confirmStopGame");
        QVERIFY(confirm);
        QTRY_VERIFY(confirm->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem()
                                      && ui.window->activeFocusItem()->objectName()
                                          == QStringLiteral("confirmDialogCancel")
                                      && confirm->isAncestorOf(ui.window->activeFocusItem()),
                                  qPrintable(focusName(ui)), Timeout);
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        shot(ui, "confirm-stop-game");
        key(ui, Qt::Key_Return); // Cancel
        QTRY_VERIFY(!confirm->isVisible());
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        QTRY_VERIFY2_WITH_TIMEOUT(stop->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(confirm->isVisible());
        key(ui, Qt::Key_Escape); // Back cancels too
        QTRY_VERIFY(!confirm->isVisible());
        QCOMPARE(ui.app->state(), QStringLiteral("running"));
        stopViaOverlay(ui);
        QTRY_COMPARE_WITH_TIMEOUT(ui.app->state(), QStringLiteral("exited"), Timeout);

        CHECK_NO_WARNINGS();
    }

    // 150 % interface scale with high contrast at both resolutions: a whole game card fits
    // above the scan summary, every tab label fits, the metadata editor and the on-screen
    // keyboard stay inside the window. Screenshots: NEXORA_SCREENSHOT_DIR.
    void layoutAtLargeScale_data() { addWindowSizeRows(); }
    void layoutAtLargeScale()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest With A Rather Long Title That Goes On.rom"));
        touch(games, QStringLiteral("Beta Racer.rom"));
        touch(games, QStringLiteral("Gamma Saga.rom"));
        touch(games, QStringLiteral("Delta Force.rom"));
        Ui ui;
        load(ui, config.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        ui.context->settings()->setTheme(QStringLiteral("highContrast"));
        ui.context->settings()->setUiScale(150);
        QTest::qWait(50);

        // Resolution factor: 1080p scales like 720p (tab height = 42 px x 1.5 x height/720).
        const qreal factor = qMax(1.0, qMin(windowSize.width() / 1280.0, windowSize.height() / 720.0));
        QTRY_COMPARE(find(ui, "tab_home")->height(), qreal(qRound(42 * 1.5 * factor)));

        goTo(ui, Games);
        auto *grid = find(ui, "gameGrid");
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 4, Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QTest::qWait(100);
        const qreal cellHeight = grid->property("cellHeight").toReal();
        QVERIFY2(cellHeight <= grid->height(),
                 qPrintable(QStringLiteral("card %1 > grid %2").arg(cellHeight).arg(grid->height())));
        QVERIFY(sceneRect(grid).bottom() <= sceneRect(find(ui, "scanSummary")).top() + 0.5);
        for (const char *tab : {"tab_home", "tab_games", "tab_systems", "tab_favorites", "tab_recent", "tab_emulators",
                                "tab_settings", "tab_system"}) {
            QQuickItem *label = firstText(find(ui, tab));
            QVERIFY2(label && !label->property("truncated").toBool(), tab);
        }
        shot(ui, qPrintable(QStringLiteral("large-scale-%1-games").arg(windowSize.height())));

        // Details -> Edit details -> keyboard.
        key(ui, Qt::Key_Return);
        auto *details = find(ui, "gameDetails");
        QTRY_VERIFY(details->isVisible());
        auto *edit = find(ui, "detailsEdit");
        for (int i = 0; i < 12 && !edit->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(edit->hasActiveFocus(), qPrintable(focusName(ui)));
        key(ui, Qt::Key_Return);
        auto *editor = find(ui, "metadataEditor");
        QTRY_VERIFY(editor->isVisible());
        QTest::qWait(100);
        const QRectF visible = sceneRect(editor).intersected(QRectF(QPointF(0, 0), QSizeF(ui.window->size())));
        auto *editorPanel = find(ui, "metadataPanel");
        QVERIFY(editorPanel);
        QVERIFY2(visible.contains(sceneRect(editorPanel)), "metadata editor panel is cut off");
        shot(ui, qPrintable(QStringLiteral("large-scale-%1-editor").arg(windowSize.height())));
        // The last row can be reached and is then shown inside the panel.
        auto *cancelRow = find(ui, "metadataCancel");
        for (int i = 0; i < 8 && !cancelRow->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QVERIFY2(cancelRow->hasActiveFocus(), qPrintable(focusName(ui)));
        QTest::qWait(100);
        QVERIFY2(sceneRect(editorPanel).contains(sceneRect(cancelRow)), "Cancel row is cut off");
        for (int i = 0; i < 8 && !find(ui, "metadataTitle")->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Up);
        QVERIFY2(find(ui, "metadataTitle")->hasActiveFocus(), qPrintable(focusName(ui)));

        key(ui, Qt::Key_Return); // Title -> keyboard
        auto *osk = find(ui, "onScreenKeyboard");
        QTRY_VERIFY(osk->isVisible());
        QTest::qWait(100);
        auto *oskPanel = find(ui, "oskPanel");
        QVERIFY(oskPanel);
        const QRectF oskArea = sceneRect(osk).intersected(QRectF(QPointF(0, 0), QSizeF(ui.window->size())));
        QVERIFY2(oskArea.contains(sceneRect(oskPanel)), "keyboard panel is cut off");
        QVERIFY2(oskArea.contains(sceneRect(find(ui, "oskKeys"))), "keyboard keys are cut off");
        shot(ui, qPrintable(QStringLiteral("large-scale-%1-osk").arg(windowSize.height())));
        key(ui, Qt::Key_Escape);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!editor->isVisible());

        CHECK_NO_WARNINGS();
    }

    // Dark and high contrast keep the docked chrome: header at the top, the tab bar under it,
    // the pages between the tab bar and the hint bar (to the bottom without hints), and the
    // window-level overlays between the header and the hint bar.
    void dockedChromeLayout_data() { addWindowSizeRows(); }
    void dockedChromeLayout()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("alpha"));
        Ui ui;
        load(ui, config.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        auto *header = find(ui, "header");
        auto *tabBar = find(ui, "tabBar");
        auto *hintBar = find(ui, "hintBar");
        QVERIFY(header && tabBar && hintBar);
        const QSizeF size = ui.window->size();
        for (const QString &theme : {QStringLiteral("dark"), QStringLiteral("highContrast")}) {
            settings->setTheme(theme);
            for (const bool hints : {true, false}) {
                settings->setShowHints(hints);
                QTest::qWait(50);
                const QString row = theme + (hints ? QStringLiteral(" + hints") : QStringLiteral(""));
                const qreal top = header->height();
                const qreal pagesTop = top + tabBar->height();
                const qreal bottom = hints ? size.height() - hintBar->height() : size.height();
                QCOMPARE(sceneRect(header), QRectF(0, 0, size.width(), top));
                QCOMPARE(sceneRect(tabBar), QRectF(0, top, size.width(), tabBar->height()));
                if (hints)
                    QCOMPARE(sceneRect(hintBar), QRectF(0, bottom, size.width(), hintBar->height()));
                for (const char *page : {"homeScreen", "gamesScreen", "systemsScreen", "favoritesScreen",
                                         "recentScreen", "emulatorsScreen", "settingsScreen", "systemScreen",
                                         "gameDetails"})
                    QVERIFY2(sceneRect(find(ui, page)) == QRectF(0, pagesTop, size.width(), bottom - pagesTop),
                             qPrintable(row + u' ' + QString::fromLatin1(page)));
                for (const char *overlay : {"runningOverlay", "tutorialOverlay", "gameOptions", "updatePanel"})
                    QVERIFY2(sceneRect(find(ui, overlay)) == QRectF(0, top, size.width(), bottom - top),
                             qPrintable(row + u' ' + QString::fromLatin1(overlay)));
                QCOMPARE(ui.window->property("chromeTop").toReal(), 0.0);
                QCOMPARE(ui.window->property("chromeBottom").toReal(), 0.0);
            }
        }

        CHECK_NO_WARNINGS();
    }

    // Glass: one shared modal scrim covers the whole window (including the gaps around the
    // floating bars) above the pages but below the bars; the overlays' own scrims are
    // transparent so nothing is dimmed twice. The docked themes keep the overlay scrims.
    void glassModalScrimCoversWindowBelowChrome_data() { addWindowSizeRows(); }
    void glassModalScrimCoversWindowBelowChrome()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("alpha"));
        Ui ui;
        load(ui, config.path(), windowSize, true);
        if (QTest::currentTestFailed())
            return;
        auto *settings = ui.context->settings();
        QCOMPARE(settings->theme(), QStringLiteral("glass"));
        auto *scrim = find(ui, "glassModalScrim");
        auto *scene = find(ui, "sceneLayer");
        auto *chrome = find(ui, "glassChrome");
        QVERIFY(scrim && scene && chrome);
        QVERIFY(find(ui, "tutorialOverlay")->isVisible());

        const auto ownScrims = {"tutorialScrim", "gameOptionsScrim", "updatePanelScrim"};
        // Glass, tutorial open: the scrim fills the window and sits between the scene and the bars.
        QTRY_VERIFY(scrim->isVisible());
        QCOMPARE(sceneRect(scrim), QRectF(QPointF(0, 0), QSizeF(ui.window->size())));
        QCOMPARE(scrim->parentItem(), scene);
        const auto sceneKids = scene->childItems();
        QCOMPARE(sceneKids.last(), scrim);
        QCOMPARE(chrome->parentItem(), scene->parentItem());
        const auto siblings = scene->parentItem()->childItems();
        QVERIFY(siblings.indexOf(chrome) > siblings.indexOf(scene));
        QVERIFY(chrome->z() >= scene->z());
        // The bars sit inside |chrome|, so they are above the scrim too.
        for (const char *bar : {"header", "hintBar"}) {
            auto *item = find(ui, bar);
            QVERIFY(item);
            QVERIFY2(chrome->isAncestorOf(item), bar);
        }
        for (const char *name : ownScrims) {
            auto *own = find(ui, name);
            QVERIFY2(own, name);
            QCOMPARE(own->property("color").value<QColor>().alpha(), 0);
        }

        // Closed: no scrim.
        settings->setTutorialCompleted(true);
        QTRY_VERIFY(!find(ui, "tutorialOverlay")->isVisible());
        QTRY_VERIFY(!scrim->isVisible());

        // The update panel also raises it.
        QMetaObject::invokeMethod(ui.window, "openUpdatePanel");
        QTRY_VERIFY(find(ui, "updatePanel")->isVisible());
        QTRY_VERIFY(scrim->isVisible());
        QMetaObject::invokeMethod(find(ui, "updatePanel"), "close");
        QTRY_VERIFY(!scrim->isVisible());

        // Docked themes: never visible, overlay scrims keep their own fill.
        for (const QString &theme : {QStringLiteral("dark"), QStringLiteral("highContrast")}) {
            settings->setTheme(theme);
            settings->setTutorialCompleted(false);
            QTRY_VERIFY(find(ui, "tutorialOverlay")->isVisible());
            QTest::qWait(50);
            QVERIFY2(!scrim->isVisible(), qPrintable(theme));
            for (const char *name : ownScrims)
                QCOMPARE(find(ui, name)->property("color").value<QColor>().alpha(), 255);
            settings->setTutorialCompleted(true);
            QMetaObject::invokeMethod(ui.window, "openUpdatePanel");
            QTRY_VERIFY(find(ui, "updatePanel")->isVisible());
            QTest::qWait(50);
            QVERIFY2(!scrim->isVisible(), qPrintable(theme));
            QMetaObject::invokeMethod(find(ui, "updatePanel"), "close");
        }

        CHECK_NO_WARNINGS();
    }

    // Glass: the header, tab bar and hint bar float (inset from the window edges) over the
    // scene layer, which fills the window; the pages keep their first and last rows clear of
    // the bars.
    void glassChromeFloatsWithoutCoveringContent_data() { addWindowSizeRows(); }
    void glassChromeFloatsWithoutCoveringContent()
    {
        QFETCH(QSize, windowSize);
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        // One emulator for every catalogue system: enough Systems cards to scroll.
        writeConfig(config, QStringLiteral("multi"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".zzz\"],\"systems\":[\"nes\",\"snes\",\"n64\",\"gb\","
                                   "\"gbc\",\"gba\",\"nds\",\"genesis\",\"sms\",\"gamegear\",\"psx\",\"ps2\","
                                   "\"psp\",\"gamecube\",\"wii\",\"pce\",\"atari2600\",\"arcade\",\"dos\"]"));
        QTemporaryDir games;
        for (int i = 0; i < 24; ++i)
            touch(games, QStringLiteral("Game %1.rom").arg(i, 2, 10, QLatin1Char('0')));
        Ui ui;
        load(ui, config.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(ui.context->settings()->theme(), QStringLiteral("glass"));

        auto *chrome = find(ui, "glassChrome");
        auto *header = find(ui, "header");
        auto *tabBar = find(ui, "tabBar");
        auto *hintBar = find(ui, "hintBar");
        auto *scene = find(ui, "sceneLayer");
        QVERIFY(chrome && header && tabBar && hintBar && scene);
        // The scene (backdrop + pages) fills the window: content scrolls under the bars.
        QCOMPARE(scene->height(), ui.window->contentItem()->height());
        QCOMPARE(sceneRect(scene), QRectF(QPointF(0, 0), QSizeF(ui.window->size())));
        // Bars float: inset from the window edges.
        QVERIFY(sceneRect(header).top() > 0);
        QVERIFY(sceneRect(header).left() > 0);
        QVERIFY(sceneRect(header).right() < ui.window->width());
        QVERIFY(sceneRect(tabBar).top() > sceneRect(header).bottom());
        QVERIFY(hintBar->isVisible());
        QVERIFY(sceneRect(hintBar).bottom() < ui.window->height());
        const qreal tabBottom = sceneRect(tabBar).bottom();
        const qreal hintTop = sceneRect(hintBar).top();
        QVERIFY(ui.window->property("chromeTop").toReal() >= tabBottom);
        QVERIFY(ui.window->property("chromeBottom").toReal() >= ui.window->height() - hintTop);

        // A grid passes under the hint bar, yet walking down to its last row keeps every
        // current card fully above the bar (and above |above|, a line pinned over the grid).
        const auto checkScrollsUnder = [&](QQuickItem *view, qreal above) {
            QVERIFY(view->hasActiveFocus());
            QVERIFY2(sceneRect(view).bottom() > hintTop, view->objectName().toLatin1());
            const int count = view->property("count").toInt();
            const int columns = view->property("columns").toInt();
            QVERIFY(columns > 0);
            const int lastRow = (count - 1) / columns * columns; // index of the last row's first card
            const qreal startY = view->property("contentY").toReal();
            qreal settledY = startY;
            for (int i = 0; i < count && view->property("currentIndex").toInt() < lastRow; ++i) {
                key(ui, Qt::Key_Down);
                // Let the scroll animation settle before measuring (mid-way the card can pass).
                for (int w = 0; w < 50; ++w) {
                    QTest::qWait(100);
                    const qreal y = view->property("contentY").toReal();
                    if (qAbs(y - settledY) < 0.01)
                        break;
                    settledY = y;
                }
                auto *current = qvariant_cast<QQuickItem *>(view->property("currentItem"));
                QVERIFY(current);
                QVERIFY2(sceneRect(current).bottom() <= above,
                         qPrintable(QStringLiteral("%1 card %2: bottom %3 > %4")
                                        .arg(view->objectName())
                                        .arg(view->property("currentIndex").toInt())
                                        .arg(sceneRect(current).bottom())
                                        .arg(above)));
            }
            QVERIFY(view->property("currentIndex").toInt() >= lastRow);
            QVERIFY2(settledY > startY, qPrintable(view->objectName() + QStringLiteral(" did not scroll")));
        };

        // Games: the toolbar is below the tab bar; the grid scrolls under the scan summary
        // and the hint bar.
        goTo(ui, Games);
        auto *grid = find(ui, "gameGrid");
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 24, Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QTest::qWait(100);
        QVERIFY(sceneRect(find(ui, "searchField")).top() >= tabBottom);
        QVERIFY(sceneRect(grid).top() >= tabBottom);
        auto *summary = find(ui, "scanSummary");
        QVERIFY(sceneRect(summary).bottom() <= hintTop);
        checkScrollsUnder(grid, sceneRect(summary).top());
        if (QTest::currentTestFailed())
            return;
        shot(ui, qPrintable(QStringLiteral("glass-chrome-%1-games").arg(windowSize.height())));

        // Systems: the same with the system cards.
        goTo(ui, Systems);
        auto *systemGrid = find(ui, "systemGrid");
        QTRY_VERIFY2_WITH_TIMEOUT(systemGrid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QVERIFY(systemGrid->property("count").toInt() >= 19);
        QTest::qWait(100);
        QVERIFY(sceneRect(systemGrid).top() >= tabBottom);
        checkScrollsUnder(systemGrid, hintTop);
        if (QTest::currentTestFailed())
            return;

        // Settings: the first row starts below the tab bar; walking to the last row scrolls
        // it fully above the hint bar.
        goTo(ui, Settings);
        QTest::qWait(100);
        QVERIFY(sceneRect(find(ui, "sectionTitle_General")).top() >= tabBottom);
        auto *resetRow = find(ui, "resetRow");
        for (int i = 0; i < 100 && !resetRow->hasActiveFocus(); ++i)
            key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(resetRow->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(sceneRect(resetRow).bottom() <= hintTop,
                                  qPrintable(QStringLiteral("row bottom %1 > hint bar top %2")
                                                 .arg(sceneRect(resetRow).bottom())
                                                 .arg(hintTop)),
                                  Timeout);
        shot(ui, qPrintable(QStringLiteral("glass-chrome-%1-settings").arg(windowSize.height())));

        CHECK_NO_WARNINGS();
    }

    // Glass: a card scrolled under the floating hint bar cannot be clicked through the bar.
    void glassBarsSwallowClicks()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        for (int i = 0; i < 24; ++i)
            touch(games, QStringLiteral("Game %1.rom").arg(i, 2, 10, QLatin1Char('0')));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(ui.context->settings()->theme(), QStringLiteral("glass"));

        goTo(ui, Games);
        auto *grid = find(ui, "gameGrid");
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 24, Timeout);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QTest::qWait(100);
        QCOMPARE(grid->property("currentIndex").toInt(), 0);
        auto *details = find(ui, "gameDetails");
        QVERIFY(details && !details->isVisible());

        // A point on the hint bar with a card (not the current one) underneath.
        const QRectF hint = sceneRect(find(ui, "hintBar"));
        QPointF point;
        int under = -1;
        for (qreal x = hint.left() + 8; x < hint.right() - 8 && under <= 0; x += 16) {
            const QPointF local = grid->mapFromScene(QPointF(x, hint.center().y()));
            if (local.y() < 0 || local.y() >= grid->height())
                break;
            int index = -1;
            QMetaObject::invokeMethod(grid, "indexAt", Q_RETURN_ARG(int, index),
                                      Q_ARG(qreal, local.x() + grid->property("contentX").toReal()),
                                      Q_ARG(qreal, local.y() + grid->property("contentY").toReal()));
            if (index > 0) {
                under = index;
                point = QPointF(x, hint.center().y());
            }
        }
        QVERIFY2(under > 0, "no card under the hint bar");
        // Aim at the middle of the card (its edges are cell padding).
        QQuickItem *card = nullptr;
        QMetaObject::invokeMethod(grid, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, card), Q_ARG(int, under));
        QVERIFY(card);
        point.setX(sceneRect(card).center().x());
        QVERIFY(sceneRect(card).contains(point) && hint.contains(point));

        QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QTest::qWait(100);
        QTest::mouseDClick(ui.window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QTest::qWait(300);
        QCOMPARE(grid->property("currentIndex").toInt(), 0);
        QVERIFY(!details->isVisible());
        QCOMPARE(ui.app->state(), QStringLiteral("idle"));

        // The tab bar's own shield sits below its tabs: a tab still takes the click.
        auto *systemsTab = find(ui, "tab_systems");
        QVERIFY(systemsTab);
        QTest::mouseClick(ui.window, Qt::LeftButton, Qt::NoModifier, sceneRect(systemsTab).center().toPoint());
        QTRY_COMPARE(currentTab(ui), int(Systems));

        CHECK_NO_WARNINGS();
    }

    // Systems: the "Other" card shows only the games without a recognised system; a system
    // without games still shows (and selects) its chip.
    void systemsOtherAndEmptySystem()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("snesemu"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".sfc\"],\"systems\":[\"snes\"]"));
        writeConfig(config, QStringLiteral("gbaemu"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".gba\"],\"systems\":[\"gba\"]"));
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.sfc"));
        touch(games, QStringLiteral("Gamma Unknown.rom"));
        touch(games, QStringLiteral("Omega Unknown.rom"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        auto *library = ui.context->library();
        auto cardIndex = [&ui](const QString &systemId) {
            const QVariantList installed = find(ui, "systemsScreen")->property("installed").toList();
            for (int i = 0; i < installed.size(); ++i) {
                if (installed.at(i).toMap().value(QStringLiteral("systemId")).toString() == systemId)
                    return i;
            }
            return -1;
        };

        goTo(ui, Systems);
        auto *systemGrid = find(ui, "systemGrid");
        QTRY_VERIFY2_WITH_TIMEOUT(systemGrid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        int other = -1;
        for (const QVariant &card : find(ui, "systemsScreen")->property("installed").toList()) {
            const QVariantMap map = card.toMap();
            if (map.value(QStringLiteral("name")).toString() == QStringLiteral("Other"))
                other = cardIndex(map.value(QStringLiteral("systemId")).toString());
        }
        QVERIFY(other >= 0);
        systemGrid->setProperty("currentIndex", other);
        key(ui, Qt::Key_Return);
        QCOMPARE(currentTab(ui), int(Games));
        auto *grid = find(ui, "gameGrid");
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 2, Timeout);
        QCOMPARE(library->totalCount(), 3);
        auto *otherChip = find(ui, "systemChip_other");
        QVERIFY(otherChip && otherChip->isVisible());
        QVERIFY(otherChip->property("selected").toBool());

        // A system with an emulator but no games: empty result, its chip selected.
        goTo(ui, Systems);
        systemGrid->forceActiveFocus();
        const int gba = cardIndex(QStringLiteral("gba"));
        QVERIFY(gba >= 0);
        systemGrid->setProperty("currentIndex", gba);
        key(ui, Qt::Key_Return);
        QCOMPARE(currentTab(ui), int(Games));
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 0, Timeout);
        auto *gbaChip = find(ui, "systemChip_gba");
        QVERIFY(gbaChip && gbaChip->isVisible());
        QVERIFY(gbaChip->property("selected").toBool());

        CHECK_NO_WARNINGS();
    }

    // Game bar › Quick settings › Restart / Power off ask through a confirmation dialog
    // (default Cancel) instead of an inline "press again".
    void gameBarPowerAsksFirst()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("alpha"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        key(ui, Qt::Key_G, Qt::ControlModifier);
        QTRY_VERIFY(find(ui, "gameBar")->isVisible());
        key(ui, Qt::Key_Right);
        QTRY_VERIFY(find(ui, "settingsPanel")->isVisible());
        for (const char *name : {"gameBarReboot", "gameBarPowerOff"}) {
            auto *tile = find(ui, name);
            QVERIFY(tile);
            tile->setProperty("enabled", true); // desktop: power control is off; the dialog is what is tested
            tile->forceActiveFocus();
            key(ui, Qt::Key_Return);
            QQuickItem *confirm
                = find(ui, qstrcmp(name, "gameBarReboot") == 0 ? "confirmGameBarReboot" : "confirmGameBarPowerOff");
            QVERIFY2(confirm, name);
            QTRY_VERIFY(confirm->isVisible());
            QTRY_VERIFY2_WITH_TIMEOUT(ui.window->activeFocusItem()
                                          && ui.window->activeFocusItem()->objectName()
                                              == QStringLiteral("confirmDialogCancel")
                                          && confirm->isAncestorOf(ui.window->activeFocusItem()),
                                      qPrintable(focusName(ui)), Timeout);
            key(ui, Qt::Key_PageDown); // nothing leaks
            key(ui, Qt::Key_Escape);
            QTRY_VERIFY(!confirm->isVisible());
            QVERIFY(ui.context->gameBar()->isOpen());
            QTRY_VERIFY2_WITH_TIMEOUT(tile->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        }
        // Confirmed: the request reaches the system layer (refused on desktop, so it says why).
        key(ui, Qt::Key_Return);
        key(ui, Qt::Key_Right);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(!ui.context->system()->lastError().isEmpty());
        QCOMPARE(currentTab(ui), int(Home));

        CHECK_NO_WARNINGS();
    }

    // The hardware probe (SystemController::refresh) never runs on the start-up path and is
    // not repeated on every tab show: once, lazily, when Settings/System become current.
    void systemInfoRefreshIsLazy()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("alpha"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        auto *system = ui.context->system();
        QTest::qWait(400);
        QCOMPARE(system->property("refreshCount").toInt(), 0);
        goTo(ui, Settings);
        QTRY_COMPARE_WITH_TIMEOUT(system->property("refreshCount").toInt(), 1, Timeout);
        goTo(ui, System);
        QTest::qWait(400);
        goTo(ui, Settings);
        QTest::qWait(400);
        QCOMPARE(system->property("refreshCount").toInt(), 1);
        QVERIFY(!system->hardware().isEmpty());

        CHECK_NO_WARNINGS();
    }

    // Back inside a page moves focus to the tab bar (Back there re-enters the page); dialogs
    // and the details screen consume Back themselves.
    void backFromPageGoesToTabBar()
    {
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, QStringLiteral("Alpha Quest.rom"));
        Ui ui;
        load(ui, config.path());
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;
        auto *tabBar = find(ui, "tabBar");
        goTo(ui, Games);
        auto *grid = find(ui, "gameGrid");
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(tabBar->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        QCOMPARE(currentTab(ui), int(Games));
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        // Details consume Back (close, focus back on the tile).
        key(ui, Qt::Key_Return);
        QTRY_VERIFY(find(ui, "gameDetails")->isVisible());
        key(ui, Qt::Key_Escape);
        QTRY_VERIFY(!find(ui, "gameDetails")->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        goTo(ui, Settings);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "settingLanguage")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Down);
        key(ui, Qt::Key_Back);
        QTRY_VERIFY2_WITH_TIMEOUT(tabBar->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Down);
        QTRY_VERIFY2_WITH_TIMEOUT(find(ui, "settingTheme")->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);

        CHECK_NO_WARNINGS();
    }
    // A file name that looks like HTML is shown literally everywhere its title appears
    // (tiles on Home and in the Games grid, the details heading), never styled or with an
    // image loaded. "/" cannot be part of a file name, so the markup has no closing tags.
    void markupInFileNameIsShownLiterally_data() { addWindowSizeRows(); }
    void markupInFileNameIsShownLiterally()
    {
        QFETCH(QSize, windowSize);
        const QString title = QStringLiteral("<b>Bold <font color=red size=40>Red <img src=cover.png>");
        QTemporaryDir config;
        writeConfig(config, QStringLiteral("fake"), QStringLiteral("\"{file}\""),
                    QStringLiteral("\"extensions\":[\".rom\"]"));
        QTemporaryDir games;
        touch(games, title + QStringLiteral(".rom"));
        Ui ui;
        load(ui, config.path(), windowSize);
        if (QTest::currentTestFailed())
            return;
        addGames(ui, games);
        if (QTest::currentTestFailed())
            return;

        const auto checkTiles = [&ui, &title] {
            QList<QQuickItem *> titles;
            QTRY_VERIFY_WITH_TIMEOUT(
                !(titles = visibleWithPrefix(ui.window->contentItem(), QStringLiteral("gameTileTitle"))).isEmpty(),
                Timeout);
            for (QQuickItem *text : std::as_const(titles)) {
                QCOMPARE(text->property("text").toString(), title);
                QCOMPARE(text->property("textFormat").toInt(), int(Qt::PlainText));
            }
        };
        QCOMPARE(currentTab(ui), int(Home));
        checkTiles();
        if (QTest::currentTestFailed())
            return;
        goTo(ui, Games);
        auto *grid = find(ui, "gameGrid");
        QTRY_COMPARE_WITH_TIMEOUT(grid->property("count").toInt(), 1, Timeout);
        checkTiles();
        if (QTest::currentTestFailed())
            return;

        QTRY_VERIFY2_WITH_TIMEOUT(grid->hasActiveFocus(), qPrintable(focusName(ui)), Timeout);
        key(ui, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(find(ui, "gameDetails")->isVisible(), Timeout);
        auto *heading = find(ui, "gameDetailsTitle");
        QCOMPARE(heading->property("text").toString(), title);
        QCOMPARE(heading->property("textFormat").toInt(), int(Qt::PlainText));
        shot(ui, "markup-title");

        CHECK_NO_WARNINGS();
    }
};

QTEST_MAIN(TestQmlSmoke)
#include "tst_qmlsmoke.moc"
