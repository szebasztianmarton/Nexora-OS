import QtQuick
import QtTest
import Nexora.Console

// The in-game quick menu against a real running (fake) emulator: hidden and refusing to open
// without one, Pause / Resume for a pausable emulator, the Display sub-page, and Exit game
// asking for confirmation (default Cancel) before it stops anything. Also the rest of the
// in-game chrome that needs no window: the running overlay's Stop button, the header's chips
// and clock, the game bar's scaled tiles and focus rings, and the empty state.
Item {
    id: root
    width: 1280
    height: 720

    function findByName(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        const kids = item.children
        if (!kids)
            return null
        for (let i = 0; i < kids.length; ++i) {
            const found = findByName(kids[i], name)
            if (found)
                return found
        }
        return null
    }
    // Like findByName, but also through non-visual children (timers).
    function findObject(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        const kids = item.data
        if (!kids)
            return null
        for (let i = 0; i < kids.length; ++i) {
            const found = findObject(kids[i], name)
            if (found)
                return found
        }
        return null
    }

    Rectangle {
        id: behind
        anchors.fill: parent
        focus: true
        activeFocusOnTab: true
    }
    Header {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        app: testApp
        library: testLibrary
    }
    EmptyState {
        id: emptyState
        visible: false
        app: testApp
    }
    RunningOverlay {
        id: running
        anchors.fill: parent
        visible: false
        app: testApp
    }
    GameBar {
        id: gameBarLayer
        anchors.fill: parent
        gameBar: testGameBar
        app: testApp
        library: testLibrary
        settings: testSettings
        system: testSystem
    }
    QuickMenu {
        id: menu
        anchors.fill: parent
        gameBar: testGameBar
        app: testApp
        settings: testSettings
        system: testSystem
    }

    TestCase {
        name: "QuickMenu"
        when: windowShown

        function cleanup() {
            testGameBar.closeQuickMenu()
            if (testGameBar.open)
                testGameBar.toggle()
            running.visible = false
            header.visible = true
            Theme.userScale = 1.0
            root.height = 720
            Theme.highContrast = false
            Theme.reducedMotion = false
            Theme.sessionActive = false
            behind.forceActiveFocus()
            if (testApp.busy) {
                testApp.stop()
                tryVerify(function() { return !testApp.busy }, 10000)
            }
        }
        function startGame() {
            verify(testLibrary.launch(testLibrary.games.gameIdAt(0)))
            tryCompare(testApp, "state", "running", 10000)
        }
        function focusRow(name) {
            const row = root.findByName(menu, name)
            verify(row !== null, name)
            for (let i = 0; i < 12 && !row.activeFocus; ++i)
                keyClick(Qt.Key_Down)
            verify(row.activeFocus, name + " not reachable with Down")
            return row
        }

        function test_hiddenWithoutARunningEmulator() {
            verify(!testApp.busy)
            verify(!testGameBar.openQuickMenu())
            verify(!testGameBar.quickMenuOpen)
            verify(!menu.visible)
        }

        function test_opensOverARunningGameAndReturns() {
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return menu.visible })
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            verify(root.findByName(menu, "quickPause").visible) // the fake emulator is pausable
            keyClick(Qt.Key_Return) // Return to game
            verify(!testGameBar.quickMenuOpen)
            verify(!menu.visible)
            compare(testApp.state, "running")
        }

        function test_pauseAndResume() {
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            focusRow("quickPause")
            keyClick(Qt.Key_Return)
            tryVerify(function() { return testApp.paused })
            keyClick(Qt.Key_Return)
            tryVerify(function() { return !testApp.paused })
        }

        // "Return to game" (Resume play) also resumes a paused emulator.
        function test_returnToGameResumesAPausedEmulator() {
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            focusRow("quickPause")
            keyClick(Qt.Key_Return)
            tryVerify(function() { return testApp.paused })
            keyClick(Qt.Key_Up)
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            keyClick(Qt.Key_Return)
            verify(!testGameBar.quickMenuOpen)
            tryVerify(function() { return !testApp.paused })
            compare(testApp.state, "running")
        }

        function test_volumeAndDisplayPage() {
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            focusRow("quickVolume")
            const volume = testSettings.volume
            keyClick(volume >= 5 ? Qt.Key_Left : Qt.Key_Right)
            compare(testSettings.volume, volume >= 5 ? volume - 5 : volume + 5)
            testSettings.volume = volume

            focusRow("quickDisplay")
            keyClick(Qt.Key_Return)
            compare(menu.page, "display")
            tryVerify(function() { return root.findByName(menu, "quickDisplayMode").activeFocus })
            keyClick(Qt.Key_Escape) // Back: to the main page, not out of the menu
            compare(menu.page, "main")
            verify(menu.visible)
            tryVerify(function() { return root.findByName(menu, "quickDisplay").activeFocus })

            focusRow("quickSystemInfo")
            keyClick(Qt.Key_Return)
            compare(menu.page, "info")
            tryVerify(function() { return root.findByName(menu, "quickInfoBack").activeFocus })
            keyClick(Qt.Key_Return)
            compare(menu.page, "main")

            keyClick(Qt.Key_Escape) // Back on the main page returns to the game
            verify(!testGameBar.quickMenuOpen)
            compare(testApp.state, "running")
        }

        function test_exitGameAsksForConfirmation() {
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            focusRow("quickExit")
            keyClick(Qt.Key_Return)
            const dialog = root.findByName(menu, "confirmExitGame")
            tryVerify(function() { return dialog.visible })
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            verify(dialog.message.indexOf("Unsaved progress") >= 0)

            // Cancel (Back): the game keeps running, the menu stays open.
            keyClick(Qt.Key_Escape)
            verify(!dialog.visible)
            compare(testApp.state, "running")
            verify(menu.visible)

            // Confirm: the emulator stops and the menu closes by itself.
            keyClick(Qt.Key_Return)
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            keyClick(Qt.Key_Right)
            keyClick(Qt.Key_Return)
            tryVerify(function() { return !testApp.busy }, 10000)
            verify(!testGameBar.quickMenuOpen)
            verify(!menu.visible)
        }

        // Exit game's confirmation lists its keys in the menu's own hint bar: the bar stays above
        // the dialog's scrim, and the dialog's panel ends above the bar.
        function test_hintBarAboveExitConfirmation() {
            verify(testSettings.showHints)
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            focusRow("quickExit")
            keyClick(Qt.Key_Return)
            const dialog = root.findByName(menu, "confirmExitGame")
            tryVerify(function() { return dialog.visible })
            const bar = root.findByName(menu, "quickMenuHints")
            verify(bar.visible)
            compare(bar.parent, dialog.parent)
            verify(bar.z > dialog.z, bar.z + " <= " + dialog.z)
            compare(bar.hints, dialog.hints)
            compare(dialog.chromeBottom, bar.height)
            keyClick(Qt.Key_Escape)
            verify(!dialog.visible)
        }

        // The menu sits above the dashboard's hint bar (its scrim dims it, its panel covers part
        // of it), so it draws its own bar inside its layer, like the game bar, and hands the
        // dashboard's bar nothing: no key is listed twice.
        function test_ownHintBarInsideTheMenu() {
            verify(testSettings.showHints)
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            const bar = root.findByName(menu, "quickMenuHints")
            verify(bar !== null)
            verify(bar.visible)
            verify(bar.hints.length > 0)
            compare(menu.hints.length, 0)
            const p = bar.mapToItem(menu, 0, 0)
            verify(p.x >= 0 && p.y >= 0 && p.x + bar.width <= menu.width && p.y + bar.height <= menu.height)
            const panel = root.findByName(menu, "quickMenuPanel")
            verify(panel.mapToItem(menu, 0, 0).y + panel.height <= p.y + 0.5) // the panel ends above it
        }

        // System info's Back row sits outside the page's NavColumn: it is scrolled into view
        // too (150 % on a short screen: the page is taller than the panel). The root is 480 px
        // high, not 720: at 720 px the whole page still fits and nothing needs scrolling.
        function test_infoBackScrolledIntoViewAtLargeScale() {
            Theme.userScale = 1.5
            root.height = 480
            startGame()
            verify(testGameBar.openQuickMenu())
            tryVerify(function() { return root.findByName(menu, "quickReturn").activeFocus })
            focusRow("quickSystemInfo")
            keyClick(Qt.Key_Return)
            const back = root.findByName(menu, "quickInfoBack")
            tryVerify(function() { return back.activeFocus })
            const flick = root.findByName(menu, "quickMenuFlick")
            verify(flick !== null)
            tryVerify(function() {
                const p = back.mapToItem(flick, 0, 0)
                return p.y >= 0 && p.y + back.height <= flick.height
            })
        }

        // Stop is a real button for assistive technology, and keeps its size on focus when
        // motion is reduced.
        function test_runningOverlayStopButton() {
            const stop = root.findByName(running, "stopButton")
            verify(stop !== null)
            compare(stop.Accessible.role, Accessible.Button)
            verify(stop.Accessible.name.indexOf("Stop") >= 0)
            Theme.reducedMotion = true
            running.visible = true
            running.forceActiveFocus()
            tryVerify(function() { return stop.activeFocus })
            compare(stop.scale, 1.0)
            // High contrast: the yellow focus ring, not the danger red.
            Theme.highContrast = true
            compare(stop.border.color, Theme.focusRing)
            compare(stop.border.width, Theme.focusWidth)
        }

        // While a game runs the recent-game tiles are disabled; a visible line says why.
        function test_gameBarRecentGamesExplainedWhileBusy() {
            startGame()
            testGameBar.toggle()
            tryVerify(function() { return gameBarLayer.visible })
            const hint = root.findByName(gameBarLayer, "homeBusyHint")
            verify(hint !== null)
            tryVerify(function() { return testLibrary.recentGames.count > 0 })
            verify(hint.visible)
            compare(hint.textFormat, Text.PlainText)
            verify(hint.text.indexOf("stopped") >= 0, hint.text)
        }

        // Header chips grow with the scale (32 px at 100 %) so their text never overflows; the
        // state reads "Running", not "running".
        function test_headerChipsScale() {
            const chip = root.findByName(header, "stateChip")
            compare(chip.height, 32)
            Theme.userScale = 2.25
            startGame()
            tryVerify(function() { return chip.visible })
            const label = root.findByName(chip, "chipLabel")
            verify(label !== null)
            verify(chip.height >= label.implicitHeight, chip.height + " < " + label.implicitHeight)
            verify(label.text.indexOf("Running") >= 0, label.text)
        }

        // The header clock only ticks while the header can be seen and no game owns the screen.
        function test_headerClockSleepsDuringASession() {
            const timer = root.findObject(header, "headerClockTimer")
            verify(timer !== null)
            verify(timer.running)
            Theme.sessionActive = true
            verify(!timer.running)
            Theme.sessionActive = false
            verify(timer.running)
            header.visible = false
            verify(!timer.running)
        }

        // The game bar's tiles scale with the interface, and its focus rings are Theme.focusWidth
        // (4 px in high contrast) with no scale jump under reduced motion.
        function test_gameBarScaledTilesAndFocusRings() {
            Theme.userScale = 1.5
            Theme.highContrast = true
            Theme.reducedMotion = true
            testGameBar.toggle()
            tryVerify(function() { return gameBarLayer.visible })
            gameBarLayer.reset()
            const tab = root.findByName(gameBarLayer, "barTab_home")
            tryVerify(function() { return tab.activeFocus })
            compare(tab.width, Math.round(84 * Theme.scale))
            compare(tab.border.width, Theme.focusWidth)
            compare(tab.scale, 1.0)
            keyClick(Qt.Key_Down)
            const tile = root.findByName(gameBarLayer, "gameBarHome")
            tryVerify(function() { return tile.activeFocus })
            compare(tile.width, Math.round(92 * Theme.scale))
            compare(tile.border.width, Theme.focusWidth)
            compare(tile.scale, 1.0)
            keyClick(Qt.Key_Up)
            keyClick(Qt.Key_Right)
            keyClick(Qt.Key_Right)
            keyClick(Qt.Key_Down)
            const volume = root.findByName(gameBarLayer, "gameBarVolume")
            tryVerify(function() { return volume.activeFocus })
            compare(volume.height, Math.round(76 * Theme.scale))
            compare(volume.border.width, Theme.focusWidth)
        }

        // The empty state is announced by its title, not as "Empty state".
        function test_emptyStateAccessibleName() {
            compare(emptyState.Accessible.name, testApp.configDirectoryFound ? "No emulators configured"
                                                                            : "Configuration directory not found")
        }
    }
}
