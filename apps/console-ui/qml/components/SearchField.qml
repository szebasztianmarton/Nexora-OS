import QtQuick
import Nexora.Core

// Search box. Physical keyboards type directly once focused. Confirm opens
// the on-screen keyboard (for controllers); Down/Escape return to results.
FocusScope {
    id: field

    property alias text: input.text
    property string placeholder: qsTr("Search")
    signal accepted()
    signal leaveRequested()

    implicitWidth: Dimensions.s(380)
    implicitHeight: Dimensions.buttonHeight
    activeFocusOnTab: true

    function openKeyboard() {
        Router.openKeyboard({ title: field.placeholder, text: input.text },
                            function(t) { input.text = t },
                            function(t) { input.text = t; field.accepted() })
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: field.activeFocus ? Theme.backgroundHover : Theme.backgroundElevated
        border.color: field.activeFocus ? Theme.borderStrong : Theme.borderSubtle
        border.width: 1
    }
    FocusFrame { shown: field.activeFocus; radiusBase: field.height / 2 }

    Icon {
        id: searchIcon
        name: "search"
        size: Dimensions.iconSizeSmall
        color: Theme.textMuted
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.spacingMd
        anchors.verticalCenter: parent.verticalCenter
    }
    TextInput {
        id: input
        focus: true
        anchors.left: searchIcon.right
        anchors.right: clearButton.visible ? clearButton.left : parent.right
        anchors.leftMargin: Dimensions.spacingSm
        anchors.rightMargin: Dimensions.spacingMd
        anchors.verticalCenter: parent.verticalCenter
        clip: true
        color: Theme.textPrimary
        selectionColor: Theme.accentSecondary
        font.family: Typography.family
        font.pixelSize: Typography.body
        Accessible.role: Accessible.EditableText
        Accessible.name: field.placeholder
        Keys.onReturnPressed: field.openKeyboard()
        Keys.onEnterPressed: field.openKeyboard()
        Keys.onEscapePressed: field.leaveRequested()
        Keys.onDownPressed: field.leaveRequested()
        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            visible: input.text === ""
            text: field.placeholder
            color: Theme.textMuted
            font: input.font
            elide: Text.ElideRight
        }
    }
    Text {
        id: clearButton
        visible: input.text !== ""
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.spacingMd
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Clear")
        font.family: Typography.family
        font.pixelSize: Typography.caption
        color: Theme.accentPrimary
        MouseArea { anchors.fill: parent; anchors.margins: -Dimensions.spacingSm; onClicked: input.text = "" }
    }
    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: input.forceActiveFocus()
    }
}
