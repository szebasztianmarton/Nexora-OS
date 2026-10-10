import QtQuick

// Compact pill used for sort/filter options. `checked` marks the active
// choice with a check icon and accent outline (not colour alone).
FocusScope {
    id: chip
    property string text
    property string iconName: ""
    property bool checked: false
    signal clicked()

    implicitHeight: Dimensions.buttonHeightSmall
    implicitWidth: content.implicitWidth + Dimensions.spacingLg * 2
    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked

    Keys.onPressed: function(event) {
        if (Input.isConfirm(event)) { event.accepted = true; if (!event.isAutoRepeat) chip.clicked() }
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: chip.checked ? Theme.accentPrimarySoft : (chip.activeFocus || mouse.containsMouse ? Theme.backgroundHover : Theme.backgroundElevated)
        border.width: 1
        border.color: chip.checked ? Theme.accentPrimary : Theme.borderSubtle
    }
    FocusFrame { shown: chip.activeFocus; radiusBase: chip.height / 2 }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Dimensions.spacingXs
        Icon {
            visible: chip.checked || chip.iconName !== ""
            name: chip.checked ? "check" : chip.iconName
            size: Dimensions.iconSizeSmall * 0.9
            color: chip.checked ? Theme.accentPrimary : Theme.textSecondary
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: chip.text
            anchors.verticalCenter: parent.verticalCenter
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            font.weight: chip.checked ? Typography.weightSemiBold : Typography.weightMedium
            color: chip.checked || chip.activeFocus ? Theme.textPrimary : Theme.textSecondary
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { chip.forceActiveFocus(); chip.clicked() }
    }
}
