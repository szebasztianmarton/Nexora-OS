import QtQuick
import QtQuick.Window
import Nexora.Core

// Settings › Display. Only real values reported by Qt are shown.
SettingsCategoryPage {
    id: root

    readonly property var win: root.Window.window
    readonly property real dpr: Screen.devicePixelRatio
    readonly property int scaleIndex: {
        var values = [0.8, 0.9, 1.0, 1.1, 1.2, 1.3]
        var best = 0
        for (var i = 0; i < values.length; ++i)
            if (Math.abs(values[i] - Settings.interfaceScale) < Math.abs(values[best] - Settings.interfaceScale)) best = i
        return best
    }

    function px(w, h) { return qsTr("%1 × %2").arg(Math.round(w)).arg(Math.round(h)) }

    SettingsSection { title: qsTr("Current display") }
    SettingsValueRow {
        label: qsTr("Screen")
        value: Screen.name !== "" ? Screen.name : qsTr("Unknown")
    }
    SettingsValueRow {
        label: qsTr("Screen resolution")
        value: Screen.width > 0 ? root.px(Screen.width * root.dpr, Screen.height * root.dpr) : qsTr("Unknown")
    }
    SettingsValueRow {
        label: qsTr("Nexora window")
        value: root.win ? root.px(root.win.width * root.dpr, root.win.height * root.dpr)
                          + (root.win.visibility === Window.FullScreen ? qsTr(" · full screen") : qsTr(" · windowed"))
                        : qsTr("Unknown")
    }
    SettingsValueRow {
        label: qsTr("Refresh rate")
        value: Screen.refreshRate > 0 ? qsTr("%1 Hz").arg(Math.round(Screen.refreshRate * 100) / 100) : qsTr("Not reported")
    }
    SettingsValueRow {
        label: qsTr("Device pixel ratio")
        value: String(Math.round(root.dpr * 100) / 100)
    }
    SettingsValueRow {
        label: qsTr("Display backend")
        description: qsTr("Qt platform plugin in use")
        value: Qt.platform.pluginName !== "" ? Qt.platform.pluginName : qsTr("Unknown")
        monospace: true
    }

    SettingsSection { title: qsTr("Options") }
    SettingsToggleRow {
        id: fullscreenRow
        label: qsTr("Full screen")
        description: typeof nexoraWindowed !== "undefined" && nexoraWindowed
                     ? qsTr("Nexora was started with --windowed, so it stays in a window for this session. The preference is still saved.")
                     : qsTr("Fill the whole screen. Applies immediately.")
        checked: Settings.fullscreen
        onToggled: function(value) { Settings.fullscreen = value }
    }
    SettingsCyclerRow {
        label: qsTr("Interface scale")
        description: qsTr("Same setting as in General.")
        options: [0.8, 0.9, 1.0, 1.1, 1.2, 1.3].map(function(v) { return { value: v, label: Math.round(v * 100) + "%" } })
        currentIndex: root.scaleIndex
        onSelected: function(index, value) { Settings.interfaceScale = value }
    }
    SettingsNote {
        text: qsTr("Changing the display resolution or refresh rate is not supported from Nexora in this release. The system and its display compositor choose the screen mode; Nexora adapts its layout to it.")
    }
    SettingsActionRow {
        label: qsTr("Reset display settings")
        description: qsTr("Full screen on, interface scale 100%.")
        buttonText: qsTr("Reset")
        iconName: "refresh"
        onActivated: {
            Settings.fullscreen = true
            Settings.interfaceScale = 1.0
            Router.toast(qsTr("Display settings reset"), "success")
        }
    }
}
