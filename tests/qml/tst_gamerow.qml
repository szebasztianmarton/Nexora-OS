import QtQuick
import QtTest
import Nexora.Console

// GameRow (the Home rows) passes every role GameTile shows.
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

    ListModel {
        id: rowModel
        function gameIdAt(i) { return i >= 0 && i < count ? get(i).gameId : -1 }
        function indexOfGame(id) {
            for (let i = 0; i < count; ++i) {
                if (get(i).gameId === id)
                    return i
            }
            return -1
        }
        ListElement {
            gameId: 1; title: "Gone Game"; emulatorName: "Fake"; favorite: false; lastPlayed: ""
            available: true; unavailableReason: ""; present: false; systemShortName: "NES"
        }
        ListElement {
            gameId: 2; title: "Here Game"; emulatorName: "Fake"; favorite: false; lastPlayed: ""
            available: true; unavailableReason: ""; present: true; systemShortName: "SNES"
        }
    }

    GameRow {
        id: gameRow
        objectName: "testRow"
        width: parent.width
        library: testLibrary
        model: rowModel
        title: "Row"
    }

    TestCase {
        name: "GameRow"
        when: windowShown

        function tileAt(index) {
            const list = root.findByName(gameRow, "testRowList")
            verify(list !== null)
            tryVerify(function() { return list.itemAtIndex(index) !== null })
            return list.itemAtIndex(index)
        }

        function test_missingGameShowsBadge() {
            const t = tileAt(0)
            compare(t.present, false)
            verify(root.findByName(t, "missingBadge").visible)
            verify(!root.findByName(t, "systemBadge").visible)
        }

        function test_systemBadgeShown() {
            const t = tileAt(1)
            compare(t.systemShortName, "SNES")
            verify(!root.findByName(t, "missingBadge").visible)
            verify(root.findByName(t, "systemBadge").visible)
        }
    }
}
