import QtQuick
import Nexora.Console

// System: version and build, update status, hardware (CPU, memory, GPUs, storage, audio,
// display outputs), the recent launch history and the diagnostics that used to have their
// own tab (configuration, input, library, settings and log state). The action buttons at the
// top open the update panel, export a redacted diagnostics report and reload the emulator
// configuration; Down scrolls the information below them.
FocusScope {
    id: screen
    objectName: "systemScreen"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system
    required property UpdateController updates
    // Height the floating glass bars cover at the top / bottom of the page (set by Main.qml;
    // 0 when the bars are docked).
    property real chromeTop: 0
    property real chromeBottom: 0

    signal updatesRequested()

    readonly property var hints: flick.activeFocus
        ? [{ button: "↑ ↓", action: "Scroll" }, { button: "A / Enter", action: "Reload config" },
           { button: "L1 / R1", action: "Switch tab" }]
        : [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Run" },
           { button: "↓", action: "Details" }, { button: "L1 / R1", action: "Switch tab" }]

    property var logLines: []
    property string lastDiagnosticsPath: ""
    // The log tail is cheap; the hardware probe is synchronous, so it runs shortly after the
    // tab becomes current (never on the start-up path) and only when the last one is older
    // than 30 s. "Reload" (an explicit request) probes again at once.
    function refresh() {
        const lines = app.recentLog()
        logLines = lines.slice(Math.max(0, lines.length - 40))
        probeTimer.restart()
    }
    function reload() {
        app.reloadConfig()
        refresh()
        system.refresh()
    }
    Timer {
        id: probeTimer
        interval: 150
        onTriggered: if (screen.visible) screen.system.refreshIfStale(30000)
    }
    function focusDefault() { updateButton.focus = true; actions.focus = true }
    function list(values) {
        const out = []
        for (let i = 0; i < values.length; ++i)
            out.push(values[i])
        return out
    }
    function hardwareText() {
        const hw = system.hardware
        const lines = []
        lines.push("CPU: " + (hw.cpuModel || "unknown") + (hw.cpuThreads ? " (" + hw.cpuThreads + " threads)" : ""))
        if (hw.memTotalKiB)
            lines.push("Memory: " + Theme.formatBytes(hw.memTotalKiB * 1024) + " total, "
                       + Theme.formatBytes((hw.memAvailableKiB || 0) * 1024) + " available")
        for (const g of (hw.gpus || []))
            lines.push("GPU: " + (g.vendorName || g.vendorId || "unknown") + " " + (g.deviceId || "")
                       + (g.driver ? " · driver " + g.driver : "") + (g.card ? " · " + g.card : ""))
        for (const st of (hw.storage || []).filter(e => e.path && Number(e.totalBytes) > 0))
            lines.push("Storage " + st.path + ": " + Theme.formatBytes(st.availableBytes) + " free of "
                       + Theme.formatBytes(st.totalBytes) + (st.fsType ? " (" + st.fsType + ")" : ""))
        for (const a of (hw.audio || []))
            lines.push("Audio: " + a.name)
        for (const o of (hw.outputs || []))
            lines.push("Display " + o.name + ": " + (o.connected ? (o.modes && o.modes.length > 0 ? o.modes[0] : "connected")
                                                                  : "disconnected"))
        return lines.join("\n")
    }
    function historyText() {
        const records = app.launchHistory
        if (records.length === 0)
            return "No games or emulators launched yet."
        const lines = []
        for (const r of records) {
            const when = r.startedAt ? new Date(r.startedAt).toLocaleString(Qt.locale(), Locale.ShortFormat) : ""
            lines.push(when + "  " + (r.gameTitle || r.emulatorId) + " — " + (r.finalState || "unknown")
                       + (r.exitCode !== undefined && r.exitCode !== null ? " (exit " + r.exitCode + ")" : "")
                       + (r.startupFailure ? " · failed to start" : "")
                       + (r.error ? " · " + r.error : ""))
        }
        return lines.join("\n")
    }
    Component.onCompleted: if (visible) refresh()
    onVisibleChanged: if (visible) refresh()

    Text {
        textFormat: Text.PlainText
        id: title
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: Theme.space6
        anchors.topMargin: Theme.space5 + screen.chromeTop
        text: "System"
        color: Theme.text
        font.pixelSize: Theme.fontHero
        font.bold: true
    }

    FocusScope {
        id: actions
        objectName: "systemActions"
        anchors.verticalCenter: title.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: Theme.space6
        width: actionRow.width
        height: actionRow.height
        Row {
            id: actionRow
            spacing: Theme.space3
            NxButton {
                id: updateButton
                objectName: "systemUpdateButton"
                focus: true
                compact: true
                glyph: "⇪"
                text: "Software update"
                KeyNavigation.right: exportButton
                onClicked: screen.updatesRequested()
            }
            NxButton {
                id: exportButton
                objectName: "exportDiagnosticsButton"
                compact: true
                glyph: "⎙"
                text: "Export diagnostics"
                KeyNavigation.left: updateButton
                KeyNavigation.right: reloadButton
                onClicked: screen.lastDiagnosticsPath = screen.system.exportDiagnostics()
            }
            NxButton {
                id: reloadButton
                objectName: "reloadButton"
                compact: true
                glyph: "↻"
                text: "Reload configuration"
                KeyNavigation.left: exportButton
                onClicked: screen.reload()
            }
        }
        Keys.onDownPressed: flick.forceActiveFocus()
    }

    Flickable {
        id: flick
        objectName: "systemDetails"
        anchors.top: title.bottom
        anchors.topMargin: Theme.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // Stops above the hint bar: the focus frame is drawn around the whole view.
        anchors.bottomMargin: screen.chromeBottom
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        contentHeight: content.implicitHeight + Theme.spacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // Up at the top returns to the action buttons.
        Keys.onUpPressed: {
            if (contentY <= 0)
                actions.forceActiveFocus()
            else
                contentY = Math.max(0, contentY - 120 * Theme.scale)
        }
        Keys.onDownPressed: contentY = Math.max(0, Math.min(contentHeight - height, contentY + 120 * Theme.scale))
        Keys.onReturnPressed: screen.reload()
        Keys.onEnterPressed: screen.reload()
        Behavior on contentY { NumberAnimation { duration: Theme.animFast } }

        Rectangle {
            anchors.fill: parent
            visible: flick.activeFocus
            color: "transparent"
            border.width: Theme.focusWidth
            border.color: Theme.focusRing
            radius: Theme.radiusSmall
            z: 5
        }

        Column {
            id: content
            width: flick.width
            spacing: Theme.spacing

            component Section: Rectangle {
                id: section
                property string heading
                property string body
                property color bodyColor: Theme.text
                property bool mono: false
                // Inside the Grid: one column's width; directly in the Column: full width.
                width: !parent ? 0
                     : parent.columns > 1 ? (parent.width - parent.columnSpacing * (parent.columns - 1)) / parent.columns
                     : parent.width
                height: sectionColumn.implicitHeight + Theme.space5
                radius: Theme.radiusSmall
                color: Theme.surface
                border.color: Theme.border
                Column {
                    id: sectionColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space3
                    spacing: Theme.space1
                    Text {
                        textFormat: Text.PlainText
                        text: section.heading
                        color: Theme.accent
                        font.pixelSize: Theme.fontTiny
                        font.bold: true
                        font.letterSpacing: 1
                        font.capitalization: Font.AllUppercase
                    }
                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        text: section.body.length > 0 ? section.body : "—"
                        color: section.bodyColor
                        font.pixelSize: section.mono ? Theme.fontTiny + 1 : Theme.fontSmall + 1
                        font.family: section.mono ? "monospace" : Qt.application.font.family
                        lineHeight: 1.15
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }

            Grid {
                id: grid
                width: parent.width
                columns: width > 900 * Theme.scale ? 2 : 1
                columnSpacing: Theme.spacing
                rowSpacing: Theme.spacing

                Section {
                    objectName: "systemVersion"
                    heading: "Nexora OS"
                    body: "Nexora " + screen.app.version
                          + (screen.system.buildId.length > 0 ? " · build " + screen.system.buildId : "")
                          + "\nOperating system: " + (screen.system.osVersion.length > 0 ? screen.system.osVersion : "unknown")
                          + "\nBoard " + screen.system.board + " · platform " + screen.system.platform
                }
                Section {
                    objectName: "systemUpdates"
                    heading: "Software update"
                    body: !screen.updates.supported ? screen.updates.message
                          : screen.updates.state === "idle" ? "No update selected · use Software update above"
                          : Theme.capitalize(screen.updates.state)
                            + (screen.updates.availableVersion.length > 0 ? " · version " + screen.updates.availableVersion : "")
                            + (screen.updates.message.length > 0 ? "\n" + screen.updates.message : "")
                    bodyColor: screen.updates.error.length > 0 ? Theme.danger : Theme.text
                }
                Section {
                    objectName: "systemHardware"
                    heading: "Hardware"
                    body: screen.hardwareText()
                }
                Section {
                    objectName: "systemLaunchHistory"
                    heading: "Launch history"
                    body: screen.historyText()
                }
                Section {
                    heading: "Diagnostics export"
                    body: screen.lastDiagnosticsPath.length > 0 ? "Saved to " + screen.lastDiagnosticsPath
                          : screen.system.lastError.length > 0 ? screen.system.lastError
                          : "A redacted text report for troubleshooting (Export diagnostics above)"
                    bodyColor: screen.lastDiagnosticsPath.length === 0 && screen.system.lastError.length > 0
                               ? Theme.danger : Theme.text
                }
                Section {
                    heading: "Security"
                    body: screen.app.runningAsRoot ? "WARNING: running as root" : "Running as a regular user"
                    bodyColor: screen.app.runningAsRoot ? Theme.danger : Theme.text
                }
                Section {
                    heading: "Configuration directory"
                    body: screen.app.configDirectory + "\nsource: " + screen.app.configDirectorySource
                          + (screen.app.configDirectoryFound ? "" : "\n(directory does not exist)")
                }
                Section {
                    heading: "Configuration issues"
                    body: screen.list(screen.app.configIssues).join("\n")
                    bodyColor: screen.app.configIssues.length > 0 ? Theme.warning : Theme.text
                }
                Section {
                    heading: "Emulator state"
                    body: screen.app.state + (screen.app.statusMessage.length > 0 ? " — " + screen.app.statusMessage : "")
                    bodyColor: screen.app.statusIsError ? Theme.danger : Theme.text
                }
                Section { heading: "Disabled emulators"; body: screen.list(screen.app.disabledEmulators).join(", ") }
                Section {
                    heading: "Controller input"
                    body: screen.app.inputBackend + " — " + screen.app.inputStatus
                          + (screen.app.inputDevices.length > 0 ? "\nDevices: " + screen.list(screen.app.inputDevices).join(", ") : "")
                }
                Section {
                    heading: "Log file"
                    body: screen.app.logFilePath.length > 0 ? screen.app.logFilePath : "stderr only (use --log-file)"
                }
                Section {
                    heading: "Emulator output log"
                    body: screen.app.emulatorLogPath.length > 0 ? screen.app.emulatorLogPath : "disabled"
                }
                Section {
                    heading: "Game library"
                    body: (screen.library.available ? screen.library.totalCount + " game(s)" : "Library database unavailable")
                          + "\n" + screen.library.lastScanSummary
                          + (screen.library.directories.length > 0 ? "\nFolders: " + screen.list(screen.library.directories).join(", ") : "")
                }
                Section {
                    heading: "Library issues"
                    body: screen.list(screen.library.issues).join("\n")
                    bodyColor: screen.library.issues.length > 0 ? Theme.warning : Theme.text
                }
                Section {
                    heading: "Settings"
                    body: "User file: " + screen.settings.userFile
                          + (screen.settings.issues.length > 0 ? "\n" + screen.list(screen.settings.issues).join("\n") : "\nNo issues")
                          + (screen.settings.lastError.length > 0 ? "\nLast error: " + screen.settings.lastError : "")
                    bodyColor: screen.settings.issues.length > 0 || screen.settings.lastError.length > 0 ? Theme.warning : Theme.text
                }
                Section {
                    heading: "System status"
                    body: screen.list(screen.system.statusLines).join("\n")
                          + (screen.system.lastError.length > 0 ? "\nLast error: " + screen.system.lastError : "")
                }
            }

            Section { heading: "Last emulator output"; body: screen.app.lastOutput; mono: true }
            Section { heading: "Recent log"; body: screen.logLines.join("\n"); mono: true }
        }
    }
}
