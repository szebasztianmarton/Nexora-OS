import QtQuick
import QtQuick.Controls
import Nexora.Console

// Games: search, filter chips, sort selector and the grid of game cards.
// Focus rows (top to bottom): [search, filter chips] → sort → grid.
FocusScope {
    id: screen
    objectName: "gamesScreen"
    required property AppController app
    required property LibraryController library

    signal navigate(string tab)
    signal optionsRequested(real gameId)

    readonly property alias grid: grid
    readonly property real currentGameId: grid.currentGameId
    readonly property bool searchActive: searchField.activeFocus
    readonly property var sortOrders: ["title", "recent", "emulator", "added"]
    readonly property var hints: searchField.activeFocus
        ? [{ button: "Type", action: "Search" }, { button: "Enter / ↓", action: "Results" },
           { button: "B / Esc", action: "Leave search" }]
        : sortSelector.activeFocus
          ? [{ button: "← →", action: "Change sort" }, { button: "↓", action: "Games" },
             { button: "↑", action: "Search & filters" }, { button: "L1 / R1", action: "Switch tab" }]
          : emptyAction.activeFocus
            ? [{ button: "A / Enter", action: emptyAction.text }, { button: "↑", action: "Search & sort" },
               { button: "L1 / R1", action: "Switch tab" }]
          : chips.activeFocus
            ? [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Apply filter" },
               { button: "↓", action: "Sort & games" }, { button: "L1 / R1", action: "Switch tab" }]
            : [{ button: "↑ ↓ ← →", action: "Move" }, { button: "A / Enter", action: "Play" },
               { button: "Y / F", action: "Favorite" }, { button: "Select / O", action: "Options" },
               { button: "X / “/”", action: "Search" },
               { button: "L1 / R1", action: "Switch tab" }]

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }
    function focusResults() {
        if (grid.count > 0)
            grid.forceActiveFocus()
        else if (emptyAction.visible)
            emptyAction.forceActiveFocus()
        else
            sortSelector.forceActiveFocus()
    }
    function focusDefault() {
        if (grid.count > 0)
            grid.focus = true
        else if (emptyAction.visible)
            emptyAction.focus = true
        else
            searchField.focus = true
    }
    function cycleSort(step) {
        const i = Math.max(0, sortOrders.indexOf(library.sortOrder))
        library.sortOrder = sortOrders[(i + step + sortOrders.length) % sortOrders.length]
    }

    Component.onCompleted: focusDefault()

    Keys.onUpPressed: (event) => {
        if (grid.activeFocus || emptyAction.activeFocus)
            sortSelector.forceActiveFocus()
        else if (sortSelector.activeFocus)
            (chips.lastFocused >= 0 ? chips : searchField).forceActiveFocus()
        else
            event.accepted = false // top row: let the tab bar take it
    }
    Keys.onDownPressed: (event) => {
        if (searchField.activeFocus || chips.activeFocus)
            sortSelector.forceActiveFocus()
        else if (sortSelector.activeFocus)
            focusResults()
    }

    // --- Row 1: search + filters --------------------------------------------------------
    Item {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.spacing * 1.25
        anchors.leftMargin: Theme.spacing * 2
        anchors.rightMargin: Theme.spacing * 2
        height: 48

        TextField {
            id: searchField
            objectName: "searchField"
            anchors.left: parent.left
            anchors.right: chips.left
            anchors.rightMargin: Theme.spacing
            height: parent.height
            text: screen.library.searchText
            onTextEdited: screen.library.searchText = text
            placeholderText: "Search games  (X / “/”)"
            placeholderTextColor: Theme.textFaint
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.text
            font.pixelSize: Theme.fontBody
            leftPadding: 44
            rightPadding: 16
            verticalAlignment: TextInput.AlignVCenter
            background: Rectangle {
                radius: 24
                color: searchField.activeFocus ? Theme.surfaceHover : Theme.surfaceRaised
                border.width: searchField.activeFocus ? Theme.focusWidth : 1
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⌕"
                    color: searchField.activeFocus ? Theme.accent : Theme.textMuted
                    font.pixelSize: 22 * Theme.scale
                }
            }

            Keys.onEscapePressed: screen.focusResults()
            Keys.onBackPressed: screen.focusResults()
            Keys.onReturnPressed: screen.focusResults()
            Keys.onEnterPressed: screen.focusResults()
            Keys.onRightPressed: (event) => {
                if (cursorPosition === length && selectedText.length === 0)
                    chips.focusChip(chips.lastFocused >= 0 ? chips.lastFocused : 0)
                else
                    event.accepted = false
            }
        }

        FocusScope {
            id: chips
            objectName: "filterChips"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: chipRow.width
            height: chipRow.height
            property int lastFocused: -1
            readonly property var filters: [{ key: "all", title: "All" },
                                            { key: "favorites", title: "★ Favorites" },
                                            { key: "recent", title: "Recent" }]
            function focusChip(i) { chipRepeater.itemAt(i).forceActiveFocus() }

            Row {
                id: chipRow
                spacing: 8
                Repeater {
                    id: chipRepeater
                    model: chips.filters
                    delegate: Rectangle {
                        id: chip
                        required property var modelData
                        required property int index
                        readonly property bool selected: screen.library.filter === modelData.key
                        objectName: "filter_" + modelData.key
                        activeFocusOnTab: true
                        height: 44
                        width: chipLabel.implicitWidth + 36
                        radius: 22
                        color: selected ? Theme.accentSoft : (chipMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
                        border.width: activeFocus ? Theme.focusWidth : 1
                        border.color: activeFocus ? Theme.accent : (selected ? Theme.accent : Theme.border)
                        onActiveFocusChanged: if (activeFocus) chips.lastFocused = index

                        Accessible.role: Accessible.RadioButton
                        Accessible.name: modelData.title
                        Accessible.description: "Filter"
                        Accessible.checkable: true
                        Accessible.checked: selected
                        Accessible.onPressAction: apply()
                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: chip.modelData.title
                            color: chip.selected ? Theme.text : Theme.textMuted
                            font.pixelSize: Theme.fontSmall + 1
                            font.bold: chip.selected
                        }
                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { chip.forceActiveFocus(); screen.library.filter = chip.modelData.key }
                        }
                        function apply() { screen.library.filter = modelData.key }
                        Keys.onReturnPressed: apply()
                        Keys.onEnterPressed: apply()
                        Keys.onSpacePressed: apply()
                        Keys.onLeftPressed: index > 0 ? chips.focusChip(index - 1) : searchField.forceActiveFocus()
                        Keys.onRightPressed: if (index < chips.filters.length - 1) chips.focusChip(index + 1)
                    }
                }
            }
        }
    }

    // --- Row 2: result count + sort ------------------------------------------------------
    Item {
        id: infoRow
        anchors.top: toolbar.bottom
        anchors.topMargin: 10
        anchors.left: toolbar.left
        anchors.right: toolbar.right
        height: 40

        Text {
            objectName: "resultCount"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: grid.count === screen.library.totalCount
                  ? grid.count + (grid.count === 1 ? " game" : " games")
                  : grid.count + " of " + screen.library.totalCount + " games"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall + 1
        }

        Text {
            objectName: "selectedDetail"
            anchors.left: parent.left
            anchors.leftMargin: 160
            anchors.right: sortSelector.left
            anchors.rightMargin: Theme.spacing
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            readonly property var game: grid.activeFocus ? grid.currentItem : null
            text: game && !game.available ? "⚠  " + game.title + ": " + game.unavailableReason : ""
            color: Theme.danger
            font.pixelSize: Theme.fontSmall
        }

        Rectangle {
            id: sortSelector
            objectName: "sortSelector"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            activeFocusOnTab: true
            height: 38
            width: sortRow.implicitWidth + 28
            radius: 19
            color: activeFocus ? Theme.surfaceHover : "transparent"
            border.width: activeFocus ? Theme.focusWidth : 1
            border.color: activeFocus ? Theme.accent : Theme.border

            Accessible.role: Accessible.ComboBox
            Accessible.name: "Sort by " + Theme.sortLabel(screen.library.sortOrder)
            Accessible.description: "Change sort order"
            Accessible.onPressAction: screen.cycleSort(1)

            Row {
                id: sortRow
                anchors.centerIn: parent
                spacing: 8
                Text {
                    text: "Sort"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "‹"
                    color: sortSelector.activeFocus ? Theme.accent : Theme.textFaint
                    font.pixelSize: Theme.fontLarge
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    objectName: "sortValue"
                    width: 130
                    horizontalAlignment: Text.AlignHCenter
                    text: Theme.sortLabel(screen.library.sortOrder)
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall + 1
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "›"
                    color: sortSelector.activeFocus ? Theme.accent : Theme.textFaint
                    font.pixelSize: Theme.fontLarge
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    sortSelector.forceActiveFocus()
                    screen.cycleSort(mouse.button === Qt.RightButton ? -1 : 1)
                }
            }
            Keys.onLeftPressed: screen.cycleSort(-1)
            Keys.onRightPressed: screen.cycleSort(1)
            Keys.onReturnPressed: screen.cycleSort(1)
            Keys.onEnterPressed: screen.cycleSort(1)
        }
    }

    // --- Grid ----------------------------------------------------------------------------
    GridView {
        id: grid
        objectName: "gameGrid"
        anchors.top: infoRow.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: summary.top
        anchors.leftMargin: Theme.spacing * 2 - 8
        anchors.rightMargin: Theme.spacing * 2 - 8
        visible: count > 0
        clip: true
        model: screen.library.games
        keyNavigationEnabled: true
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 120
        highlightRangeMode: GridView.ApplyRange
        preferredHighlightBegin: 12
        preferredHighlightEnd: height - cellHeight - 12
        topMargin: 12
        bottomMargin: 12

        readonly property int columns: Math.max(2, Math.floor(width / 240))
        cellWidth: Math.floor(width / columns)
        cellHeight: Math.round(cellWidth * 0.62) + 70 + 16

        property real currentGameId: -1
        onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex < count) currentGameId = model.gameIdAt(currentIndex)
        function restoreSelection() {
            if (currentGameId < 0)
                return
            const i = model.indexOfGame(currentGameId)
            if (i >= 0 && i !== currentIndex)
                currentIndex = i
            else if (i < 0 && currentIndex >= 0 && currentIndex < count)
                currentGameId = model.gameIdAt(currentIndex) // the game left the list
        }
        function syncSelection() {
            restoreSelection()
            if (count > 0 && currentIndex < 0)
                currentIndex = 0
        }
        // Deferred: after a model reset the view picks its own current index first.
        onCountChanged: Qt.callLater(syncSelection)

        delegate: Item {
            id: cell
            required property int index
            required property real gameId
            required property string title
            required property string emulatorName
            required property bool favorite
            required property string lastPlayed
            required property bool available
            required property string unavailableReason
            width: grid.cellWidth
            height: grid.cellHeight
            z: GridView.isCurrentItem ? 2 : 1

            GameTile {
                anchors.fill: parent
                anchors.margins: 8
                gameId: cell.gameId
                title: cell.title
                emulatorName: cell.emulatorName
                favorite: cell.favorite
                lastPlayed: cell.lastPlayed
                available: cell.available
                unavailableReason: cell.unavailableReason
                selected: cell.GridView.isCurrentItem && grid.activeFocus
                onPressed: { grid.currentIndex = cell.index; grid.forceActiveFocus() }
                onActivated: screen.library.launch(cell.gameId)
                onFavoriteClicked: screen.library.toggleFavorite(cell.gameId)
            }
        }

        Keys.onPressed: (event) => {
            if (currentIndex < 0)
                return
            const id = model.gameIdAt(currentIndex)
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                screen.library.launch(id)
                event.accepted = true
            } else if (event.key === Qt.Key_Favorites
                       || (event.key === Qt.Key_F && event.modifiers === Qt.NoModifier)) {
                screen.library.toggleFavorite(id)
                event.accepted = true
            } else if (event.key === Qt.Key_Context1 || (event.key === Qt.Key_O && event.modifiers === Qt.NoModifier)) {
                screen.optionsRequested(id)
                event.accepted = true
            }
        }
    }

    Connections {
        target: screen.library
        function onContentChanged() { Qt.callLater(grid.syncSelection) }
    }

    // --- Loading / empty states ------------------------------------------------------------
    Column {
        id: loading
        objectName: "gamesLoading"
        anchors.centerIn: grid
        visible: screen.library.scanning && grid.count === 0
        spacing: Theme.spacing
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 56
            height: 56
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: Theme.border
                border.width: 5
            }
            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: Theme.accent
                x: parent.width / 2 - 7
                y: -4
            }
            RotationAnimation on rotation {
                // A continuous spin, not a discrete transition: reducedMotion stops it
                // outright rather than collapsing its duration to 0.
                running: loading.visible && !Theme.reducedMotion
                from: 0
                to: 360
                duration: 1100
                loops: Animation.Infinite
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Scanning your library…"
            color: Theme.text
            font.pixelSize: Theme.fontLarge
        }
    }

    Column {
        id: empty
        objectName: "gamesEmpty"
        anchors.centerIn: grid
        width: Math.min(parent.width - 96, 720)
        visible: !screen.library.scanning && grid.count === 0
        spacing: Theme.spacing
        readonly property string mode: screen.library.totalCount === 0
            ? (screen.library.directories.length === 0 ? "noDirectories" : "noGames")
            : (screen.library.searchText.length > 0 ? "noResults"
               : (screen.library.filter === "favorites" ? "noFavorites" : "noRecent"))

        Text {
            objectName: "gamesEmptyTitle"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: empty.mode === "noDirectories" ? "No library folders yet"
                : empty.mode === "noGames" ? "No games found"
                : empty.mode === "noResults" ? "No results for “" + screen.library.searchText + "”"
                : empty.mode === "noFavorites" ? "No favorites yet"
                : "Nothing played yet"
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: empty.mode === "noDirectories"
                  ? "Add the folder that holds your game files in Settings → Library folders."
                : empty.mode === "noGames"
                  ? "None of the files in your library folders match an emulator's file extensions."
                : empty.mode === "noResults"
                  ? "Try a shorter search or a different filter."
                : empty.mode === "noFavorites"
                  ? "Press Y / F on a game to mark it as a favorite."
                  : "Games you launch will appear here."
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }
        NxButton {
            id: emptyAction
            objectName: "gamesEmptyAction"
            anchors.horizontalCenter: parent.horizontalCenter
            kind: "primary"
            text: empty.mode === "noDirectories" ? "Open Settings"
                : empty.mode === "noGames" ? "Rescan library"
                : empty.mode === "noResults" ? "Clear search"
                : "Show all games"
            onClicked: {
                if (empty.mode === "noDirectories") {
                    screen.navigate("settings")
                } else if (empty.mode === "noGames") {
                    screen.library.refresh()
                } else {
                    if (empty.mode === "noResults")
                        screen.library.searchText = ""
                    screen.library.filter = "all"
                    Qt.callLater(screen.focusResults)
                }
            }
        }
    }

    Text {
        id: summary
        objectName: "scanSummary"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spacing * 2
        anchors.rightMargin: Theme.spacing * 2
        anchors.bottomMargin: 8
        text: screen.library.lastScanSummary
        color: Theme.textFaint
        font.pixelSize: Theme.fontTiny
        elide: Text.ElideRight
    }
}
