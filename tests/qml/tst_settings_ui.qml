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
    SettingRow {
        id: longRow
        y: 900
        label: "Restore backup"
        value: "1 / 3 · nexora-backup-2026-10-17T12-00-00-with-a-very-long-directory-name-that-goes-on-and-on"
        adjustable: true
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

        // Theme is process-wide; leave the scale as found.
        function cleanup() {
            Theme.userScale = 1.0
            Theme.glass = false
            dialog.confirmText = "Confirm"
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
            // Every focus stop of the screen, in focus-chain order (visible, enabled rows only).
            const expected = []
            const first = screen.focusedItem
            for (let item = first, guard = 0; guard < 400; ++guard) {
                if (screen.isInside(item) && item.visible && item.enabled && item.activeFocusOnTab
                        && expected.indexOf(item) < 0)
                    expected.push(item)
                item = item.nextItemInFocusChain(true)
                if (item === first)
                    break
            }
            verify(expected.length > 20, "only " + expected.length + " focus stops found")
            const visited = [first]
            for (let i = 0; i < 150; ++i) {
                keyClick(Qt.Key_Down)
                verify(screen.focusedItem === null || screen.isInside(screen.focusedItem))
                if (screen.focusedItem && visited.indexOf(screen.focusedItem) < 0)
                    visited.push(screen.focusedItem)
            }
            for (const item of expected)
                verify(visited.indexOf(item) >= 0, "not reachable with Down: " + item.objectName + " " + item)
            // The walk ends on the true last row (Restart / Power off are hidden on desktop).
            compare(screen.focusedItem.objectName, "resetRow")
        }

        // A long value is elided instead of squeezing the label (at 100 % too).
        function test_longValueElidesAndKeepsTheLabelReadable() {
            longRow.width = 700
            const value = root.findByName(longRow, "value")
            verify(value !== null)
            verify(value.width <= longRow.width * 0.5 + 1, "value " + value.width + " of " + longRow.width)
            verify(value.truncated)
            compare(value.textFormat, Text.PlainText)
            const label = root.findByName(longRow, "label")
            verify(label !== null)
            // Row lays out on the next polish: wait for it.
            tryVerify(function() { return label.width >= longRow.width * 0.4 }, 1000, "label " + label.width)
            verify(!label.truncated)
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

        // At 150 % interface scale both buttons (each at least 140 px x scale) still fit inside
        // the panel, right-aligned within its content margins.
        function test_confirmDialogButtonsFitThePanelAtLargeScale() {
            Theme.userScale = 1.5
            dialog.open()
            waitForRendering(dialog) // positioners lay out on polish
            const panel = findByName(dialog, "confirmDialogPanel")
            verify(panel !== null)
            const margin = Theme.spacing * 1.5
            const confirmPos = dialog.confirmButton.mapToItem(panel, 0, 0)
            const cancelPos = dialog.cancelButton.mapToItem(panel, 0, 0)
            verify(confirmPos.x + dialog.confirmButton.width <= panel.width - margin + 0.5,
                   "confirm right edge " + (confirmPos.x + dialog.confirmButton.width) + " panel " + panel.width)
            verify(cancelPos.x >= margin - 0.5, "cancel x " + cancelPos.x)
            compare(Math.round(confirmPos.x + dialog.confirmButton.width), Math.round(panel.width - margin))
            verify(panel.width <= root.width)
        }

        // The panel outline is a plain 1 px border: the only focus ring in the dialog is the
        // focused button's. In glass the GlassSurface rim replaces the border.
        function test_confirmDialogPanelHasNoFocusRingBorder() {
            dialog.open()
            const panel = findByName(dialog, "confirmDialogPanel")
            compare(panel.border.width, 1)
            compare(panel.border.color, Theme.border)
            verify(!Qt.colorEqual(panel.border.color, Theme.focusRing))
            Theme.glass = true
            verify(Theme.glassActive)
            compare(panel.border.width, 0)
            verify(!Qt.colorEqual(panel.border.color, Theme.focusRing))
        }

        // When the buttons wrap onto two lines (Cancel below Confirm), Up/Down move between
        // them; side by side, Up/Down stay where they are.
        function test_confirmDialogWrappedButtonsNavigateVertically() {
            dialog.open()
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            keyClick(Qt.Key_Up)
            verify(dialog.cancelButton.activeFocus)
            dialog.close()
            dialog.confirmText = "Delete every save state and screenshot of this game"
            dialog.open()
            waitForRendering(dialog) // positioners lay out on polish
            tryVerify(function() { return dialog.cancelButton.y > dialog.confirmButton.y })
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            keyClick(Qt.Key_Up) // Cancel -> Confirm
            tryVerify(function() { return dialog.confirmButton.activeFocus })
            keyClick(Qt.Key_Down) // Confirm -> Cancel
            tryVerify(function() { return dialog.cancelButton.activeFocus })
        }
    }
}
