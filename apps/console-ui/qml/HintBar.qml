import QtQuick
import Nexora.Console

// Bottom bar listing the controls available in the current context.
Rectangle {
    id: bar
    objectName: "hintBar"
    property var hints: []   // [{ button: "A / Enter", action: "Launch" }, ...]

    implicitHeight: 48
    color: Theme.surface

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    Row {
        anchors.centerIn: parent
        spacing: Theme.spacing * 1.5
        Repeater {
            model: bar.hints
            delegate: Row {
                id: hint
                required property var modelData
                spacing: 8
                Rectangle {
                    height: 26
                    width: Math.max(26, keyLabel.implicitWidth + 16)
                    radius: 6
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    Text {
                        id: keyLabel
                        anchors.centerIn: parent
                        text: hint.modelData.button
                        color: Theme.text
                        font.pixelSize: Theme.fontTiny + 1
                        font.bold: true
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: hint.modelData.action
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }
}
