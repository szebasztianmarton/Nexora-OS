import QtQuick
import Nexora.Console

// Controller-operable text entry (the idea of prototypes/console-ui's OnScreenKeyboard,
// rebuilt on this UI's Theme). The D-pad / arrow keys move across the keys, Accept types the
// focused key, Menu (Start) or the Done key finishes, Back cancels and keeps the original
// text. The Favorite button (Y / F on a pad) deletes one character, Search (X) types a space.
// A physical keyboard types straight into the text; Backspace deletes, Return finishes.
//
// Modal: open(title, text) traps focus until accepted(text) or cancelled() is emitted.
FocusScope {
    id: osk
    objectName: "onScreenKeyboard"
    anchors.fill: parent
    visible: false
    z: 60

    property string title: ""
    property string text: ""
    property int maxLength: 200
    property bool shifted: false
    property int row: 1
    property int column: 0
    property Item restoreFocusItem: null

    readonly property var rows: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l", "-"],
        ["z", "x", "c", "v", "b", "n", "m", ".", "'", ":"],
        ["SHIFT", "DEL", "SPACE", "CLEAR", "DONE"]
    ]
    readonly property var hints: [{ button: "← → ↑ ↓", action: "Choose key" }, { button: "A / Enter", action: "Type" },
                                  { button: "Y", action: "Delete" }, { button: "Start", action: "Done" },
                                  { button: "B / Esc", action: "Cancel" }]

    signal accepted(string text)
    signal cancelled()

    function open(titleText, initialText) {
        title = titleText
        text = initialText || ""
        shifted = false
        row = 1
        column = 0
        restoreFocusItem = Window.activeFocusItem
        visible = true
        keys.forceActiveFocus()
    }
    function close() {
        visible = false
        if (restoreFocusItem && restoreFocusItem.visible)
            restoreFocusItem.forceActiveFocus()
        restoreFocusItem = null
    }
    function finish() {
        const result = text
        close()
        accepted(result)
    }
    function cancel() {
        close()
        cancelled()
    }
    function label(key) {
        switch (key) {
        case "SHIFT": return shifted ? "⇧ ABC" : "⇧ abc"
        case "DEL": return "⌫ Delete"
        case "SPACE": return "Space"
        case "CLEAR": return "Clear"
        case "DONE": return "Done"
        default: return shifted ? key.toUpperCase() : key
        }
    }
    function insert(value) {
        if (text.length + value.length <= maxLength)
            text += value
    }
    function press(key) {
        switch (key) {
        case "SHIFT": shifted = !shifted; break
        case "DEL": text = text.slice(0, -1); break
        case "SPACE": insert(" "); break
        case "CLEAR": text = ""; break
        case "DONE": finish(); break
        default: insert(shifted ? key.toUpperCase() : key)
        }
    }
    // Moves the cursor; columns map proportionally between rows of different lengths.
    function move(dRow, dColumn) {
        if (dColumn !== 0) {
            column = Math.max(0, Math.min(rows[row].length - 1, column + dColumn))
            return
        }
        const target = Math.max(0, Math.min(rows.length - 1, row + dRow))
        if (target === row)
            return
        const ratio = (column + 0.5) / rows[row].length
        row = target
        column = Math.min(rows[row].length - 1, Math.floor(ratio * rows[row].length))
    }

    Rectangle { anchors.fill: parent; color: Theme.background; opacity: 0.85 }
    MouseArea { anchors.fill: parent; onClicked: osk.cancel() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space6 * 2, 980 * Theme.scale)
        height: content.implicitHeight + Theme.space5 * 2
        radius: Theme.radiusLarge
        color: Theme.surface
        border.color: Theme.border
        MouseArea { anchors.fill: parent }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space5
            spacing: Theme.space4

            Text {
                text: osk.title
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
            }
            Rectangle {
                width: parent.width
                height: Theme.controlHeight
                radius: Theme.radiusSmall
                color: Theme.surfaceRaised
                border.width: Theme.focusWidth
                border.color: Theme.focusRing
                Text {
                    objectName: "oskText"
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    verticalAlignment: Text.AlignVCenter
                    text: osk.text.length === 0 ? "Start typing…" : osk.text + "▏"
                    color: osk.text.length === 0 ? Theme.textFaint : Theme.text
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideLeft
                }
            }

            Column {
                id: keys
                objectName: "oskKeys"
                width: parent.width
                focus: true
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
                                readonly property bool current: keys.activeFocus && osk.row === keyRow.index
                                                                && osk.column === index
                                width: keys.width / keyRow.modelData.length
                                height: Math.round(58 * Theme.scale)
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: Theme.space1
                                    radius: Theme.radiusSmall
                                    color: keyItem.current ? Theme.accent
                                         : keyItem.modelData === "DONE" ? Theme.accentSoft
                                         : keyItem.modelData === "SHIFT" && osk.shifted ? Theme.accentSoft
                                         : Theme.surfaceRaised
                                    border.width: keyItem.current ? Theme.focusWidth : 1
                                    border.color: keyItem.current ? Theme.focusRing : Theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: osk.label(keyItem.modelData)
                                        color: keyItem.current && Theme.highContrast ? Theme.background : Theme.text
                                        font.pixelSize: Theme.fontBody
                                        font.bold: keyItem.current
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            osk.row = keyRow.index
                                            osk.column = keyItem.index
                                            keys.forceActiveFocus()
                                            osk.press(keyItem.modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Keys.onPressed: (event) => {
                    switch (event.key) {
                    case Qt.Key_Up: osk.move(-1, 0); break
                    case Qt.Key_Down: osk.move(1, 0); break
                    case Qt.Key_Left: osk.move(0, -1); break
                    case Qt.Key_Right: osk.move(0, 1); break
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        if (event.isAutoRepeat)
                            break
                        // Accept types the focused key; Ctrl+Return finishes from a keyboard.
                        if (event.modifiers & Qt.ControlModifier)
                            osk.finish()
                        else
                            osk.press(osk.rows[osk.row][osk.column])
                        break
                    case Qt.Key_Menu:
                    case Qt.Key_F1:
                        osk.finish()
                        break
                    case Qt.Key_Escape:
                    case Qt.Key_Back:
                        osk.cancel()
                        break
                    case Qt.Key_Backspace:
                    case Qt.Key_Favorites:
                        osk.press("DEL")
                        break
                    case Qt.Key_Search:
                        osk.press("SPACE")
                        break
                    default:
                        // Physical typing; tab switching and everything else stays inside.
                        if (event.text.length === 1 && event.text >= " " && event.text !== "\x7f")
                            osk.insert(event.text)
                    }
                    event.accepted = true
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.Wrap
                text: "A types · Y deletes · X space · Start finishes · B cancels"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}
