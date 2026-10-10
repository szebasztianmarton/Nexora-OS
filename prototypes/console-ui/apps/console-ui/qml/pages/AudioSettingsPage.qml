import QtQuick
import Nexora.Core

// Settings › Audio. System audio control is not implemented yet; this page
// says so instead of showing controls that would do nothing.
SettingsCategoryPage {
    id: root

    Item { width: parent.width; height: Dimensions.spacingXl }
    EmptyState {
        width: parent.width
        height: implicitHeight
        kind: "unavailable"
        iconName: "speaker"
        title: qsTr("Audio settings are not available yet")
        message: qsTr("Choosing the audio output and controlling the volume from Nexora is planned for a future release.")
        details: qsTr("Emulators play sound through the system's default audio device. Use each emulator's own settings to adjust its volume.")
    }
}
