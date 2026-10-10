import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Nexora.Core

// Reflects the real Emulator Manager lifecycle while an external process is
// launching/running/stopping. A full-screen emulator usually covers this
// window; the overlay is what remains visible underneath (or in windowed
// mode) and blocks conflicting actions. Focus is captured and restored.
FocusScope {
    id: overlay

    readonly property bool active: Launcher.state !== Launcher.Idle
    property string gameTitle: ""
    property Item returnFocusItem: null

    anchors.fill: parent
    visible: opacity > 0
    opacity: active ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Motion.dialog } }

    onActiveChanged: {
        if (active) {
            var w = overlay.Window.window
            returnFocusItem = w ? w.activeFocusItem : null
            stopButton.forceActiveFocus()
        } else {
            // The stored item can have been destroyed meanwhile (its JS wrapper
            // then has no methods); fall back to the current page's focus.
            if (returnFocusItem && typeof returnFocusItem.forceActiveFocus === "function")
                returnFocusItem.forceActiveFocus()
            else
                Router.focusContent()
            returnFocusItem = null
        }
    }

    Keys.onPressed: function(event) { event.accepted = true }

    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundPrimary
        opacity: 0.94
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Dimensions.safeMargin * 4, Dimensions.s(760))
        spacing: Dimensions.spacingLg

        LoadingIndicator {
            Layout.alignment: Qt.AlignHCenter
            size: Dimensions.s(56)
            visible: Launcher.state === Launcher.Launching || Launcher.state === Launcher.Stopping
        }
        Icon {
            Layout.alignment: Qt.AlignHCenter
            name: "play"
            size: Dimensions.s(56)
            color: Theme.accentPrimary
            visible: Launcher.state === Launcher.Running
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Launcher.state === Launcher.Launching ? qsTr("Launching…")
                : Launcher.state === Launcher.Stopping ? qsTr("Stopping…")
                : qsTr("Now playing")
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            font.weight: Typography.weightSemiBold
            font.letterSpacing: Dimensions.s(2)
            color: Theme.accentPrimary
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: overlay.gameTitle !== "" ? overlay.gameTitle : Launcher.activeEmulatorName
            font.family: Typography.family
            font.pixelSize: Typography.pageTitle
            font.weight: Typography.weightBold
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: overlay.gameTitle !== ""
            text: qsTr("in %1").arg(Launcher.activeEmulatorName)
            font.family: Typography.family
            font.pixelSize: Typography.body
            color: Theme.textSecondary
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Nexora returns automatically when the application exits.")
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            color: Theme.textMuted
        }
        FocusButton {
            id: stopButton
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Dimensions.spacingMd
            text: qsTr("Stop application")
            iconName: "stop"
            variant: "danger"
            enabled: Launcher.state === Launcher.Running
            onClicked: Router.confirm({
                title: qsTr("Stop %1?").arg(Launcher.activeEmulatorName),
                message: qsTr("Unsaved progress in the running application may be lost."),
                confirmText: qsTr("Stop"),
                destructive: true
            }, function() { Launcher.terminate() })
        }
    }
}
