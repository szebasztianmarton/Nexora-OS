import QtQuick
import QtQuick.Layouts

// Page title row: title + optional subtitle on the left, actions on the right.
Item {
    id: header
    property string title
    property string subtitle
    readonly property alias actionsSlot: actions

    implicitHeight: Math.max(titleColumn.implicitHeight, actions.implicitHeight)
    height: implicitHeight

    Column {
        id: titleColumn
        anchors.left: parent.left
        anchors.right: actions.left
        anchors.rightMargin: Dimensions.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        spacing: Dimensions.s(4)
        Text {
            width: parent.width
            text: header.title
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.pageTitle
            font.weight: Typography.weightBold
            color: Theme.textPrimary
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: header.subtitle
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            color: Theme.textSecondary
        }
    }
    Item {
        id: actions
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
        width: implicitWidth
        height: implicitHeight
    }
}
