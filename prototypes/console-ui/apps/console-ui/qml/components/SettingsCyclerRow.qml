import QtQuick

// Choice between a fixed list of options. Left/Right step through the options
// (always accepted inside the row, even at the ends, so focus never jumps to
// the sidebar by surprise — use Back to leave the settings panel). Confirm
// advances to the next option (wrapping).
// options: [{ value, label }]; the owner binds currentIndex to the setting and
// writes it in onSelected(index, value).
SettingsRow {
    id: row
    property var options: []
    property int currentIndex: 0
    property bool wrap: false
    signal selected(int index, var value)

    readonly property string valueText: currentIndex >= 0 && currentIndex < options.length
                                        ? options[currentIndex].label : qsTr("Unknown")
    readonly property bool canDecrease: wrap ? options.length > 1 : currentIndex > 0
    readonly property bool canIncrease: wrap ? options.length > 1 : currentIndex < options.length - 1

    settingsHint: qsTr("Change")
    Accessible.role: Accessible.ComboBox
    Accessible.description: valueText

    function step(delta) {
        var n = options.length
        if (n === 0) return
        var i = currentIndex < 0 ? 0 : currentIndex + delta
        if (wrap) i = (i + n) % n
        else i = Math.max(0, Math.min(n - 1, i))
        if (i !== currentIndex) selected(i, options[i].value)
    }

    onActivated: {
        var n = options.length
        if (n > 0) selected((currentIndex + 1) % n, options[(currentIndex + 1) % n].value)
    }
    Keys.onLeftPressed: function(event) { event.accepted = true; step(-1) }
    Keys.onRightPressed: function(event) { event.accepted = true; step(1) }

    Row {
        spacing: Dimensions.spacingSm
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron-left"
            size: Dimensions.iconSizeSmall
            color: row.canDecrease ? (row.activeFocus ? Theme.accentPrimary : Theme.textSecondary) : Theme.textDisabled
            MouseArea { anchors.fill: parent; anchors.margins: -Dimensions.spacingSm; onClicked: { row.forceActiveFocus(); row.step(-1) } }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(implicitWidth, Dimensions.s(150))
            horizontalAlignment: Text.AlignHCenter
            text: row.valueText
            font.family: Typography.family
            font.pixelSize: Typography.body
            font.weight: Typography.weightSemiBold
            color: row.activeFocus ? Theme.textPrimary : Theme.textSecondary
        }
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron-right"
            size: Dimensions.iconSizeSmall
            color: row.canIncrease ? (row.activeFocus ? Theme.accentPrimary : Theme.textSecondary) : Theme.textDisabled
            MouseArea { anchors.fill: parent; anchors.margins: -Dimensions.spacingSm; onClicked: { row.forceActiveFocus(); row.step(1) } }
        }
    }
}
