import QtQuick

// The focus ring drawn around a focused control. Place as a child and anchor
// to the control (it fills the parent with an outset). Visible without any
// animation so focus is never ambiguous when reduced motion is on.
Rectangle {
    id: frame
    property bool shown: false
    property real radiusBase: Dimensions.radiusMd

    anchors.fill: parent
    anchors.margins: -Dimensions.focusOffset
    radius: radiusBase + Dimensions.focusOffset
    color: "transparent"
    border.color: Theme.focusRing
    border.width: Dimensions.focusWidth
    opacity: shown ? 1 : 0
    visible: opacity > 0
    z: 10
    Behavior on opacity { NumberAnimation { duration: Motion.focus } }

    // Soft outer glow; purely decorative and cheap (a second rectangle).
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Dimensions.focusWidth
        radius: parent.radius + Dimensions.focusWidth
        color: "transparent"
        border.color: Theme.focusGlow
        border.width: Dimensions.focusWidth
    }
}
