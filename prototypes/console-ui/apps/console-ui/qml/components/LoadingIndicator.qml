import QtQuick
import QtQuick.Shapes

// Small rotating arc. Keeps rotating even with reduced motion (it conveys
// state), but is lightweight: a single Shape rotated by the scene graph.
Item {
    id: root
    property real size: Dimensions.iconSize
    property color color: Theme.accentPrimary
    property bool running: visible

    implicitWidth: size
    implicitHeight: size

    Shape {
        id: arc
        width: 24; height: 24
        scale: root.size / 24
        anchors.centerIn: parent
        ShapePath {
            strokeColor: root.color
            strokeWidth: 2.6
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M12 3 A9 9 0 1 1 3 12" }
        }
        RotationAnimator on rotation {
            from: 0; to: 360; duration: 900; loops: Animation.Infinite
            running: root.running
        }
    }
}
