import QtQuick

// Row that performs an action on Confirm. The trailing "button" is a visual
// affordance only; the whole row is the focus target. `danger` styles both
// the label and the button for destructive actions (which must confirm).
SettingsRow {
    id: row
    property string buttonText: ""
    property string iconName: ""
    property bool busy: false

    settingsHint: buttonText !== "" ? buttonText : qsTr("Select")
    Accessible.role: Accessible.Button

    Rectangle {
        visible: row.buttonText !== "" || row.iconName !== ""
        height: Dimensions.buttonHeightSmall
        width: content.implicitWidth + Dimensions.spacingLg * 2
        radius: Dimensions.radiusMd
        color: row.danger ? Qt.rgba(1, 0.45, 0.5, row.activeFocus ? 0.24 : 0.12)
             : (row.activeFocus ? Theme.accentPrimary : Theme.backgroundElevated)
        border.width: row.activeFocus && !row.danger ? 0 : 1
        border.color: row.danger ? Theme.statusError : Theme.borderStrong
        Row {
            id: content
            anchors.centerIn: parent
            spacing: Dimensions.spacingSm
            LoadingIndicator {
                visible: row.busy
                anchors.verticalCenter: parent.verticalCenter
                size: Dimensions.iconSizeSmall
                color: row.activeFocus && !row.danger ? Theme.onAccent : Theme.accentPrimary
            }
            Icon {
                visible: row.iconName !== "" && !row.busy
                anchors.verticalCenter: parent.verticalCenter
                name: row.iconName
                size: Dimensions.iconSizeSmall
                color: label.color
            }
            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                visible: text !== ""
                text: row.buttonText
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                font.weight: Typography.weightSemiBold
                color: row.danger ? Theme.statusError
                     : (row.activeFocus ? Theme.onAccent : Theme.textPrimary)
            }
        }
    }
}
