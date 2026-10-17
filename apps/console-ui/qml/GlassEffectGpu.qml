import QtQuick
import QtQuick.Effects
import Nexora.Console

// GPU glass: the part of |backdrop| under this surface, blurred and saturated (MultiEffect),
// clipped to the rounded shape, with the flat layers on top. Loaded by GlassSurface only when
// Theme.glassBlur; on Qt < 6.5 this file fails to load and GlassSurface stays flat.
Item {
    id: gpu
    property color fillColor: "transparent"
    property real radius: 0
    property color rimColor: "transparent"
    property Item backdrop: null
    // A GlassGroup's already blurred backdrop, if any: then this surface only crops it.
    property Item blurredSource: null

    // The surface's rectangle in the backdrop's coordinates.
    readonly property rect area: {
        void gpu.x; void gpu.y; void gpu.width; void gpu.height
        return backdrop ? gpu.mapToItem(backdrop, 0, 0, gpu.width, gpu.height) : Qt.rect(0, 0, 0, 0)
    }

    ShaderEffectSource {
        id: crop
        anchors.fill: parent
        sourceItem: gpu.blurredSource ? gpu.blurredSource : gpu.backdrop
        sourceRect: gpu.area
        live: true
        visible: false
    }
    Rectangle {
        id: shape
        anchors.fill: parent
        radius: gpu.radius
        visible: false
        layer.enabled: true
    }
    MultiEffect {
        anchors.fill: parent
        source: crop
        blurEnabled: !gpu.blurredSource
        blurMax: Math.max(1, Theme.glassBlurRadius)
        blur: 1.0
        saturation: gpu.blurredSource ? 0 : Theme.glassSaturation
        maskEnabled: true
        maskSource: shape
        shadowEnabled: true
        shadowColor: Theme.glassShadow
        shadowBlur: 0.6
        shadowVerticalOffset: Math.round(8 * Theme.scale)
    }
    GlassEffectFlat {
        anchors.fill: parent
        fillColor: gpu.fillColor
        radius: gpu.radius
        rimColor: gpu.rimColor
    }
}
