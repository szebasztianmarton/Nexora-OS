import QtQuick
import QtTest
import Nexora.Console

// Layout of the Emulators tab (cards, launch-profile picker), the game options dialog and the
// System tab's focus frame on the real sandboxed backend (see main.cpp): everything stays
// inside its panel at a large interface scale and the focus ring stays on screen.
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

    EmulatorsScreen {
        id: emulators
        anchors.fill: parent
        app: testApp
        settings: testSettings
        visible: false
    }
    GameOptionsDialog {
        id: options
        anchors.fill: parent
        library: testLibrary
    }
    DiagnosticsScreen {
        id: system
        width: parent.width
        height: 400
        app: testApp
        library: testLibrary
        settings: testSettings
        system: testSystem
        updates: testUpdates
        visible: false
    }
    EmulatorCard {
        id: card
        index: 0
        emulatorId: "fake"
        name: "Fake Emulator"
        status: "unavailable"
        statusText: "Unavailable"
        detail: "The executable /opt/emulators/fake/bin/fake-emulator was not found on this system, "
                + "install it or fix the path in the emulator configuration file"
        available: false
        profiles: [{ id: "a", name: "A", enabled: true }, { id: "b", name: "B", enabled: true }]
        extensions: ".rom, .bin, .iso, .cue, .chd, .zip, .7z"
        visible: false
    }

    TestCase {
        name: "EmulatorsUi"
        when: windowShown

        function cleanup() {
            if (options.visible)
                options.close()
            const picker = root.findByName(emulators, "profilePicker")
            if (picker.visible)
                picker.close()
            emulators.visible = false
            system.visible = false
            card.visible = false
            Theme.userScale = 1.0
            Theme.reducedMotion = false
            Theme.highContrast = false
        }

        function openOptionsWithRows(count) {
            options.open(testLibrary.games.gameIdAt(0), null)
            tryVerify(function() { return options.visible })
            const rows = []
            for (let i = 0; i < count; ++i)
                rows.push({ kind: "favorite", id: "", label: "Option " + i, detail: i % 2 ? "" : "Detail " + i })
            options.rows = rows
            return root.findByName(options, "gameOptionsList")
        }

        // The frame is the view's, not the scrolled content's: the whole ring stays visible.
        function test_systemFocusFrameMatchesTheView() {
            system.visible = true
            const flick = root.findByName(system, "systemDetails")
            const frame = root.findByName(system, "systemDetailsFocus")
            verify(frame !== null)
            flick.forceActiveFocus()
            tryVerify(function() { return frame.visible })
            verify(flick.contentHeight > flick.height, "content must be taller than the view")
            compare(frame.height, flick.height)
            compare(frame.width, flick.width)
            const topLeft = frame.mapToItem(flick, 0, 0)
            compare(topLeft.y, 0)
        }

        function test_gameOptionsListFitsThePanel() {
            Theme.userScale = 2.25
            const list = openOptionsWithRows(10)
            const panel = root.findByName(options, "gameOptionsPanel")
            tryVerify(function() { return list.contentHeight > list.height }, 1000, "10 rows scroll inside the list")
            verify(list.clip)
            tryVerify(function() { return list.mapToItem(panel, 0, list.height).y <= panel.height }, 1000,
                      "list bottom " + list.mapToItem(panel, 0, list.height).y + " > panel " + panel.height)
            verify(panel.height <= options.height)
        }

        function test_gameOptionsRowsGrowWithTheFont() {
            let list = openOptionsWithRows(4)
            // Scale 1.0 keeps the original row heights.
            compare(list.itemAtIndex(0).height, 56)
            compare(list.itemAtIndex(1).height, 44)
            Theme.userScale = 2.25
            for (let i = 0; i < 2; ++i) {
                const row = list.itemAtIndex(i)
                const content = root.findByName(row, "gameOptionContent")
                verify(row.height >= content.implicitHeight, row.height + " < " + content.implicitHeight)
            }
        }

        function test_gameOptionsFocusRing() {
            Theme.highContrast = true
            const list = openOptionsWithRows(2)
            tryVerify(function() { return list.activeFocus })
            compare(list.itemAtIndex(0).border.color, Theme.focusRing)
        }
    }
}
