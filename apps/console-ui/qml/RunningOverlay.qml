import QtQuick
import Nexora.Console

// Modal layer shown while an emulator process is active. Only "Stop" is available.
FocusScope {
    id: overlay
    objectName: "runningOverlay"
    required property AppController app

    Rectangle {
        anchors.fill: parent
        color: Theme.background
        opacity: 0.92
    }
    // Swallow pointer input to the screens underneath.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacing * 1.5

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "NOW PLAYING"
            color: Theme.accent
            font.pixelSize: Theme.fontSmall
            font.bold: true
            font.letterSpacing: 2
        }
        Text {
            objectName: "runningTitle"
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, overlay.width - 96)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: overlay.app.activeEmulatorName
            color: Theme.text
            font.pixelSize: Theme.fontHero
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: overlay.app.state === "starting" ? "Starting…"
                : overlay.app.state === "stopping" ? "Stopping…"
                : "Running"
            color: Theme.statusColor(overlay.app.state)
            font.pixelSize: Theme.fontTitle
        }
        Rectangle {
            id: stopButton
            objectName: "stopButton"
            anchors.horizontalCenter: parent.horizontalCenter
            focus: true
            height: 56
            width: 240
            radius: 12
            color: Theme.surfaceRaised
            border.width: activeFocus ? Theme.focusWidth : 1
            border.color: activeFocus ? Theme.accent : Theme.border
            opacity: overlay.app.state === "stopping" ? 0.5 : 1.0
            scale: activeFocus ? 1.04 : 1.0
            Text {
                anchors.centerIn: parent
                text: "■  Stop emulator"
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.bold: true
            }
            MouseArea { anchors.fill: parent; onClicked: overlay.app.stop() }
            Keys.onReturnPressed: overlay.app.stop()
            Keys.onEnterPressed: overlay.app.stop()
            Keys.onSpacePressed: overlay.app.stop()
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Nexora returns here automatically when the emulator exits.\n"
                  + "Controller: hold Select + Start to stop the emulator and return."
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }
    }

    // Back / Menu / navigation do nothing while an emulator runs; consume them here.
    Keys.onPressed: (event) => { event.accepted = true }
}
