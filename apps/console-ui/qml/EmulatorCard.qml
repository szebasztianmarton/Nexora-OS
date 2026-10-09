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

    signal activated()

    width: 300
    height: 360
    radius: Theme.radius
    color: Theme.surfaceRaised
    opacity: available ? 1.0 : 0.6
    border.width: activeFocus || ListView.isCurrentItem && ListView.view.activeFocus ? Theme.focusWidth : 1
    border.color: activeFocus || ListView.isCurrentItem && ListView.view.activeFocus ? Theme.accent : Theme.border
    scale: ListView.isCurrentItem && ListView.view.activeFocus ? 1.04 : 1.0
    Behavior on scale { NumberAnimation { duration: 90 } }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacing * 1.5
        spacing: Theme.spacing

        Rectangle {
            width: parent.width
            height: 160
            radius: Theme.radius - 4
            color: Theme.surface
            Text {
                anchors.centerIn: parent
                text: card.name.length > 0 ? card.name.charAt(0).toUpperCase() : "?"
                color: Theme.accent
                font.pixelSize: 72
                font.bold: true
            }
        }

        Text {
            width: parent.width
            text: card.name
            color: Theme.text
            font.pixelSize: Theme.fontBody + 2
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
            text: card.detail
            visible: text.length > 0
            color: card.status === "failed" || card.status === "unavailable" ? Theme.danger : Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            maximumLineCount: 4
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            card.ListView.view.currentIndex = card.index
            card.ListView.view.forceActiveFocus()
        }
        onDoubleClicked: card.activated()
    }
}
