import QtQuick
import Nexora.Console

// Software update panel (Settings › System › Software update, the System tab, the Home
// update card). Bound to the `updates` UpdateController: lists update bundles found on
// removable media / in the home directory, checks one (signature + version), verifies the
// image hashes with progress, and installs after a confirmation. Every state is stated
// plainly; on the desktop build updates are not supported and the panel says exactly that
// instead of offering buttons that cannot work. The result of the last update attempt
// (installed, or rolled back by the boot check) stays on screen until it is dismissed.
// Everything that comes from a bundle (version, notes, paths) is untrusted and shown as
// plain text only.
FocusScope {
    id: panel
    objectName: "updatePanel"
    required property UpdateController updates
    required property SystemController system
    anchors.fill: parent
    visible: false
    z: 40

    property var candidates: []
    property bool scanned: false
    property Item restoreFocusItem: null
    // Set by Main.qml: the window's scene, blurred behind the panel (glass blur only).
    property Item glassBackdrop: null

    readonly property string updateState: updates.state
    readonly property bool busy: updateState === "checking" || updateState === "verifying" || updateState === "installing"
    readonly property var hints: confirmInstall.visible
        ? [{ button: "← →", action: "Choose" }, { button: "A / Enter", action: "Confirm" },
           { button: "B / Esc", action: "Cancel" }]
        : [{ button: "↑ ↓", action: "Move" }, { button: "A / Enter", action: "Select" },
           { button: "B / Esc", action: "Close" }]

    function open() {
        restoreFocusItem = Window.activeFocusItem
        visible = true
        rescan()
        Qt.callLater(() => rows.focusFirst())
    }
    function close() {
        visible = false
        if (restoreFocusItem && restoreFocusItem.visible)
            restoreFocusItem.forceActiveFocus()
        restoreFocusItem = null
    }
    function rescan() {
        candidates = updates.supported ? updates.candidateBundles() : []
        scanned = true
    }
    function stateText() {
        switch (updateState) {
        case "idle": return updates.supported ? "No update selected" : "Not available on this build"
        case "checking": return "Checking the update bundle…"
        case "available": return "Update " + updates.availableVersion + " is available"
        case "verifying": return "Verifying the update image…"
        case "readytoinstall": return "Update " + updates.availableVersion + " is verified and ready to install"
        case "installing": return "Installing…"
        case "restartrequired": return "Installed — restart to finish the update"
        case "completed": return "The update was installed successfully"
        case "failed": return "The update failed"
        case "recoveryrequired": return "The last update was rolled back"
        default: return Theme.capitalize(updateState)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.glassActive ? Qt.rgba(0, 0, 0, 0.40) : Theme.background
        opacity: Theme.glassActive ? 1.0 : 0.9
    }
    MouseArea { anchors.fill: parent; onClicked: if (!panel.busy) panel.close() }

    Rectangle {
        id: box
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space6 * 2, 820 * Theme.scale)
        height: Math.min(parent.height - Theme.space5 * 2, content.implicitHeight + Theme.space5 * 2)
        radius: Theme.glassActive ? Theme.radiusXL : Theme.radiusLarge
        color: Theme.glassActive ? "transparent" : Theme.surfaceFloating
        GlassSurface { anchors.fill: parent; z: -1; radius: box.radius; dense: true; backdrop: panel.glassBackdrop }
        border.color: Theme.border
        MouseArea { anchors.fill: parent }

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: Theme.space5
            contentWidth: width
            contentHeight: content.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            function ensureVisible(item) {
                if (!item)
                    return
                const p = item.mapToItem(content, 0, 0)
                if (p.y < contentY)
                    contentY = Math.max(0, p.y - Theme.space2)
                else if (p.y + item.height > contentY + height)
                    contentY = Math.min(p.y + item.height - height + Theme.space2, Math.max(0, contentHeight - height))
            }

            Column {
                id: content
                width: flick.width
                spacing: Theme.space3

                Text {
                    text: "Software update"
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                }
                Text {
                    objectName: "updateCurrentVersion"
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Installed: Nexora OS " + panel.updates.currentVersion
                          + (panel.updates.currentBuildId.length > 0 ? " (build " + panel.updates.currentBuildId + ")" : "")
                    textFormat: Text.PlainText
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontBody
                }
                Text {
                    objectName: "updateState"
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: panel.stateText()
                    textFormat: Text.PlainText
                    color: panel.updateState === "failed" || panel.updateState === "recoveryrequired" ? Theme.danger
                         : panel.updateState === "readytoinstall" || panel.updateState === "completed" ? Theme.success
                         : Theme.text
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                Text {
                    objectName: "updateMessage"
                    width: parent.width
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                    text: (panel.updates.error.length > 0 ? panel.updates.error + ": " : "") + panel.updates.message
                    textFormat: Text.PlainText
                    color: panel.updates.error.length > 0 ? Theme.danger : Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    objectName: "updateUnsupported"
                    visible: !panel.updates.supported
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "This copy of Nexora runs on a desktop system, where the operating system is managed "
                          + "by your distribution. Signed Nexora OS update bundles can only be installed on the "
                          + "Nexora OS console image."
                    textFormat: Text.PlainText
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontBody
                }

                // Release notes of the checked bundle.
                Rectangle {
                    visible: panel.updates.notes.length > 0
                    width: parent.width
                    height: notesText.implicitHeight + Theme.space4 * 2
                    radius: Theme.radiusSmall
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    Text {
                        id: notesText
                        objectName: "updateNotes"
                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        wrapMode: Text.Wrap
                        text: panel.updates.notes
                        textFormat: Text.PlainText // release notes are untrusted bundle content
                        color: Theme.text
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Verification progress.
                Rectangle {
                    objectName: "updateProgress"
                    visible: panel.updateState === "verifying" || panel.updateState === "installing"
                    width: parent.width
                    height: Math.round(10 * Theme.scale)
                    radius: height / 2
                    color: Theme.border
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, panel.updates.progress))
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent
                    }
                }

                NavColumn {
                    id: rows
                    width: parent.width
                    onLastFocusedChanged: flick.ensureVisible(lastFocused)

                    Text {
                        visible: panel.updates.supported && panel.scanned && panel.candidates.length === 0
                                 && !panel.busy && panel.updateState !== "readytoinstall"
                        objectName: "updateNoBundles"
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "No update bundles found. Copy a nexora-update-<version> folder to a USB drive "
                              + "(or your home folder), connect it and select Search again."
                        textFormat: Text.PlainText
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }
                    Repeater {
                        model: panel.updates.supported && !panel.busy ? panel.candidates : []
                        delegate: SettingRow {
                            required property string modelData
                            required property int index
                            objectName: "updateBundle_" + index
                            label: modelData.split("/").pop()
                            description: modelData
                            actionText: "Check"
                            onAccepted: panel.updates.checkBundle(modelData)
                        }
                    }
                    SettingRow {
                        objectName: "updateVerify"
                        visible: panel.updateState === "available"
                        label: "Verify update " + panel.updates.availableVersion
                        description: "Checks the image against the signed manifest before anything is installed"
                        actionText: "Verify"
                        onAccepted: panel.updates.verify()
                    }
                    SettingRow {
                        objectName: "updateInstall"
                        visible: panel.updateState === "readytoinstall"
                        label: "Install update " + panel.updates.availableVersion
                        description: "Writes the update to the inactive system slot"
                        actionText: "Install"
                        onAccepted: confirmInstall.open()
                    }
                    SettingRow {
                        objectName: "updateAcknowledge"
                        visible: panel.updateState === "completed" || panel.updateState === "recoveryrequired"
                        label: panel.updateState === "completed" ? "Got it" : "OK, keep the current version"
                        description: "Dismisses this message; it is not shown again"
                        actionText: "OK"
                        onAccepted: panel.updates.acknowledgeLastResult()
                    }
                    SettingRow {
                        objectName: "updateRestart"
                        visible: panel.updateState === "restartrequired" && panel.system.powerSupported
                        label: "Restart now"
                        actionText: "Restart"
                        onAccepted: panel.system.reboot()
                    }
                    SettingRow {
                        objectName: "updateCancel"
                        visible: panel.updateState === "verifying" || panel.updateState === "available" || panel.updateState === "readytoinstall"
                        label: "Cancel"
                        description: "Discards the selected update"
                        actionText: "Cancel"
                        onAccepted: panel.updates.cancel()
                    }
                    SettingRow {
                        objectName: "updateRescan"
                        visible: panel.updates.supported && !panel.busy
                        label: "Search again"
                        description: "Looks for update bundles on removable media and in your home folder"
                        actionText: "Search"
                        onAccepted: panel.rescan()
                    }
                    SettingRow {
                        objectName: "updateClose"
                        enabled: !panel.busy || panel.updateState === "verifying"
                        label: "Close"
                        actionText: "Close"
                        onAccepted: panel.close()
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirmInstall
        objectName: "confirmInstallUpdate"
        title: "Install update " + panel.updates.availableVersion + "?"
        message: "Do not power off the console while the update installs. When it has finished, choose "
                 + "Restart now to start the new version; until then the current version keeps running."
        confirmText: "Install"
        onConfirmed: panel.updates.install()
    }

    // Keep focus on a visible row when the state machine hides the focused one.
    onUpdateStateChanged: Qt.callLater(() => {
        if (panel.visible && !confirmInstall.visible && rows.currentIndex() < 0)
            rows.focusFirst()
    })

    Keys.onPressed: (event) => {
        if ((event.key === Qt.Key_Escape || event.key === Qt.Key_Back) && (!panel.busy || panel.updateState === "verifying"))
            panel.close()
        event.accepted = true
    }
}
