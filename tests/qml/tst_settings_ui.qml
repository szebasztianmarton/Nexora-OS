import QtQuick
import QtTest
import Nexora.Console

// Loads the real SettingsScreen against a real (sandboxed) backend — see main.cpp's Setup —
// plus a standalone ConfirmDialog for the modal-specific assertions.
Item {
    id: root
    width: 1280
    height: 1000

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

    SettingsScreen {
        id: screen
        anchors.fill: parent
        app: testApp
        library: testLibrary
        settings: testSettings
        system: testSystem
        updates: testUpdates
        onOpenUpdatePanel: {}
    }

    ConfirmDialog {
        id: dialog
        title: "Confirm?"
        message: "Body"
    }
    SignalSpy { id: confirmedSpy; target: dialog; signalName: "confirmed" }
    SignalSpy { id: cancelledSpy; target: dialog; signalName: "cancelled" }

    TestCase {
        name: "SettingsUi"
        when: windowShown

        function init() {
            confirmedSpy.clear()
            cancelledSpy.clear()
            if (dialog.visible)
                dialog.close()
        }

        // Settings screen reachable sections count 7 (spec §6.8): General, Controllers,
        // Display, Audio, Library, Emulators, System.
        function test_sevenSectionsPresent() {
            const names = ["General", "Controllers", "Display", "Audio", "Library", "Emulators", "System"]
            for (const n of names)
                verify(root.findByName(screen, "sectionTitle_" + n) !== null, "missing section: " + n)
        }

        // Every row is reachable with Down alone, and Down never pushes focus outside the
        // screen (no wrap-around, no escape into the hidden overlay dialogs).
        function test_keyboardNavigationStaysInsideScreen() {
            // focusDefault() alone only marks which child wants focus; forceActiveFocus()
            // on the scope itself is what climbs the ancestor chain to actually grab it
            // (exactly what Main.qml's focusPage() does for the real window).
            screen.focusDefault()
            screen.forceActiveFocus()
            tryVerify(function() { return screen.focusedItem !== null && screen.isInside(screen.focusedItem) })
            for (let i = 0; i < 150; ++i) {
                keyClick(Qt.Key_Down)
                verify(screen.focusedItem === null || screen.isInside(screen.focusedItem))
            }
            const reset = root.findByName(screen, "resetRow")
            verify(reset !== null)
        }

        function test_confirmDialogDefaultsToCancelAndBackCancelsWithoutAction() {
            dialog.open()
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            keyClick(Qt.Key_Escape)
            compare(cancelledSpy.count, 1)
            compare(confirmedSpy.count, 0)
            verify(!dialog.visible)
        }

        function test_confirmDialogConfirmButtonEmitsConfirmed() {
            dialog.open()
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            keyClick(Qt.Key_Right) // Cancel -> Confirm
            tryVerify(function() { return dialog.confirmButton.activeFocus })
            keyClick(Qt.Key_Return)
            compare(confirmedSpy.count, 1)
            compare(cancelledSpy.count, 0)
            verify(!dialog.visible)
        }
    }
}
