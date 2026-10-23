pragma Singleton
import QtQuick

// Central design tokens for the console interface: colours, spacing, typography, radii,
// icon sizes, motion and the small game/display helpers screens share.
//
// Accessibility inputs: set once at start-up (see Main.qml), everything else derives from
// them so every screen reacts the same way without having to know about settings itself.
//   userScale      Theme.userScale (= settings.uiScale / 100, Settings › Interface scale).
//   resolutionScale  Theme.resolutionScale (= max(1, min(window width / 1280, window height
//                  / 720)), set by Main.qml): the layout is designed for 1280x720, so a
//                  1080p screen draws everything 1.5x instead of with 720p-sized text.
//   scale          userScale x resolutionScale (read-only). Multiplies spacing, typography,
//                  radii, icon sizes and the game/emulator card grid.
//   glass          Theme.glass (= settings.theme === "glass", the default). Translucent "glass"
//                  surfaces over the Backdrop's soft light glows; floating panels (dialogs,
//                  keyboard, toasts) use the denser surfaceFloating so whatever lies behind them
//                  never competes with their text. High contrast always wins over glass.
//   highContrast   Theme.highContrast (= settings.theme === "highContrast"). Swaps the whole
//                  palette for a pure-black-background / white-text / yellow-focus-ring one;
//                  every colour that can appear as text keeps a contrast ratio of at least
//                  7:1 against the background it is drawn on.
//   reduceTransparency  Theme.reduceTransparency (= settings.reduceTransparency). Glass fills
//                  become the dense surfaceFloating and the backdrop blur is switched off.
//   gpuEffects     Theme.gpuEffects (= UiCapabilities.gpuEffects). True only on a hardware
//                  renderer; the precondition for the real backdrop blur.
//   sessionActive  Theme.sessionActive (an emulator is starting/running/stopping). The blur is
//                  off then so the GPU is left to the emulator.
//   glassBlur      Derived: glass look + gpuEffects, with no reduceTransparency, no session.
//   reducedMotion  Theme.reducedMotion (= settings.reducedMotion). Collapses animFast/
//                  animNormal/animSlow to 0; continuous ambient animations (the library-scan
//                  spinner, a pulsing status dot) stop outright instead of running at speed 0.
QtObject {
    id: theme

    property real userScale: 1.0
    property real resolutionScale: 1.0
    readonly property real scale: userScale * resolutionScale
    property bool highContrast: false
    property bool glass: false
    property bool reducedMotion: false
    property bool reduceTransparency: false
    // UiCapabilities.gpuEffects (hardware renderer) and an emulator session being active; both
    // bound by Main.qml.
    property bool gpuEffects: false
    property bool sessionActive: false
    // Glass look in effect (high contrast replaces it).
    readonly property bool glassActive: glass && !highContrast
    // Real backdrop blur (GlassEffectGpu) in effect; otherwise glass surfaces use the flat layers.
    readonly property bool glassBlur: glassActive && gpuEffects && !reduceTransparency && !sessionActive

    // --- Palette -----------------------------------------------------------------------------
    readonly property color background: highContrast ? "#000000" : glass ? "#070a14" : "#0b0d13"
    readonly property color surface: highContrast ? "#000000" : glass ? Qt.rgba(1, 1, 1, 0.06) : "#141821"
    readonly property color surfaceRaised: highContrast ? "#141414" : glass ? Qt.rgba(1, 1, 1, 0.09) : "#1d2230"
    readonly property color surfaceHover: highContrast ? "#262626" : glass ? Qt.rgba(1, 1, 1, 0.15) : "#252b3c"
    // Panels that float over other content (dialogs, on-screen keyboard, toasts, update panel):
    // in glass a dense tinted glass, otherwise the plain surface.
    readonly property color surfaceFloating: highContrast ? "#000000" : glass ? Qt.rgba(0.10, 0.12, 0.19, 0.90) : "#141821"
    readonly property color border: highContrast ? "#ffffff" : glass ? Qt.rgba(1, 1, 1, 0.16) : "#2c3244"
    readonly property color accent: highContrast ? "#ffd400" : glass ? "#6f9bff" : "#5b8cff"
    readonly property color accentSoft: highContrast ? "#4a3d00" : glass ? Qt.rgba(0.44, 0.61, 1, 0.30) : "#263a6b"
    readonly property color text: highContrast ? "#ffffff" : "#eef1f7"
    readonly property color textMuted: highContrast ? "#cfcfcf" : glass ? "#b6bdce" : "#a2a9bc"
    readonly property color textFaint: highContrast ? "#b5b5b5" : glass ? "#8c94aa" : "#6f778c"
    readonly property color success: highContrast ? "#00ff6a" : "#3ecf8e"
    readonly property color warning: highContrast ? "#ffa500" : "#f2b84b"
    readonly property color danger: highContrast ? "#ff3b3b" : "#ff6372"
    readonly property color favorite: highContrast ? "#ffd400" : "#ffc94d"

    // --- Liquid Glass ---------------------------------------------------------------------------
    readonly property real glassBlurRadius: Math.round(32 * scale)
    readonly property real glassSaturation: 0.35
    readonly property color glassFillRegular: reduceTransparency ? surfaceFloating : Qt.rgba(1, 1, 1, 0.10)
    readonly property color glassFillRegularActive: reduceTransparency ? surfaceHover : Qt.rgba(1, 1, 1, 0.15)
    readonly property color glassFillClear: reduceTransparency ? surfaceFloating : Qt.rgba(1, 1, 1, 0.05)
    // Dense floating panels on the GPU path: a dark tint of surfaceFloating over the blurred
    // backdrop, so body text keeps >= 4.5:1 also over brightly blurred cover art.
    readonly property color glassFillDense: reduceTransparency ? surfaceFloating : Qt.rgba(0.10, 0.12, 0.19, 0.65)
    // Prominent (tinted) fill. Reduce transparency: an opaque 50 % mix of the tint into the
    // background, so the light label keeps >= 4.5:1 (about 6.7:1 for the accent).
    function prominentFill(tint) {
        return reduceTransparency
            ? Qt.rgba((tint.r + background.r) / 2, (tint.g + background.g) / 2, (tint.b + background.b) / 2, 1)
            : Qt.rgba(tint.r, tint.g, tint.b, 0.30)
    }
    readonly property color glassFillProminent: prominentFill(accent)
    readonly property color glassRim: Qt.rgba(1, 1, 1, 0.20)
    readonly property color glassRimStrong: Qt.rgba(1, 1, 1, 0.30)
    readonly property color glassHighlight: Qt.rgba(1, 1, 1, reduceTransparency ? 0.06 : 0.18)
    // Dedicated focus-ring colour: equal to accent in the normal theme, but kept separate so
    // a future palette can tune focus visibility without touching the accent colour itself.
    readonly property color focusRing: highContrast ? "#ffd400" : accent

    // Game bar / quick menu palette: neutral graphite surfaces with light selected buttons in
    // the normal theme, the same black/white/yellow scheme as everything else in high contrast.
    readonly property color overlayScrim: "#000000"
    readonly property color overlaySurface: highContrast ? "#000000" : glass ? Qt.rgba(0.11, 0.12, 0.16, 0.86) : "#2a2c30"
    readonly property color overlayFooter: highContrast ? "#000000" : glass ? Qt.rgba(0.04, 0.05, 0.08, 0.92) : "#101114"
    readonly property color overlayTile: highContrast ? "#141414" : glass ? Qt.rgba(1, 1, 1, 0.09) : "#3c3e42"
    readonly property color overlayTileHover: highContrast ? "#262626" : glass ? Qt.rgba(1, 1, 1, 0.16) : "#4a4d53"
    readonly property color overlaySelected: highContrast ? "#ffd400" : "#e6e6e6"
    readonly property color overlayInk: highContrast ? "#000000" : "#1b1b1b"
    readonly property color overlayLine: highContrast ? "#ffffff" : glass ? Qt.rgba(1, 1, 1, 0.16) : "#45484e"
    readonly property color overlayText: "#ffffff"
    readonly property color overlayFocus: highContrast ? "#ffd400" : "#ffffff"
    readonly property color overlayTextSoft: highContrast ? "#ffffff" : "#e6e6e6"
    readonly property color overlayTextMuted: highContrast ? "#cfcfcf" : "#b9bcc2"
    readonly property color overlayTextFaint: highContrast ? "#b5b5b5" : "#9a9da3"
    readonly property color overlayDisabled: highContrast ? "#b5b5b5" : "#8a8d93"
    readonly property color overlayTrack: highContrast ? "#ffffff" : "#5a5d63"
    readonly property color overlayAccent: highContrast ? "#00ff6a" : "#107c10"
    readonly property color overlayGraph: highContrast ? "#00ff6a" : "#5dc21e"
    readonly property color overlayDanger: highContrast ? "#ff3b3b" : "#a4262c"
    readonly property color overlayDangerSoft: highContrast ? "#ff3b3b" : "#ff99a4"
    readonly property color overlayArmed: highContrast ? "#4a0000" : "#6b1015"
    readonly property color overlayArtEmpty: highContrast ? "#000000" : glass ? Qt.rgba(0, 0, 0, 0.35) : "#0d0f14"
    // Translucent tints for warning panels and badges over artwork.
    readonly property color dangerTint: highContrast ? "#000000" : Qt.rgba(1, 0.39, 0.45, 0.10)
    // Scrim behind glass modals (dialogs, keyboard, pickers, window-level overlays); docked
    // themes dim with the background instead.
    readonly property color modalScrim: highContrast ? background : Qt.rgba(0, 0, 0, 0.40)
    readonly property color badgeScrim: highContrast ? "#000000" : Qt.rgba(0, 0, 0, 0.6)
    readonly property color artInk: highContrast ? "#ffffff" : Qt.rgba(1, 1, 1, 0.85)

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
    readonly property real radiusXL: Math.round(24 * scale)

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

    // List/grid highlight movement (key navigation inside views).
    readonly property int animMove: reducedMotion ? 0 : 120

    // --- Control sizes (× scale) -------------------------------------------------------------
    readonly property real controlHeight: Math.round(48 * scale)
    readonly property real controlHeightCompact: Math.round(40 * scale)
    readonly property real chipHeight: Math.round(44 * scale)
    readonly property real tabHeight: Math.round(42 * scale)
    readonly property real rowHeight: Math.round(60 * scale)
    readonly property real barHeight: Math.round(48 * scale)
    readonly property real headerHeight: Math.round(64 * scale)

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
