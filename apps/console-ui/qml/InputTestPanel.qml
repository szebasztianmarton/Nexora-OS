import QtQuick
import Nexora.Console

// Full-screen "Test input" overlay (Settings → Controllers). While open, button presses and
// stick movements are reported as text instead of navigating anything (InputController's
// test mode): useful to check a pad is wired up correctly without it driving the UI. Holding
// the button mapped to Back (|backButton|, "east"/B by default) for 2 seconds exits on its
// own; Escape exits at once for a keyboard-only session with no pad to test. Every other key
// stays inside, and hiding the panel from outside (e.g. its tab goes away) closes it and
// leaves test mode.
FocusScope {
    id: panel
    objectName: "inputTestPanel"
    required property AppController app
    // Gamepad button name currently mapped to the Back action (see Theme.buttonLabel).
    property string backButton: "east"
    anchors.fill: parent
    visible: false
    z: 50

    Accessible.role: Accessible.Dialog
    Accessible.name: "Test input"
    Accessible.description: panel.available ? "Press any button or move a stick. Hold " + Theme.buttonLabel(backButton) + " for 2 seconds to exit, or press Escape." : "No controller input layer is active."

    readonly property var inputController: app ? app.input : null
    readonly property bool available: inputController !== null
    readonly property var hints: [{ button: "B / Esc", action: "Close" },
                                  { button: "Hold " + Theme.buttonLabel(backButton), action: "Exit test" }]

    property Item restoreFocusItem: null

    property bool isOpen: false
    function open() {
        restoreFocusItem = Window.activeFocusItem
        isOpen = true
        visible = true
        forceActiveFocus()
        if (available)
            inputController.setTestMode(true)
    }
    function close() {
        if (available && inputController.testMode)
            inputController.setTestMode(false)
        isOpen = false
        visible = false
        if (restoreFocusItem && restoreFocusItem.visible)
            restoreFocusItem.forceActiveFocus()
        restoreFocusItem = null
    }

    Connections {
        target: panel.inputController
        enabled: panel.available
        // The hold-to-exit gesture already turned test mode off on the input side; just
        // close the panel to match.
        function onTestModeExited() { panel.close() }
    }

    Backdrop { anchors.fill: parent }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing * 4, 640)
        spacing: Theme.spacing

        Text {
            textFormat: Text.PlainText
            width: parent.width
            text: "Test input"
            color: Theme.text
            font.pixelSize: Theme.fontHero
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            objectName: "inputTestHelp"
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            text: panel.available
                  ? "Press any button or move a stick. Hold " + Theme.buttonLabel(panel.backButton)
                    + " for 2 seconds to exit, or press Escape."
                  : "No controller input layer is active (" + (panel.app ? panel.app.inputStatus : "") + ")."
        }

        Rectangle {
            width: parent.width
            height: 96
            radius: Theme.radius
            color: Theme.surfaceRaised
            border.color: Theme.border
            Text {
                textFormat: Text.PlainText
                objectName: "lastRawEvent"
                anchors.centerIn: parent
                width: parent.width - Theme.spacing * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.accent
                font.pixelSize: Theme.fontLarge
                font.bold: true
                text: panel.available && panel.inputController.lastRawEvent.length > 0
                      ? panel.inputController.lastRawEvent : "Waiting for input…"
            }
        }

        Text {
            textFormat: Text.PlainText
            width: parent.width
            visible: panel.available
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: {
                if (!panel.available)
                    return ""
                const names = []
                for (const d of panel.inputController.devices)
                    names.push(d.name)
                return names.length > 0 ? "Connected: " + names.join(", ") : "No gamepad connected yet"
            }
        }

        NxButton {
            objectName: "inputTestPanelClose"
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Close  (Escape)"
            focus: true
            onClicked: panel.close()
        }
    }

    // Hidden from outside while open (tab switch, game started): close and leave test mode.
    onVisibleChanged: if (!visible && isOpen) panel.close()

    // Traps focus: nothing here (not even L1/R1, Menu or Search) reaches the screen behind.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace)
            panel.close()
        event.accepted = true
    }
}
