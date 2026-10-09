import QtQuick
import Nexora.Console

Rectangle {
    id: header
    required property AppController app

    implicitHeight: 72
    color: Theme.surface

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing * 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Text {
            text: "NEXORA"
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.bold: true
            font.letterSpacing: 4
        }
        Text {
            text: "OS"
            color: Theme.accent
            font.pixelSize: Theme.fontTitle
            font.letterSpacing: 4
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacing

        Rectangle {
            visible: header.app.runningAsRoot
            radius: height / 2
            color: Theme.danger
            height: 30
            width: rootLabel.implicitWidth + 24
            Text {
                id: rootLabel
                anchors.centerIn: parent
                text: "Running as root"
                color: Theme.background
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
        }

        Rectangle {
            objectName: "stateChip"
            radius: height / 2
            height: 30
            width: stateLabel.implicitWidth + 24
            color: Theme.surfaceRaised
            border.color: header.app.busy ? Theme.statusColor(header.app.state) : Theme.border
            border.width: 2
            Text {
                id: stateLabel
                anchors.centerIn: parent
                text: header.app.busy ? header.app.activeEmulatorName + " · " + header.app.state
                                      : "Ready"
                color: Theme.text
                font.pixelSize: Theme.fontSmall
            }
        }

        Text {
            id: clock
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            text: Qt.formatTime(new Date(), "hh:mm")
            Timer {
                interval: 10000
                running: true
                repeat: true
                onTriggered: clock.text = Qt.formatTime(new Date(), "hh:mm")
            }
        }
    }
}
