import QtQuick
import Nexora.Console

// One-pixel light edge along the top of a glass panel (the "rim" that makes translucent
// surfaces read as glass). Put it inside the panel; invisible outside the glass theme. The
// ends stay clear of the rounded corners: |inset| defaults to the largest panel radius.
Rectangle {
    property real inset: Theme.radiusLarge
    visible: Theme.glassActive
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.leftMargin: inset
    anchors.rightMargin: inset
    anchors.topMargin: 1
    height: 1
    color: Theme.glassSheen
}
