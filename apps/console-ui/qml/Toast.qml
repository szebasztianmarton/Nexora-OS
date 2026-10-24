import QtQuick
import Nexora.Console

// The visible notification: a view of NotificationService.current (one at a time; the
// service decides queueing, dedup and duration). Never takes focus. A repeated event shows
// its count ("×3").
Rectangle {
    id: toast
    objectName: "toast"
    property NotificationService service
    readonly property var notification: service ? service.current : ({})
    readonly property bool hasNotification: notification.text !== undefined
    readonly property string severity: hasNotification ? notification.severity : "info"
    readonly property bool isError: severity !== "info"
    readonly property string message: hasNotification
                                      ? (notification.count > 1 ? notification.text + "  ×" + notification.count
                                                                : notification.text)
                                      : ""
    readonly property bool shown: opacity > 0
    // Set by Main.qml: the window's scene, blurred behind the toast (glass blur only).
    property Item glassBackdrop: null
    // Kept while fading out, so the text does not vanish before the box does.
    property string displayedMessage
    onMessageChanged: if (message.length > 0) displayedMessage = message

    Accessible.role: Accessible.AlertMessage
    Accessible.name: message

    width: Math.min(label.implicitWidth + Math.round(56 * Theme.scale),
                    parent ? parent.width - Theme.space6 * 2 : Math.round(800 * Theme.scale))
    height: Math.max(Theme.controlHeight, label.implicitHeight + Theme.space5)
    radius: Theme.glassActive ? Math.min(Theme.radiusXL, height / 2) : Theme.radius
    color: Theme.glassActive ? "transparent" : Theme.surfaceFloating
    GlassSurface { anchors.fill: parent; z: -1; radius: toast.radius; dense: true; backdrop: toast.glassBackdrop }
    border.color: severity === "critical" ? Theme.danger : isError ? Theme.warning : Theme.accent
    border.width: Theme.glassActive ? 0 : 1
    opacity: hasNotification ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: Math.round(10 * Theme.scale)
        width: Theme.space1
        radius: width / 2
        color: toast.severity === "critical" ? Theme.danger : toast.isError ? Theme.warning : Theme.accent
    }
    Text {
        textFormat: Text.PlainText
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, toast.parent ? toast.parent.width - Math.round(120 * Theme.scale)
                                                    : Math.round(740 * Theme.scale))
        text: toast.displayedMessage
        color: Theme.text
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
        maximumLineCount: 3
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
    }
}
