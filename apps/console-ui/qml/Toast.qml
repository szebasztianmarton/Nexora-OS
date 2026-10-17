import QtQuick
import Nexora.Console

// Transient notice ("Added /games", launch errors). Never takes focus.
Rectangle {
    id: toast
    objectName: "toast"
    property string message
    property bool isError: false
    readonly property bool shown: opacity > 0

    function show(text, error) {
        if (!text || text.length === 0)
            return
        message = text
        isError = error
        opacity = 1
        hideTimer.restart()
    }

    width: Math.min(label.implicitWidth + 56, parent ? parent.width - 64 : 800)
    height: Math.max(48, label.implicitHeight + 24)
    radius: 12
    color: Theme.surfaceFloating
    GlassSheen {}
    border.color: isError ? Theme.danger : Theme.accent
    border.width: 1
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 4
        radius: 2
        color: toast.isError ? Theme.danger : Theme.accent
    }
    Text {
        textFormat: Text.PlainText
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.parent ? toast.parent.width - 120 : 740)
        text: toast.message
        color: Theme.text
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
    }
    Timer {
        id: hideTimer
        interval: toast.isError ? 6000 : 3500
        onTriggered: toast.opacity = 0
    }
}
