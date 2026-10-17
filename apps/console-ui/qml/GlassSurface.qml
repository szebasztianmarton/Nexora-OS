import QtQuick
import Nexora.Console

// The Liquid Glass background (≈ SwiftUI glassEffect). Put it inside a panel, filling it, below
// the content. Variants: "regular", "clear" (lighter, for cards and rows) and "prominent"
// (accent tinted, primary actions). |interactive| + |active| (focus/press, set by the owner)
// brighten the fill and the rim and lift the surface slightly.
//
// Theme.glassBlur && allowBlur: GlassEffectGpu blurs |backdrop| (or the enclosing GlassGroup's
// backdrop) under the surface; otherwise, or when that file cannot load, GlassEffectFlat. The
// backdrop must not be an ancestor of the surface; such a backdrop is ignored. Invisible
// outside the glass theme.
Item {
    id: surface
    property string variant: "regular"
    property color tint: Theme.accent
    property real radius: Theme.radiusLarge
    property bool interactive: false
    property bool active: false
    property Item backdrop: null
    property bool allowBlur: true

    readonly property Item group: {
        for (let p = surface.parent; p; p = p.parent) {
            if (p.blurredSource !== undefined && p.backdrop !== undefined)
                return p
        }
        return null
    }
    readonly property Item effectiveBackdrop: {
        const b = backdrop ? backdrop : (group ? group.backdrop : null)
        for (let p = surface.parent; p; p = p.parent) {
            if (p === b)
                return null
        }
        return b
    }
    readonly property bool lit: interactive && active
    readonly property color fillColor: variant === "prominent"
        ? (tint === Theme.accent ? Theme.glassFillProminent : Qt.rgba(tint.r, tint.g, tint.b, Theme.glassFillProminent.a))
        : variant === "clear" ? Theme.glassFillClear
        : lit ? Theme.glassFillRegularActive : Theme.glassFillRegular
    readonly property color rimColor: lit ? Theme.glassRimStrong : Theme.glassRim
    readonly property bool wantsGpu: Theme.glassBlur && allowBlur && effectiveBackdrop !== null
    readonly property bool blurred: state === "gpu"

    visible: Theme.glassActive
    state: wantsGpu && gpuLoader.status === Loader.Ready ? "gpu" : "flat"
    states: [State { name: "flat" }, State { name: "gpu" }]

    scale: lit && !Theme.reducedMotion ? 1.02 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

    Loader {
        id: gpuLoader
        anchors.fill: parent
        active: surface.wantsGpu
        source: "GlassEffectGpu.qml"
        onStatusChanged: if (status === Loader.Error) console.debug("nexora.ui: glass blur unavailable, using flat glass")
        onLoaded: {
            item.fillColor = Qt.binding(() => surface.fillColor)
            item.radius = Qt.binding(() => surface.radius)
            item.rimColor = Qt.binding(() => surface.rimColor)
            item.backdrop = Qt.binding(() => surface.effectiveBackdrop)
            item.blurredSource = Qt.binding(() => surface.group && !surface.backdrop ? surface.group.blurredSource : null)
        }
    }
    GlassEffectFlat {
        anchors.fill: parent
        visible: surface.state === "flat"
        fillColor: surface.fillColor
        radius: surface.radius
        rimColor: surface.rimColor
    }
}
