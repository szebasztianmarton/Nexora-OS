import QtQuick
import QtTest
import Nexora.Console

// SystemsScreen cards against the sandboxed library (two games, no recognised system).
// Runs in the shared sandbox: every change to the library is undone in cleanup().
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

    SystemsScreen {
        id: systems
        anchors.fill: parent
        app: testApp
        library: testLibrary
    }

    TestCase {
        name: "Systems"
        when: windowShown

        property real movedGame: -1

        function cleanup() {
            if (movedGame >= 0)
                testLibrary.setGameSystem(movedGame, "")
            movedGame = -1
        }

        // The spoken game count uses the same singular/plural wording as the card's text:
        // one game moved to NES leaves one game on each card.
        function test_cardDescriptionCountsGames() {
            movedGame = testLibrary.games.gameIdAt(0)
            verify(movedGame >= 0)
            verify(testLibrary.setGameSystem(movedGame, "nes"))
            tryVerify(function() { return systems.installed.length === 2 }, 5000)
            const grid = root.findByName(systems, "systemGrid")
            for (let i = 0; i < systems.installed.length; ++i) {
                const s = systems.installed[i]
                compare(s.gameCount, 1)
                tryVerify(function() { return grid.itemAtIndex(i) !== null })
                const card = root.findByName(grid.itemAtIndex(i),
                                             "systemCard_" + (s.systemId.length > 0 ? s.systemId : "other"))
                verify(card !== null)
                compare(card.Accessible.description, "1 game")
            }
        }
    }
}
