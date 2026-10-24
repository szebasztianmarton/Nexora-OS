import QtQuick
import Nexora.Console

// The game grid shared by Games, Favorites and Recent: cover cards (grid) or compact rows
// (|listMode|). Arrow keys move, Accept opens the game's details, Y / F toggles the favorite,
// Select / O opens the launch options. Keeps the same game selected when the model is rebuilt.
// Card size follows the space available: when a whole card does not fit the view's height
// (large interface scale at 720p) the cards get smaller (more columns); when even small cards
// do not fit, the view shows compact rows instead (|effectiveListMode|).
GridView {
    id: grid
    required property LibraryController library
    property bool listMode: false
    // Height the glass hint bar covers at the bottom of the view: the cards scroll under it,
    // but the current one stays above it.
    property real chromeBottom: 0
    readonly property bool effectiveListMode: listMode || !cardsFit

    signal detailsRequested(real gameId)
    signal optionsRequested(real gameId)

    clip: true
    keyNavigationEnabled: true
    boundsBehavior: Flickable.StopAtBounds
    highlightMoveDuration: Theme.animMove
    highlightRangeMode: GridView.ApplyRange
    preferredHighlightBegin: Theme.space3
    preferredHighlightEnd: height - chromeBottom - cellHeight - Theme.space3
    topMargin: Theme.space3
    bottomMargin: Theme.space3 + chromeBottom

    // A card is its art (0.62 x the cell width) plus the title/emulator area.
    readonly property real cardInfoHeight: Math.round(70 * Theme.scale) + Theme.space4
    readonly property int preferredColumns: Math.max(2, Math.floor(width / (240 * Theme.scale)))
    // Enough columns that one row of cards fits the visible height.
    readonly property int fitColumns: {
        const room = height - topMargin - bottomMargin - cardInfoHeight - 1
        return room > 0 ? Math.max(preferredColumns, Math.ceil(width * 0.62 / room)) : 1000
    }
    readonly property bool cardsFit: width <= 0 || height <= 0 || Math.floor(width / fitColumns) >= 160 * Theme.scale
    readonly property int columns: effectiveListMode ? 1 : fitColumns
    cellWidth: Math.floor(width / columns)
    cellHeight: effectiveListMode ? Math.round(76 * Theme.scale)
                                  : Math.round(cellWidth * 0.62) + cardInfoHeight

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
    onEffectiveListModeChanged: Qt.callLater(() => positionViewAtIndex(currentIndex, GridView.Contain))

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
            visible: !grid.effectiveListMode
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
            visible: grid.effectiveListMode
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
                    textFormat: Text.PlainText
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
                    textFormat: Text.PlainText
                    width: parent.width
                    text: cell.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    textFormat: Text.PlainText
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
                textFormat: Text.PlainText
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
