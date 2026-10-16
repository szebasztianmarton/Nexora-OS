import QtQuick
import Nexora.Console

// Top-level section tabs. L1/R1 (PageUp/PageDown) switch from anywhere; when the bar
// itself has focus, Left/Right switch and Down/Accept enter the section.
FocusScope {
    id: bar
    property var tabs: []          // [{ key: "home", title: "Home" }, ...]
    property int currentIndex: 0

    signal tabRequested(int index)
    signal enterRequested()

    Accessible.role: Accessible.PageTabList
    Accessible.name: "Navigation tabs"
    Accessible.description: "Use Left and Right to switch tabs"

    implicitHeight: 60

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    component KeyCap: Rectangle {
        property string label
        height: 24
        width: capLabel.implicitWidth + 14
        radius: 6
        color: Theme.surfaceRaised
        border.color: Theme.border
        Text {
            id: capLabel
            anchors.centerIn: parent
            text: parent.label
            color: Theme.textMuted
            font.pixelSize: Theme.fontTiny
            font.bold: true
        }
    }

    Row {
        id: tabRow
        anchors.centerIn: parent
        spacing: 6

        KeyCap {
            label: "L1"
            anchors.verticalCenter: parent.verticalCenter
        }
        Item { width: 10; height: 1 }

        Repeater {
            model: bar.tabs
            delegate: Rectangle {
                id: tab
                required property var modelData
                required property int index
                readonly property bool current: index === bar.currentIndex
                objectName: "tab_" + modelData.key
                anchors.verticalCenter: parent.verticalCenter
                height: 42
                width: Math.max(120, tabLabel.implicitWidth + 40)
                radius: 21
                color: current ? (bar.activeFocus ? Theme.accent : Theme.accentSoft)
                     : tabMouse.containsMouse ? Theme.surfaceRaised : "transparent"
                border.width: current && bar.activeFocus ? 2 : 0
                border.color: Theme.text

                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.title
                Accessible.selected: current
                Accessible.description: "Tab"
                Accessible.onPressAction: bar.tabRequested(index)

                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    text: tab.modelData.title
                    color: tab.current ? Theme.text : Theme.textMuted
                    font.pixelSize: Theme.fontBody
                    font.bold: tab.current
                }
                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bar.tabRequested(tab.index)
                }
            }
        }

        Item { width: 10; height: 1 }
        KeyCap {
            label: "R1"
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Keys.onLeftPressed: tabRequested(Math.max(0, currentIndex - 1))
    Keys.onRightPressed: tabRequested(Math.min(tabs.length - 1, currentIndex + 1))
    Keys.onDownPressed: enterRequested()
    Keys.onReturnPressed: enterRequested()
    Keys.onEnterPressed: enterRequested()
    Keys.onUpPressed: {} // already at the top
}
