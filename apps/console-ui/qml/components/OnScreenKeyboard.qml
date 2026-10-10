import QtQuick
import QtQuick.Layouts

// Controller-friendly text entry. D-pad moves across keys, Confirm types,
// Secondary (X / F) deletes, Options (Start / M) finishes, Back closes
// (keeping the text typed so far). Physical keyboards can type directly.
ModalBase {
    id: osk
    property string text: ""
    signal textEdited(string text)
    signal done(string text)
    panelWidth: Dimensions.s(980)
    initialFocusItem: grid

    readonly property var keys: [
        "1","2","3","4","5","6","7","8","9","0",
        "q","w","e","r","t","y","u","i","o","p",
        "a","s","d","f","g","h","j","k","l","-",
        "z","x","c","v","b","n","m",".","'",":",
        "⌫","SPACE","CLEAR","DONE"
    ]

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
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.preferredHeight: cellHeight * 5
            focus: true
            interactive: false
            keyNavigationEnabled: true
            cellWidth: width / 10
            cellHeight: Dimensions.s(64)
            model: osk.keys
            delegate: Item {
                id: keyItem
                required property string modelData
                required property int index
                readonly property bool wide: index >= 40
                width: wide ? grid.cellWidth * 2.5 : grid.cellWidth
                height: grid.cellHeight
                // Wide bottom-row keys are laid out manually across the row.
                x: wide ? (index - 40) * grid.cellWidth * 2.5 : (index % 10) * grid.cellWidth
                readonly property bool current: GridView.isCurrentItem && grid.activeFocus
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
                    MouseArea { anchors.fill: parent; onClicked: { grid.currentIndex = keyItem.index; osk._type(keyItem.modelData) } }
                }
            }
            // The bottom row has 4 wide keys; map horizontal moves accordingly.
            Keys.onPressed: function(event) {
                var i = currentIndex
                if (Input.isConfirm(event) && !event.isAutoRepeat) { osk._type(osk.keys[i]); event.accepted = true }
                else if (Input.isSecondary(event) && event.key !== Qt.Key_F) { osk._type("⌫"); event.accepted = true }
                else if (event.key === Qt.Key_Menu) { osk._finish(); event.accepted = true }
                else if (event.key === Qt.Key_Down && i >= 30 && i < 40) { currentIndex = 40 + Math.min(3, Math.floor((i - 30) / 2.5)); event.accepted = true }
                else if (event.key === Qt.Key_Up && i >= 40) { currentIndex = 30 + Math.round((i - 40) * 2.5); event.accepted = true }
                else if (event.key === Qt.Key_Right && i >= 40) { if (i < 43) currentIndex = i + 1; event.accepted = true }
                else if (event.key === Qt.Key_Left && i >= 40) { if (i > 40) currentIndex = i - 1; event.accepted = true }
                else if (event.key === Qt.Key_Down && i >= 40) { event.accepted = true }
                else if (event.text.length === 1 && event.text >= " " && !event.isAutoRepeat
                         && event.key !== Qt.Key_Space && !Input.isConfirm(event)) {
                    // Physical keyboard typing goes straight into the text.
                    osk.text += event.text; osk.textEdited(osk.text); event.accepted = true
                } else if (event.key === Qt.Key_Backspace) {
                    osk._type("⌫"); event.accepted = true
                }
            }
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
            text: qsTr("Confirm types · Back closes · Start/M finishes")
            font.family: Typography.family
            font.pixelSize: Typography.caption
            color: Theme.textMuted
        }
    }
}
