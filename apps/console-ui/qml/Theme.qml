pragma Singleton
import QtQuick

// Central colours and metrics for the console interface (dark theme).
QtObject {
    readonly property color background: "#0e1016"
    readonly property color surface: "#171a23"
    readonly property color surfaceRaised: "#202432"
    readonly property color border: "#2c3142"
    readonly property color accent: "#5b8cff"
    readonly property color text: "#e9ecf3"
    readonly property color textMuted: "#9aa1b5"
    readonly property color success: "#3ecf8e"
    readonly property color warning: "#f2b84b"
    readonly property color danger: "#ff5c6c"

    readonly property int spacing: 16
    readonly property int radius: 12
    readonly property int focusWidth: 4

    readonly property int fontSmall: 14
    readonly property int fontBody: 18
    readonly property int fontTitle: 26
    readonly property int fontHero: 34

    function statusColor(status) {
        switch (status) {
        case "running": return success
        case "starting":
        case "stopping": return warning
        case "failed":
        case "unavailable": return danger
        default: return textMuted
        }
    }
}
