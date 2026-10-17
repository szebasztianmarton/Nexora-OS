import QtQuick
import Nexora.Console

// Focusable push button for controller/keyboard use. Accept (Return/Enter/Space) or a click
// activates it. With |requireConfirm| the first activation only arms it ("press again").
Rectangle {
    id: button
    property string text
    property string glyph            // optional leading symbol
    property string kind: "normal"   // normal, primary, danger
    property bool requireConfirm: false
    property string confirmText: "Press again to confirm"
    property bool armed: false
    property bool compact: false
    readonly property bool highlighted: activeFocus || mouse.containsMouse
    // High contrast: a focused primary button is filled with the yellow accent, so its label
    // is drawn in the background colour (white on #ffd400 would be ~1.4:1).
    readonly property bool inkOnAccent: Theme.highContrast && kind === "primary" && activeFocus && !armed

    Accessible.role: Accessible.Button
    Accessible.name: text || confirmText || glyph || "Button"
    Accessible.description: requireConfirm ? "Requires confirmation" : ""
    Accessible.onPressAction: activate()

    signal clicked()

    function activate() {
        if (!enabled)
            return
        if (requireConfirm && !armed) {
            armed = true
            disarmTimer.restart()
            return
        }
        armed = false
        clicked()
    }

    activeFocusOnTab: true
    implicitHeight: compact ? Theme.controlHeightCompact : Theme.controlHeight
    implicitWidth: Math.max((compact ? 96 : 140) * Theme.scale, content.implicitWidth + (compact ? Theme.space5 + Theme.space1 : Theme.space6 + Theme.space2))
    radius: Theme.radiusSmall
    opacity: enabled ? 1.0 : 0.45
    color: armed ? Qt.darker(Theme.danger, 2.4)
         : kind === "primary" ? (activeFocus ? Theme.accent : Theme.accentSoft)
         : mouse.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised
    border.width: activeFocus ? Theme.focusWidth : 1
    border.color: activeFocus ? (armed || kind === "danger" ? Theme.danger
                                 : Theme.highContrast ? Theme.focusRing
                                 : (kind === "primary" ? Theme.text : Theme.accent))
                              : (armed ? Theme.danger : Theme.border)
    Behavior on color { ColorAnimation { duration: Theme.animFast } }

    onActiveFocusChanged: if (!activeFocus) armed = false

    Timer {
        id: disarmTimer
        interval: 4000
        onTriggered: button.armed = false
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 8
        Text {
            textFormat: Text.PlainText
            visible: button.glyph.length > 0 && !button.armed
            text: button.glyph
            color: button.kind === "danger" ? Theme.danger
                 : button.inkOnAccent ? Theme.background
                 : (button.kind === "primary" ? Theme.text : Theme.accent)
            font.pixelSize: button.compact ? Theme.fontBody : Theme.fontLarge
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            textFormat: Text.PlainText
            text: button.armed ? button.confirmText : button.text
            color: button.armed ? Theme.danger
                 : button.kind === "danger" ? Theme.danger
                 : button.inkOnAccent ? Theme.background : Theme.text
            font.pixelSize: button.compact ? Theme.fontSmall : Theme.fontBody
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { button.forceActiveFocus(); button.activate() }
    }

    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()
    // Back cancels a pending confirmation; otherwise it bubbles up.
    Keys.onEscapePressed: (event) => {
        if (armed)
            armed = false
        else
            event.accepted = false
    }
}
