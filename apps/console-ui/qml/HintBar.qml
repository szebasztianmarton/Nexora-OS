import QtQuick
import Nexora.Console

// Bottom bar listing the controls available in the current context. Docked in dark / high
// contrast; a floating glass bar in the glass theme (Main.qml places it).
Rectangle {
    id: bar
    objectName: "hintBar"
    property var hints: []   // [{ button: "A / Enter", action: "Launch" }, ...]

    implicitHeight: Theme.barHeight
    color: Theme.glassActive ? "transparent" : Theme.surface

    GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL }
    // Glass: the bar floats over the scrolling pages; presses on it (also between its controls)
    // must not reach a card scrolled underneath. Below the bar's own content in z.
    MouseArea {
        objectName: "hintBarClickShield"
        anchors.fill: parent
        z: -1
        enabled: Theme.glassActive
        hoverEnabled: false
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = false // scrolling still reaches the page
    }
    Rectangle {
        visible: !Theme.glassActive
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    // Centred, never wider than the bar: hints that do not fit are left out from the end (the
    // arrays list the least important last). Each hint's width does not depend on whether it
    // is shown, so the count is measured on all of them.
    readonly property real available: width - 2 * Theme.space4
    property int hintRevision: 0 // itemAt() is not a notifying read; bumped as delegates come and go
    readonly property int shownCount: {
        void hintRevision
        let used = 0
        let n = 0
        for (let i = 0; i < hintRepeater.count; ++i) {
            const item = hintRepeater.itemAt(i)
            if (!item)
                break
            const next = used + (n > 0 ? hintRow.spacing : 0) + item.width
            if (next > available)
                break
            used = next
            ++n
        }
        return n
    }

    Row {
        id: hintRow
        anchors.centerIn: parent
        spacing: Theme.spacing * 1.5
        Repeater {
            id: hintRepeater
            model: bar.hints
            onItemAdded: ++bar.hintRevision
            onItemRemoved: ++bar.hintRevision
            delegate: Item {
                id: hint
                objectName: "hintItem"
                required property var modelData
                required property int index
                readonly property string action: modelData.action
                readonly property bool hasKey: modelData.button.length > 0
                visible: index < bar.shownCount
                width: (hasKey ? keyCap.width + Theme.space2 : 0) + actionLabel.implicitWidth
                height: keyCap.height
                Rectangle {
                    id: keyCap
                    visible: hint.hasKey
                    anchors.verticalCenter: parent.verticalCenter
                    height: Math.round(26 * Theme.scale)
                    width: Math.max(height, keyLabel.implicitWidth + Theme.space4)
                    radius: Theme.space1
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    Text {
                        textFormat: Text.PlainText
                        id: keyLabel
                        anchors.centerIn: parent
                        text: hint.modelData.button
                        color: Theme.text
                        font.pixelSize: Theme.fontTiny
                        font.bold: true
                    }
                }
                Text {
                    id: actionLabel
                    textFormat: Text.PlainText
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: hint.modelData.action
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }
}
