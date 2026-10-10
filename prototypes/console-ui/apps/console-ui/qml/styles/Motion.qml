pragma Singleton
import QtQuick

// Central animation timings. When reduced motion is on, durations collapse to
// zero; all focus states remain visible without animation.
QtObject {
    property bool reduced: false                   // bound to Settings.reduceMotion by Main.qml

    readonly property int focus: reduced ? 0 : 130
    readonly property int card: reduced ? 0 : 150
    readonly property int dialog: reduced ? 0 : 180
    readonly property int page: reduced ? 0 : 200
    readonly property int toast: reduced ? 0 : 180

    readonly property int easing: Easing.OutCubic
}
