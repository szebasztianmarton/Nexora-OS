import QtQuick
import Nexora.Console

// In-game quick menu: opens over a running emulator with the controller's Guide button or by
// holding Start for a second (GameBarController.quickMenuOpen), or Menu / F1 on the running
// overlay from a keyboard. Items: Return to game, Suspend and go Home (Quick Resume: saves
// a state through the emulator's save-state adapter, then closes it; explains why when it
// is not possible), Pause / Resume (only when the emulator supports it), controller status,
// volume and mute, a Display settings page, a System info page and Exit game (asks for
// confirmation first: unsaved emulator progress may be lost).
//
// On EGLFS the emulator owns the display; the menu becomes visible as soon as the shell gets
// the screen back (Main.qml raises the window when the menu opens, like the game bar).
FocusScope {
    id: menu
    objectName: "quickMenu"
    required property GameBarController gameBar
    required property AppController app
    required property SettingsController settings
    required property SystemController system
    property GameSessionManager sessions: null
    readonly property bool sessionActive: sessions !== null && sessions.active.id !== undefined
    readonly property bool sessionWorking: sessions !== null && sessions.working

    property string page: "main" // main, display, info
    readonly property bool confirmOpen: confirmExit.visible
    readonly property var hints: confirmExit.visible
        ? [{ button: "← →", action: "Choose" }, { button: "A / Enter", action: "Confirm" },
           { button: "B / Esc", action: "Cancel" }]
        : [{ button: "↑ ↓", action: "Move" }, { button: "← →", action: "Adjust" },
           { button: "A / Enter", action: "Select" },
           { button: "B / Guide", action: page === "main" ? "Back to game" : "Back" }]

    visible: gameBar.quickMenuOpen
    z: 30

    function reset() {
        page = "main"
        Qt.callLater(focusDefault)
    }
    function focusDefault() {
        if (!visible)
            return
        if (page === "display")
            displayRows.focusFirst()
        else if (page === "info")
            infoBack.forceActiveFocus()
        else
            mainRows.focusFirst()
    }
    function showPage(name) {
        if (name === "info")
            system.refreshIfStale(30000) // the hardware probe only runs on demand
        page = name
        Qt.callLater(focusDefault)
    }
    // "Return to game" means playing again: a paused emulator is resumed first.
    function returnToGame() {
        if (app.paused)
            app.resume()
        gameBar.closeQuickMenu()
    }
    function back() {
        if (page !== "main") {
            page = "main"
            Qt.callLater(() => mainRows.focusCurrent())
        } else {
            gameBar.closeQuickMenu()
        }
    }
    function controllerSummary() {
        const ic = app.input
        const devices = ic ? ic.devices : []
        if (devices.length === 0)
            return "No controller connected — " + app.inputStatus
        const names = []
        for (const d of devices)
            names.push(d.name)
        return (devices.length === 1 ? "Controller connected: " : devices.length + " controllers connected: ") + names.join(", ")
    }
    function memoryText() {
        const hw = system.hardware
        if (!hw.memTotalKiB)
            return "—"
        return Theme.formatBytes((hw.memTotalKiB - (hw.memAvailableKiB || 0)) * 1024) + " used of "
               + Theme.formatBytes(hw.memTotalKiB * 1024)
    }

    onVisibleChanged: if (visible) reset()

    Rectangle { anchors.fill: parent; color: Theme.overlayScrim; opacity: 0.6 }
    MouseArea { anchors.fill: parent; onClicked: menu.gameBar.closeQuickMenu() }

    Rectangle {
        id: panel
        objectName: "quickMenuPanel"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(parent.width * 0.42, 560 * Theme.scale)
        color: Theme.overlaySurface
        border.color: Theme.overlayLine
        // Glass: the highlight and rim over the overlay colours (no blur).
        GlassSurface { anchors.fill: parent; variant: "clear"; allowBlur: false; radius: 0 }
        MouseArea { anchors.fill: parent }

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: Theme.space5
            contentWidth: width
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            function ensureVisible(item) {
                if (!item)
                    return
                const p = item.mapToItem(body, 0, 0)
                if (p.y < contentY)
                    contentY = Math.max(0, p.y - Theme.space2)
                else if (p.y + item.height > contentY + height)
                    contentY = Math.min(p.y + item.height - height + Theme.space2, Math.max(0, contentHeight - height))
            }

            // Glass: one capsule glides to the focused entry of the visible page.
            GlassFocusPill {
                radius: Theme.radiusSmall // SettingRow's corners
                target: {
                    const nav = menu.page === "display" ? displayRows : menu.page === "info" ? null : mainRows
                    const t = nav ? nav.lastFocused : infoBack
                    return t && t.visible && menu.visible ? t : null
                }
            }

            Column {
                id: body
                width: flick.width
                spacing: Theme.space3

                Text {
                    textFormat: Text.PlainText
                    text: "QUICK MENU"
                    color: Theme.overlayTextMuted
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    font.letterSpacing: 2
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "quickMenuTitle"
                    width: parent.width
                    elide: Text.ElideRight
                    text: menu.app.activeEmulatorName.length > 0 ? menu.app.activeEmulatorName : "No game running"
                    color: Theme.overlayText
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "quickMenuState"
                    width: parent.width
                    text: menu.sessionActive && menu.sessions.active.state === "savingState" ? "Saving for Quick Resume…"
                          : menu.sessionActive && menu.sessions.active.state === "closing" ? "Saved — closing the emulator…"
                          : menu.app.paused ? "Paused" : Theme.capitalize(menu.app.state)
                    color: menu.sessionWorking ? Theme.accent
                           : menu.app.paused ? Theme.warning : Theme.statusColor(menu.app.state)
                    font.pixelSize: Theme.fontBody
                }

                // --- Main page -------------------------------------------------------------
                NavColumn {
                    id: mainRows
                    objectName: "quickMenuMain"
                    visible: menu.page === "main"
                    width: parent.width
                    onLastFocusedChanged: flick.ensureVisible(lastFocused)

                    SettingRow {
                        objectName: "quickReturn"
                        label: "Return to game"
                        description: menu.app.paused ? "Resumes the paused emulator" : ""
                        actionText: "Resume play"
                        onAccepted: menu.returnToGame()
                    }
                    SettingRow {
                        objectName: "quickSuspend"
                        visible: menu.sessionActive
                        label: "Suspend and go Home"
                        description: menu.sessionWorking ? "Please wait…"
                                     : menu.sessions && menu.sessions.canSuspend
                                       ? "Saves the game for Quick Resume and closes the emulator"
                                       : (menu.sessions ? menu.sessions.suspendUnavailableReason : "")
                        descriptionColor: menu.sessions && (menu.sessions.canSuspend || menu.sessionWorking)
                                          ? Theme.textMuted : Theme.warning
                        actionText: menu.sessions && menu.sessions.canSuspend && !menu.sessionWorking ? "Suspend" : ""
                        onAccepted: if (menu.sessions && menu.sessions.canSuspend && !menu.sessionWorking) menu.sessions.suspendToHome()
                    }
                    SettingRow {
                        objectName: "quickPause"
                        visible: menu.app.canPause
                        label: menu.app.paused ? "Resume emulator" : "Pause emulator"
                        description: menu.app.paused ? "The emulator is suspended" : "Suspends the emulator process"
                        actionText: menu.app.paused ? "Resume" : "Pause"
                        onAccepted: menu.app.paused ? menu.app.resume() : menu.app.pause()
                    }
                    Text {
                        textFormat: Text.PlainText
                        objectName: "quickControllers"
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: menu.controllerSummary()
                        color: Theme.overlayTextMuted
                        font.pixelSize: Theme.fontSmall
                        topPadding: Theme.space1
                        bottomPadding: Theme.space1
                    }
                    SettingRow {
                        objectName: "quickVolume"
                        label: "Volume"
                        description: menu.settings.volumeSupported ? "" : "Preference only on this platform"
                        value: menu.settings.muted ? "Muted" : menu.settings.volume + " %"
                        meter: menu.settings.volume / 100
                        adjustable: true
                        onDecrement: menu.settings.volume = Math.max(0, menu.settings.volume - 5)
                        onIncrement: menu.settings.volume = Math.min(100, menu.settings.volume + 5)
                    }
                    SettingRow {
                        objectName: "quickMute"
                        label: "Mute"
                        value: menu.settings.muted ? "On" : "Off"
                        valueColor: menu.settings.muted ? Theme.danger : Theme.textMuted
                        adjustable: true
                        onDecrement: menu.settings.muted = !menu.settings.muted
                        onIncrement: menu.settings.muted = !menu.settings.muted
                        onAccepted: menu.settings.muted = !menu.settings.muted
                    }
                    SettingRow {
                        objectName: "quickDisplay"
                        label: "Display settings"
                        actionText: "Open ›"
                        onAccepted: menu.showPage("display")
                    }
                    SettingRow {
                        objectName: "quickSystemInfo"
                        label: "System info"
                        actionText: "Open ›"
                        onAccepted: menu.showPage("info")
                    }
                    SettingRow {
                        objectName: "quickExit"
                        label: "Exit game"
                        description: "Stops the emulator and returns to Nexora"
                        labelColor: Theme.danger
                        actionText: "Exit"
                        onAccepted: confirmExit.open()
                    }
                }

                // --- Display page ----------------------------------------------------------
                NavColumn {
                    id: displayRows
                    objectName: "quickMenuDisplay"
                    visible: menu.page === "display"
                    width: parent.width
                    onLastFocusedChanged: flick.ensureVisible(lastFocused)

                    Text {
                        textFormat: Text.PlainText
                        text: "Display settings"
                        color: Theme.overlayText
                        font.pixelSize: Theme.fontLarge
                        font.bold: true
                    }
                    SettingRow {
                        objectName: "quickDisplayMode"
                        label: "Display mode"
                        value: Theme.capitalize(menu.settings.displayMode)
                        adjustable: true
                        onDecrement: menu.settings.cycle("display.mode", -1)
                        onIncrement: menu.settings.cycle("display.mode", 1)
                    }
                    SettingRow {
                        objectName: "quickResolution"
                        label: "Resolution"
                        description: "Applies to Nexora's window on the next start"
                        value: menu.settings.resolution === "auto" ? "Automatic" : menu.settings.resolution.replace("x", " × ")
                        adjustable: true
                        onDecrement: menu.settings.cycle("display.resolution", -1)
                        onIncrement: menu.settings.cycle("display.resolution", 1)
                    }
                    SettingRow {
                        objectName: "quickUiScale"
                        label: "Interface scale"
                        value: menu.settings.uiScale + " %"
                        adjustable: true
                        onDecrement: menu.settings.uiScale = Math.max(75, menu.settings.uiScale - 5)
                        onIncrement: menu.settings.uiScale = Math.min(150, menu.settings.uiScale + 5)
                    }
                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "The emulator's own video settings are not changed from here."
                        color: Theme.overlayTextFaint
                        font.pixelSize: Theme.fontSmall
                    }
                    SettingRow {
                        objectName: "quickDisplayBack"
                        label: "Back"
                        actionText: "‹ Quick menu"
                        onAccepted: menu.back()
                    }
                }

                // --- System info page ------------------------------------------------------
                Column {
                    objectName: "quickMenuInfo"
                    visible: menu.page === "info"
                    width: parent.width
                    spacing: Theme.space2
                    Text {
                        textFormat: Text.PlainText
                        text: "System info"
                        color: Theme.overlayText
                        font.pixelSize: Theme.fontLarge
                        font.bold: true
                    }
                    Repeater {
                        model: [
                            ["Nexora", menu.app.version + (menu.system.buildId.length > 0 ? " (build " + menu.system.buildId + ")" : "")],
                            ["OS", menu.system.osVersion.length > 0 ? menu.system.osVersion : "unknown"],
                            ["Board", menu.system.board],
                            ["Platform", menu.system.platform],
                            ["CPU", (menu.system.hardware.cpuModel || "—")
                                    + (menu.system.hardware.cpuThreads ? " · " + menu.system.hardware.cpuThreads + " threads" : "")],
                            ["Memory", menu.memoryText()],
                            ["Emulator", menu.app.activeEmulatorName.length > 0 ? menu.app.activeEmulatorName : "—"]
                        ]
                        delegate: Row {
                            required property var modelData
                            spacing: Theme.space3
                            Text {
                                textFormat: Text.PlainText
                                width: Math.round(110 * Theme.scale)
                                text: modelData[0]
                                color: Theme.overlayTextMuted
                                font.pixelSize: Theme.fontSmall
                            }
                            Text {
                                textFormat: Text.PlainText
                                width: body.width - Math.round(110 * Theme.scale) - Theme.space3
                                text: modelData[1]
                                color: Theme.overlayText
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                    SettingRow {
                        id: infoBack
                        objectName: "quickInfoBack"
                        label: "Back"
                        actionText: "‹ Quick menu"
                        onAccepted: menu.back()
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirmExit
        objectName: "confirmExitGame"
        title: "Exit the game?"
        message: "Unsaved progress in the emulator may be lost."
        confirmText: "Exit game"
        dangerous: true
        onConfirmed: {
            menu.app.stop()
            menu.gameBar.closeQuickMenu()
        }
    }

    onPageChanged: flick.contentY = 0
    // Back steps out (sub-page -> main -> game); nothing reaches the shell underneath.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace)
            menu.back()
        event.accepted = true
    }
}
