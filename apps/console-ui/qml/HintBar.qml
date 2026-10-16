import QtQuick
import Nexora.Console

// Bottom bar listing the controls available in the current context.
Rectangle {
    id: bar
    objectName: "hintBar"
    property var hints: []   // [{ button: "A / Enter", action: "Launch" }, ...]

    implicitHeight: Theme.barHeight
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
                    height: Math.round(26 * Theme.scale)
                    width: Math.max(height, keyLabel.implicitWidth + Theme.space4)
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
