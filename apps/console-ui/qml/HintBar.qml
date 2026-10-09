import QtQuick
import Nexora.Console

// Bottom bar listing the controls available on the current screen.
Rectangle {
    id: bar
    property var hints: []   // [{ button: "A / Enter", action: "Launch" }, ...]

    implicitHeight: 52
    color: Theme.surface

    Row {
        anchors.centerIn: parent
        spacing: Theme.spacing * 2
        Repeater {
            model: bar.hints
            delegate: Row {
                id: hint
                required property var modelData
                spacing: 8
                Rectangle {
                    height: 26
                    width: keyLabel.implicitWidth + 16
                    radius: 6
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    Text {
                        id: keyLabel
                        anchors.centerIn: parent
                        text: hint.modelData.button
                        color: Theme.text
                        font.pixelSize: Theme.fontSmall
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
