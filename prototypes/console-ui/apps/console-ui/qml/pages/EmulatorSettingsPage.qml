import QtQuick
import Nexora.Core

// Settings › Emulator Configuration. Read-only overview of emulators.json:
// editing is not supported from the UI in this release.
SettingsCategoryPage {
    id: root

    SettingsNote {
        visible: Emulators.loadError !== ""
        kind: "error"
        title: qsTr("emulators.json could not be loaded")
        text: Emulators.loadError
    }

    SettingsSection { title: qsTr("Configuration file") }
    SettingsValueRow {
        label: qsTr("Location")
        description: Emulators.configExists ? qsTr("File found") : qsTr("File does not exist yet")
        value: Emulators.configPath
        monospace: true
        elideMiddle: true
        valueColor: Emulators.configExists ? Theme.textPrimary : Theme.statusWarning
    }
    SettingsValueRow {
        label: qsTr("Emulators")
        value: qsTr("%1 configured · %2 available").arg(Emulators.count).arg(Emulators.availableCount)
    }
    SettingsActionRow {
        label: qsTr("Reload configuration")
        description: qsTr("Read emulators.json again after editing it. The library is rescanned afterwards.")
        buttonText: qsTr("Reload")
        iconName: "refresh"
        onActivated: {
            Emulators.reload()
            if (Emulators.loadError !== "")
                Router.showError({ title: qsTr("emulators.json could not be loaded"), message: Emulators.loadError })
            else
                Router.toast(qsTr("Emulator configuration reloaded (%n emulator(s))", "", Emulators.count), "success")
        }
    }
    SettingsActionRow {
        label: qsTr("Validate all emulators")
        description: qsTr("Check again that every executable exists and can be run.")
        buttonText: qsTr("Validate")
        iconName: "check"
        onActivated: {
            Emulators.validateAll()
            Router.toast(qsTr("%1 of %2 emulators available").arg(Emulators.availableCount).arg(Emulators.count),
                         Emulators.availableCount === Emulators.count ? "success" : "warning")
        }
    }

    SettingsSection {
        title: qsTr("Emulators")
        description: qsTr("Select an emulator to see its details.")
    }
    SettingsNote {
        visible: Emulators.count === 0
        kind: "warning"
        text: qsTr("No emulators are configured. Add entries to emulators.json, then choose Reload configuration.")
    }
    Repeater {
        model: Emulators
        delegate: SettingsRow {
            id: emuRow
            required property var model
            label: model.name
            description: qsTr("%n game(s)", "", model.gameCount)
            settingsHint: qsTr("Details")
            trailingMaxWidth: width * 0.45
            onActivated: {
                Router.showSection("emulators")
                Router.push("EmulatorDetailsPage", { emulatorId: emuRow.model.id })
            }
            StatusIndicator { status: emuRow.model.status; text: emuRow.model.statusText }
        }
    }
    SettingsSection {
        visible: Emulators.skippedEntries.length > 0
        title: qsTr("Skipped entries")
        description: qsTr("These entries in emulators.json were ignored.")
    }
    SettingsNote {
        visible: Emulators.skippedEntries.length > 0
        kind: "warning"
        monospaceText: true
        text: Emulators.skippedEntries.join("\n")
    }

    SettingsSection { title: qsTr("How launching works") }
    SettingsNote {
        text: qsTr("Nexora runs one emulator at a time. While it runs, the emulator owns the screen and controller; when it exits, Nexora comes back and reports how it ended. Emulators are started directly with their configured arguments — never through a shell.")
    }
    SettingsNote {
        kind: "warning"
        text: qsTr("Editing emulators from the interface is not supported in this release. Edit emulators.json with a text editor, then choose Reload configuration.")
    }
}
