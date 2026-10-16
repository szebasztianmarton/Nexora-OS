import QtQuick
import Nexora.Console

Rectangle {
    id: card
    required property int index
    required property string emulatorId
    required property string name
    required property string status
    required property string statusText
    required property string detail
    required property bool available
    required property var profiles
    required property string extensions

    readonly property bool selected: activeFocus || ListView.isCurrentItem && ListView.view.activeFocus
    readonly property int enabledProfileCount: {
        let n = 0
        for (let i = 0; i < profiles.length; ++i)
            n += profiles[i].enabled ? 1 : 0
        return n
    }

    signal activated()

    Accessible.role: Accessible.Button
    Accessible.name: name + ", " + statusText
    Accessible.description: "Starts the emulator without a game"
    Accessible.onPressAction: activated()

    width: 300
    height: 360
    radius: Theme.radius
    color: selected ? Theme.surfaceHover : Theme.surfaceRaised
    opacity: available ? 1.0 : 0.65
    border.width: selected ? Theme.focusWidth : 1
    border.color: selected ? Theme.accent : Theme.border
    scale: selected ? 1.04 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast } }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacing * 1.25
        spacing: 12

        Rectangle {
            width: parent.width
            height: Math.max(90, card.height * 0.38)
            radius: Theme.radius - 4
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.artColor(card.emulatorId, 0.30) }
                GradientStop { position: 1.0; color: Theme.surface }
            }
            Text {
                anchors.centerIn: parent
                text: card.name.length > 0 ? card.name.charAt(0).toUpperCase() : "?"
                color: Theme.artInk
                font.pixelSize: Math.round(64 * Theme.scale)
                font.bold: true
            }
        }

        Text {
            width: parent.width
            text: card.name
            color: Theme.text
            font.pixelSize: Theme.fontLarge
            font.bold: true
            elide: Text.ElideRight
        }

        Rectangle {
            objectName: "statusPill"
            height: 28
            width: pillLabel.implicitWidth + 24
            radius: height / 2
            color: "transparent"
            border.color: Theme.statusColor(card.status)
            border.width: 2
            Text {
                id: pillLabel
                anchors.centerIn: parent
                text: card.statusText
                color: Theme.statusColor(card.status)
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
        }

        Text {
            width: parent.width
            text: (card.extensions.length > 0 ? "Files: " + card.extensions : "No game file types")
                  + (card.enabledProfileCount > 0
                     ? "\n" + card.enabledProfileCount + " launch profile" + (card.enabledProfileCount === 1 ? "" : "s")
                     : "")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: card.detail
            visible: text.length > 0
            color: card.status === "failed" || card.status === "unavailable" ? Theme.danger : Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            card.ListView.view.currentIndex = card.index
            card.ListView.view.forceActiveFocus()
        }
        onDoubleClicked: card.activated()
    }
}
