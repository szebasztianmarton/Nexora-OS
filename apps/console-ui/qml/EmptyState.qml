import QtQuick
import Nexora.Console

// Shown when no enabled emulator could be loaded.
Column {
    id: empty
    required property AppController app
    readonly property string title: app.configDirectoryFound ? "No emulators configured"
                                                             : "Configuration directory not found"

    Accessible.role: Accessible.StaticText
    Accessible.name: title

    spacing: Theme.spacing
    width: Math.min(parent ? parent.width - Theme.space6 * 2 : Math.round(800 * Theme.scale),
                    Math.round(900 * Theme.scale))

    Text {
        textFormat: Text.PlainText
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: empty.title
        color: Theme.text
        font.pixelSize: Theme.fontHero
        font.bold: true
    }
    Text {
        textFormat: Text.PlainText
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: "Add emulator definitions (*.json) to:\n" + empty.app.configDirectory
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }
    Text {
        textFormat: Text.PlainText
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: empty.app.configIssues.length > 0
        text: empty.app.configIssues.length > 0
              ? empty.app.configIssues[0]
                + (empty.app.configIssues.length > 1
                   ? "\n(+" + (empty.app.configIssues.length - 1) + " more issue(s) on the System tab)" : "")
              : ""
        color: Theme.danger
        font.pixelSize: Theme.fontSmall
    }
    Text {
        textFormat: Text.PlainText
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: "Press Menu / F1 for system information"
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }
}
