pragma Singleton
import QtQuick

// Colour tokens. Use these instead of literal colours anywhere in the UI.
QtObject {
    // Backgrounds
    readonly property color backgroundPrimary: "#0A0D14"
    readonly property color backgroundSecondary: "#101621"
    readonly property color backgroundElevated: "#171F2D"
    readonly property color backgroundHover: "#1D2738"
    readonly property color backgroundOverlay: "#CC05070B"

    // Borders
    readonly property color borderSubtle: "#273244"
    readonly property color borderStrong: "#3A4961"

    // Accents
    readonly property color accentPrimary: "#6CE5E8"
    readonly property color accentSecondary: "#8B7CFF"
    readonly property color accentHover: "#94F0F1"
    readonly property color accentPrimarySoft: "#246CE5E8"   // translucent fill for selected surfaces
    readonly property color accentSecondarySoft: "#248B7CFF"
    readonly property color onAccent: "#061114"              // text on accent-filled buttons

    // Text
    readonly property color textPrimary: "#F2F5FA"
    readonly property color textSecondary: "#A6B0C2"
    readonly property color textMuted: "#768297"
    readonly property color textDisabled: "#596376"

    // Status
    readonly property color statusSuccess: "#65D6A0"
    readonly property color statusWarning: "#F4C96B"
    readonly property color statusError: "#FF747F"
    readonly property color statusInformation: "#79B9FF"

    // Focus ring (always the strongest emphasis on screen)
    readonly property color focusRing: accentPrimary
    readonly property color focusGlow: "#406CE5E8"

    // Maps a status/kind key to its colour: success|warning|error|info|neutral
    function statusColor(kind) {
        switch (kind) {
        case "success": case "available": return statusSuccess
        case "warning": case "disabled": case "unknown": return statusWarning
        case "error": case "missing": case "not-executable": case "invalid": return statusError
        case "info": return statusInformation
        default: return textSecondary
        }
    }

    // Deterministic pleasant gradient pair for artwork placeholders.
    function placeholderColors(seed) {
        var h = 0
        var s = String(seed || "")
        for (var i = 0; i < s.length; ++i)
            h = (h * 31 + s.charCodeAt(i)) % 360
        return [Qt.hsla(h / 360, 0.45, 0.24, 1), Qt.hsla(((h + 50) % 360) / 360, 0.55, 0.13, 1)]
    }
}
