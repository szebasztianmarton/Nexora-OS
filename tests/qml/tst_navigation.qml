import QtQuick
import QtTest
import Nexora.Console

// Navigation building blocks with keys only: the eight-tab bar (fits at 1280 px, also at
// 150 % UI scale), NavColumn skipping hidden rows, and GameGrid's Accept / Favorite keys and
// grid/list modes. The whole window (all eight tabs through Main.qml) is driven by
// tests/integration/tst_qmlsmoke.cpp.
Item {
    id: root
    width: 1280
    height: 720

    readonly property var tabs: [{ key: "home", title: "Home" }, { key: "games", title: "Games" },
                                 { key: "systems", title: "Systems" }, { key: "favorites", title: "Favorites" },
                                 { key: "recent", title: "Recent" }, { key: "emulators", title: "Emulators" },
                                 { key: "settings", title: "Settings" }, { key: "system", title: "System" }]

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

    NavTabBar {
        id: tabBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        tabs: root.tabs
        currentIndex: 0
        onTabRequested: (index) => currentIndex = index
    }
    SignalSpy { id: enterSpy; target: tabBar; signalName: "enterRequested" }

    FocusScope {
        id: columnHost
        anchors.top: tabBar.bottom
        width: 400
        height: 300
        property int bubbledUp: 0
        Keys.onUpPressed: bubbledUp += 1
        NavColumn {
            id: column
            width: parent.width
            SettingRow { id: rowA; objectName: "rowA"; label: "A"; actionText: "Go" }
            Text { text: "Info text is not a focus stop" }
            SettingRow { id: rowB; objectName: "rowB"; label: "B"; actionText: "Go"; visible: false }
            SettingRow { id: rowC; objectName: "rowC"; label: "C"; actionText: "Go" }
        }
    }

    GameGrid {
        id: grid
        anchors.top: columnHost.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        library: testLibrary
        model: testLibrary.games
    }
    SignalSpy { id: detailsSpy; target: grid; signalName: "detailsRequested" }

    TestCase {
        name: "Navigation"
        when: windowShown

        function cleanup() {
            Theme.userScale = 1.0
        }

        function tabsWidth() {
            let total = 0
            for (const t of root.tabs)
                total += root.findByName(tabBar, "tab_" + t.key).width
            return total
        }

        function test_eightTabsWithKeysOnly() {
            tabBar.forceActiveFocus()
            for (let i = 1; i < root.tabs.length; ++i) {
                keyClick(Qt.Key_Right)
                compare(tabBar.currentIndex, i)
            }
            keyClick(Qt.Key_Right) // last tab: stays
            compare(tabBar.currentIndex, root.tabs.length - 1)
            for (let i = root.tabs.length - 2; i >= 0; --i) {
                keyClick(Qt.Key_Left)
                compare(tabBar.currentIndex, i)
            }
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Return)
            compare(enterSpy.count, 2)
        }

        function test_tabsFitAtTvResolutionAndLargeScale() {
            verify(tabsWidth() <= tabBar.width, "tabs " + tabsWidth() + " > " + tabBar.width)
            Theme.userScale = 1.5
            verify(tabsWidth() <= tabBar.width, "tabs at 150% " + tabsWidth() + " > " + tabBar.width)
        }

        function test_navColumnSkipsHiddenAndInfoRows() {
            columnHost.bubbledUp = 0
            verify(column.focusFirst())
            tryVerify(function() { return rowA.activeFocus })
            keyClick(Qt.Key_Down)
            verify(rowC.activeFocus) // B is hidden, the Text is not a stop
            keyClick(Qt.Key_Down) // last: stays, not accepted
            verify(rowC.activeFocus)
            keyClick(Qt.Key_Up)
            verify(rowA.activeFocus)
            keyClick(Qt.Key_Up) // first: bubbles to the container
            compare(columnHost.bubbledUp, 1)
            rowB.visible = true
            keyClick(Qt.Key_Down)
            verify(rowB.activeFocus)
            rowB.visible = false
            tryVerify(function() { return column.focusCurrent() && (rowA.activeFocus || rowC.activeFocus) })
        }

        function test_gameGridAcceptFavoriteAndListMode() {
            tryCompare(grid, "count", 2)
            grid.forceActiveFocus()
            grid.currentIndex = 0
            detailsSpy.clear()
            keyClick(Qt.Key_Right)
            compare(grid.currentIndex, 1)
            keyClick(Qt.Key_Return)
            compare(detailsSpy.count, 1)
            compare(detailsSpy.signalArguments[0][0], testLibrary.games.gameIdAt(1))

            const id = testLibrary.games.gameIdAt(1)
            const favorite = testLibrary.gameDetails(id).favorite
            keyClick(Qt.Key_F)
            compare(testLibrary.gameDetails(id).favorite, !favorite)
            keyClick(Qt.Key_F)
            compare(testLibrary.gameDetails(id).favorite, favorite)

            const gridCell = grid.cellHeight
            grid.listMode = true
            compare(grid.columns, 1)
            verify(grid.cellHeight < gridCell)
            keyClick(Qt.Key_Up) // one column: Up moves to the first game
            compare(grid.currentIndex, 0)
            grid.listMode = false
        }
    }
}
