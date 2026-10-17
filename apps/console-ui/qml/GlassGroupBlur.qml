import QtQuick
import QtQuick.Effects
import Nexora.Console

// GlassGroup's shared pass: the whole backdrop blurred once, kept off-screen as |output|.
Item {
    id: pass
    property Item backdrop: null
    readonly property Item output: blurred

    ShaderEffectSource {
        id: source
        sourceItem: pass.backdrop
        width: pass.backdrop ? pass.backdrop.width : 0
        height: pass.backdrop ? pass.backdrop.height : 0
        live: true
        visible: false
    }
    MultiEffect {
        id: blurred
        width: source.width
        height: source.height
        source: source
        blurEnabled: true
        blurMax: Math.max(1, Theme.glassBlurRadius)
        blur: 1.0
        saturation: Theme.glassSaturation
        visible: false
        layer.enabled: true
    }
}
