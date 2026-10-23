import QtQuick
import Nexora.Console

// Systems: one card per gaming system that has games or an emulator (the built-in catalog
// knows more; those are summarised underneath). Accept opens the Games tab filtered to that
// system. Loading state during the first scan, an explanation when nothing is installed.
FocusScope {
    id: screen
    objectName: "systemsScreen"
    required property AppController app
    required property LibraryController library
    // Height the floating glass bars cover at the top / bottom of the page (set by Main.qml;
    // 0 when the bars are docked).
    property real chromeTop: 0
    property real chromeBottom: 0

    signal navigate(string tab)
    signal systemChosen(string systemId)

    readonly property var hints: grid.activeFocus
        ? [{ button: "↑ ↓ ← →", action: "Move" }, { button: "A / Enter", action: "Show games" },
           { button: "L1 / R1", action: "Switch tab" }]
        : [{ button: "A / Enter", action: emptyAction.text }, { button: "L1 / R1", action: "Switch tab" }]

    property int revision: 0
    // [{systemId, name, shortName, gameCount, emulatorCount}] for installed systems.
    readonly property var installed: {
        void revision
        const list = []
        const model = library.systems
        for (let i = 0; i < model.count; ++i) {
            const s = model.get(i)
            if (s.gameCount > 0 || s.emulatorCount > 0)
                list.push(s)
        }
        return list
    }
    readonly property int otherCount: library.systems.count - installed.length

    Connections {
        target: screen.library
        function onContentChanged() { screen.revision += 1 }
    }
    Connections {
        target: screen.app
        function onConfigChanged() { screen.revision += 1 }
    }

    function focusDefault() {
        if (grid.count > 0)
            grid.focus = true
        else
            emptyAction.focus = true
    }
    function choose(index) {
        const s = installed[index]
        if (s)
            screen.systemChosen(s.systemId)
    }
    Component.onCompleted: focusDefault()
    onInstalledChanged: {
        if (!visible)
            return
        if (installed.length === 0 && grid.activeFocus)
            emptyAction.forceActiveFocus()
        else if (installed.length > 0 && emptyAction.activeFocus)
            grid.forceActiveFocus()
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
            text: "Systems"
            color: Theme.text
            font.pixelSize: Theme.fontHero
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            objectName: "systemsSummary"
            width: parent.width
            wrapMode: Text.Wrap
            text: (screen.installed.length === 1 ? "1 system" : screen.installed.length + " systems")
                  + " with games or emulators"
                  + (screen.otherCount > 0 ? " · " + screen.otherCount + " more supported once you add games and an emulator for them" : "")
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }
    }

    GridView {
        id: grid
        objectName: "systemGrid"
        anchors.top: heading.bottom
        anchors.topMargin: Theme.space3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space6 - Theme.space2
        anchors.rightMargin: Theme.space6 - Theme.space2
        visible: count > 0
        focus: true
        clip: true
        model: screen.installed
        keyNavigationEnabled: true
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: Theme.animMove
        // Glass: the cards scroll under the hint bar; the highlight range keeps the current one
        // above it (plain tracking would only scroll it to the view's own edge). Docked: the
        // default tracking, as before.
        highlightRangeMode: screen.chromeBottom > 0 ? GridView.ApplyRange : GridView.NoHighlightRange
        preferredHighlightBegin: Theme.space3
        preferredHighlightEnd: height - screen.chromeBottom - cellHeight - Theme.space3
        topMargin: Theme.space3
        bottomMargin: Theme.space3 + screen.chromeBottom
        readonly property int columns: Math.max(2, Math.floor(width / (280 * Theme.scale)))
        cellWidth: Math.floor(width / columns)
        cellHeight: Math.round(150 * Theme.scale)

        delegate: Item {
            id: cell
            required property var modelData
            required property int index
            readonly property bool selected: GridView.isCurrentItem && grid.activeFocus
            width: grid.cellWidth
            height: grid.cellHeight
            Rectangle {
                objectName: "systemCard_" + (cell.modelData.systemId.length > 0 ? cell.modelData.systemId : "other")
                anchors.fill: parent
                anchors.margins: Theme.space2
                radius: Theme.radius
                color: cell.selected ? Theme.surfaceHover : Theme.surfaceRaised
                border.width: cell.selected ? Theme.focusWidth : 1
                border.color: cell.selected ? Theme.focusRing : Theme.border
                scale: cell.selected && !Theme.reducedMotion ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: Theme.animFast } }
                Accessible.role: Accessible.Button
                Accessible.name: cell.modelData.name
                Accessible.description: cell.modelData.gameCount === 1 ? "1 game" : cell.modelData.gameCount + " games"

                Rectangle {
                    id: badge
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: Theme.space4
                    height: Math.round(44 * Theme.scale)
                    width: Math.max(height, badgeText.implicitWidth + Theme.space4)
                    radius: Theme.radiusSmall
                    color: Theme.artColor(cell.modelData.name, 0.3)
                    Text {
                        textFormat: Text.PlainText
                        id: badgeText
                        anchors.centerIn: parent
                        text: cell.modelData.shortName
                        color: Theme.artInk
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }
                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space4
                    spacing: 2
                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        elide: Text.ElideRight
                        text: cell.modelData.name
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }
                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        elide: Text.ElideRight
                        text: (cell.modelData.gameCount === 1 ? "1 game" : cell.modelData.gameCount + " games")
                              + " · " + (cell.modelData.systemId.length === 0 ? "no recognised system"
                                         : cell.modelData.emulatorCount === 0 ? "no emulator"
                                         : cell.modelData.emulatorCount === 1 ? "1 emulator"
                                         : cell.modelData.emulatorCount + " emulators")
                        color: cell.modelData.emulatorCount === 0 && cell.modelData.systemId.length > 0
                               ? Theme.warning : Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: { grid.currentIndex = cell.index; grid.forceActiveFocus() }
                    onDoubleClicked: screen.choose(cell.index)
                }
            }
        }

        Keys.onReturnPressed: screen.choose(currentIndex)
        Keys.onEnterPressed: screen.choose(currentIndex)
    }

    Column {
        objectName: "systemsLoading"
        anchors.centerIn: grid
        visible: grid.count === 0 && screen.library.scanning
        Text {
            textFormat: Text.PlainText
            text: "Scanning your library…"
            color: Theme.text
            font.pixelSize: Theme.fontLarge
        }
    }

    Column {
        objectName: "systemsEmpty"
        anchors.centerIn: grid
        width: Math.min(parent.width - Theme.space6 * 3, 680 * Theme.scale)
        visible: grid.count === 0 && !screen.library.scanning
        spacing: Theme.space4
        Text {
            textFormat: Text.PlainText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "No gaming systems installed yet"
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "A system appears here once Nexora finds games for it or an emulator that declares it. "
                  + "Add a game folder in Settings › Library and emulator definitions in "
                  + screen.app.configDirectory + "."
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }
        NxButton {
            id: emptyAction
            objectName: "systemsEmptyAction"
            anchors.horizontalCenter: parent.horizontalCenter
            kind: "primary"
            text: "Open Settings"
            onClicked: screen.navigate("settings")
        }
    }
}
