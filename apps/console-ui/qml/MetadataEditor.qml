import QtQuick
import Nexora.Console

// Edits the user metadata of one game (display title and notes) with the on-screen keyboard.
// Modal: Up/Down choose a row, Accept edits it / runs it, Back cancels without saving.
// Saving goes through LibraryController.setGameMetadata(); an empty title means "use the
// title derived from the file name".
FocusScope {
    id: editor
    objectName: "metadataEditor"
    required property LibraryController library
    anchors.fill: parent
    visible: false
    z: 55

    property real gameId: -1
    property string originalTitle: ""
    property string title: ""
    property string notes: ""
    property Item restoreFocusItem: null

    readonly property bool keyboardOpen: keyboard.visible
    readonly property var hints: keyboard.visible ? keyboard.hints
        : [{ button: "↑ ↓", action: "Move" }, { button: "A / Enter", action: "Edit" },
           { button: "B / Esc", action: "Cancel" }]

    signal saved()

    function open(id) {
        const details = library.gameDetails(id)
        if (!details.title)
            return false
        gameId = id
        originalTitle = details.originalTitle || ""
        // A title equal to the derived one is shown as "not overridden".
        title = details.title === originalTitle ? "" : details.title
        notes = details.notes || ""
        restoreFocusItem = Window.activeFocusItem
        visible = true
        rows.focusFirst()
        return true
    }
    function close() {
        visible = false
        if (restoreFocusItem && restoreFocusItem.visible)
            restoreFocusItem.forceActiveFocus()
        restoreFocusItem = null
    }
    function save() {
        const ok = library.setGameMetadata(gameId, title.trim(), notes.trim())
        close()
        if (ok)
            saved()
    }

    Rectangle { anchors.fill: parent; color: Theme.background; opacity: 0.8 }
    MouseArea { anchors.fill: parent; onClicked: editor.close() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space6 * 2, 720 * Theme.scale)
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
                text: "Edit game details"
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Only the library entry changes; the game file is never renamed or modified."
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }

            NavColumn {
                id: rows
                width: parent.width
                SettingRow {
                    objectName: "metadataTitle"
                    label: "Title"
                    value: editor.title.length > 0 ? editor.title : editor.originalTitle + " (from file name)"
                    description: "A / Enter: edit with the on-screen keyboard"
                    actionText: ""
                    onAccepted: keyboard.open("Title", editor.title.length > 0 ? editor.title : editor.originalTitle)
                }
                SettingRow {
                    objectName: "metadataNotes"
                    label: "Notes"
                    value: editor.notes.length > 0 ? editor.notes : "None"
                    valueColor: editor.notes.length > 0 ? Theme.text : Theme.textFaint
                    onAccepted: keyboard.open("Notes", editor.notes)
                }
                SettingRow {
                    objectName: "metadataResetTitle"
                    visible: editor.title.length > 0
                    label: "Use the title from the file name"
                    actionText: "Reset"
                    onAccepted: editor.title = ""
                }
                SettingRow {
                    objectName: "metadataSave"
                    label: "Save changes"
                    actionText: "Save"
                    onAccepted: editor.save()
                }
                SettingRow {
                    objectName: "metadataCancel"
                    label: "Discard changes"
                    actionText: "Cancel"
                    onAccepted: editor.close()
                }
            }
        }
    }

    OnScreenKeyboard {
        id: keyboard
        onAccepted: (value) => {
            if (keyboard.title === "Title")
                editor.title = value.trim() === editor.originalTitle ? "" : value.trim()
            else
                editor.notes = value
        }
    }

    // Modal: Back cancels, nothing else reaches the screen behind it.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back)
            editor.close()
        event.accepted = true
    }
}
