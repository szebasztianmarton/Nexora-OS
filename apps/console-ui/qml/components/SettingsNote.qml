import QtQuick

// Inline explanatory note (not focusable): used for honest "not available in
// this release" statements, partial-state warnings and errors.
// kind: info | warning | error | success
Rectangle {
    id: note
    property string kind: "info"
    property string text: ""
    property string title: ""
    property bool monospaceText: false
    readonly property color tone: Theme.statusColor(kind)

    width: parent ? parent.width : implicitWidth
    implicitWidth: Dimensions.s(600)
    implicitHeight: column.implicitHeight + Dimensions.spacingMd * 2
    radius: Dimensions.radiusMd
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.08)
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.35)

    Icon {
        id: icon
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.spacingMd
        anchors.top: parent.top
        anchors.topMargin: Dimensions.spacingMd
        name: note.kind === "warning" ? "warning" : note.kind === "error" ? "error"
            : note.kind === "success" ? "success" : "info"
        size: Dimensions.iconSizeSmall
        color: note.tone
    }
    Column {
        id: column
        anchors.left: icon.right
        anchors.leftMargin: Dimensions.spacingSm
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.spacingMd
        anchors.top: parent.top
        anchors.topMargin: Dimensions.spacingMd
        spacing: Dimensions.s(4)
        Text {
            width: parent.width
            visible: text !== ""
            text: note.title
            wrapMode: Text.WordWrap
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            font.weight: Typography.weightSemiBold
            color: Theme.textPrimary
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: note.text
            wrapMode: note.monospaceText ? Text.WrapAnywhere : Text.WordWrap
            font.family: note.monospaceText ? "monospace" : Typography.family
            font.pixelSize: note.monospaceText ? Typography.caption : Typography.secondary
            color: Theme.textSecondary
            lineHeight: 1.1
        }
    }
}
