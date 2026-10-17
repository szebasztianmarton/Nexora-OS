import QtQuick
import Nexora.Console

// One shared blur pass for several GlassSurfaces (≈ SwiftUI GlassEffectContainer): surfaces
// inside the group crop |blurredSource| instead of blurring the backdrop each. On the flat
// path it is a plain Item. |backdrop| must not contain the group.
Item {
    id: group
    property Item backdrop: null
    // False when |backdrop| is missing or contains the group (the pass would render itself).
    readonly property bool backdropUsable: {
        if (!backdrop)
            return false
        for (let p = group; p; p = p.parent) {
            if (p === backdrop)
                return false
        }
        return true
    }
    readonly property Item blurredSource: backdropUsable && loader.item ? loader.item.output : null

    Loader {
        id: loader
        active: Theme.glassBlur && group.backdropUsable
        source: "GlassGroupBlur.qml"
        onLoaded: item.backdrop = Qt.binding(() => group.backdrop)
        onStatusChanged: if (status === Loader.Error) console.debug("nexora.ui: glass blur unavailable, using flat glass")
    }
}
