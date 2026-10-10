import QtQuick
import QtQuick.Window
import Nexora.Core

// Settings › Game Library: configured folders, scanning, supported file
// types, skipped entries and games removed from the library.
SettingsCategoryPage {
    id: root

    Component { id: folderDialogComponent; FolderBrowserDialog {} }

    function addFolder() {
        var layer = root.Window.window ? root.Window.window.contentItem : null
        if (!layer) return
        // The dialog validates and adds the folder itself (errors are shown
        // inline so another folder can be chosen without starting over).
        // Start next to the existing game folders (usually where more are).
        var dirs = Settings.libraryDirectories
        var start = ""
        if (dirs.length > 0) {
            var last = String(dirs[dirs.length - 1])
            start = last.lastIndexOf("/") > 0 ? last.substring(0, last.lastIndexOf("/")) : last
        }
        var dlg = folderDialogComponent.createObject(layer, { addToLibrary: true, startPath: start })
        if (!dlg) {
            Router.showError({ title: qsTr("Can't open the folder browser"), message: folderDialogComponent.errorString() })
            return
        }
        dlg.folderChosen.connect(function(path) {
            Router.toast(qsTr("Folder added — scanning for games"), "success")
        })
    }
    function removeFolder(path) {
        Router.confirm({
            title: qsTr("Remove this folder from the library?"),
            message: qsTr("%1\n\nGames in this folder will no longer appear in Nexora. The folder and its files stay on disk.").arg(path),
            confirmText: qsTr("Remove folder"),
            destructive: true
        }, function() {
            if (Settings.removeLibraryDirectory(path))
                Router.toast(qsTr("Folder removed from the library"), "info")
            else
                Router.toast(qsTr("The folder was already removed"), "warning")
            Qt.callLater(function() { addRow.forceActiveFocus() })
        })
    }
    function scanTimeText() {
        if (Library.scanning && !Library.hasScanned) return qsTr("Scanning now…")
        if (!Library.hasScanned) return qsTr("Never")
        var t = Library.lastScanTime
        if (!t || isNaN(t.getTime())) return qsTr("Never")
        return GameActions.formatRelative(t) + " · " + Qt.formatTime(t, Qt.locale().timeFormat(Locale.ShortFormat))
    }

    SettingsNote {
        visible: Library.stateError !== ""
        kind: "error"
        title: qsTr("Library data problem")
        text: Library.stateError
    }

    SettingsSection {
        title: qsTr("Game folders")
        description: qsTr("Nexora only looks inside these folders (up to 4 levels deep). Nothing else on the system is scanned.")
    }
    SettingsNote {
        visible: Settings.libraryDirectories.length === 0
        kind: "warning"
        text: qsTr("No game folders yet. Add the folder that contains your game files.")
    }
    Repeater {
        model: Settings.libraryDirectories
        delegate: SettingsActionRow {
            required property string modelData
            label: modelData
            description: qsTr("Game folder")
            buttonText: qsTr("Remove")
            iconName: "trash"
            danger: false
            settingsHint: qsTr("Remove")
            onActivated: root.removeFolder(modelData)
        }
    }
    SettingsActionRow {
        id: addRow
        label: qsTr("Add folder")
        description: qsTr("Browse to a folder with game files.")
        buttonText: qsTr("Add folder")
        iconName: "folder-plus"
        onActivated: root.addFolder()
    }

    SettingsSection { title: qsTr("Scanning") }
    SettingsActionRow {
        label: qsTr("Refresh library")
        description: Library.scanning ? qsTr("Scanning your game folders…")
                                      : qsTr("Look for new, changed or removed game files.")
        buttonText: Library.scanning ? qsTr("Scanning") : qsTr("Refresh")
        iconName: "refresh"
        busy: Library.scanning
        settingsHint: qsTr("Refresh")
        onActivated: {
            if (Library.scanning) { Router.toast(qsTr("A scan is already running; another will follow"), "info") }
            Library.refresh()
        }
    }
    SettingsValueRow {
        label: qsTr("Last scan")
        value: root.scanTimeText()
    }
    SettingsValueRow {
        label: qsTr("Games in library")
        value: Library.scanning && !Library.hasScanned ? qsTr("Scanning…") : String(Library.model.count)
    }
    SettingsValueRow {
        label: qsTr("Supported file types")
        description: qsTr("Comes from the enabled emulators in emulators.json.")
        value: Emulators.supportedExtensions.length > 0
               ? Emulators.supportedExtensions.map(function(e) { return "." + e }).join("  ")
               : qsTr("None — no enabled emulator")
        valueColor: Emulators.supportedExtensions.length > 0 ? Theme.textPrimary : Theme.statusWarning
        wrapValue: true
    }

    SettingsSection {
        visible: Library.skippedEntries.length > 0
        title: qsTr("Skipped during the last scan")
        description: qsTr("These locations could not be read. Check that drives are connected and folders are readable.")
    }
    SettingsNote {
        visible: Library.skippedEntries.length > 0
        kind: "warning"
        monospaceText: true
        text: Library.skippedEntries.join("\n")
    }

    SettingsSection { title: qsTr("Removed games") }
    SettingsActionRow {
        label: qsTr("Restore removed games")
        description: Library.hiddenCount > 0
                     ? qsTr("%n game(s) removed from the library. Their files were never deleted.", "", Library.hiddenCount)
                     : qsTr("No games have been removed from the library.")
        buttonText: qsTr("Restore")
        iconName: "refresh"
        onActivated: {
            if (Library.hiddenCount === 0) {
                Router.toast(qsTr("There are no removed games to restore"), "info")
                return
            }
            root.confirmRestore()
        }
    }
    function confirmRestore() {
        Router.confirm({
            title: qsTr("Restore removed games?"),
            message: qsTr("%n removed game(s) will appear in the library again after a rescan.", "", Library.hiddenCount),
            confirmText: qsTr("Restore")
        }, function() {
            Library.restoreHiddenEntries()
            Router.toast(qsTr("Removed games restored — rescanning"), "success")
        })
    }
}
