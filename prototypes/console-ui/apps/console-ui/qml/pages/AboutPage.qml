import QtQuick
import Nexora.Core

// Settings › About Nexora OS.
SettingsCategoryPage {
    id: root

    readonly property bool interAvailable: Qt.fontFamilies().indexOf("Inter") >= 0

    Item {
        width: parent.width
        height: hero.implicitHeight + Dimensions.spacingLg * 2
        Column {
            id: hero
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Dimensions.spacingLg
            anchors.verticalCenter: parent.verticalCenter
            spacing: Dimensions.spacingMd
            NexoraLogo { markSize: Dimensions.s(64); showWordmark: false }
            Text {
                text: qsTr("Nexora OS")
                font.family: Typography.family
                font.pixelSize: Typography.pageTitle
                font.weight: Typography.weightBold
                color: Theme.textPrimary
            }
            Text {
                width: parent.width
                text: qsTr("Version %1 · pre-release development build").arg(Diagnostics.appVersion)
                wrapMode: Text.WordWrap
                font.family: Typography.family
                font.pixelSize: Typography.body
                color: Theme.accentPrimary
            }
            Text {
                width: parent.width
                text: qsTr("An open-source, controller-first gaming operating system for x86-64 PCs and mini PCs. Nexora brings existing native Linux emulators together in one interface you can use from the couch. It does not include any games, BIOS or firmware files — you provide your own.")
                wrapMode: Text.WordWrap
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                color: Theme.textSecondary
                lineHeight: 1.15
            }
        }
    }

    SettingsSection { title: qsTr("This build") }
    SettingsValueRow {
        label: qsTr("Version")
        description: qsTr("Not a released version yet")
        value: Diagnostics.appVersion
    }
    SettingsValueRow {
        label: qsTr("Build")
        value: Diagnostics.buildId !== "" ? Diagnostics.buildId : qsTr("Unknown")
        monospace: true
    }
    SettingsValueRow {
        label: qsTr("License")
        description: qsTr("The project has not chosen a license yet.")
        value: qsTr("Not yet specified")
        valueColor: Theme.statusWarning
    }

    SettingsSection {
        title: qsTr("Open-source acknowledgements")
        description: qsTr("Nexora is built with these projects. Each remains under its own license.")
    }
    SettingsValueRow {
        label: qsTr("Qt %1").arg(Diagnostics.qtVersion)
        description: qsTr("Application framework and QML user interface")
        value: qsTr("LGPL v3")
    }
    SettingsValueRow {
        visible: Controllers.available
        label: qsTr("SDL2")
        description: qsTr("Game controller input")
        value: qsTr("zlib License")
    }
    SettingsValueRow {
        visible: root.interAvailable
        label: qsTr("Inter")
        description: qsTr("Interface typeface by Rasmus Andersson")
        value: qsTr("SIL Open Font License 1.1")
    }
    SettingsValueRow {
        label: qsTr("Linux")
        description: qsTr("Operating system kernel")
        value: qsTr("GPL v2")
    }
    SettingsNote {
        title: qsTr("Third-party notices")
        text: qsTr("Qt is used under the GNU Lesser General Public License v3; you may replace the Qt libraries with your own builds. SDL2 is distributed under the zlib License. The Inter typeface is licensed under the SIL Open Font License 1.1. Emulators you install are separate programs under their own licenses and are not part of Nexora.")
    }
}
