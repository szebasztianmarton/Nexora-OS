import QtQuick
import Nexora.Console

// Shown when no enabled emulator could be loaded.
Column {
    id: empty
    required property AppController app

    spacing: Theme.spacing
    width: Math.min(parent ? parent.width - 64 : 800, 900)

    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: empty.app.configDirectoryFound ? "No emulators configured"
                                             : "Configuration directory not found"
        color: Theme.text
        font.pixelSize: Theme.fontHero
        font.bold: true
    }
    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: "Add emulator definitions (*.json) to:\n" + empty.app.configDirectory
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }
    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: empty.app.configIssues.length > 0
        text: empty.app.configIssues.length > 0
              ? empty.app.configIssues[0]
                + (empty.app.configIssues.length > 1
                   ? "\n(+" + (empty.app.configIssues.length - 1) + " more issue(s) in Diagnostics)" : "")
              : ""
        color: Theme.danger
        font.pixelSize: Theme.fontSmall
    }
    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: "Press Menu / F1 for diagnostics"
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }
}
