import QtQuick
import Nexora.Console

// The game grid shared by Games, Favorites and Recent: cover cards (grid) or compact rows
// (|listMode|). Arrow keys move, Accept opens the game's details, Y / F toggles the favorite,
// Select / O opens the launch options. Keeps the same game selected when the model is rebuilt.
GridView {
    id: grid
    required property LibraryController library
    property bool listMode: false

    signal detailsRequested(real gameId)
    signal optionsRequested(real gameId)

    clip: true
    keyNavigationEnabled: true
    boundsBehavior: Flickable.StopAtBounds
    highlightMoveDuration: Theme.animMove
    highlightRangeMode: GridView.ApplyRange
    preferredHighlightBegin: Theme.space3
    preferredHighlightEnd: height - cellHeight - Theme.space3
    topMargin: Theme.space3
    bottomMargin: Theme.space3

    readonly property int columns: listMode ? 1 : Math.max(2, Math.floor(width / (240 * Theme.scale)))
    cellWidth: Math.floor(width / columns)
    cellHeight: listMode ? Math.round(76 * Theme.scale)
                         : Math.round(cellWidth * 0.62) + Math.round(70 * Theme.scale) + Theme.space4

    property real currentGameId: -1
    onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex < count) currentGameId = model.gameIdAt(currentIndex)
    function restoreSelection() {
        if (currentGameId < 0 || !model)
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
    onListModeChanged: Qt.callLater(() => positionViewAtIndex(currentIndex, GridView.Contain))

    Connections {
        target: grid.library
        function onContentChanged() { Qt.callLater(grid.syncSelection) }
    }

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
        required property bool present
        required property string systemShortName
        readonly property bool selected: GridView.isCurrentItem && grid.activeFocus
        width: grid.cellWidth
        height: grid.cellHeight
        z: GridView.isCurrentItem ? 2 : 1

        GameTile {
            visible: !grid.listMode
            anchors.fill: parent
            anchors.margins: Theme.space2
            gameId: cell.gameId
            title: cell.title
            emulatorName: cell.emulatorName
            favorite: cell.favorite
            lastPlayed: cell.lastPlayed
            available: cell.available
            present: cell.present
            systemShortName: cell.systemShortName
            unavailableReason: cell.unavailableReason
            selected: cell.selected
            onPressed: { grid.currentIndex = cell.index; grid.forceActiveFocus() }
            onActivated: grid.detailsRequested(cell.gameId)
            onFavoriteClicked: grid.library.toggleFavorite(cell.gameId)
        }

        // List mode: one compact row per game.
        Rectangle {
            objectName: "gameListRow"
            visible: grid.listMode
            anchors.fill: parent
            anchors.margins: Theme.space1
            radius: Theme.radiusSmall
            color: cell.selected ? Theme.surfaceHover : Theme.surfaceRaised
            border.width: cell.selected ? Theme.focusWidth : 1
            border.color: cell.selected ? Theme.focusRing : Theme.border

            Accessible.role: Accessible.Button
            Accessible.name: cell.title

            Rectangle {
                id: thumb
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height - Theme.space3
                width: height * 1.4
                radius: Theme.space1 + 2
                opacity: cell.available && cell.present ? 1.0 : 0.45
                color: Theme.artColor(cell.title, 0.28)
                Text {
                    anchors.centerIn: parent
                    text: Theme.initials(cell.title)
                    color: Theme.artInk
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                }
            }
            Column {
                anchors.left: thumb.right
                anchors.leftMargin: Theme.space3
                anchors.right: badges.left
                anchors.rightMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    width: parent.width
                    text: cell.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: (cell.systemShortName.length > 0 ? cell.systemShortName + " · " : "")
                          + (!cell.present ? "File missing"
                             : cell.available ? (cell.emulatorName.length > 0 ? cell.emulatorName : "Unknown emulator")
                             : cell.unavailableReason)
                          + (cell.lastPlayed.length > 0 ? " · Played " + cell.lastPlayed : "")
                    color: !cell.present ? Theme.warning : cell.available ? Theme.textMuted : Theme.danger
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
            }
            Text {
                id: badges
                anchors.right: parent.right
                anchors.rightMargin: Theme.space4
                anchors.verticalCenter: parent.verticalCenter
                text: cell.favorite ? "★" : ""
                color: Theme.favorite
                font.pixelSize: Theme.fontLarge
            }
            MouseArea {
                anchors.fill: parent
                onClicked: { grid.currentIndex = cell.index; grid.forceActiveFocus() }
                onDoubleClicked: grid.detailsRequested(cell.gameId)
            }
        }
    }

    Keys.onPressed: (event) => {
        if (currentIndex < 0 || currentIndex >= count)
            return
        const id = model.gameIdAt(currentIndex)
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            grid.detailsRequested(id)
            event.accepted = true
        } else if (event.key === Qt.Key_Favorites
                   || (event.key === Qt.Key_F && event.modifiers === Qt.NoModifier)) {
            grid.library.toggleFavorite(id)
            event.accepted = true
        } else if (event.key === Qt.Key_Context1 || (event.key === Qt.Key_O && event.modifiers === Qt.NoModifier)) {
            grid.optionsRequested(id)
            event.accepted = true
        }
    }
}
