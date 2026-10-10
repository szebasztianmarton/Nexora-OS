import QtQuick
import QtQuick.Controls
import Nexora.Console

// Settings: one vertical list of rows. Up/Down move, Left/Right change values,
// Accept activates actions (remap, remove folder, reset, power).
FocusScope {
    id: screen
    objectName: "settingsScreen"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system

    readonly property var focusedItem: Window.activeFocusItem
    readonly property bool inTextField: addDirField.activeFocus
    readonly property var hints: inTextField
        ? [{ button: "Type", action: "Folder path" }, { button: "Enter", action: "Add folder" },
           { button: "B / Esc", action: "Leave field" }]
        : [{ button: "↑ ↓", action: "Move" }, { button: "← →", action: "Change" },
           { button: "A / Enter", action: "Select" }, { button: "L1 / R1", action: "Switch tab" }]

    function focusDefault() { languageRow.focus = true }
    function focusAddFolder() { addDirField.forceActiveFocus() }

    function isInside(item) {
        for (let p = item; p; p = p.parent) {
            if (p === column)
                return true
        }
        return false
    }
    // Moves along the focus chain but never leaves this screen.
    function step(forward) {
        let item = focusedItem
        if (!item || !isInside(item))
            return false
        for (let guard = 0; guard < 200; ++guard) {
            item = item.nextItemInFocusChain(forward)
            if (!item || !isInside(item))
                return false
            if (item.visible && item.enabled && item.activeFocusOnTab) {
                // The chain wraps around at the ends; do not.
                const from = focusedItem.mapToItem(column, 0, 0).y
                const to = item.mapToItem(column, 0, 0).y
                if (forward ? to < from : to > from)
                    return false
                item.forceActiveFocus()
                return true
            }
        }
        return false
    }
    function ensureVisible(item) {
        if (!item || !isInside(item))
            return
        const p = item.mapToItem(flick.contentItem, 0, 0)
        const top = p.y - 48
        const bottom = p.y + item.height + Theme.spacing * 3
        if (top < flick.contentY)
            flick.contentY = Math.max(0, top)
        else if (bottom > flick.contentY + flick.height)
            flick.contentY = Math.min(bottom - flick.height, Math.max(0, flick.contentHeight - flick.height))
    }
    function nextButton(current, delta) {
        const names = Theme.buttonNames
        const i = names.indexOf(current)
        return names[((i < 0 ? 0 : i + delta) + names.length) % names.length]
    }

    // Rows built from lists (mapping, folders) are recreated when the list changes; these
    // remember which one had focus so it can be restored.
    property string lastMappingAction
    function refocusDirectory(i) {
        if (dirRepeater.count > 0)
            dirRepeater.itemAt(Math.min(i, dirRepeater.count - 1)).forceActiveFocus()
        else
            addDirButton.forceActiveFocus()
    }

    onFocusedItemChanged: {
        if (!focusedItem || !isInside(focusedItem))
            return
        lastMappingAction = focusedItem.mappingAction !== undefined ? focusedItem.mappingAction : ""
        if (visible)
            ensureVisible(focusedItem)
    }
    Component.onCompleted: focusDefault()

    Keys.onDownPressed: step(true)
    Keys.onUpPressed: (event) => { event.accepted = step(false) }

    component SectionTitle: Text {
        width: parent ? parent.width : 0
        topPadding: Theme.spacing
        color: Theme.accent
        font.pixelSize: Theme.fontSmall
        font.bold: true
        font.letterSpacing: 1.5
        font.capitalization: Font.AllUppercase
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + Theme.spacing * 4
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Behavior on contentY { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

        Column {
            id: column
            width: Math.min(flick.width - Theme.spacing * 4, 980)
            x: (flick.width - width) / 2
            y: Theme.spacing * 1.5
            spacing: 8

            Text {
                text: "Settings"
                color: Theme.text
                font.pixelSize: Theme.fontHero
                font.bold: true
            }
            Text {
                width: parent.width
                text: "Changes are saved immediately to " + screen.settings.userFile
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideMiddle
            }

            Rectangle {
                objectName: "settingsIssues"
                visible: screen.settings.lastError.length > 0 || screen.settings.issues.length > 0
                width: parent.width
                height: issuesText.implicitHeight + 24
                radius: 10
                color: Qt.rgba(1, 0.39, 0.45, 0.10)
                border.color: Theme.danger
                Text {
                    id: issuesText
                    anchors.fill: parent
                    anchors.margins: 12
                    wrapMode: Text.Wrap
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                    text: (screen.settings.lastError.length > 0 ? "⚠ " + screen.settings.lastError : "")
                          + (screen.settings.lastError.length > 0 && screen.settings.issues.length > 0 ? "\n" : "")
                          + screen.settings.issues.map(i => "• " + i).join("\n")
                }
            }

            // --- General -------------------------------------------------------------------
            SectionTitle { text: "General" }
            SettingRow {
                id: languageRow
                objectName: "settingLanguage"
                label: "Language"
                value: screen.settings.language === "en" ? "English" : screen.settings.language
                description: screen.settings.languages.length > 1 ? "" : "More languages will follow"
                adjustable: true
                onDecrement: screen.settings.cycle("ui.language", -1)
                onIncrement: screen.settings.cycle("ui.language", 1)
            }
            SettingRow {
                objectName: "settingHints"
                label: "Show control hints"
                description: "The bar at the bottom listing the buttons you can press"
                value: screen.settings.showHints ? "On" : "Off"
                valueColor: screen.settings.showHints ? Theme.success : Theme.textMuted
                adjustable: true
                onDecrement: screen.settings.showHints = !screen.settings.showHints
                onIncrement: screen.settings.showHints = !screen.settings.showHints
                onAccepted: screen.settings.showHints = !screen.settings.showHints
            }
            SettingRow {
                objectName: "settingSort"
                label: "Default game sort"
                description: "Used when Nexora starts"
                value: Theme.sortLabel(screen.settings.sortOrder)
                adjustable: true
                onDecrement: screen.settings.cycle("library.sortOrder", -1)
                onIncrement: screen.settings.cycle("library.sortOrder", 1)
            }

            // --- Display & audio ----------------------------------------------------------
            SectionTitle { text: "Display & audio" }
            SettingRow {
                objectName: "settingDisplayMode"
                label: "Display mode"
                value: Theme.capitalize(screen.settings.displayMode)
                adjustable: true
                onDecrement: screen.settings.cycle("display.mode", -1)
                onIncrement: screen.settings.cycle("display.mode", 1)
            }
            SettingRow {
                objectName: "settingResolution"
                label: "Resolution"
                description: "Window size on start-up"
                value: screen.settings.resolution === "auto" ? "Automatic" : screen.settings.resolution.replace("x", " × ")
                adjustable: true
                onDecrement: screen.settings.cycle("display.resolution", -1)
                onIncrement: screen.settings.cycle("display.resolution", 1)
            }
            SettingRow {
                objectName: "settingVolume"
                label: "Volume"
                description: screen.settings.volumeSupported ? "" : "Preference only on desktop (system volume is not changed)"
                value: screen.settings.volume + " %"
                meter: screen.settings.volume / 100
                adjustable: true
                onDecrement: screen.settings.volume = Math.max(0, screen.settings.volume - 5)
                onIncrement: screen.settings.volume = Math.min(100, screen.settings.volume + 5)
            }

            // --- Controller ----------------------------------------------------------------
            SectionTitle { text: "Controller" }
            SettingRow {
                objectName: "settingBackend"
                label: "Controller backend"
                description: "Applies after restart · currently " + screen.app.inputBackend
                value: screen.settings.inputBackend === "sdl3" ? "SDL3"
                     : screen.settings.inputBackend === "evdev" ? "evdev"
                     : Theme.capitalize(screen.settings.inputBackend)
                adjustable: true
                onDecrement: screen.settings.cycle("input.backend", -1)
                onIncrement: screen.settings.cycle("input.backend", 1)
            }
            Repeater {
                model: screen.settings.mappingRows
                onItemAdded: (index, item) => {
                    if (item.mappingAction === screen.lastMappingAction && screen.lastMappingAction.length > 0)
                        item.forceActiveFocus()
                }
                delegate: SettingRow {
                    id: mappingRow
                    required property var modelData
                    readonly property string mappingAction: modelData.action
                    property bool noController: false
                    readonly property bool capturing: screen.settings.capturingAction === modelData.action
                    objectName: "mapping_" + modelData.action
                    label: modelData.label
                    description: capturing ? "Press a button on your controller… (5 s)  ·  Esc or Confirm again cancels"
                               : noController ? "No controller input available — use ← → to choose a button"
                               : mappingRow.activeFocus ? "A / Enter: press a button to assign · ← →: choose" : ""
                    value: capturing ? "Waiting…" : Theme.buttonLabel(modelData.button)
                    valueColor: capturing ? Theme.warning : Theme.text
                    adjustable: true
                    onDecrement: screen.settings.remap(modelData.action, screen.nextButton(modelData.button, -1))
                    onIncrement: screen.settings.remap(modelData.action, screen.nextButton(modelData.button, 1))
                    onAccepted: {
                        if (capturing)
                            screen.settings.cancelRemap()
                        else
                            noController = !screen.settings.beginRemap(modelData.action)
                    }
                    onActiveFocusChanged: if (!activeFocus) noController = false
                    Keys.onEscapePressed: (event) => {
                        if (capturing)
                            screen.settings.cancelRemap()
                        else
                            event.accepted = false
                    }
                }
            }

            // --- Library ---------------------------------------------------------------------
            SectionTitle { text: "Library folders" }
            Text {
                visible: screen.library.directories.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: "No folders yet. Type the full path of a folder with game files below and select Add."
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }
            Repeater {
                id: dirRepeater
                model: screen.library.directories
                delegate: SettingRow {
                    required property string modelData
                    required property int index
                    objectName: "libraryDir_" + index
                    label: modelData
                    actionText: "Remove"
                    requireConfirm: true
                    confirmText: "Press A / Enter again to remove this folder from the library"
                    onAccepted: {
                        const i = index
                        if (screen.library.removeDirectory(modelData))
                            Qt.callLater(screen.refocusDirectory, i)
                    }
                }
            }
            Item {
                width: parent.width
                height: 52
                TextField {
                    id: addDirField
                    objectName: "addDirField"
                    anchors.left: parent.left
                    anchors.right: addDirButton.left
                    anchors.rightMargin: 12
                    height: parent.height
                    placeholderText: "/path/to/your/games"
                    placeholderTextColor: Theme.textFaint
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.text
                    font.pixelSize: Theme.fontBody
                    leftPadding: 16
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        radius: 10
                        color: addDirField.activeFocus ? Theme.surfaceHover : Theme.surface
                        border.width: addDirField.activeFocus ? Theme.focusWidth : 1
                        border.color: addDirField.activeFocus ? Theme.accent : Theme.border
                    }
                    function submit() {
                        if (screen.library.addDirectory(text))
                            text = ""
                    }
                    Keys.onReturnPressed: submit()
                    Keys.onEnterPressed: submit()
                    Keys.onEscapePressed: addDirButton.forceActiveFocus()
                    Keys.onBackPressed: addDirButton.forceActiveFocus()
                    Keys.onRightPressed: (event) => {
                        if (cursorPosition === length && selectedText.length === 0)
                            addDirButton.forceActiveFocus()
                        else
                            event.accepted = false
                    }
                }
                NxButton {
                    id: addDirButton
                    objectName: "addDirButton"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: "+"
                    text: "Add folder"
                    onClicked: {
                        if (addDirField.text.trim().length === 0)
                            addDirField.forceActiveFocus()
                        else
                            addDirField.submit()
                    }
                    Keys.onLeftPressed: addDirField.forceActiveFocus()
                }
            }
            SettingRow {
                objectName: "rescanRow"
                label: screen.library.scanning ? "Scanning library…" : "Rescan library"
                description: screen.library.lastScanSummary
                actionText: "Rescan"
                onAccepted: screen.library.refresh()
            }

            // --- System ----------------------------------------------------------------------
            SectionTitle { text: "System" }
            SettingRow {
                objectName: "resetRow"
                label: "Reset preferences"
                description: "Restore every setting above to its default value"
                actionText: "Reset"
                labelColor: Theme.text
                requireConfirm: true
                confirmText: "Press A / Enter again to reset all preferences"
                onAccepted: screen.settings.resetToDefaults()
            }
            SettingRow {
                objectName: "rebootRow"
                visible: screen.system.powerSupported
                label: "Restart"
                actionText: "Restart"
                requireConfirm: true
                confirmText: "Press A / Enter again to restart the system"
                onAccepted: screen.system.reboot()
            }
            SettingRow {
                objectName: "powerOffRow"
                visible: screen.system.powerSupported
                label: "Power off"
                actionText: "Power off"
                labelColor: Theme.danger
                requireConfirm: true
                confirmText: "Press A / Enter again to power off"
                onAccepted: screen.system.powerOff()
            }
            Text {
                visible: !screen.system.powerSupported
                width: parent.width
                wrapMode: Text.Wrap
                text: "Restart and power off are available on the Nexora console build (platform: "
                      + screen.system.platform + ")."
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
            }
            Text {
                visible: screen.system.lastError.length > 0
                width: parent.width
                wrapMode: Text.Wrap
                text: screen.system.lastError
                color: Theme.danger
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}
