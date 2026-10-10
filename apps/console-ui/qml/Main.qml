import QtQuick
import QtQuick.Window
import Nexora.Core

// Root window. Owns the global scale and motion preferences and switches
// between the first-run flow and the main shell.
Window {
    id: window

    width: typeof nexoraInitialSize !== "undefined" ? nexoraInitialSize.width : 1920
    height: typeof nexoraInitialSize !== "undefined" ? nexoraInitialSize.height : 1080
    minimumWidth: 960
    minimumHeight: 540
    visible: true
    title: qsTr("Nexora OS")
    color: Theme.backgroundPrimary
    visibility: (typeof nexoraWindowed !== "undefined" && nexoraWindowed) || !Settings.fullscreen
                ? Window.Windowed : Window.FullScreen

    // Scale relative to the 1920x1080 reference, honouring the user preference.
    Binding { target: Dimensions; property: "scale"
              value: Math.max(0.55, Math.min(window.width / 1920, window.height / 1080)) * Settings.interfaceScale }
    Binding { target: Dimensions; property: "compact"; value: window.width < 1400 }
    Binding { target: Motion; property: "reduced"; value: Settings.reduceMotion }

    Loader {
        id: rootLoader
        anchors.fill: parent
        anchors.margins: 0
        focus: true
        sourceComponent: Settings.firstRunCompleted ? shellComponent : firstRunComponent
        onLoaded: item.forceActiveFocus()
    }

    Component { id: shellComponent; Shell {} }
    Component {
        id: firstRunComponent
        FirstRunPage { onFinished: Settings.firstRunCompleted = true }
    }
}
