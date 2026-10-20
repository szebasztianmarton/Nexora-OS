import QtQuick
import Nexora.Console

// Bottom bar listing the controls available in the current context. Docked in dark / high
// contrast; a floating glass bar in the glass theme (Main.qml places it).
Rectangle {
    id: bar
    objectName: "hintBar"
    property var hints: []   // [{ button: "A / Enter", action: "Launch" }, ...]

    implicitHeight: Theme.barHeight
    color: Theme.glassActive ? "transparent" : Theme.surface

    GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL }
    // Glass: the bar floats over the scrolling pages; presses on it (also between its controls)
    // must not reach a card scrolled underneath. Below the bar's own content in z.
    MouseArea {
        objectName: "hintBarClickShield"
        anchors.fill: parent
        z: -1
        enabled: Theme.glassActive
        hoverEnabled: false
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = false // scrolling still reaches the page
    }
    Rectangle {
        visible: !Theme.glassActive
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
                        textFormat: Text.PlainText
                        id: keyLabel
                        anchors.centerIn: parent
                        text: hint.modelData.button
                        color: Theme.text
                        font.pixelSize: Theme.fontTiny + 1
                        font.bold: true
                    }
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                    text: hint.modelData.action
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }
}
