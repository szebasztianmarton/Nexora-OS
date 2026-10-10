pragma Singleton
import QtQuick

// Central colours, metrics and display helpers for the console interface (dark theme).
QtObject {
    readonly property color background: "#0b0d13"
    readonly property color surface: "#141821"
    readonly property color surfaceRaised: "#1d2230"
    readonly property color surfaceHover: "#252b3c"
    readonly property color border: "#2c3244"
    readonly property color accent: "#5b8cff"
    readonly property color accentSoft: "#263a6b"
    readonly property color text: "#eef1f7"
    readonly property color textMuted: "#a2a9bc"
    readonly property color textFaint: "#6f778c"
    readonly property color success: "#3ecf8e"
    readonly property color warning: "#f2b84b"
    readonly property color danger: "#ff6372"
    readonly property color favorite: "#ffc94d"

    readonly property int spacing: 16
    readonly property int radius: 12
    readonly property int focusWidth: 3
    readonly property int animFast: 90
    readonly property int animNormal: 160

    readonly property int fontTiny: 12
    readonly property int fontSmall: 14
    readonly property int fontBody: 17
    readonly property int fontLarge: 20
    readonly property int fontTitle: 24
    readonly property int fontHero: 30

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

    // Gamepad buttons are stored by position; show the common labels too.
    readonly property var buttonNames: ["south", "east", "west", "north", "start", "select", "guide", "l1", "r1"]
    function buttonLabel(name) {
        switch (name) {
        case "south": return "South (A / Cross)"
        case "east": return "East (B / Circle)"
        case "west": return "West (X / Square)"
        case "north": return "North (Y / Triangle)"
        case "start": return "Start"
        case "select": return "Select"
        case "guide": return "Guide"
        case "l1": return "L1"
        case "r1": return "R1"
        case "": return "Not bound"
        default: return name
        }
    }

    function sortLabel(order) {
        switch (order) {
        case "title": return "Title"
        case "recent": return "Recently played"
        case "emulator": return "Emulator"
        case "added": return "Recently added"
        default: return order
        }
    }

    function capitalize(value) {
        const s = String(value)
        return s.length > 0 ? s.charAt(0).toUpperCase() + s.slice(1) : s
    }

    // Stable per-title accent for placeholder cover art.
    function artColor(seed, lightness) {
        let h = 0
        const s = String(seed)
        for (let i = 0; i < s.length; ++i)
            h = (h * 31 + s.charCodeAt(i)) % 360
        return Qt.hsla(h / 360, 0.45, lightness, 1.0)
    }

    function initials(title) {
        const words = String(title).replace(/[^A-Za-z0-9 ]/g, " ").split(" ").filter(w => w.length > 0)
        if (words.length === 0)
            return "?"
        if (words.length === 1)
            return words[0].substring(0, 2).toUpperCase()
        return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase()
    }
}
