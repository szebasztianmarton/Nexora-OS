import QtQuick

// On/off preference. Confirm (or click) emits toggled(!checked); the owner
// writes the setting, so `checked` always reflects the persisted value.
// The state is shown as text ("On"/"Off") and as a switch, never colour alone.
SettingsRow {
    id: row
    property bool checked: false
    signal toggled(bool value)

    settingsHint: qsTr("Toggle")
    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: checked
    onActivated: toggled(!checked)

    Row {
        spacing: Dimensions.spacingMd
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: row.checked ? qsTr("On") : qsTr("Off")
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            font.weight: Typography.weightSemiBold
            color: row.checked ? Theme.accentPrimary : Theme.textSecondary
        }
        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: Dimensions.s(60)
            height: Dimensions.s(32)
            radius: height / 2
            color: row.checked ? Theme.accentPrimarySoft : Theme.backgroundPrimary
            border.width: Math.max(1, Dimensions.s(2))
            border.color: row.checked ? Theme.accentPrimary : Theme.borderStrong
            Behavior on color { ColorAnimation { duration: Motion.focus } }
            Rectangle {
                width: track.height - Dimensions.s(10)
                height: width
                radius: width / 2
                anchors.verticalCenter: parent.verticalCenter
                x: row.checked ? track.width - width - Dimensions.s(5) : Dimensions.s(5)
                color: row.checked ? Theme.accentPrimary : Theme.textMuted
                Behavior on x { NumberAnimation { duration: Motion.focus; easing.type: Motion.easing } }
            }
        }
    }
}
