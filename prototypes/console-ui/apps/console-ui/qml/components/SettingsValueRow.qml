import QtQuick

// Read-only "label: value" line. Focusable only when it has an action
// (`interactive: true`, then Confirm emits activated()). Paths elide in the
// middle so both the start and the file name stay visible.
SettingsRow {
    id: row
    property string value: ""
    property bool monospace: false
    property bool elideMiddle: false
    property color valueColor: Theme.textPrimary
    property bool wrapValue: false

    interactive: false
    settingsHint: qsTr("Open")
    Accessible.description: value

    Text {
        width: row.wrapValue ? row.trailingMaxWidth : Math.min(implicitWidth, row.trailingMaxWidth)
        horizontalAlignment: Text.AlignRight
        text: row.value
        elide: row.wrapValue ? Text.ElideNone : (row.elideMiddle ? Text.ElideMiddle : Text.ElideRight)
        wrapMode: row.wrapValue ? Text.WrapAnywhere : Text.NoWrap
        font.family: row.monospace ? "monospace" : Typography.family
        font.pixelSize: row.monospace ? Typography.caption : Typography.secondary
        font.weight: row.monospace ? Typography.weightRegular : Typography.weightMedium
        color: row.valueColor
    }
}
