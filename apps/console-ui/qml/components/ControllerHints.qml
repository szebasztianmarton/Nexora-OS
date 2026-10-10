import QtQuick
import Nexora.Core

// Bottom-edge hint bar. Shows only actions that currently work. Button labels
// follow the last used input device (keyboard keys or the connected
// controller's labels); text labels keep it understandable without glyphs.
Item {
    id: bar
    property var hints: []
    implicitHeight: Dimensions.hintBarHeight

    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Dimensions.spacingLg
        layoutDirection: Qt.RightToLeft

        Repeater {
            model: bar.hints
            delegate: Row {
                required property var modelData
                spacing: Dimensions.spacingSm
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: Dimensions.s(30)
                    width: Math.max(height, keyLabel.implicitWidth + Dimensions.spacingMd)
                    radius: Controllers.lastInputDevice === "controller" ? height / 2 : Dimensions.radiusSm
                    color: Theme.backgroundElevated
                    border.color: Theme.borderStrong
                    border.width: 1
                    Text {
                        id: keyLabel
                        anchors.centerIn: parent
                        // Re-evaluated when the input device changes.
                        text: Controllers.lastInputDevice, Controllers.controllerType, Controllers.buttonLabel(modelData.action)
                        font.family: Typography.family
                        font.pixelSize: Typography.caption
                        font.weight: Typography.weightSemiBold
                        color: Theme.textPrimary
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    font.family: Typography.family
                    font.pixelSize: Typography.secondary
                    color: Theme.textSecondary
                }
            }
        }
    }
}
