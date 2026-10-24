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
    // When the widest label does not fit (large UI scale at 1280 px), every tab switches to a
    // slightly smaller font and tighter padding instead of eliding a label.
    FontMetrics {
        id: labelMetrics
        font.pixelSize: Theme.fontBody
        font.bold: true
    }
    readonly property real widestLabel: {
        void labelMetrics.font.pixelSize
        let widest = 0
        for (const t of tabs)
            widest = Math.max(widest, labelMetrics.advanceWidth(t.title))
        return widest
    }
    readonly property bool compact: widestLabel + Theme.space5 > maxTabWidth
    readonly property real labelPadding: compact ? Theme.space2 : Theme.space5
    readonly property real labelFontSize: compact ? Math.round(Theme.fontBody * 0.82) : Theme.fontBody

    // Docked (dark / high contrast): a plain strip with a bottom rule. Glass: a floating glass
    // bar (Main.qml places it).
    GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL }
    // Glass: the bar floats over the scrolling pages; presses on it (also between its controls)
    // must not reach a card scrolled underneath. Below the bar's own content in z.
    MouseArea {
        objectName: "tabBarClickShield"
        anchors.fill: parent
        z: -1
        enabled: Theme.glassActive
        hoverEnabled: false
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = false // scrolling still reaches the page
    }
    Rectangle {
        visible: !Theme.glassActive
        anchors.fill: parent
        color: Theme.background
    }
    Rectangle {
        visible: !Theme.glassActive
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    // Glass: one capsule slides onto the current tab (the tab itself stays transparent).
    GlassFocusPill {
        target: { void tabRepeater.count; return tabRepeater.itemAt(bar.currentIndex) }
    }

    component KeyCap: Rectangle {
        property string label
        height: Math.round(24 * Theme.scale)
        width: capLabel.implicitWidth + Theme.space3
        radius: Theme.space1 + 2
        color: Theme.surfaceRaised
        border.color: Theme.border
        Text {
            textFormat: Text.PlainText
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
            id: tabRepeater
            model: bar.tabs
            delegate: Rectangle {
                id: tab
                required property var modelData
                required property int index
                readonly property bool current: index === bar.currentIndex
                objectName: "tab_" + modelData.key
                anchors.verticalCenter: parent.verticalCenter
                height: Theme.tabHeight
                width: Math.min(bar.maxTabWidth, Math.max(bar.compact ? 0 : 96 * Theme.scale,
                                                          tabLabel.implicitWidth + bar.labelPadding))
                radius: height / 2
                color: current ? (Theme.glassActive ? "transparent" : bar.activeFocus ? Theme.accent : Theme.accentSoft)
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
                    textFormat: Text.PlainText
                    id: tabLabel
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, tab.width - bar.labelPadding / 2)
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    text: tab.modelData.title
                    color: tab.current ? (bar.activeFocus && Theme.highContrast ? Theme.background : Theme.text)
                                       : Theme.textMuted
                    font.pixelSize: bar.labelFontSize
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
