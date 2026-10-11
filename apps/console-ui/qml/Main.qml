import QtQuick
import QtQuick.Controls
import Nexora.Console

// Full-screen dashboard: header, top-level tabs (Home, Games, Emulators, Settings,
// Diagnostics), the running-emulator overlay, the game bar and transient notices.
//
// Controllers arrive as key events (see InputController::keyForAction): arrows, Return
// (Accept), Escape/Back (Back), Menu/F1, Favorites, Search, PageUp/PageDown (L1/R1). The
// game bar button (Guide) arrives through GameBarController instead; Ctrl+G on a keyboard.
ApplicationWindow {
    id: window
    objectName: "mainWindow"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system
    required property GameBarController gameBar

    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Nexora OS"
    color: Theme.background

    readonly property var tabs: [{ key: "home", title: "Home" },
                                 { key: "games", title: "Games" },
                                 { key: "emulators", title: "Emulators" },
                                 { key: "settings", title: "Settings" },
                                 { key: "diagnostics", title: "Diagnostics" }]
    property int currentTab: 0
    readonly property string currentTabKey: tabs[currentTab].key
    readonly property var pages: [homeScreen, gamesScreen, emulatorsScreen, settingsScreen, diagnosticsScreen]

    // Last focused item per tab (restored when switching back) and before the overlay.
    property var tabFocus: [null, null, null, null, null]
    property Item focusBeforeOverlay: null
    property Item focusBeforeGameBar: null
    // The bar raised this window above a running emulator; lower it again on close.
    property bool raisedForGameBar: false

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
        openTab(key)
        if (key === "settings" && window.library.directories.length === 0)
            settingsScreen.focusAddFolder()
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
    function restoreFocus() {
        if (window.gameBar.open) {
            gameBarLayer.focusDefault()
            return
        }
        if (window.app.busy) {
            overlay.forceActiveFocus()
            return
        }
        const saved = focusBeforeOverlay
        focusBeforeOverlay = null
        if (saved && saved.visible)
            saved.forceActiveFocus()
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
        hints: window.app.busy
               ? [{ button: "A / Enter", action: "Stop emulator" },
                  { button: "Select + Start (hold)", action: "Return to Nexora" }]
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
                onNavigate: (tab) => window.navigate(tab)
            }
            GamesScreen {
                id: gamesScreen
                anchors.fill: parent
                visible: window.currentTab === 1
                app: window.app
                library: window.library
                onNavigate: (tab) => window.navigate(tab)
            }
            EmulatorsScreen {
                id: emulatorsScreen
                anchors.fill: parent
                visible: window.currentTab === 2
                app: window.app
            }
            SettingsScreen {
                id: settingsScreen
                anchors.fill: parent
                visible: window.currentTab === 3
                app: window.app
                library: window.library
                settings: window.settings
                system: window.system
            }
            DiagnosticsScreen {
                id: diagnosticsScreen
                anchors.fill: parent
                visible: window.currentTab === 4
                app: window.app
                library: window.library
                settings: window.settings
                system: window.system
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
                window.openTab("diagnostics")
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
                // A console UI never quits on Back; from the tab bar it re-enters the tab.
                if (tabBar.activeFocus)
                    window.focusPage()
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
                toast.show(message, isError)
        }
    }
    Connections {
        target: window.app
        function onStatusMessageChanged() {
            if (window.app.statusIsError)
                toast.show(window.app.statusMessage, true)
        }
    }

    RunningOverlay {
        id: overlay
        anchors.fill: parent
        z: 10
        app: window.app
        visible: window.app.busy
        onVisibleChanged: {
            if (visible) {
                const item = window.activeFocusItem
                if (item && window.pageIndexOf(item) >= 0)
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

    // Development convenience; the console build will not expose quitting.
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Component.onCompleted: focusPage()
}
