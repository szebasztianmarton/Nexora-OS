import QtQuick
import Nexora.Console

// Generic modal confirmation. Embed one instance per destructive action and call open();
// it traps focus while visible (Left/Right move between the two buttons, every other key -
// L1/R1, Menu, Search, "/" included - stops here) and defaults focus to Cancel, the safe
// choice. Escape/Back cancels from anywhere inside it, exactly like activating Cancel. If it
// is hidden from outside while open (its page or layer goes away), it cancels itself
// instead of staying armed for later.
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
    // Glass: the floating bars cover the top and bottom of the page; the panel stays between
    // them (the owning page passes its chromeTop/chromeBottom, 0 when docked). The scrim still
    // covers the whole page.
    property real chromeTop: 0
    property real chromeBottom: 0

    readonly property alias confirmButton: confirmButton
    // Hint bar entries while open; every owner lists these so the wording cannot drift. Focus
    // starts on Cancel, so Accept selects the focused button rather than confirming.
    readonly property var hints: [{ button: "← →", action: "Choose" }, { button: "A / Enter", action: "Select" },
                                  { button: "B / Esc", action: "Cancel" }]
    readonly property alias cancelButton: cancelButton

    signal confirmed()
    signal cancelled()

    property Item restoreFocusItem: null
    property bool isOpen: false

    function open() {
        restoreFocusItem = Window.activeFocusItem
        isOpen = true
        visible = true
        cancelButton.forceActiveFocus()
    }
    function close() {
        isOpen = false
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
        color: Theme.glassActive ? Theme.modalScrim : Theme.background
        opacity: Theme.glassActive ? 1.0 : 0.75
    }
    // Clicking outside the panel is the same as Cancel.
    MouseArea { anchors.fill: parent; onClicked: dialog.cancel() }

    Rectangle {
        id: panel
        objectName: "confirmDialogPanel"
        x: (parent.width - width) / 2
        y: dialog.chromeTop + (parent.height - dialog.chromeTop - dialog.chromeBottom - height) / 2
        width: Math.min(parent.width - Theme.spacing * 4, 460 * Theme.scale)
        height: content.implicitHeight + Theme.spacing * 3
        radius: Theme.glassActive ? Theme.radiusXL : Theme.radius
        color: Theme.glassActive ? "transparent" : Theme.surfaceFloating
        GlassSurface { anchors.fill: parent; z: -1; radius: panel.radius; dense: true }
        // A plain outline (glass: the rim replaces it): the focused button carries the only focus ring.
        border.width: Theme.glassActive ? 0 : 1
        border.color: Theme.border
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
                textFormat: Text.PlainText // titles can name untrusted items (update versions, games)
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                wrapMode: Text.Wrap
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: dialog.message
                textFormat: Text.PlainText
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }
            // Right-aligned (Confirm rightmost, Cancel to its left); when both do not fit on
            // one line, Cancel wraps below instead of overflowing the panel (a fallback; only then
            // do Up/Down move between them).
            Flow {
                width: parent.width
                spacing: Theme.spacing
                layoutDirection: Qt.RightToLeft
                NxButton {
                    id: confirmButton
                    objectName: "confirmDialogConfirm"
                    text: dialog.confirmText
                    kind: dialog.dangerous ? "danger" : "primary"
                    KeyNavigation.left: cancelButton
                    KeyNavigation.down: cancelButton.y > y ? cancelButton : null
                    onClicked: dialog.accept()
                }
                NxButton {
                    id: cancelButton
                    objectName: "confirmDialogCancel"
                    text: dialog.cancelText
                    focus: true
                    KeyNavigation.right: confirmButton
                    KeyNavigation.up: y > confirmButton.y ? confirmButton : null
                    onClicked: dialog.cancel()
                }
            }
        }
    }

    // The page or layer around it was hidden (tab switch, game started, panel closed).
    onVisibleChanged: if (!visible && isOpen) dialog.cancel()

    // Traps focus inside the dialog: nothing here propagates up to the screen behind it.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace)
            dialog.cancel()
        event.accepted = true
    }
}
