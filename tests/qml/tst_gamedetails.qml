import QtQuick
import QtTest
import Nexora.Console

// The game details flow on the real library (two sandboxed games, see main.cpp): Accept on a
// grid tile opens the details, Back restores focus to the same tile, the remove dialog
// defaults to Cancel and says the file is not deleted, and Edit details works with the
// on-screen keyboard alone.
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

    GameGrid {
        id: grid
        anchors.fill: parent
        library: testLibrary
        model: testLibrary.games
        focus: true
        onDetailsRequested: (gameId) => details.open(gameId, grid)
    }
    GameDetailsScreen {
        id: details
        anchors.fill: parent
        app: testApp
        library: testLibrary
    }
    SignalSpy { id: closedSpy; target: details; signalName: "closed" }

    TestCase {
        name: "GameDetails"
        when: windowShown

        function init() {
            closedSpy.clear()
            if (details.visible)
                details.close()
            testLibrary.restoreRemovedGames()
            tryCompare(testLibrary.games, "count", 2)
            grid.currentIndex = 0
            grid.forceActiveFocus()
            tryVerify(function() { return grid.activeFocus })
        }
        function focusRow(name) {
            const row = root.findByName(details, name)
            verify(row !== null, name)
            for (let i = 0; i < 12 && !row.activeFocus; ++i)
                keyClick(Qt.Key_Down)
            verify(row.activeFocus, name + " not reachable with Down")
            return row
        }

        function test_acceptOpensDetailsAndBackRestoresTheSameTile() {
            keyClick(Qt.Key_Right)
            compare(grid.currentIndex, 1)
            const id = testLibrary.games.gameIdAt(1)
            keyClick(Qt.Key_Return)
            tryVerify(function() { return details.visible })
            compare(details.gameId, id)
            compare(root.findByName(details, "gameDetailsTitle").text, testLibrary.gameDetails(id).title)
            // Play is the default action.
            tryVerify(function() { return root.findByName(details, "detailsLaunch").activeFocus })

            // Tab switching keys stay inside (modal); Back closes.
            keyClick(Qt.Key_PageDown)
            verify(details.visible)
            keyClick(Qt.Key_Escape)
            verify(!details.visible)
            compare(closedSpy.count, 1)
            tryVerify(function() { return grid.activeFocus })
            compare(grid.currentIndex, 1)
            compare(testLibrary.games.gameIdAt(grid.currentIndex), id)
        }

        function test_favoriteToggleFromDetails() {
            keyClick(Qt.Key_Return)
            tryVerify(function() { return details.visible })
            const id = details.gameId
            const before = testLibrary.gameDetails(id).favorite
            focusRow("detailsFavorite")
            keyClick(Qt.Key_Return)
            compare(testLibrary.gameDetails(id).favorite, !before)
            keyClick(Qt.Key_Right)
            compare(testLibrary.gameDetails(id).favorite, before)
        }

        function test_removeDialogDefaultsToCancelAndSaysTheFileStays() {
            keyClick(Qt.Key_Return)
            tryVerify(function() { return details.visible })
            const id = details.gameId
            focusRow("detailsRemove")
            keyClick(Qt.Key_Return)
            const dialog = root.findByName(details, "confirmRemoveGame")
            tryVerify(function() { return dialog.visible })
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            verify(dialog.message.indexOf("not deleted") >= 0, dialog.message)

            // Back = Cancel: nothing removed, the details stay open on the Remove row.
            keyClick(Qt.Key_Escape)
            verify(!dialog.visible)
            verify(details.visible)
            compare(testLibrary.games.count, 2)
            tryVerify(function() { return root.findByName(details, "detailsRemove").activeFocus })

            // Confirm: the entry is hidden, the details close, focus is back on the grid.
            keyClick(Qt.Key_Return)
            tryVerify(function() { return dialog.cancelButton.activeFocus })
            keyClick(Qt.Key_Right)
            keyClick(Qt.Key_Return)
            verify(!details.visible)
            tryCompare(testLibrary.games, "count", 1)
            compare(testLibrary.games.indexOfGame(id), -1)
            tryVerify(function() { return grid.activeFocus })
            compare(testLibrary.restoreRemovedGames(), 1)
        }

        function test_editTitleWithTheOnScreenKeyboard() {
            keyClick(Qt.Key_Return)
            tryVerify(function() { return details.visible })
            const id = details.gameId
            const original = testLibrary.gameDetails(id).title
            focusRow("detailsEdit")
            keyClick(Qt.Key_Return)
            const editor = root.findByName(details, "metadataEditor")
            tryVerify(function() { return editor.visible })
            tryVerify(function() { return root.findByName(editor, "metadataTitle").activeFocus })
            keyClick(Qt.Key_Return) // opens the keyboard on the current title
            const osk = root.findByName(editor, "onScreenKeyboard")
            tryVerify(function() { return osk.visible })
            compare(osk.text, original)
            // Controller only: Down to the letter rows... the cursor starts on "q".
            keyClick(Qt.Key_Backspace)  // delete one character
            keyClick(Qt.Key_Return)     // types the focused key ("q")
            keyClick(Qt.Key_Down)       // "a"
            keyClick(Qt.Key_Return)
            compare(osk.text, original.slice(0, -1) + "qa")
            keyClick(Qt.Key_Menu)       // Start = done
            verify(!osk.visible)
            compare(editor.title, original.slice(0, -1) + "qa")
            tryVerify(function() { return root.findByName(editor, "metadataTitle").activeFocus })

            // Back in the keyboard cancels and keeps the previous text.
            keyClick(Qt.Key_Return)
            tryVerify(function() { return osk.visible })
            keyClick(Qt.Key_Backspace)
            keyClick(Qt.Key_Escape)
            verify(!osk.visible)
            compare(editor.title, original.slice(0, -1) + "qa")

            // Save stores it in the library (the file itself is untouched).
            const save = root.findByName(editor, "metadataSave")
            for (let i = 0; i < 6 && !save.activeFocus; ++i)
                keyClick(Qt.Key_Down)
            verify(save.activeFocus)
            keyClick(Qt.Key_Return)
            verify(!editor.visible)
            tryCompare(root.findByName(details, "gameDetailsTitle"), "text", original.slice(0, -1) + "qa")
            compare(testLibrary.gameDetails(id).title, original.slice(0, -1) + "qa")
            verify(testLibrary.setGameMetadata(id, "", ""))
            compare(testLibrary.gameDetails(id).title, original)
        }

        function test_unknownGameShowsErrorState() {
            details.open(-12345, grid)
            verify(details.visible)
            verify(root.findByName(details, "gameDetailsError").visible)
            tryVerify(function() { return root.findByName(details, "gameDetailsBack").activeFocus })
            keyClick(Qt.Key_Return)
            verify(!details.visible)
            tryVerify(function() { return grid.activeFocus })
        }
    }
}
