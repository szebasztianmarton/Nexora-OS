import QtQuick
import Nexora.Console

// Window background. In the glass theme: the dark base plus three large, soft light glows
// (pre-rendered PNGs, scripts/make-glass-glows.py) that the translucent surfaces let through.
// Static on purpose: an animated full-screen background would repaint every frame, also behind
// a running emulator. No blur or shader effects, so it costs a few textured quads and renders
// the same with the software renderer and on llvmpipe. In the dark and high-contrast themes it
// is just the plain background colour.
Rectangle {
    id: backdrop
    color: Theme.background

    Item {
        anchors.fill: parent
        visible: Theme.glassActive
        clip: true

        readonly property real unit: Math.max(width, height)

        Image {
            source: "images/glow-blue.png"
            smooth: true
            opacity: 0.38
            width: parent.unit * 0.95
            height: width
            x: -width * 0.30
            y: -height * 0.35
        }
        Image {
            source: "images/glow-violet.png"
            smooth: true
            opacity: 0.30
            width: parent.unit * 0.85
            height: width
            x: parent.width - width * 0.62
            y: parent.height * 0.10
        }
        Image {
            source: "images/glow-teal.png"
            smooth: true
            opacity: 0.22
            width: parent.unit * 0.80
            height: width
            x: parent.width * 0.18
            y: parent.height - height * 0.45
        }
        // Gentle vignette so the glows never wash out text near the edges.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.03, 0.04, 0.08, 0.10) }
                GradientStop { position: 1.0; color: Qt.rgba(0.03, 0.04, 0.08, 0.45) }
            }
        }
    }
}
