import QtQuick
import Nexora.Console

// A titled horizontal row of games (Home: "Continue playing", "Favorites").
// The row itself takes focus; Left/Right move, Accept launches, Favorite toggles.
FocusScope {
    id: row
    required property LibraryController library
    property alias model: list.model
    property string title
    property string emptyText
    readonly property alias count: list.count
    readonly property alias currentIndex: list.currentIndex
    readonly property bool hasGames: list.count > 0
    property real currentGameId: -1

    signal activated(real gameId)
    signal optionsRequested(real gameId)

    implicitHeight: heading.height + 10 + (hasGames ? tileHeight + 24 : emptyBox.height)
    readonly property real tileWidth: Theme.cardWidth
    readonly property real tileHeight: Theme.cardHeight

    function selectedGameId() {
        return list.currentIndex >= 0 && list.model ? list.model.gameIdAt(list.currentIndex) : -1
    }
    // Keeps the same game selected when the list is rebuilt (e.g. resorted after a launch).
    function restoreSelection() {
        if (!list.model || currentGameId < 0)
            return
        const i = list.model.indexOfGame(currentGameId)
        if (i >= 0)
            list.currentIndex = i
        else if (list.currentIndex >= 0 && list.currentIndex < list.count)
            currentGameId = list.model.gameIdAt(list.currentIndex) // the game left the list
    }

    Row {
        id: heading
        spacing: 12
        Text {
            textFormat: Text.PlainText
            id: headingLabel
            text: row.title
            color: row.activeFocus ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontLarge
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            anchors.baseline: headingLabel.baseline
            visible: row.hasGames
            text: row.count
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
        }
    }

    ListView {
        id: list
        objectName: row.objectName + "List"
        anchors.top: heading.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        height: row.tileHeight + 24
        visible: row.hasGames
        focus: true
        orientation: ListView.Horizontal
        spacing: Theme.spacing
        leftMargin: 8
        rightMargin: 8
        topMargin: 12
        clip: false
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animMove
        preferredHighlightBegin: 8
        preferredHighlightEnd: width - row.tileWidth - 8
        highlightRangeMode: ListView.ApplyRange
        boundsBehavior: Flickable.StopAtBounds

        onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex < count) row.currentGameId = model.gameIdAt(currentIndex)
        function syncSelection() {
            row.restoreSelection()
            if (count > 0 && currentIndex < 0)
                currentIndex = 0
        }
        // Deferred: after a model reset the view picks its own current index first.
        onCountChanged: Qt.callLater(syncSelection)

        delegate: GameTile {
            required property int index
            required gameId
            required title
            required emulatorName
            required favorite
            required lastPlayed
            required available
            required unavailableReason
            required present
            required systemShortName
            width: row.tileWidth
            height: row.tileHeight
            selected: ListView.isCurrentItem && list.activeFocus
            onPressed: { list.currentIndex = index; list.forceActiveFocus() }
            onActivated: row.activated(gameId)
            onFavoriteClicked: row.library.toggleFavorite(gameId)
        }

        Keys.onPressed: (event) => {
            const id = row.selectedGameId()
            if (id < 0)
                return
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                row.activated(id)
                event.accepted = true
            } else if (event.key === Qt.Key_Favorites
                       || (event.key === Qt.Key_F && event.modifiers === Qt.NoModifier)) {
                row.library.toggleFavorite(id)
                event.accepted = true
            } else if (event.key === Qt.Key_Context1 || (event.key === Qt.Key_O && event.modifiers === Qt.NoModifier)) {
                row.optionsRequested(id)
                event.accepted = true
            }
        }
    }

    Connections {
        target: row.library
        function onContentChanged() { Qt.callLater(list.syncSelection) }
    }

    Rectangle {
        id: emptyBox
        visible: !row.hasGames
        anchors.top: heading.bottom
        anchors.topMargin: 10
        width: parent.width
        height: 64
        radius: Theme.radius
        color: "transparent"
        border.color: Theme.border
        border.width: 1
        Text {
            textFormat: Text.PlainText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacing
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacing
            text: row.emptyText
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideRight
        }
    }
}
