import QtQuick
import QtQuick.Shapes

// Temporary Nexora brand mark (geometric "N" in a rounded hexagon) plus
// wordmark. Replace the mark with an official asset when one exists.
Row {
    id: logo
    property bool showWordmark: true
    property real markSize: Dimensions.s(40)
    spacing: Dimensions.spacingSm

    Item {
        width: logo.markSize; height: logo.markSize
        anchors.verticalCenter: parent.verticalCenter
        Shape {
            width: 40; height: 40
            scale: logo.markSize / 40
            transformOrigin: Item.TopLeft
            ShapePath {
                strokeWidth: 0
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: 0; x2: 40; y2: 40
                    GradientStop { position: 0; color: Theme.accentPrimary }
                    GradientStop { position: 1; color: Theme.accentSecondary }
                }
                PathSvg { path: "M20 2 L35.6 11 V29 L20 38 L4.4 29 V11 Z" }
            }
            ShapePath {
                strokeColor: Theme.backgroundPrimary
                strokeWidth: 3.6
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M14 27 V13 L26 27 V13" }
            }
        }
    }
    Column {
        visible: logo.showWordmark
        anchors.verticalCenter: parent.verticalCenter
        spacing: -Dimensions.s(2)
        Text {
            text: "NEXORA"
            font.family: Typography.family
            font.pixelSize: Dimensions.s(22)
            font.weight: Typography.weightBold
            font.letterSpacing: Dimensions.s(3)
            color: Theme.textPrimary
        }
        Text {
            text: "OS"
            font.family: Typography.family
            font.pixelSize: Dimensions.s(12)
            font.weight: Typography.weightSemiBold
            font.letterSpacing: Dimensions.s(5)
            color: Theme.accentPrimary
        }
    }
}
