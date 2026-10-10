import QtQuick

// Stack of short-lived, non-blocking notifications (max 3 visible). Identical
// consecutive messages are merged. Never the only place a significant error
// is explained — use a MessageDialog for those.
Item {
    id: host
    property int maxVisible: 3

    function show(message, kind) {
        if (toasts.count > 0 && toasts.get(toasts.count - 1).message === message) {
            toasts.setProperty(toasts.count - 1, "stamp", Date.now())
            return
        }
        toasts.append({ message: message, kind: kind || "info", stamp: Date.now() })
        while (toasts.count > maxVisible)
            toasts.remove(0)
    }

    ListModel { id: toasts }

    Timer {
        interval: 250; repeat: true
        running: toasts.count > 0
        onTriggered: {
            var now = Date.now()
            for (var i = toasts.count - 1; i >= 0; --i) {
                var t = toasts.get(i)
                var life = t.kind === "error" ? 6000 : 3200
                if (now - t.stamp > life) toasts.remove(i)
            }
        }
    }

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: Dimensions.spacingSm
        Repeater {
            model: toasts
            delegate: Rectangle {
                id: toast
                required property string message
                required property string kind
                readonly property color tone: Theme.statusColor(kind)
                width: Math.min(Dimensions.s(520), host.width)
                height: row.implicitHeight + Dimensions.spacingMd * 2
                radius: Dimensions.radiusMd
                color: Theme.backgroundElevated
                border.color: Theme.borderStrong
                border.width: 1
                opacity: 0
                Component.onCompleted: opacity = 1
                Behavior on opacity { NumberAnimation { duration: Motion.toast } }
                Accessible.role: Accessible.AlertMessage
                Accessible.name: message
                Rectangle {
                    width: Dimensions.s(4)
                    anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: Dimensions.spacingSm
                    radius: width / 2
                    color: toast.tone
                }
                Row {
                    id: row
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.spacingLg
                    anchors.right: parent.right
                    anchors.rightMargin: Dimensions.spacingMd
                    spacing: Dimensions.spacingSm
                    Icon {
                        name: toast.kind === "success" ? "success" : toast.kind === "error" ? "error"
                            : toast.kind === "warning" ? "warning" : "info"
                        size: Dimensions.iconSizeSmall
                        color: toast.tone
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        width: parent.width - Dimensions.iconSizeSmall - parent.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        text: toast.message
                        wrapMode: Text.WordWrap
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: Theme.textPrimary
                    }
                }
            }
        }
    }
}
