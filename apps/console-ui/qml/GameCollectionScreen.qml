import QtQuick
import Nexora.Console

// A titled grid of one game list: the Favorites and Recently played tabs. Accept opens a
// game's details, Y / F toggles the favorite, Select / O opens the launch options. Shows a
// loading state while the first scan runs and an explanation while the list is empty.
FocusScope {
    id: screen
    required property LibraryController library
    required property GameListModel games
    property string title
    property string subtitle
    property string emptyTitle
    property string emptyText
    // Height the floating glass bars cover at the top / bottom of the page (set by Main.qml;
    // 0 when the bars are docked).
    property real chromeTop: 0
    property real chromeBottom: 0

    signal navigate(string tab)
    signal optionsRequested(real gameId)
    signal detailsRequested(real gameId)

    readonly property var hints: grid.activeFocus
        ? [{ button: "↑ ↓ ← →", action: "Move" }, { button: "A / Enter", action: "Details" },
           { button: "Y / F", action: "Favorite" }, { button: "Select / O", action: "Options" },
           { button: "L1 / R1", action: "Switch tab" }, { button: "B / Esc", action: "Tabs" }]
        : [{ button: "A / Enter", action: browseButton.text }, { button: "L1 / R1", action: "Switch tab" },
           { button: "B / Esc", action: "Tabs" }]

    function focusDefault() {
        if (grid.count > 0)
            grid.focus = true
        else
            browseButton.focus = true
    }
    Component.onCompleted: focusDefault()
    // The list fills or empties while the tab is shown: keep focus on something visible.
    Connections {
        target: screen.games
        function onCountChanged() {
            if (!screen.visible)
                return
            if (screen.games.count === 0 && grid.activeFocus)
                browseButton.forceActiveFocus()
            else if (screen.games.count > 0 && browseButton.activeFocus)
                grid.forceActiveFocus()
        }
    }

    Column {
        id: heading
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.space5 + screen.chromeTop
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        spacing: Theme.space1
        Text {
            textFormat: Text.PlainText
            text: screen.title
            color: Theme.text
            font.pixelSize: Theme.fontHero
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            objectName: screen.objectName + "Count"
            width: parent.width
            elide: Text.ElideRight
            text: (grid.count === 1 ? "1 game" : grid.count + " games") + (screen.subtitle.length > 0 ? " · " + screen.subtitle : "")
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }
    }

    GameGrid {
        id: grid
        objectName: screen.objectName + "Grid"
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space6 - Theme.space2
        anchors.rightMargin: Theme.space6 - Theme.space2
        visible: count > 0
        focus: true
        library: screen.library
        model: screen.games
        chromeBottom: screen.chromeBottom
        onDetailsRequested: (gameId) => screen.detailsRequested(gameId)
        onOptionsRequested: (gameId) => screen.optionsRequested(gameId)
    }

    Column {
        id: loading
        objectName: screen.objectName + "Loading"
        anchors.centerIn: grid
        visible: grid.count === 0 && screen.library.scanning && screen.library.totalCount === 0
        spacing: Theme.space3
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.round(56 * Theme.scale)
            height: width
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.color: Theme.border
                border.width: Math.round(5 * Theme.scale)
            }
            Rectangle {
                width: Math.round(14 * Theme.scale)
                height: width
                radius: width / 2
                color: Theme.accent
                x: parent.width / 2 - width / 2
                y: -width / 4
            }
            RotationAnimation on rotation {
                // A continuous spin, not a discrete transition: reducedMotion stops it
                // outright rather than collapsing its duration to 0 (the Games screen's spinner).
                running: loading.visible && !Theme.reducedMotion
                from: 0
                to: 360
                duration: 1100
                loops: Animation.Infinite
            }
        }
        Text {
            textFormat: Text.PlainText
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Scanning your library…"
            color: Theme.text
            font.pixelSize: Theme.fontLarge
        }
    }

    Column {
        objectName: screen.objectName + "Empty"
        anchors.centerIn: grid
        width: Math.min(parent.width - Theme.space6 * 3, 680 * Theme.scale)
        visible: grid.count === 0 && !(screen.library.scanning && screen.library.totalCount === 0)
        spacing: Theme.space4
        Text {
            textFormat: Text.PlainText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: screen.emptyTitle
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: screen.library.totalCount === 0
                  ? "Your library is empty. Add a game folder in Settings › Library first."
                  : screen.emptyText
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }
        NxButton {
            id: browseButton
            objectName: screen.objectName + "Browse"
            anchors.horizontalCenter: parent.horizontalCenter
            kind: "primary"
            text: screen.library.totalCount === 0 ? "Add a game folder" : "Browse games"
            onClicked: screen.navigate(screen.library.totalCount === 0 ? "settings" : "games")
        }
    }
}
