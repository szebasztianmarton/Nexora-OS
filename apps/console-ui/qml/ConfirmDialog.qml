import QtQuick
import Nexora.Console

// Generic modal confirmation. Embed one instance per destructive action and call open();
// it traps focus while visible (Up/Down do nothing, Left/Right move between the two
// buttons) and defaults focus to Cancel, the safe choice. Escape/Back cancels from
// anywhere inside it, exactly like activating Cancel.
FocusScope {
    id: dialog
    objectName: "confirmDialog"
    anchors.fill: parent
    visible: false
    z: 50

    property string title: "Are you sure?"
    property string message: ""
    property string confirmText: "Confirm"
    property string cancelText: "Cancel"
    // Styles the confirm button as a destructive action (danger colour) rather than accent.
    property bool dangerous: false

    readonly property alias confirmButton: confirmButton
    readonly property alias cancelButton: cancelButton

    signal confirmed()
    signal cancelled()

    property Item restoreFocusItem: null

    function open() {
        restoreFocusItem = Window.activeFocusItem
        visible = true
        cancelButton.forceActiveFocus()
    }
    function close() {
        visible = false
        if (restoreFocusItem && restoreFocusItem.visible)
            restoreFocusItem.forceActiveFocus()
        restoreFocusItem = null
    }
    function accept() {
        close()
        confirmed()
    }
    function cancel() {
        close()
        cancelled()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
        opacity: 0.75
    }
    // Clicking outside the panel is the same as Cancel.
    MouseArea { anchors.fill: parent; onClicked: dialog.cancel() }

    Rectangle {
        id: panel
        objectName: "confirmDialogPanel"
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing * 4, 460)
        height: content.implicitHeight + Theme.spacing * 3
        radius: Theme.radius
        color: Theme.surface
        border.width: Theme.focusWidth
        border.color: Theme.focusRing
        // Absorb clicks inside the panel so they never fall through to the Cancel-on-outside-click area.
        MouseArea { anchors.fill: parent }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.spacing * 1.5
            spacing: Theme.spacing

            Text {
                width: parent.width
                text: dialog.title
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                wrapMode: Text.Wrap
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: dialog.message
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            Row {
                spacing: Theme.spacing
                NxButton {
                    id: cancelButton
                    objectName: "confirmDialogCancel"
                    text: dialog.cancelText
                    focus: true
                    KeyNavigation.right: confirmButton
                    onClicked: dialog.cancel()
                }
                NxButton {
                    id: confirmButton
                    objectName: "confirmDialogConfirm"
                    text: dialog.confirmText
                    kind: dialog.dangerous ? "danger" : "primary"
                    KeyNavigation.left: cancelButton
                    onClicked: dialog.accept()
                }
            }
        }
    }

    // Traps focus inside the dialog: nothing here propagates up to the screen behind it.
    Keys.onUpPressed: {}
    Keys.onDownPressed: {}
    Keys.onEscapePressed: dialog.cancel()
    Keys.onBackPressed: dialog.cancel()
}
