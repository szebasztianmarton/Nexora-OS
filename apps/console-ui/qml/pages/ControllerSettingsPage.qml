import QtQuick
import Nexora.Core

// Settings › Controller: support status, connected controllers, a live input
// test, the (read-only) button mapping and auto-repeat timings.
SettingsCategoryPage {
    id: root

    function typeName(type) {
        switch (type) {
        case "xbox": return qsTr("Xbox-style")
        case "playstation": return qsTr("PlayStation-style")
        case "nintendo": return qsTr("Nintendo-style")
        default: return qsTr("Generic")
        }
    }

    SettingsSection { title: qsTr("Controllers") }
    SettingsNote {
        visible: !Controllers.available
        kind: "warning"
        title: qsTr("Controller support is unavailable")
        text: (Controllers.unavailableReason !== "" ? Controllers.unavailableReason + "\n" : "")
              + qsTr("Keyboard navigation works: arrows, Enter, Esc, F, /, M.")
    }
    SettingsNote {
        visible: Controllers.available && Controllers.count === 0
        title: qsTr("No controller connected")
        text: qsTr("Connect a controller over USB or Bluetooth; it is detected automatically. Keyboard navigation works: arrows, Enter, Esc, F, /, M.")
    }
    Repeater {
        model: Controllers.available ? Controllers.controllers : []
        delegate: SettingsValueRow {
            required property var modelData
            label: modelData.name || qsTr("Unnamed controller")
            description: qsTr("Connected")
            value: root.typeName(modelData.type)
        }
    }
    SettingsValueRow {
        label: qsTr("Last input device")
        value: Controllers.lastInputDevice === "controller" ? qsTr("Controller") : qsTr("Keyboard")
    }

    Item { width: parent.width; height: Dimensions.spacingSm; visible: Controllers.available }
    SettingsInputTester {
        id: tester
        visible: Controllers.available
    }

    SettingsSection {
        title: qsTr("Button mapping")
        description: qsTr("Remapping will be available in a future release.")
    }
    Rectangle {
        width: parent.width
        height: mappingColumn.implicitHeight
        radius: Dimensions.radiusMd
        color: Theme.backgroundSecondary
        border.color: Theme.borderSubtle
        border.width: 1
        Column {
            id: mappingColumn
            width: parent.width
            Repeater {
                model: [{ label: qsTr("Action"), controllerInput: qsTr("Controller"), keyboardInput: qsTr("Keyboard"), header: true }]
                         .concat(Controllers.mappings)
                delegate: Item {
                    id: mapRow
                    required property var modelData
                    required property int index
                    width: mappingColumn.width
                    height: Math.max(Dimensions.s(50), Math.max(c1.implicitHeight, c2.implicitHeight, c3.implicitHeight) + Dimensions.spacingSm * 2)
                    Rectangle {
                        visible: mapRow.index > 0
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.leftMargin: Dimensions.spacingLg; anchors.rightMargin: Dimensions.spacingLg
                        height: 1
                        color: Theme.borderSubtle
                    }
                    Text {
                        id: c1
                        x: Dimensions.spacingLg
                        width: parent.width * 0.34 - Dimensions.spacingLg
                        anchors.verticalCenter: parent.verticalCenter
                        text: mapRow.modelData.label
                        wrapMode: Text.WordWrap
                        font.family: Typography.family
                        font.pixelSize: mapRow.modelData.header ? Typography.caption : Typography.secondary
                        font.weight: mapRow.modelData.header ? Typography.weightSemiBold : Typography.weightMedium
                        color: mapRow.modelData.header ? Theme.textMuted : Theme.textPrimary
                    }
                    Text {
                        id: c2
                        x: parent.width * 0.34 + Dimensions.spacingSm
                        width: parent.width * 0.36 - Dimensions.spacingSm
                        anchors.verticalCenter: parent.verticalCenter
                        text: mapRow.modelData.controllerInput
                        wrapMode: Text.WordWrap
                        font.family: Typography.family
                        font.pixelSize: mapRow.modelData.header ? Typography.caption : Typography.secondary
                        font.weight: mapRow.modelData.header ? Typography.weightSemiBold : Typography.weightRegular
                        color: mapRow.modelData.header ? Theme.textMuted : Theme.textSecondary
                    }
                    Text {
                        id: c3
                        x: parent.width * 0.70 + Dimensions.spacingSm
                        width: parent.width * 0.30 - Dimensions.spacingSm - Dimensions.spacingLg
                        anchors.verticalCenter: parent.verticalCenter
                        text: mapRow.modelData.keyboardInput
                        wrapMode: Text.WordWrap
                        font.family: Typography.family
                        font.pixelSize: mapRow.modelData.header ? Typography.caption : Typography.secondary
                        font.weight: mapRow.modelData.header ? Typography.weightSemiBold : Typography.weightRegular
                        color: mapRow.modelData.header ? Theme.textMuted : Theme.textSecondary
                    }
                }
            }
        }
    }

    SettingsSection {
        title: qsTr("Repeat")
        description: qsTr("Holding a direction on the D-pad or left stick repeats it. Action buttons never repeat.")
    }
    SettingsValueRow {
        label: qsTr("Repeat delay")
        description: qsTr("Time before a held direction starts repeating")
        value: qsTr("%1 ms").arg(Controllers.repeatDelayMs)
    }
    SettingsValueRow {
        label: qsTr("Repeat interval")
        description: qsTr("Time between repeats while held")
        value: qsTr("%1 ms").arg(Controllers.repeatIntervalMs)
    }
}
