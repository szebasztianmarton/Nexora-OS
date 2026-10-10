import QtQuick
import QtQuick.Controls
import Nexora.Console

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system

    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Nexora OS"
    color: Theme.background

    header: Header { app: window.app }
    footer: HintBar {
        hints: window.app.busy
               ? [{ button: "Enter", action: "Stop emulator" },
                  { button: "Select + Start (hold)", action: "Return to launcher" }]
               : stack.depth > 1
                 ? [{ button: "A / Enter", action: "Reload config" },
                    { button: "↑ ↓", action: "Scroll" },
                    { button: "B / Esc", action: "Back" }]
                 : [{ button: "← →", action: "Select" },
                    { button: "A / Enter", action: "Launch" },
                    { button: "Menu / F1", action: "Diagnostics" }]
    }

    function goBack() {
        if (stack.depth > 1)
            stack.pop()
    }
    function openDiagnostics() {
        if (stack.depth === 1)
            stack.push(diagnosticsComponent)
    }
    function restoreFocus() {
        if (window.app.busy)
            overlay.forceActiveFocus()
        else if (stack.currentItem)
            stack.currentItem.forceActiveFocus()
    }

    StackView {
        id: stack
        objectName: "stack"
        anchors.fill: parent
        focus: true
        initialItem: LibraryScreen { app: window.app }

        onCurrentItemChanged: Qt.callLater(window.restoreFocus)

        // Keys not handled by the focused screen bubble up to here.
        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Escape:
            case Qt.Key_Back:
            case Qt.Key_Backspace:
                window.goBack()
                event.accepted = true
                break
            case Qt.Key_Menu:
            case Qt.Key_F1:
                window.openDiagnostics()
                event.accepted = true
                break
            }
        }
    }

    Component {
        id: diagnosticsComponent
        DiagnosticsScreen { app: window.app }
    }

    RunningOverlay {
        id: overlay
        anchors.fill: parent
        z: 10
        app: window.app
        visible: window.app.busy
        onVisibleChanged: Qt.callLater(window.restoreFocus)
    }

    // Development convenience; the console build will not expose quitting.
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Component.onCompleted: restoreFocus()
}
