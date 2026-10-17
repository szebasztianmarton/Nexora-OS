import QtQuick
import Nexora.Console

// One shared blur pass for several GlassSurfaces (≈ SwiftUI GlassEffectContainer): surfaces
// inside the group crop |blurredSource| instead of blurring the backdrop each. On the flat
// path it is a plain Item. |backdrop| must not contain the group.
Item {
    id: group
    property Item backdrop: null
    readonly property Item blurredSource: loader.item ? loader.item.output : null

    Loader {
        id: loader
        active: Theme.glassBlur && group.backdrop !== null
        source: "GlassGroupBlur.qml"
        onLoaded: item.backdrop = Qt.binding(() => group.backdrop)
        onStatusChanged: if (status === Loader.Error) console.debug("nexora.ui: glass blur unavailable, using flat glass")
    }
}
