pragma Singleton
import QtQuick

// Spacing, sizes and radii. Everything is expressed for a 1920x1080 reference
// and multiplied by `scale`, which Main.qml keeps in sync with the window size
// and the user's interface-scale preference.
QtObject {
    id: dims

    property real scale: 1.0                       // set by Main.qml
    function s(value) { return Math.round(value * scale) }

    // Window is considered "compact" (e.g. 1280x720 or narrow dev windows)
    property bool compact: false                   // set by Main.qml

    // Spacing scale
    readonly property int spacingXs: s(6)
    readonly property int spacingSm: s(10)
    readonly property int spacingMd: s(16)
    readonly property int spacingLg: s(24)
    readonly property int spacingXl: s(36)
    readonly property int spacingXxl: s(52)

    // Page layout
    readonly property int pagePaddingH: s(52)
    readonly property int pagePaddingV: s(38)
    readonly property int safeMargin: s(24)        // TV overscan safety around the whole UI

    // Sidebar
    readonly property int sidebarWidth: s(248)
    readonly property int sidebarCompactWidth: s(92)
    readonly property int navItemHeight: s(58)

    // Controls
    readonly property int buttonHeight: s(56)
    readonly property int buttonHeightSmall: s(44)
    readonly property int controlMinWidth: s(140)
    readonly property int iconSize: s(26)
    readonly property int iconSizeSmall: s(20)
    readonly property int iconSizeLarge: s(40)
    readonly property int rowHeight: s(72)

    // Game cards (portrait 2:3 artwork)
    readonly property int cardGap: s(22)
    readonly property int cardMinWidth: s(190)
    readonly property real cardAspect: 1.5         // height = width * 1.5
    readonly property int cardTitleHeight: s(64)
    readonly property int rowCardWidth: s(210)     // cards in horizontal rows

    // Radii
    readonly property int radiusSm: s(8)
    readonly property int radiusMd: s(12)
    readonly property int radiusLg: s(18)

    // Focus
    readonly property int focusWidth: Math.max(2, s(3))
    readonly property int focusOffset: Math.max(2, s(4))
    readonly property real focusScale: 1.035

    // Bottom hint bar
    readonly property int hintBarHeight: s(56)
}
