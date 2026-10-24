import QtQuick
import QtQuick.Controls
import Nexora.Console

// Full-screen dashboard: header, top-level tabs (Home, Games, Systems, Favorites, Recent,
// Emulators, Settings, System), the game details screen, the running-emulator overlay, the
// in-game quick menu, the game bar, the update panel and transient notices.
//
// Controllers arrive as key events (see InputController::keyForAction): arrows, Return
// (Accept), Escape/Back (Back), Menu/F1, Favorites, Search, PageUp/PageDown (L1/R1). The
// Guide button arrives through GameBarController instead (quick menu while a game runs, the
// game bar otherwise); Ctrl+G toggles the game bar from a keyboard.
ApplicationWindow {
    id: window
    objectName: "mainWindow"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system
    required property GameBarController gameBar
    required property UpdateController updates
    required property NotificationService notifications
    required property GameSessionManager sessions

    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Nexora OS"
    color: Theme.background
    background: Backdrop { objectName: "backdrop" }

    // Only implemented capabilities get a tab (no Downloads / Imports).
    readonly property var tabs: [{ key: "home", title: "Home" },
                                 { key: "games", title: "Games" },
                                 { key: "systems", title: "Systems" },
                                 { key: "favorites", title: "Favorites" },
                                 { key: "recent", title: "Recent" },
                                 { key: "emulators", title: "Emulators" },
                                 { key: "settings", title: "Settings" },
                                 { key: "system", title: "System" }]
    property int currentTab: 0
    readonly property string currentTabKey: tabs[currentTab].key
    readonly property var pages: [homeScreen, gamesScreen, systemsScreen, favoritesScreen, recentScreen,
                                  emulatorsScreen, settingsScreen, systemScreen]

    // Last focused item per tab (restored when switching back) and before the overlay.
    property var tabFocus: [null, null, null, null, null, null, null, null]
    property Item focusBeforeOverlay: null
    property Item focusBeforeGameBar: null
    property Item focusBeforeQuickMenu: null
    // The bar / quick menu raised this window above a running emulator; lower it on close.
    property bool raisedForGameBar: false
    property bool raisedForQuickMenu: false

    function tabIndex(key) {
        for (let i = 0; i < tabs.length; ++i) {
            if (tabs[i].key === key)
                return i
        }
        return -1
    }
    function pageIndexOf(item) {
        for (let p = item; p; p = p.parent) {
            const i = pages.indexOf(p)
            if (i >= 0)
                return i
        }
        return -1
    }
    function focusPage() {
        const page = pages[currentTab]
        const remembered = tabFocus[currentTab]
        if (remembered && remembered.visible && pageIndexOf(remembered) === currentTab) {
            remembered.forceActiveFocus()
            return
        }
        // First visit (or the remembered item is gone): the page picks its default.
        page.focusDefault()
        page.forceActiveFocus()
    }
    function switchTab(index) {
        if (index < 0 || index >= tabs.length)
            return
        const stayOnBar = tabBar.activeFocus
        currentTab = index
        if (!stayOnBar)
            focusPage()
    }
    function openTab(key) {
        switchTab(tabIndex(key))
        if (tabBar.activeFocus)
            focusPage()
    }
    // Home / Games buttons; with no library folder the Settings field to add one gets focus.
    function navigate(key) {
        if (details.visible)
            details.close()
        openTab(key)
        if (key === "settings" && window.library.directories.length === 0)
            settingsScreen.focusAddFolder()
    }
    function openSettingsSection(name) {
        openTab("settings")
        settingsScreen.focusSection(name)
    }
    // Systems tab / Home systems row: the Games tab filtered to one system. The "Other"
    // bucket has no system id; it maps to the library's filter for games without one.
    function showSystem(systemId) {
        window.library.searchText = ""
        window.library.filter = "all"
        window.library.systemFilter = systemId.length > 0 ? systemId : window.library.otherSystemFilter
        openTab("games")
    }
    function openGameDetails(gameId) {
        details.open(gameId, window.activeFocusItem)
    }
    function openUpdatePanel() {
        updatePanel.open()
    }
    function focusSearch() {
        openTab("games")
        gamesScreen.focusSearch()
    }
    function inGameBar(item) {
        for (let p = item; p; p = p.parent) {
            if (p === gameBarLayer)
                return true
        }
        return false
    }
    function inLayer(item, layer) {
        for (let p = item; p; p = p.parent) {
            if (p === layer)
                return true
        }
        return false
    }
    function openGameOptions(gameId) {
        gameOptions.open(gameId, window.activeFocusItem)
    }
    // True while |item| is inside an open ConfirmDialog (it keeps focus until answered).
    function inOpenDialog(item) {
        for (let p = item; p; p = p.parent) {
            if (p.isOpen === true)
                return true
        }
        return false
    }
    function restoreFocus() {
        // A dialog opened before this deferred call ran (e.g. "Can't resume" right after a
        // game ended) keeps its focus.
        if (inOpenDialog(window.activeFocusItem))
            return
        if (resumeFailedDialog.visible) {
            resumeFailedDialog.cancelButton.forceActiveFocus()
            return
        }
        if (window.gameBar.quickMenuOpen) {
            quickMenu.focusDefault()
            return
        }
        if (window.gameBar.open) {
            gameBarLayer.focusDefault()
            return
        }
        if (window.app.busy) {
            overlay.forceActiveFocus()
            return
        }
        if (updatePanel.visible) {
            updatePanel.forceActiveFocus()
            return
        }
        if (tutorial.visible) {
            tutorial.forceActiveFocus()
            return
        }
        const saved = focusBeforeOverlay
        focusBeforeOverlay = null
        if (saved && saved.visible)
            saved.forceActiveFocus()
        else if (details.visible)
            details.focusDefault()
        else
            focusPage()
    }

    onActiveFocusItemChanged: {
        const item = window.activeFocusItem
        const page = pageIndexOf(item)
        if (page >= 0 && item !== pages[page]) {
            const copy = tabFocus.slice()
            copy[page] = item
            tabFocus = copy
        }
        // Focus can drop out (e.g. the focused row was removed from its model).
        if (!item || item === window.contentItem)
            focusGuard.restart()
    }
    Timer {
        id: focusGuard
        interval: 50
        onTriggered: {
            const item = window.activeFocusItem
            if (!item || item === window.contentItem)
                window.restoreFocus()
        }
    }

    header: Header {
        app: window.app
        library: window.library
    }
    footer: HintBar {
        visible: window.settings.showHints
        hints: window.gameBar.quickMenuOpen ? quickMenu.hints
               : window.app.busy ? overlay.hints
               : tutorial.visible ? tutorial.hints
               : updatePanel.visible ? updatePanel.hints
               : gameOptions.visible ? gameOptions.hints
               : details.visible ? details.hints
               : tabBar.activeFocus
                 ? [{ button: "← →", action: "Choose tab" }, { button: "↓ / A", action: "Open" },
                    { button: "L1 / R1", action: "Switch tab" }]
                 : window.pages[window.currentTab].hints
    }

    FocusScope {
        id: contentRoot
        objectName: "contentRoot"
        anchors.fill: parent
        focus: true

        NavTabBar {
            id: tabBar
            objectName: "tabBar"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            tabs: window.tabs
            currentIndex: window.currentTab
            onTabRequested: (index) => {
                window.switchTab(index)
            }
            onEnterRequested: window.focusPage()
        }

        Item {
            id: pageArea
            anchors.top: tabBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            HomeScreen {
                id: homeScreen
                anchors.fill: parent
                visible: window.currentTab === 0
                app: window.app
                library: window.library
                updates: window.updates
                sessions: window.sessions
                onNavigate: (tab) => window.navigate(tab)
                onOptionsRequested: (gameId) => window.openGameOptions(gameId)
                onDetailsRequested: (gameId) => window.openGameDetails(gameId)
                onSystemChosen: (systemId) => window.showSystem(systemId)
                onSettingsSectionRequested: (section) => window.openSettingsSection(section)
                onUpdatesRequested: window.openUpdatePanel()
            }
            GamesScreen {
                id: gamesScreen
                anchors.fill: parent
                visible: window.currentTab === 1
                app: window.app
                library: window.library
                onNavigate: (tab) => window.navigate(tab)
                onOptionsRequested: (gameId) => window.openGameOptions(gameId)
                onDetailsRequested: (gameId) => window.openGameDetails(gameId)
            }
            SystemsScreen {
                id: systemsScreen
                anchors.fill: parent
                visible: window.currentTab === 2
                app: window.app
                library: window.library
                onNavigate: (tab) => window.navigate(tab)
                onSystemChosen: (systemId) => window.showSystem(systemId)
            }
            GameCollectionScreen {
                id: favoritesScreen
                objectName: "favoritesScreen"
                anchors.fill: parent
                visible: window.currentTab === 3
                library: window.library
                games: window.library.favoriteGames
                title: "Favorites"
                subtitle: "Press Y / F on any game to add or remove it"
                emptyTitle: "No favorites yet"
                emptyText: "Press Y / F on a game in Games or Home to mark it as a favorite."
                onNavigate: (tab) => window.navigate(tab)
                onOptionsRequested: (gameId) => window.openGameOptions(gameId)
                onDetailsRequested: (gameId) => window.openGameDetails(gameId)
            }
            GameCollectionScreen {
                id: recentScreen
                objectName: "recentScreen"
                anchors.fill: parent
                visible: window.currentTab === 4
                library: window.library
                games: window.library.recentGames
                title: "Recently played"
                subtitle: "Most recent first"
                emptyTitle: "Nothing played yet"
                emptyText: "Games you launch show up here, most recent first."
                onNavigate: (tab) => window.navigate(tab)
                onOptionsRequested: (gameId) => window.openGameOptions(gameId)
                onDetailsRequested: (gameId) => window.openGameDetails(gameId)
            }
            EmulatorsScreen {
                id: emulatorsScreen
                anchors.fill: parent
                visible: window.currentTab === 5
                app: window.app
                settings: window.settings
            }
            SettingsScreen {
                id: settingsScreen
                anchors.fill: parent
                visible: window.currentTab === 6
                app: window.app
                library: window.library
                settings: window.settings
                system: window.system
                updates: window.updates
                onOpenUpdatePanel: window.openUpdatePanel()
                onNotice: (message, isError) => window.notifications.postMessage("system", message, isError)
            }
            DiagnosticsScreen {
                id: systemScreen
                anchors.fill: parent
                visible: window.currentTab === 7
                app: window.app
                library: window.library
                settings: window.settings
                system: window.system
                updates: window.updates
                notifications: window.notifications
                onUpdatesRequested: window.openUpdatePanel()
            }

            // Above the pages (modal), below the dialogs and overlays.
            GameDetailsScreen {
                id: details
                anchors.fill: parent
                app: window.app
                library: window.library
                onOptionsRequested: (gameId) => window.openGameOptions(gameId)
            }
        }

        // Keys the focused control did not handle bubble up to here.
        Keys.onPressed: (event) => {
            const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
            const shift = (event.modifiers & Qt.ShiftModifier) !== 0
            switch (event.key) {
            case Qt.Key_PageUp:
                window.switchTab(window.currentTab - 1)
                break
            case Qt.Key_PageDown:
                window.switchTab(window.currentTab + 1)
                break
            case Qt.Key_Tab:
            case Qt.Key_Backtab:
                if (!ctrl)
                    return
                window.switchTab(window.currentTab + (shift || event.key === Qt.Key_Backtab ? -1 : 1))
                break
            case Qt.Key_Menu:
            case Qt.Key_F1:
                window.openTab("system")
                break
            case Qt.Key_Search:
                window.focusSearch()
                break
            case Qt.Key_F:
                if (ctrl)
                    window.focusSearch()
                break
            case Qt.Key_Slash:
                window.focusSearch()
                break
            case Qt.Key_Escape:
            case Qt.Key_Back:
            case Qt.Key_Backspace:
                // A console UI never quits on Back. Inside a page (nothing there consumed
                // it: no dialog, details or field) it moves to the tab bar; from the tab bar
                // it re-enters the tab.
                if (tabBar.activeFocus)
                    window.focusPage()
                else
                    tabBar.forceActiveFocus()
                break
            case Qt.Key_Up:
                tabBar.forceActiveFocus()
                break
            default:
                return
            }
            event.accepted = true
        }
    }

    Timer {
        id: startupQuiet
        interval: 3000
        running: true
    }
    Toast {
        id: toast
        service: window.notifications
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing * 1.5
        z: 20
    }
    Connections {
        target: window.library
        function onNotice(message, isError) {
            // The automatic scan at start-up is not worth a notice unless it failed.
            if (isError || !startupQuiet.running)
                window.notifications.postMessage("library", message, isError)
        }
    }
    // Launch errors (app.statusMessage) reach the notification service in C++ (UiContext).

    GameOptionsDialog {
        id: gameOptions
        anchors.fill: parent
        library: window.library
        sessions: window.sessions
    }

    UpdatePanel {
        id: updatePanel
        updates: window.updates
        system: window.system
    }

    // First-start tour; above the pages, below the running-emulator overlay.
    TutorialOverlay {
        id: tutorial
        anchors.fill: parent
        z: 9
        library: window.library
        visible: !window.settings.tutorialCompleted
        onVisibleChanged: {
            if (visible)
                tutorial.restart()
            else
                Qt.callLater(window.restoreFocus)
        }
        // Hiding restores the focus of the current tab (see onVisibleChanged).
        onDone: (openSettings) => {
            window.settings.tutorialCompleted = true
            if (openSettings)
                window.navigate("settings")
        }
    }

    RunningOverlay {
        id: overlay
        anchors.fill: parent
        z: 10
        app: window.app
        visible: window.app.busy
        onQuickMenuRequested: window.gameBar.openQuickMenu()
        onVisibleChanged: {
            if (visible) {
                const item = window.activeFocusItem
                if (item && (window.pageIndexOf(item) >= 0 || window.inLayer(item, details)))
                    window.focusBeforeOverlay = item
                overlay.forceActiveFocus()
            } else {
                Qt.callLater(window.restoreFocus)
            }
        }
    }

    GameBar {
        id: gameBarLayer
        // Above everything, header and hint bar included, like the Xbox Game Bar.
        parent: Overlay.overlay
        anchors.fill: parent
        gameBar: window.gameBar
        app: window.app
        library: window.library
        settings: window.settings
        system: window.system
        onNavigate: (tab) => window.navigate(tab)
    }
    Connections {
        target: window.gameBar
        function onOpenChanged() {
            if (window.gameBar.open) {
                const item = window.activeFocusItem
                window.focusBeforeGameBar = item && item !== window.contentItem && !window.inGameBar(item) ? item : null
                // On top of a running emulator: bring Nexora to the front so it gets input.
                if (window.app.busy && !window.active) {
                    window.raisedForGameBar = true
                    window.raise()
                    window.requestActivate()
                }
                gameBarLayer.reset()
            } else {
                const saved = window.focusBeforeGameBar
                window.focusBeforeGameBar = null
                // A game started from the bar: the running overlay takes focus, not the page.
                if (window.app.busy && saved !== null && window.pageIndexOf(saved) >= 0)
                    window.restoreFocus()
                else if (saved && saved.visible)
                    saved.forceActiveFocus()
                else
                    window.restoreFocus()
                if (window.raisedForGameBar) {
                    window.raisedForGameBar = false
                    if (window.app.busy)
                        window.lower() // back to the game
                }
            }
        }
    }
    Shortcut {
        sequence: "Ctrl+G"
        onActivated: window.gameBar.toggle()
    }

    QuickMenu {
        id: quickMenu
        parent: Overlay.overlay
        anchors.fill: parent
        gameBar: window.gameBar
        app: window.app
        settings: window.settings
        system: window.system
        sessions: window.sessions
    }
    Connections {
        target: window.gameBar
        function onQuickMenuOpenChanged() {
            if (window.gameBar.quickMenuOpen) {
                const item = window.activeFocusItem
                window.focusBeforeQuickMenu = item && item !== window.contentItem && !window.inLayer(item, quickMenu)
                                              ? item : null
                // Over a running emulator: bring Nexora to the front so it gets input.
                if (window.app.busy && !window.active) {
                    window.raisedForQuickMenu = true
                    window.raise()
                    window.requestActivate()
                }
            } else {
                const saved = window.focusBeforeQuickMenu
                window.focusBeforeQuickMenu = null
                if (window.gameBar.open)
                    gameBarLayer.focusDefault()
                else if (saved && saved.visible)
                    saved.forceActiveFocus()
                else
                    window.restoreFocus()
                if (window.raisedForQuickMenu) {
                    window.raisedForQuickMenu = false
                    if (window.app.busy && !window.gameBar.open)
                        window.lower() // back to the game
                }
            }
        }
    }

    // A Quick Resume state that cannot be resumed (or did not load): the state is kept.
    ConfirmDialog {
        id: resumeFailedDialog
        objectName: "resumeFailedDialog"
        property string sessionId
        z: 60
        confirmText: "Start fresh"
        cancelText: "Not now"
        onConfirmed: window.sessions.startFresh(sessionId)
    }
    Connections {
        target: window.sessions
        function onResumeFailed(sessionId, title, reason) {
            resumeFailedDialog.sessionId = sessionId
            resumeFailedDialog.title = "Can't resume " + title
            resumeFailedDialog.message = reason + ".\n\nThe saved state is kept. Start fresh begins the game "
                + "without it; you can remove the state from Home later."
            resumeFailedDialog.open()
        }
    }

    // Development convenience; the console build will not expose quitting.
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Component.onCompleted: {
        // Every screen reads these back off Theme; binding them once here keeps the
        // look and accessibility settings (theme, UI scale, reduced motion) wired to the
        // whole interface without each screen having to know about SettingsController.
        Theme.userScale = Qt.binding(() => window.settings.uiScale / 100)
        // The layout is designed for 1280x720; larger windows scale up (never down).
        Theme.resolutionScale = Qt.binding(() => Math.max(1, Math.min(window.width / 1280, window.height / 720)))
        Theme.highContrast = Qt.binding(() => window.settings.theme === "highContrast")
        Theme.glass = Qt.binding(() => window.settings.theme === "glass")
        Theme.reducedMotion = Qt.binding(() => window.settings.reducedMotion)
        // Settings › General › Start screen.
        if (window.settings.startScreen === "games")
            currentTab = tabIndex("games")
        focusPage()
        if (tutorial.visible)
            tutorial.restart()
    }
}
