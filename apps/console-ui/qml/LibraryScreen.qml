import QtQuick
import Nexora.Console

// Emulator library: a horizontal row of cards navigated with arrows / D-pad.
FocusScope {
    id: screen
    objectName: "libraryScreen"
    required property AppController app

    readonly property alias currentIndex: list.currentIndex

    Text {
        id: title
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacing * 2
        text: "Library"
        color: Theme.text
        font.pixelSize: Theme.fontHero
        font.bold: true
    }

    Text {
        id: statusLine
        objectName: "statusLine"
        anchors.top: title.bottom
        anchors.left: title.left
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 2
        anchors.topMargin: 6
        text: screen.app.statusMessage
        color: screen.app.statusIsError ? Theme.danger : Theme.textMuted
        font.pixelSize: Theme.fontBody
        elide: Text.ElideRight
    }

    ListView {
        id: list
        objectName: "emulatorList"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 24
        height: 400
        leftMargin: Theme.spacing * 2
        rightMargin: Theme.spacing * 2
        orientation: ListView.Horizontal
        spacing: Theme.spacing * 1.5
        clip: false
        focus: true
        visible: count > 0
        keyNavigationEnabled: true
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 120
        preferredHighlightBegin: Theme.spacing * 2
        preferredHighlightEnd: width - 300 - Theme.spacing * 2
        highlightRangeMode: ListView.ApplyRange
        model: screen.app.emulators

        delegate: EmulatorCard {
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            onActivated: screen.app.launch(emulatorId)
        }

        function launchCurrent() {
            if (currentIndex >= 0)
                screen.app.launch(screen.app.emulators.emulatorIdAt(currentIndex))
        }
        Keys.onReturnPressed: launchCurrent()
        Keys.onEnterPressed: launchCurrent()
        Keys.onSpacePressed: launchCurrent()
    }

    EmptyState {
        objectName: "emptyState"
        app: screen.app
        anchors.centerIn: parent
        visible: list.count === 0
    }
}
