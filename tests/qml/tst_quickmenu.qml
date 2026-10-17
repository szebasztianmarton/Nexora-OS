import QtQuick
import QtTest
import Nexora.Console

// The in-game quick menu against a real running (fake) emulator: hidden and refusing to open
// without one, Pause / Resume for a pausable emulator, the Display sub-page, and Exit game
// asking for confirmation (default Cancel) before it stops anything.
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

    Rectangle {
        id: behind
        anchors.fill: parent
        focus: true
        activeFocusOnTab: true
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
    }
}
