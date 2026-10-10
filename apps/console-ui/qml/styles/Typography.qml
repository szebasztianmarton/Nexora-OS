pragma Singleton
import QtQuick

// Type scale. Pixel sizes follow Dimensions.scale.
QtObject {
    readonly property string family: "Inter"

    readonly property int pageTitle: Dimensions.s(42)
    readonly property int display: Dimensions.s(56)
    readonly property int sectionTitle: Dimensions.s(27)
    readonly property int cardTitle: Dimensions.s(21)
    readonly property int body: Dimensions.s(20)
    readonly property int secondary: Dimensions.s(17)
    readonly property int button: Dimensions.s(19)
    readonly property int caption: Dimensions.s(15)

    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightSemiBold: Font.DemiBold
    readonly property int weightBold: Font.Bold
}
