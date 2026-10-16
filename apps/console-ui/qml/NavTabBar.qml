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

    implicitHeight: Theme.tabHeight + Theme.space4
    // Eight sections have to fit side by side at 1280 px, also with a larger UI scale: tabs
    // shrink (down to their label) rather than overflow the window.
    readonly property real keyCapsWidth: Theme.space6 * 3
    readonly property real maxTabWidth: tabs.length > 0
        ? Math.max(0, (width - keyCapsWidth - tabs.length * tabRow.spacing) / tabs.length) : 0

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
        height: Math.round(24 * Theme.scale)
        width: capLabel.implicitWidth + Theme.space3
        radius: Theme.space1 + 2
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
        spacing: Theme.space1

        KeyCap {
            label: "L1"
            anchors.verticalCenter: parent.verticalCenter
        }
        Item { width: Theme.space2; height: 1 }

        Repeater {
            model: bar.tabs
            delegate: Rectangle {
                id: tab
                required property var modelData
                required property int index
                readonly property bool current: index === bar.currentIndex
                objectName: "tab_" + modelData.key
                anchors.verticalCenter: parent.verticalCenter
                height: Theme.tabHeight
                width: Math.min(bar.maxTabWidth, Math.max(96 * Theme.scale, tabLabel.implicitWidth + Theme.space5))
                radius: height / 2
                color: current ? (bar.activeFocus ? Theme.accent : Theme.accentSoft)
                     : tabMouse.containsMouse ? Theme.surfaceRaised : "transparent"
                border.width: current && bar.activeFocus ? Theme.focusWidth : 0
                border.color: Theme.highContrast ? Theme.focusRing : Theme.text

                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.title
                Accessible.checkable: true
                Accessible.checked: current
                Accessible.description: "Tab"
                Accessible.onPressAction: bar.tabRequested(index)

                Text {
                    id: tabLabel
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, tab.width - Theme.space3)
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    text: tab.modelData.title
                    color: tab.current ? (bar.activeFocus && Theme.highContrast ? Theme.background : Theme.text)
                                       : Theme.textMuted
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

        Item { width: Theme.space2; height: 1 }
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
