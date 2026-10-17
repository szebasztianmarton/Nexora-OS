import QtQuick
import Nexora.Console

// One focusable settings row: label (+ description) on the left, value on the right.
// Left/Right adjust (|adjustable|), Accept activates; |requireConfirm| needs Accept twice.
// The value takes at most half the row and elides, so a long value never squeezes the label.
Rectangle {
    id: row
    property string label
    property string description
    property color descriptionColor: Theme.textMuted // e.g. Theme.danger for a failed action
    property string value
    property color valueColor: Theme.text
    property bool adjustable: false
    property bool requireConfirm: false
    property string confirmText: "Press A / Enter again to confirm"
    property string actionText        // shown instead of a value for action rows
    property color labelColor: Theme.text
    property real meter: -1           // 0..1 draws a level bar under the value
    property bool armed: false
    readonly property real maxValueWidth: Math.max(Math.round(80 * Theme.scale),
        width * 0.5 - (adjustable ? leftArrow.implicitWidth + rightArrow.implicitWidth + 2 * valueRow.spacing : 0))

    signal decrement()
    signal increment()
    signal accepted()

    function activate() {
        if (requireConfirm && !armed) {
            armed = true
            disarm.restart()
            return
        }
        armed = false
        accepted()
    }

    Accessible.role: adjustable ? Accessible.SpinBox : (actionText.length > 0 ? Accessible.Button : Accessible.StaticText)
    Accessible.name: label + (value.length > 0 ? ": " + value : "")
    Accessible.description: description
    Accessible.onPressAction: if (actionText.length > 0 || !adjustable) activate()

    activeFocusOnTab: true
    width: parent ? parent.width : 600
    implicitHeight: Math.max(Theme.rowHeight, texts.implicitHeight + Theme.space5)
    radius: Theme.radiusSmall
    GlassSheen {}
    color: activeFocus ? Theme.surfaceHover : (mouse.containsMouse ? Theme.surfaceRaised : Theme.surface)
    border.width: activeFocus ? Theme.focusWidth : 1
    border.color: activeFocus ? (armed ? Theme.danger : Theme.focusRing) : Theme.border
    Behavior on color { ColorAnimation { duration: Theme.animFast } }

    onActiveFocusChanged: if (!activeFocus) armed = false

    Timer {
        id: disarm
        interval: 4000
        onTriggered: row.armed = false
    }

    Column {
        id: texts
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing
        anchors.right: valueArea.left
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        Text {
            objectName: "label"
            width: parent.width
            text: row.label
            textFormat: Text.PlainText // labels can carry untrusted text (paths, update bundles)
            color: row.labelColor
            font.pixelSize: Theme.fontBody
            font.bold: row.activeFocus
            elide: Text.ElideMiddle
        }
        Text {
            textFormat: Text.PlainText
            width: parent.width
            visible: text.length > 0
            text: row.armed ? row.confirmText : row.description
            color: row.armed ? Theme.danger : row.descriptionColor
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }
    }

    Item {
        id: valueArea
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(valueRow.implicitWidth, row.meter >= 0 ? 220 : 0)
        height: valueRow.implicitHeight + (row.meter >= 0 ? 12 : 0)

        Row {
            id: valueRow
            anchors.right: parent.right
            spacing: 10
            Text {
                textFormat: Text.PlainText
                id: leftArrow
                visible: row.adjustable
                text: "‹"
                color: row.activeFocus ? Theme.accent : Theme.textFaint
                font.pixelSize: Theme.fontLarge
                anchors.verticalCenter: parent.verticalCenter
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: { row.forceActiveFocus(); row.decrement() } }
            }
            Text {
                objectName: "value"
                text: row.actionText.length > 0 ? row.actionText : row.value
                textFormat: Text.PlainText
                color: row.actionText.length > 0 ? (row.activeFocus ? Theme.accent : Theme.textMuted) : row.valueColor
                font.pixelSize: Theme.fontBody
                font.bold: row.actionText.length === 0
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                width: Math.min(row.adjustable ? Math.max(implicitWidth, 150) : implicitWidth, row.maxValueWidth)
            }
            Text {
                textFormat: Text.PlainText
                id: rightArrow
                visible: row.adjustable
                text: "›"
                color: row.activeFocus ? Theme.accent : Theme.textFaint
                font.pixelSize: Theme.fontLarge
                anchors.verticalCenter: parent.verticalCenter
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: { row.forceActiveFocus(); row.increment() } }
            }
        }
        Rectangle {
            visible: row.meter >= 0
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: parent.width
            height: 6
            radius: 3
            color: Theme.border
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, row.meter))
                height: parent.height
                radius: 3
                color: Theme.accent
                Behavior on width { NumberAnimation { duration: Theme.animFast } }
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        z: -1
        hoverEnabled: true
        onClicked: { row.forceActiveFocus(); if (!row.adjustable) row.activate() }
    }

    Keys.onLeftPressed: (event) => { if (adjustable) decrement(); else event.accepted = false }
    Keys.onRightPressed: (event) => { if (adjustable) increment(); else event.accepted = false }
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()
    Keys.onEscapePressed: (event) => {
        if (armed)
            armed = false
        else
            event.accepted = false
    }
}
