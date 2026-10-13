import QtQuick
import QtQuick.Controls
import Nexora.Console

// Settings: one vertical list of rows, grouped into General, Controllers, Display, Audio,
// Library, Emulators and System. Up/Down move, Left/Right change values, Accept activates
// actions (remap, remove folder, reset, power, backups…). Read-only information (device
// lists, output modes, storage, emulator health) is plain text, not a focus stop: nothing
// about it can be "operated".
FocusScope {
    id: screen
    objectName: "settingsScreen"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system
    required property UpdateController updates

    // Task 9 wires this to the real update panel; Settings only offers the entry point.
    signal openUpdatePanel()

    readonly property var focusedItem: Window.activeFocusItem
    readonly property bool inTextField: addDirField.activeFocus
    readonly property bool overlayOpen: testPanel.visible || confirmRestart.visible
                                        || confirmPowerOff.visible || confirmRestore.visible
    readonly property var hints: overlayOpen
        ? [{ button: "A / Enter", action: "Confirm" }, { button: "B / Esc", action: "Cancel" }]
        : inTextField
          ? [{ button: "Type", action: "Folder path" }, { button: "Enter", action: "Add folder" },
             { button: "B / Esc", action: "Leave field" }]
          : [{ button: "↑ ↓", action: "Move" }, { button: "← →", action: "Change" },
             { button: "A / Enter", action: "Select" }, { button: "L1 / R1", action: "Switch tab" }]

    // Action feedback (export/backup paths, restore count) shown under their own row only;
    // there is no settings-level property for any of these, so it is kept here.
    property string lastDiagnosticsPath: ""
    property string lastBackupPath: ""
    property int lastRestoredCount: -1
    property var backupList: []
    property int selectedBackupIndex: 0

    function refreshBackups() {
        backupList = screen.system.backups()
        if (selectedBackupIndex >= backupList.length)
            selectedBackupIndex = Math.max(0, backupList.length - 1)
    }
    function storageSummary() {
        const storage = (screen.system.hardware.storage || [])
        const lines = []
        for (const s of storage)
            lines.push(s.path + ": " + Theme.formatBytes(s.availableBytes) + " free of " + Theme.formatBytes(s.totalBytes))
        return lines.join("\n")
    }
    // Cycles the default emulator for |systemId| through "Automatic" + every enabled emulator.
    function cycleDefaultEmulator(systemId, step) {
        const count = screen.app.emulators.count
        if (count === 0)
            return
        const ids = [""]
        for (let i = 0; i < count; ++i)
            ids.push(screen.app.emulators.emulatorIdAt(i))
        const current = screen.settings.defaultEmulators[systemId] || ""
        let idx = ids.indexOf(current)
        if (idx < 0)
            idx = 0
        idx = ((idx + step) % ids.length + ids.length) % ids.length
        screen.settings.setDefaultEmulatorForSystem(systemId, ids[idx])
    }

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
    function refresh() {
        screen.system.refresh()
        screen.refreshBackups()
    }
    Component.onCompleted: { refresh(); focusDefault() }
    onVisibleChanged: if (visible) refresh()

    Keys.onDownPressed: step(true)
    Keys.onUpPressed: (event) => { event.accepted = step(false) }

    component SectionTitle: Text {
        width: parent ? parent.width : 0
        objectName: "sectionTitle_" + text
        topPadding: Theme.spacing
        color: Theme.accent
        font.pixelSize: Theme.fontSmall
        font.bold: true
        font.letterSpacing: 1.5
        font.capitalization: Font.AllUppercase
    }
    // A non-focusable informational line: read-only hardware/status data is not a control.
    component InfoText: Text {
        width: parent ? parent.width : 0
        wrapMode: Text.Wrap
        color: Theme.textFaint
        font.pixelSize: Theme.fontSmall
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
                radius: Theme.radiusSmall
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
                objectName: "settingTheme"
                label: "Theme"
                description: "High contrast uses a pure black background and a yellow focus ring"
                value: screen.settings.theme === "highContrast" ? "High contrast" : "Dark"
                adjustable: true
                onDecrement: screen.settings.theme = screen.settings.theme === "highContrast" ? "dark" : "highContrast"
                onIncrement: screen.settings.theme = screen.settings.theme === "highContrast" ? "dark" : "highContrast"
            }
            SettingRow {
                objectName: "settingUiScale"
                label: "Interface scale"
                description: "Resizes text, spacing and icons (75–150 %)"
                value: screen.settings.uiScale + " %"
                adjustable: true
                onDecrement: screen.settings.uiScale = Math.max(75, screen.settings.uiScale - 5)
                onIncrement: screen.settings.uiScale = Math.min(150, screen.settings.uiScale + 5)
            }
            SettingRow {
                objectName: "settingReducedMotion"
                label: "Reduce motion"
                description: "Turns off animations and transitions"
                value: screen.settings.reducedMotion ? "On" : "Off"
                valueColor: screen.settings.reducedMotion ? Theme.success : Theme.textMuted
                adjustable: true
                onDecrement: screen.settings.reducedMotion = !screen.settings.reducedMotion
                onIncrement: screen.settings.reducedMotion = !screen.settings.reducedMotion
                onAccepted: screen.settings.reducedMotion = !screen.settings.reducedMotion
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

            // --- Controllers -----------------------------------------------------------------
            SectionTitle { text: "Controllers" }
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
            SettingRow {
                objectName: "settingDeadzone"
                label: "Analog stick dead zone"
                description: "Ignores small stick drift near the centre"
                value: screen.settings.deadzone + " %"
                meter: screen.settings.deadzone / 100
                adjustable: true
                onDecrement: screen.settings.deadzone = Math.max(5, screen.settings.deadzone - 5)
                onIncrement: screen.settings.deadzone = Math.min(60, screen.settings.deadzone + 5)
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
            InfoText {
                text: {
                    const ic = screen.app.input
                    if (!ic || ic.devices.length === 0)
                        return "No controller connected — " + screen.app.inputStatus
                    const names = []
                    for (const d of ic.devices)
                        names.push(d.name + " (" + d.backend + ")")
                    return "Connected: " + names.join(", ")
                }
            }
            SettingRow {
                objectName: "testInputRow"
                label: "Test input"
                description: "Shows raw button presses and stick movement from your controller"
                actionText: "Test"
                onAccepted: testPanel.open()
            }

            // --- Display -----------------------------------------------------------------------
            SectionTitle { text: "Display" }
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
            InfoText {
                text: {
                    const outputs = (screen.system.hardware.outputs || [])
                    if (outputs.length === 0)
                        return "No display outputs detected"
                    const lines = []
                    for (const o of outputs) {
                        if (!o.connected)
                            continue
                        lines.push(o.name + ": " + (o.modes && o.modes.length > 0 ? o.modes.join(", ") : "no modes reported"))
                    }
                    return lines.length > 0 ? lines.join("\n") : "No connected outputs detected"
                }
            }

            // --- Audio -------------------------------------------------------------------------
            SectionTitle { text: "Audio" }
            InfoText {
                text: {
                    const audio = (screen.system.hardware.audio || [])
                    const names = []
                    for (const a of audio)
                        names.push(a.name)
                    return (names.length > 0 ? "Output devices: " + names.join(", ") : "No audio devices detected")
                           + "  ·  Selection not supported yet"
                }
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
            SettingRow {
                objectName: "settingMute"
                label: "Mute"
                value: screen.settings.muted ? "On" : "Off"
                valueColor: screen.settings.muted ? Theme.danger : Theme.textMuted
                adjustable: true
                onDecrement: screen.settings.muted = !screen.settings.muted
                onIncrement: screen.settings.muted = !screen.settings.muted
                onAccepted: screen.settings.muted = !screen.settings.muted
            }

            // --- Library -----------------------------------------------------------------------
            SectionTitle { text: "Library" }
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
                        radius: Theme.radiusSmall
                        color: addDirField.activeFocus ? Theme.surfaceHover : Theme.surface
                        border.width: addDirField.activeFocus ? Theme.focusWidth : 1
                        border.color: addDirField.activeFocus ? Theme.focusRing : Theme.border
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
                description: screen.library.scanning && screen.library.scanCurrentRoot.length > 0
                             ? "Scanning " + screen.library.scanCurrentRoot + "… (" + screen.library.scanFilesSeen + " files seen)"
                             : screen.library.lastScanSummary
                actionText: "Rescan"
                onAccepted: screen.library.refresh()
            }
            SettingRow {
                objectName: "purgeRow"
                visible: screen.library.missingCount > 0
                label: "Remove missing games"
                description: screen.library.missingCount + " game(s) whose files are gone are kept for their favorites "
                             + "and history; remove them for good"
                actionText: "Remove"
                labelColor: Theme.text
                requireConfirm: true
                confirmText: "Press A / Enter again to remove " + screen.library.missingCount + " missing game(s)"
                onAccepted: screen.library.purgeMissing()
            }
            SettingRow {
                objectName: "cancelScanRow"
                visible: screen.library.scanning
                label: "Cancel scan"
                actionText: "Cancel"
                onAccepted: screen.library.cancelScan()
            }
            SettingRow {
                objectName: "restoreRemovedRow"
                label: "Restore removed games"
                description: screen.lastRestoredCount < 0
                             ? "Brings back games you removed from the library (their files are untouched)"
                             : screen.lastRestoredCount === 0 ? "Nothing to restore"
                             : "Restored " + screen.lastRestoredCount + " game(s)"
                actionText: "Restore"
                onAccepted: screen.lastRestoredCount = screen.library.restoreRemovedGames()
            }

            // --- Emulators -----------------------------------------------------------------------
            SectionTitle { text: "Emulators" }
            InfoText {
                visible: screen.app.emulators.count === 0
                text: "No emulators configured yet."
            }
            Repeater {
                model: screen.app.emulators
                delegate: InfoText {
                    required property string name
                    required property string health
                    required property string healthDetail
                    color: health === "available" ? Theme.textMuted : Theme.warning
                    text: name + " — " + Theme.capitalize(health) + (healthDetail.length > 0 ? " (" + healthDetail + ")" : "")
                }
            }
            InfoText {
                visible: screen.library.systems.count === 0
                text: "No game systems detected yet — default emulators appear here once games are found."
            }
            Repeater {
                model: screen.library.systems
                delegate: SettingRow {
                    required property string systemId
                    required property string name
                    objectName: "defaultEmulator_" + systemId
                    label: "Default emulator — " + name
                    value: {
                        const id = screen.settings.defaultEmulators[systemId] || ""
                        return id.length > 0 ? screen.app.emulators.nameForId(id) : "Automatic"
                    }
                    description: screen.app.emulators.count > 0 ? "" : "No emulators configured"
                    adjustable: screen.app.emulators.count > 0
                    onDecrement: screen.cycleDefaultEmulator(systemId, -1)
                    onIncrement: screen.cycleDefaultEmulator(systemId, 1)
                }
            }

            // --- System ----------------------------------------------------------------------
            SectionTitle { text: "System" }
            InfoText {
                text: "Nexora OS " + screen.app.version + "  ·  build " + screen.system.buildId + "  ·  " + screen.system.platform
                      + (screen.storageSummary().length > 0 ? "\n" + screen.storageSummary() : "")
            }
            SettingRow {
                objectName: "softwareUpdateRow"
                label: "Software update"
                value: screen.updates.supported ? Theme.capitalize(screen.updates.state) : "Not available on this build"
                description: screen.updates.supported ? "Current version " + screen.updates.currentVersion : ""
                actionText: "Details"
                onAccepted: screen.openUpdatePanel()
            }
            SettingRow {
                objectName: "exportDiagnosticsRow"
                label: "Export diagnostics"
                description: screen.lastDiagnosticsPath.length > 0 ? "Saved to " + screen.lastDiagnosticsPath
                             : "A text report for troubleshooting, with sensitive paths redacted"
                actionText: "Export"
                onAccepted: screen.lastDiagnosticsPath = screen.system.exportDiagnostics()
            }
            SettingRow {
                objectName: "createBackupRow"
                label: "Create backup"
                description: screen.lastBackupPath.length > 0 ? "Saved to " + screen.lastBackupPath
                             : "Includes settings, library and saves"
                actionText: "Create"
                onAccepted: { screen.lastBackupPath = screen.system.createBackup(true); screen.refreshBackups() }
            }
            SettingRow {
                objectName: "restoreBackupRow"
                label: "Restore backup"
                value: screen.backupList.length === 0 ? "No backups yet"
                       : (screen.selectedBackupIndex + 1) + " / " + screen.backupList.length + " · "
                         + screen.backupList[screen.selectedBackupIndex].split("/").pop()
                description: screen.backupList.length > 1 ? "← → choose a backup" : ""
                adjustable: screen.backupList.length > 1
                onDecrement: screen.selectedBackupIndex
                    = (screen.selectedBackupIndex - 1 + screen.backupList.length) % screen.backupList.length
                onIncrement: screen.selectedBackupIndex = (screen.selectedBackupIndex + 1) % screen.backupList.length
                onAccepted: if (screen.backupList.length > 0) confirmRestore.open()
            }
            SettingRow {
                objectName: "clearArtworkCacheRow"
                label: "Clear artwork cache"
                description: "Regenerated automatically; does not touch saves or the library"
                actionText: "Clear"
                requireConfirm: true
                confirmText: "Press A / Enter again to clear the artwork cache"
                onAccepted: screen.system.clearArtworkCache()
            }
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
                onAccepted: confirmRestart.open()
            }
            SettingRow {
                objectName: "powerOffRow"
                visible: screen.system.powerSupported
                label: "Power off"
                actionText: "Power off"
                labelColor: Theme.danger
                onAccepted: confirmPowerOff.open()
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

    ConfirmDialog {
        id: confirmRestart
        title: "Restart Nexora?"
        message: "The system will restart immediately. Any running emulator will be stopped."
        confirmText: "Restart"
        dangerous: true
        onConfirmed: screen.system.reboot()
    }
    ConfirmDialog {
        id: confirmPowerOff
        title: "Power off?"
        message: "The system will shut down immediately. Any running emulator will be stopped."
        confirmText: "Power off"
        dangerous: true
        onConfirmed: screen.system.powerOff()
    }
    ConfirmDialog {
        id: confirmRestore
        title: "Restore this backup?"
        message: "Current settings, library and (if the backup includes them) saves will be replaced. This cannot be undone from within Nexora."
        confirmText: "Restore"
        dangerous: true
        onConfirmed: {
            screen.system.restoreBackup(screen.backupList[screen.selectedBackupIndex])
            screen.refreshBackups()
        }
    }
    InputTestPanel {
        id: testPanel
        app: screen.app
    }
}
