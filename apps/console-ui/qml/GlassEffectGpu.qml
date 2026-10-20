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

    // The surface's rectangle in the backdrop's coordinates. Ancestors move, scroll and scale
    // without any signal on this item, so it is refreshed once per frame (after animations ran)
    // while this item exists, and only reassigned when it really changed.
    property rect area: Qt.rect(0, 0, 0, 0)
    function refreshArea() {
        const r = backdrop ? gpu.mapToItem(backdrop, 0, 0, gpu.width, gpu.height) : Qt.rect(0, 0, 0, 0)
        if (r.x !== area.x || r.y !== area.y || r.width !== area.width || r.height !== area.height)
            area = r
    }
    onBackdropChanged: refreshArea()
    onWidthChanged: refreshArea()
    onHeightChanged: refreshArea()
    Component.onCompleted: refreshArea()
    Connections {
        target: gpu.Window.window
        function onAfterAnimating() { gpu.refreshArea() }
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
        // No drop shadow: the mask clips it away (the crop is opaque inside the shape), and
        // MultiEffect builds its blur levels whenever blur or shadow is on, so a shadow would
        // make every grouped surface blur again.
        shadowEnabled: false
        // The item never draws outside its bounds; with padding the blur would fade to
        // transparent towards the edges.
        autoPaddingEnabled: false
    }
    GlassEffectFlat {
        anchors.fill: parent
        fillColor: gpu.fillColor
        radius: gpu.radius
        rimColor: gpu.rimColor
    }
}
