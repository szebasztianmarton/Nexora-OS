import QtQuick
import Nexora.Console

// Edits the user metadata of one game (display title and notes) with the on-screen keyboard.
// Modal: Up/Down choose a row, Accept edits it / runs it, Back (Esc, Backspace) cancels without
// saving; with unsaved edits it asks first ("Discard changes?", Cancel by default).
// Saving goes through LibraryController.setGameMetadata(); an empty title means "use the
// title derived from the file name". A failed save keeps the editor open with the edits (the
// library shows why in a notice). The panel never grows past the space it is given (large
// interface scale): its rows scroll to keep the focused one visible.
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
    // The values open() loaded: Back asks before throwing away anything that differs.
    property string loadedTitle: ""
    property string loadedNotes: ""
    readonly property bool hasChanges: title !== loadedTitle || notes !== loadedNotes
    property Item restoreFocusItem: null
    // Glass: the panel stays between the floating bars (the owning page passes its
    // chromeTop/chromeBottom, 0 when docked); the scrim still covers the whole page.
    property real chromeTop: 0
    property real chromeBottom: 0

    readonly property bool keyboardOpen: keyboard.visible
    readonly property var hints: keyboard.visible ? keyboard.hints
        : confirmDiscard.visible ? confirmDiscard.hints
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
        loadedTitle = title
        loadedNotes = notes
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
    // Back: closes at once when nothing changed, otherwise asks before discarding the edits.
    function cancel() {
        if (hasChanges)
            confirmDiscard.open()
        else
            close()
    }
    function save() {
        if (!library.setGameMetadata(gameId, title.trim(), notes.trim()))
            return false // stays open; LibraryController emitted the error notice
        close()
        saved()
        return true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.glassActive ? Theme.modalScrim : Theme.background
        opacity: Theme.glassActive ? 1.0 : 0.8
    }
    MouseArea { anchors.fill: parent; onClicked: editor.close() }

    Rectangle {
        id: panel
        objectName: "metadataPanel"
        x: (parent.width - width) / 2
        y: editor.chromeTop + (parent.height - editor.chromeTop - editor.chromeBottom - height) / 2
        width: Math.min(parent.width - Theme.space6 * 2, 720 * Theme.scale)
        height: Math.min(content.implicitHeight + Theme.space5 * 2,
                         parent.height - editor.chromeTop - editor.chromeBottom - Theme.space2 * 2)
        radius: Theme.glassActive ? Theme.radiusXL : Theme.radiusLarge
        color: Theme.glassActive ? "transparent" : Theme.surfaceFloating
        GlassSurface { anchors.fill: parent; z: -1; radius: panel.radius; dense: true }
        border.color: Theme.border
        border.width: Theme.glassActive ? 0 : 1
        MouseArea { anchors.fill: parent }

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: Theme.space5
            contentWidth: width
            contentHeight: content.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            function ensureVisible(item) {
                if (!item)
                    return
                const p = item.mapToItem(content, 0, 0)
                if (p.y < contentY)
                    contentY = Math.max(0, p.y - Theme.space2)
                else if (p.y + item.height > contentY + height)
                    contentY = Math.min(p.y + item.height - height + Theme.space2, Math.max(0, contentHeight - height))
            }

            Column {
                id: content
                width: flick.width
                spacing: Theme.space4

                Text {
                    textFormat: Text.PlainText
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Edit game details"
                    color: Theme.text
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                }
                Text {
                    textFormat: Text.PlainText
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Only the library entry changes; the game file is never renamed or modified."
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                NavColumn {
                    id: rows
                    width: parent.width
                    onLastFocusedChanged: flick.ensureVisible(lastFocused)
                    SettingRow {
                        objectName: "metadataTitle"
                        label: "Title"
                        value: editor.title.length > 0 ? editor.title : editor.originalTitle + " (from file name)"
                        description: "A / Enter: edit with the on-screen keyboard"
                        actionText: ""
                        activatable: true
                        onAccepted: keyboard.open("Title", editor.title.length > 0 ? editor.title : editor.originalTitle)
                    }
                    SettingRow {
                        objectName: "metadataNotes"
                        label: "Notes"
                        value: editor.notes.length > 0 ? editor.notes : "None"
                        valueColor: editor.notes.length > 0 ? Theme.text : Theme.textFaint
                        activatable: true
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
    }

    OnScreenKeyboard {
        id: keyboard
        chromeTop: editor.chromeTop
        chromeBottom: editor.chromeBottom
        onAccepted: (value) => {
            if (keyboard.title === "Title")
                editor.title = value.trim() === editor.originalTitle ? "" : value.trim()
            else
                editor.notes = value
        }
    }

    ConfirmDialog {
        id: confirmDiscard
        objectName: "confirmDiscardEdits"
        chromeTop: editor.chromeTop
        chromeBottom: editor.chromeBottom
        title: "Discard changes?"
        message: "The title and notes you edited are not saved."
        confirmText: "Discard"
        dangerous: true
        onConfirmed: editor.close()
    }

    // Modal: Back cancels, nothing else reaches the screen behind it.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace)
            editor.cancel()
        event.accepted = true
    }
}
