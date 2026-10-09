import QtQuick
import Nexora.Console

// Read-only diagnostics plus a configuration reload action.
FocusScope {
    id: screen
    objectName: "diagnosticsScreen"
    required property AppController app

    property var logLines: []
    function refresh() { logLines = app.recentLog().slice(-40) }
    Component.onCompleted: refresh()

    Text {
        id: title
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacing * 2
        text: "Diagnostics"
        color: Theme.text
        font.pixelSize: Theme.fontHero
        font.bold: true
    }

    Rectangle {
        id: reloadButton
        objectName: "reloadButton"
        anchors.verticalCenter: title.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 2
        height: 44
        width: reloadLabel.implicitWidth + 40
        radius: 8
        color: Theme.surfaceRaised
        border.width: flick.activeFocus ? Theme.focusWidth : 1
        border.color: flick.activeFocus ? Theme.accent : Theme.border
        Text {
            id: reloadLabel
            anchors.centerIn: parent
            text: "Reload configuration"
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        MouseArea {
            anchors.fill: parent
            onClicked: { screen.app.reloadConfig(); screen.refresh() }
        }
    }

    Flickable {
        id: flick
        anchors.top: title.bottom
        anchors.topMargin: Theme.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.spacing * 2
        anchors.rightMargin: Theme.spacing * 2
        contentHeight: content.implicitHeight + Theme.spacing
        clip: true
        focus: true
        boundsBehavior: Flickable.StopAtBounds

        Keys.onUpPressed: contentY = Math.max(0, contentY - 80)
        Keys.onDownPressed: contentY = Math.max(0, Math.min(contentHeight - height, contentY + 80))
        Keys.onReturnPressed: { screen.app.reloadConfig(); screen.refresh() }
        Keys.onEnterPressed: { screen.app.reloadConfig(); screen.refresh() }

        Column {
            id: content
            width: flick.width
            spacing: Theme.spacing

            component Section: Column {
                property string heading
                property string body
                property color bodyColor: Theme.text
                width: parent ? parent.width : 0
                spacing: 4
                Text {
                    text: parent.heading
                    color: Theme.accent
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                }
                Text {
                    width: parent.width
                    text: parent.body.length > 0 ? parent.body : "—"
                    color: parent.bodyColor
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WrapAnywhere
                }
            }

            Section { heading: "Version"; body: "Nexora OS " + screen.app.version + " (MVP 1 desktop prototype)" }
            Section {
                heading: "Configuration directory"
                body: screen.app.configDirectory + "\nsource: " + screen.app.configDirectorySource
                      + (screen.app.configDirectoryFound ? "" : "\n(directory does not exist)")
            }
            Section {
                heading: "Configuration issues"
                body: screen.app.configIssues.join("\n")
                bodyColor: screen.app.configIssues.length > 0 ? Theme.warning : Theme.text
            }
            Section { heading: "Disabled emulators"; body: screen.app.disabledEmulators.join(", ") }
            Section {
                heading: "Emulator state"
                body: screen.app.state + (screen.app.statusMessage.length > 0 ? " — " + screen.app.statusMessage : "")
                bodyColor: screen.app.statusIsError ? Theme.danger : Theme.text
            }
            Section { heading: "Last emulator output"; body: screen.app.lastOutput }
            Section {
                heading: "Controller input"
                body: screen.app.inputBackend + " — " + screen.app.inputStatus
                      + (screen.app.inputDevices.length > 0 ? "\nDevices: " + screen.app.inputDevices.join(", ") : "")
            }
            Section {
                heading: "Security"
                body: screen.app.runningAsRoot ? "WARNING: running as root" : "Running as a regular user"
                bodyColor: screen.app.runningAsRoot ? Theme.danger : Theme.text
            }
            Section {
                heading: "Log file"
                body: screen.app.logFilePath.length > 0 ? screen.app.logFilePath : "stderr only (use --log-file)"
            }
            Section { heading: "Recent log"; body: screen.logLines.join("\n") }
        }
    }
}
