import QtQuick

// One sidebar entry. `active` = the section currently shown (marked by an
// indicator bar and bold label, not colour alone); focus = FocusFrame.
Item {
    id: item
    property string label
    property string iconName
    property bool active: false
    property bool compact: false
    signal activated()

    implicitHeight: Dimensions.navItemHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: label
    Accessible.selected: item.active
    Accessible.onPressAction: item.activated()

    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusMd
        color: item.active ? Theme.accentPrimarySoft
             : (item.activeFocus || mouse.containsMouse) ? Theme.backgroundHover : "transparent"
        Behavior on color { ColorAnimation { duration: Motion.focus } }
    }
    Rectangle {
        // Active indicator bar
        width: Dimensions.s(4)
        height: parent.height * 0.5
        radius: width / 2
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.s(4)
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.accentPrimary
        visible: item.active
    }
    FocusFrame { shown: item.activeFocus }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: item.compact ? (item.width - Dimensions.iconSize) / 2 : Dimensions.spacingLg
        spacing: Dimensions.spacingMd
        Icon {
            name: item.iconName
            anchors.verticalCenter: parent.verticalCenter
            color: item.active ? Theme.accentPrimary : (item.activeFocus ? Theme.textPrimary : Theme.textSecondary)
        }
        Text {
            visible: !item.compact
            anchors.verticalCenter: parent.verticalCenter
            text: item.label
            font.family: Typography.family
            font.pixelSize: Typography.body
            font.weight: item.active ? Typography.weightSemiBold : Typography.weightMedium
            color: item.active || item.activeFocus ? Theme.textPrimary : Theme.textSecondary
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { item.forceActiveFocus(); item.activated() }
    }
}
