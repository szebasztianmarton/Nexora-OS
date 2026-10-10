import QtQuick
import QtQuick.Window
import Nexora.Core

// Settings › System & Diagnostics. Only values the backend actually knows;
// no estimated CPU/RAM/temperature figures.
SettingsCategoryPage {
    id: root

    Component { id: logViewerComponent; SettingsLogViewer {} }

    function launcherStateText() {
        switch (Launcher.state) {
        case Launcher.Launching: return qsTr("Starting %1").arg(Launcher.activeEmulatorName)
        case Launcher.Running: return qsTr("Running %1").arg(Launcher.activeEmulatorName)
        case Launcher.Stopping: return qsTr("Stopping %1").arg(Launcher.activeEmulatorName)
        default: return qsTr("Idle")
        }
    }
    function lastResultText() {
        var name = Launcher.lastEmulatorId !== "" ? (Emulators.byId(Launcher.lastEmulatorId).name || Launcher.lastEmulatorId) : ""
        switch (Launcher.lastResult) {
        case Launcher.NormalExit: return qsTr("%1 exited normally").arg(name)
        case Launcher.ErrorExit: return qsTr("%1 exited with code %2").arg(name).arg(Launcher.lastExitCode)
        case Launcher.Crashed: return qsTr("%1 crashed").arg(name)
        case Launcher.FailedToStart: return qsTr("%1 failed to start").arg(name)
        default: return qsTr("Nothing launched this session")
        }
    }
    function openLog() {
        var layer = root.Window.window ? root.Window.window.contentItem : null
        if (!layer) return
        var v = logViewerComponent.createObject(layer)
        if (!v) Router.showError({ title: qsTr("Can't open the log viewer"), message: logViewerComponent.errorString() })
    }
    function copyReport() {
        if (Diagnostics.copyToClipboard(Diagnostics.report()))
            Router.toast(qsTr("Diagnostics report copied to the clipboard"), "success")
        else
            Router.showError({ title: qsTr("Couldn't copy the report"),
                               message: qsTr("The clipboard is not available on this system. The same information is in the log file."),
                               details: Diagnostics.logFilePath })
    }

    SettingsSection { title: qsTr("Tools") }
    SettingsActionRow {
        id: logRow
        label: qsTr("View log")
        description: qsTr("Show the last 200 lines of the application log.")
        buttonText: qsTr("View log")
        iconName: "terminal"
        onActivated: root.openLog()
    }
    SettingsActionRow {
        label: qsTr("Copy diagnostics")
        description: qsTr("Copy a plain-text report for bug reports. Your home folder is shown as ~.")
        buttonText: qsTr("Copy")
        iconName: "copy"
        onActivated: root.copyReport()
    }
    SettingsActionRow {
        label: qsTr("Refresh")
        description: qsTr("Update the status and recent errors.")
        buttonText: qsTr("Refresh")
        iconName: "refresh"
        onActivated: {
            Diagnostics.refresh()
            Router.toast(qsTr("Diagnostics refreshed"), "info")
        }
    }

    SettingsSection { title: qsTr("Software") }
    SettingsValueRow {
        label: qsTr("Nexora version")
        value: qsTr("%1 (development build)").arg(Diagnostics.appVersion)
    }
    SettingsValueRow {
        label: qsTr("Build")
        value: Diagnostics.buildId !== "" ? Diagnostics.buildId : qsTr("Unknown")
        monospace: true
    }
    SettingsValueRow {
        visible: Diagnostics.imageVersion !== ""
        label: qsTr("System image")
        value: Diagnostics.imageVersion
    }
    SettingsValueRow {
        label: qsTr("Operating system")
        value: Diagnostics.osName !== "" ? Diagnostics.osName : qsTr("Unknown")
    }
    SettingsValueRow {
        label: qsTr("Kernel")
        value: Diagnostics.kernel !== "" ? Diagnostics.kernel : qsTr("Unknown")
    }
    SettingsValueRow {
        label: qsTr("Architecture")
        value: Diagnostics.architecture !== "" ? Diagnostics.architecture : qsTr("Unknown")
    }
    SettingsValueRow {
        label: qsTr("Qt version")
        value: Diagnostics.qtVersion
    }

    SettingsSection { title: qsTr("Status") }
    SettingsValueRow {
        label: qsTr("Emulator manager")
        description: root.lastResultText()
                     + (Launcher.lastErrorMessage !== "" && Launcher.lastResult !== Launcher.NormalExit
                        ? " — " + Launcher.lastErrorMessage : "")
        value: root.launcherStateText()
    }
    SettingsValueRow {
        label: qsTr("Controllers")
        value: !Controllers.available ? qsTr("Unavailable")
             : Controllers.count === 0 ? qsTr("None connected")
             : qsTr("%n connected", "", Controllers.count)
        description: !Controllers.available ? Controllers.unavailableReason : ""
        valueColor: Controllers.available ? Theme.textPrimary : Theme.statusWarning
    }
    SettingsValueRow {
        label: qsTr("Game library")
        value: Library.scanning ? qsTr("Scanning…") : qsTr("%n game(s)", "", Library.model.count)
        description: Library.skippedEntries.length > 0
                     ? qsTr("%n location(s) skipped during the last scan", "", Library.skippedEntries.length) : ""
    }
    SettingsValueRow {
        label: qsTr("Emulators")
        value: qsTr("%1 of %2 available").arg(Emulators.availableCount).arg(Emulators.count)
    }

    SettingsSection { title: qsTr("Files") }
    SettingsValueRow {
        label: qsTr("Configuration folder")
        value: Diagnostics.configDirectory
        monospace: true
        elideMiddle: true
    }
    SettingsValueRow {
        label: qsTr("Log file")
        value: Diagnostics.logFilePath !== "" ? Diagnostics.logFilePath : qsTr("Not logging to a file")
        monospace: true
        elideMiddle: true
    }

    SettingsSection { title: qsTr("Recent errors") }
    SettingsNote {
        visible: Diagnostics.recentErrors.length === 0
        kind: "success"
        text: qsTr("No recent errors")
    }
    Repeater {
        model: Diagnostics.recentErrors.slice().reverse()
        delegate: SettingsNote {
            required property string modelData
            kind: modelData.indexOf("[WARN]") >= 0 ? "warning" : "error"
            monospaceText: true
            text: modelData
        }
    }
    SettingsActionRow {
        visible: Diagnostics.recentErrors.length > 0
        label: qsTr("Clear recent errors")
        description: qsTr("Clears this list. The log file is not changed.")
        buttonText: qsTr("Clear")
        iconName: "close"
        onActivated: {
            Diagnostics.clearRecentErrors()
            Router.toast(qsTr("Recent errors cleared"), "info")
            Qt.callLater(function() { logRow.forceActiveFocus() })
        }
    }

    SettingsSection { title: qsTr("Reset") }
    SettingsActionRow {
        label: qsTr("Reset application preferences")
        description: qsTr("Restores interface and display preferences. Library folders, favorites and emulator configuration are kept.")
        buttonText: qsTr("Reset")
        danger: true
        onActivated: Router.confirm({
            title: qsTr("Reset application preferences?"),
            message: qsTr("Interface scale, sorting, motion, sidebar and full-screen preferences return to their defaults. Library folders, favorites and emulator configuration are kept."),
            confirmText: qsTr("Reset"),
            destructive: true
        }, function() {
            Settings.restoreDefaults(false)
            Router.toast(qsTr("Preferences reset"), "success")
        })
    }
}
