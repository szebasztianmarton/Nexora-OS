import QtQuick
import QtQuick.Layouts

// Vertical list of contextual actions ("More actions"). Up/Down to move,
// Confirm to choose, Back to cancel.
ModalBase {
    id: menu
    property var actions: []          // [{ id, label, icon, enabled, destructive }]
    signal selected(string actionId)
    panelWidth: Dimensions.s(560)
    initialFocusItem: list

    ColumnLayout {
        width: parent.width
        spacing: Dimensions.spacingMd
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: menu.title
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.sectionTitle
            font.weight: Typography.weightSemiBold
            color: Theme.textPrimary
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, menu.maxPanelHeight - Dimensions.s(160))
            model: menu.actions
            clip: true
            focus: true
            spacing: Dimensions.spacingXs
            keyNavigationEnabled: true
            currentIndex: 0
            Component.onCompleted: {
                // Start on the first enabled entry.
                for (var i = 0; i < menu.actions.length; ++i)
                    if (menu.actions[i].enabled !== false) { currentIndex = i; break }
            }
            delegate: Item {
                id: entry
                required property var modelData
                required property int index
                readonly property bool isEnabled: modelData.enabled !== false
                readonly property bool current: ListView.isCurrentItem && list.activeFocus
                width: list.width
                height: Dimensions.buttonHeight
                opacity: isEnabled ? 1 : 0.45
                Rectangle {
                    anchors.fill: parent
                    radius: Dimensions.radiusMd
                    color: entry.current ? Theme.backgroundHover : "transparent"
                }
                FocusFrame { shown: entry.current }
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.spacingMd
                    spacing: Dimensions.spacingMd
                    Icon {
                        visible: !!entry.modelData.icon
                        name: entry.modelData.icon || "info"
                        size: Dimensions.iconSizeSmall
                        anchors.verticalCenter: parent.verticalCenter
                        color: entry.modelData.destructive ? Theme.statusError : Theme.textSecondary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.label
                        font.family: Typography.family
                        font.pixelSize: Typography.body
                        font.weight: entry.current ? Typography.weightSemiBold : Typography.weightRegular
                        color: entry.modelData.destructive ? Theme.statusError : Theme.textPrimary
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: list.currentIndex = entry.index
                    onClicked: menu._choose(entry.index)
                }
            }
            Keys.onPressed: function(event) {
                if (Input.isConfirm(event)) {
                    event.accepted = true
                    if (!event.isAutoRepeat) menu._choose(currentIndex)
                }
            }
        }
    }

    function _choose(i) {
        var action = actions[i]
        if (!action || action.enabled === false) return
        close()
        selected(action.id)
    }
}
