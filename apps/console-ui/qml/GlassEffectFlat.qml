import QtQuick
import Nexora.Console

// Blur-free glass layers: fill, specular highlight and rim. Used by GlassSurface whenever the
// GPU path is off (software renderer, llvmpipe, reduce transparency, a running emulator).
Item {
    id: flat
    required property color fillColor
    required property real radius
    required property color rimColor

    Rectangle {
        anchors.fill: parent
        radius: flat.radius
        color: flat.fillColor
    }
    // Specular highlight: light from above, fading out by 45 % of the height.
    Rectangle {
        anchors.fill: parent
        radius: flat.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.glassHighlight }
            GradientStop { position: 0.45; color: "transparent" }
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: flat.radius
        color: "transparent"
        border.width: 1
        border.color: flat.rimColor
    }
}
