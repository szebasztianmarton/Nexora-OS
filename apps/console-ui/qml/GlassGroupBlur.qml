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
        // Half resolution: after a 32-48 px blur the difference is invisible and the capture
        // and blur passes fill a quarter of the pixels.
        textureSize: Qt.size(Math.ceil(width / 2), Math.ceil(height / 2))
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
        // The output covers exactly the backdrop: without padding the edges keep their colour
        // instead of fading to transparent within blurMax of the window edge.
        autoPaddingEnabled: false
        visible: false
        layer.enabled: true
    }
}
