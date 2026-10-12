pragma Singleton
import QtQuick

// Central design tokens for the console interface: colours, spacing, typography, radii,
// icon sizes, motion and the small game/display helpers screens share.
//
// Accessibility inputs: set once at start-up (see Main.qml), everything else derives from
// them so every screen reacts the same way without having to know about settings itself.
//   scale          Theme.scale (= settings.uiScale / 100). Multiplies spacing, typography,
//                  radii, icon sizes and the game/emulator card grid.
//   highContrast   Theme.highContrast (= settings.theme === "highContrast"). Swaps the whole
//                  palette for a pure-black-background / white-text / yellow-focus-ring one;
//                  every colour that can appear as text keeps a contrast ratio of at least
//                  7:1 against the background it is drawn on.
//   reducedMotion  Theme.reducedMotion (= settings.reducedMotion). Collapses animFast/
//                  animNormal/animSlow to 0; continuous ambient animations (the library-scan
//                  spinner, a pulsing status dot) stop outright instead of running at speed 0.
QtObject {
    id: theme

    property real scale: 1.0
    property bool highContrast: false
    property bool reducedMotion: false

    // --- Palette -----------------------------------------------------------------------------
    readonly property color background: highContrast ? "#000000" : "#0b0d13"
    readonly property color surface: highContrast ? "#000000" : "#141821"
    readonly property color surfaceRaised: highContrast ? "#141414" : "#1d2230"
    readonly property color surfaceHover: highContrast ? "#262626" : "#252b3c"
    readonly property color border: highContrast ? "#ffffff" : "#2c3244"
    readonly property color accent: highContrast ? "#ffd400" : "#5b8cff"
    readonly property color accentSoft: highContrast ? "#4a3d00" : "#263a6b"
    readonly property color text: highContrast ? "#ffffff" : "#eef1f7"
    readonly property color textMuted: highContrast ? "#cfcfcf" : "#a2a9bc"
    readonly property color textFaint: highContrast ? "#b5b5b5" : "#6f778c"
    readonly property color success: highContrast ? "#00ff6a" : "#3ecf8e"
    readonly property color warning: highContrast ? "#ffa500" : "#f2b84b"
    readonly property color danger: highContrast ? "#ff3b3b" : "#ff6372"
    readonly property color favorite: highContrast ? "#ffd400" : "#ffc94d"
    // Dedicated focus-ring colour: equal to accent in the normal theme, but kept separate so
    // a future palette can tune focus visibility without touching the accent colour itself.
    readonly property color focusRing: highContrast ? "#ffd400" : accent

    // --- Spacing scale (4, 8, 12, 16, 24, 32 base px, × scale) --------------------------------
    readonly property real space1: Math.round(4 * scale)
    readonly property real space2: Math.round(8 * scale)
    readonly property real space3: Math.round(12 * scale)
    readonly property real space4: Math.round(16 * scale)
    readonly property real space5: Math.round(24 * scale)
    readonly property real space6: Math.round(32 * scale)
    // Pre-existing name: every current screen uses Theme.spacing for the "normal" gap.
    readonly property real spacing: space4

    // --- Radii ---------------------------------------------------------------------------------
    readonly property real radiusSmall: Math.round(10 * scale)
    readonly property real radius: Math.round(12 * scale)
    readonly property real radiusLarge: Math.round(18 * scale)

    // --- Typography (base px × scale) -----------------------------------------------------------
    readonly property real fontTiny: Math.round(12 * scale)
    readonly property real fontSmall: Math.round(14 * scale)
    readonly property real fontBody: Math.round(17 * scale)
    readonly property real fontLarge: Math.round(20 * scale)
    readonly property real fontTitle: Math.round(24 * scale)
    readonly property real fontHero: Math.round(30 * scale)

    // --- Icon sizes ------------------------------------------------------------------------------
    readonly property real iconSmall: Math.round(16 * scale)
    readonly property real icon: Math.round(24 * scale)
    readonly property real iconLarge: Math.round(40 * scale)

    // --- Focus / motion --------------------------------------------------------------------------
    readonly property int focusWidth: highContrast ? 4 : 3
    // 0 under reducedMotion: any Behavior/Animation keyed to these collapses to an instant
    // change instead of a transition.
    readonly property int animFast: reducedMotion ? 0 : 90
    readonly property int animNormal: reducedMotion ? 0 : 160
    readonly property int animSlow: reducedMotion ? 0 : 240

    // --- Game / emulator card grid -----------------------------------------------------------------
    readonly property real cardWidth: Math.round(230 * scale)
    readonly property real cardHeight: Math.round(200 * scale)
    readonly property real cardSpacing: space4

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

    // Bytes -> a short human string ("128 MB", "2.3 GB"). Used by read-only storage info.
    function formatBytes(bytes) {
        const n = Number(bytes)
        if (!isFinite(n) || n < 0)
            return "—"
        const units = ["B", "KB", "MB", "GB", "TB"]
        let value = n
        let i = 0
        while (value >= 1024 && i < units.length - 1) {
            value /= 1024
            ++i
        }
        return (i === 0 ? value.toFixed(0) : value.toFixed(1)) + " " + units[i]
    }
}
