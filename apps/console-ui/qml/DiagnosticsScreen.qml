import QtQuick
import Nexora.Console

// Read-only diagnostics plus a configuration reload action.
FocusScope {
    id: screen
    objectName: "diagnosticsScreen"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system

    readonly property var hints: [{ button: "↑ ↓", action: "Scroll" }, { button: "A / Enter", action: "Reload config" },
                                  { button: "L1 / R1", action: "Switch tab" }]

    property var logLines: []
    function refresh() {
        logLines = app.recentLog().slice(-40)
        system.refresh()
    }
    function reload() {
        app.reloadConfig()
        refresh()
    }
    function focusDefault() { flick.focus = true }
    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    Text {
        id: title
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing * 2
        anchors.topMargin: Theme.spacing * 1.5
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
        radius: 10
        color: Theme.surfaceRaised
        border.width: flick.activeFocus ? Theme.focusWidth : 1
        border.color: flick.activeFocus ? Theme.accent : Theme.border
        Text {
            id: reloadLabel
            anchors.centerIn: parent
            text: "↻  Reload configuration  (A / Enter)"
            color: Theme.text
            font.pixelSize: Theme.fontSmall + 1
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: screen.reload()
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

        // Up at the top hands focus to the tab bar.
        Keys.onUpPressed: (event) => {
            if (contentY <= 0)
                event.accepted = false
            else
                contentY = Math.max(0, contentY - 120)
        }
        Keys.onDownPressed: contentY = Math.max(0, Math.min(contentHeight - height, contentY + 120))
        Keys.onReturnPressed: screen.reload()
        Keys.onEnterPressed: screen.reload()
        Behavior on contentY { NumberAnimation { duration: Theme.animFast } }

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
                width: parent ? (parent.columns ? (parent.width - parent.columnSpacing * (parent.columns - 1)) / parent.columns
                                                : parent.width) : 0
                height: sectionColumn.implicitHeight + 24
                radius: 10
                color: Theme.surface
                border.color: Theme.border
                Column {
                    id: sectionColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 4
                    Text {
                        text: section.heading
                        color: Theme.accent
                        font.pixelSize: Theme.fontTiny
                        font.bold: true
                        font.letterSpacing: 1
                        font.capitalization: Font.AllUppercase
                    }
                    Text {
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
                columns: width > 900 ? 2 : 1
                columnSpacing: Theme.spacing
                rowSpacing: Theme.spacing

                Section {
                    heading: "Version"
                    body: "Nexora OS " + screen.app.version + " (MVP 2 prototype)\nPlatform: " + screen.system.platform
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
                    body: screen.app.configIssues.join("\n")
                    bodyColor: screen.app.configIssues.length > 0 ? Theme.warning : Theme.text
                }
                Section {
                    heading: "Emulator state"
                    body: screen.app.state + (screen.app.statusMessage.length > 0 ? " — " + screen.app.statusMessage : "")
                    bodyColor: screen.app.statusIsError ? Theme.danger : Theme.text
                }
                Section { heading: "Disabled emulators"; body: screen.app.disabledEmulators.join(", ") }
                Section {
                    heading: "Controller input"
                    body: screen.app.inputBackend + " — " + screen.app.inputStatus
                          + (screen.app.inputDevices.length > 0 ? "\nDevices: " + screen.app.inputDevices.join(", ") : "")
                }
                Section {
                    heading: "Log file"
                    body: screen.app.logFilePath.length > 0 ? screen.app.logFilePath : "stderr only (use --log-file)"
                }
                Section {
                    heading: "Game library"
                    body: (screen.library.available ? screen.library.totalCount + " game(s)" : "Library database unavailable")
                          + "\n" + screen.library.lastScanSummary
                          + (screen.library.directories.length > 0 ? "\nFolders: " + screen.library.directories.join(", ") : "")
                }
                Section {
                    heading: "Library issues"
                    body: screen.library.issues.join("\n")
                    bodyColor: screen.library.issues.length > 0 ? Theme.warning : Theme.text
                }
                Section {
                    heading: "Settings"
                    body: "User file: " + screen.settings.userFile
                          + (screen.settings.issues.length > 0 ? "\n" + screen.settings.issues.join("\n") : "\nNo issues")
                          + (screen.settings.lastError.length > 0 ? "\nLast error: " + screen.settings.lastError : "")
                    bodyColor: screen.settings.issues.length > 0 || screen.settings.lastError.length > 0 ? Theme.warning : Theme.text
                }
                Section {
                    heading: "System"
                    body: screen.system.statusLines.join("\n")
                          + (screen.system.lastError.length > 0 ? "\nLast error: " + screen.system.lastError : "")
                }
            }

            Section { heading: "Last emulator output"; body: screen.app.lastOutput; mono: true }
            Section { heading: "Recent log"; body: screen.logLines.join("\n"); mono: true }
        }
    }
}
