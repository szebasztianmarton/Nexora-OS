import QtQuick
import QtQuick.Layouts

// Controller-friendly text entry. D-pad moves across keys, Confirm types,
// Backspace / controller West deletes, Options (Start / M) finishes, Back closes
// (keeping the text typed so far). Physical keyboards can type directly.
ModalBase {
    id: osk
    property string text: ""
    signal textEdited(string text)
    signal done(string text)
    panelWidth: Dimensions.s(980)
    initialFocusItem: grid

    // Rows of keys; the last row has four wide keys.
    readonly property var rows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["q","w","e","r","t","y","u","i","o","p"],
        ["a","s","d","f","g","h","j","k","l","-"],
        ["z","x","c","v","b","n","m",".","'",":"],
        ["⌫","SPACE","CLEAR","DONE"]
    ]
    property int row: 0
    property int column: 0

    // Moves the cursor, mapping columns proportionally between rows of
    // different lengths.
    function _move(dRow, dColumn) {
        if (dColumn !== 0) {
            column = Math.max(0, Math.min(rows[row].length - 1, column + dColumn))
            return
        }
        var target = Math.max(0, Math.min(rows.length - 1, row + dRow))
        if (target === row) return
        var ratio = (column + 0.5) / rows[row].length
        row = target
        column = Math.min(rows[row].length - 1, Math.floor(ratio * rows[row].length))
    }

    function _type(k) {
        if (k === "⌫") text = text.slice(0, -1)
        else if (k === "SPACE") text += " "
        else if (k === "CLEAR") text = ""
        else if (k === "DONE") { _finish(); return }
        else text += k
        textEdited(text)
    }
    function _finish() { done(text); close() }

    ColumnLayout {
        width: parent.width
        spacing: Dimensions.spacingLg
        Text {
            text: osk.title
            font.family: Typography.family
            font.pixelSize: Typography.sectionTitle
            font.weight: Typography.weightSemiBold
            color: Theme.textPrimary
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Dimensions.buttonHeight
            radius: Dimensions.radiusMd
            color: Theme.backgroundSecondary
            border.color: Theme.borderStrong
            Text {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.spacingMd
                verticalAlignment: Text.AlignVCenter
                text: osk.text === "" ? qsTr("Start typing…") : osk.text + "▏"
                color: osk.text === "" ? Theme.textMuted : Theme.textPrimary
                font.family: Typography.family
                font.pixelSize: Typography.body
                elide: Text.ElideLeft
            }
        }
        Column {
            id: grid
            Layout.fillWidth: true
            focus: true
            spacing: 0
            Repeater {
                model: osk.rows
                delegate: Row {
                    id: keyRow
                    required property var modelData
                    required property int index
                    Repeater {
                        model: keyRow.modelData
                        delegate: Item {
                            id: keyItem
                            required property string modelData
                            required property int index
                            readonly property bool current: grid.activeFocus && osk.row === keyRow.index && osk.column === index
                            width: grid.width / keyRow.modelData.length
                            height: Dimensions.s(64)
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: Dimensions.s(4)
                                radius: Dimensions.radiusSm
                                color: keyItem.current ? Theme.accentPrimary
                                     : (keyItem.modelData === "DONE" ? Theme.accentPrimarySoft : Theme.backgroundHover)
                                Text {
                                    anchors.centerIn: parent
                                    text: keyItem.modelData === "SPACE" ? qsTr("Space")
                                        : keyItem.modelData === "CLEAR" ? qsTr("Clear")
                                        : keyItem.modelData === "DONE" ? qsTr("Done") : keyItem.modelData
                                    font.family: Typography.family
                                    font.pixelSize: Typography.body
                                    font.weight: Typography.weightMedium
                                    color: keyItem.current ? Theme.onAccent : Theme.textPrimary
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { osk.row = keyRow.index; osk.column = keyItem.index; osk._type(keyItem.modelData) }
                                }
                            }
                        }
                    }
                }
            }
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Up) { osk._move(-1, 0); event.accepted = true }
                else if (event.key === Qt.Key_Down) { osk._move(1, 0); event.accepted = true }
                else if (event.key === Qt.Key_Left) { osk._move(0, -1); event.accepted = true }
                else if (event.key === Qt.Key_Right) { osk._move(0, 1); event.accepted = true }
                else if (Input.isConfirm(event)) {
                    if (!event.isAutoRepeat) osk._type(osk.rows[osk.row][osk.column])
                    event.accepted = true
                } else if (event.key === Qt.Key_Menu) { osk._finish(); event.accepted = true }
                else if (event.key === Qt.Key_Backspace
                         || (event.key === Qt.Key_F && event.text === "")) {   // controller West
                    osk._type("⌫"); event.accepted = true
                }
                else if (event.text.length === 1 && event.text > " " && !event.isAutoRepeat) {
                    // Physical keyboard typing goes straight into the text.
                    osk.text += event.text; osk.textEdited(osk.text); event.accepted = true
                }
            }
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: qsTr("Confirm types · West button deletes · Start finishes · Back closes")
            font.family: Typography.family
            font.pixelSize: Typography.caption
            color: Theme.textMuted
        }
    }
}
