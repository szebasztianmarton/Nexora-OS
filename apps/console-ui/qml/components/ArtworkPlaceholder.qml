import QtQuick

// Designed stand-in for missing or unreadable artwork: a subtle gradient
// derived from the title, the title's initials and a controller glyph.
Rectangle {
    id: placeholder
    property string title: ""
    property bool showTitle: true

    readonly property var _colors: Theme.placeholderColors(title)
    gradient: Gradient {
        GradientStop { position: 0.0; color: placeholder._colors[0] }
        GradientStop { position: 1.0; color: placeholder._colors[1] }
    }

    function initials(text) {
        var words = String(text).replace(/[^A-Za-z0-9 ]/g, " ").split(" ").filter(function(w) { return w.length > 0 })
        if (words.length === 0) return "?"
        if (words.length === 1) return words[0].substring(0, 2).toUpperCase()
        return (words[0][0] + words[1][0]).toUpperCase()
    }

    Icon {
        name: "gamepad"
        size: Math.min(parent.width, parent.height) * 0.42
        color: Qt.rgba(1, 1, 1, 0.07)
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: -size * 0.12
        strokeWidth: 1.4
    }
    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: placeholder.showTitle ? -parent.height * 0.08 : 0
        text: placeholder.initials(placeholder.title)
        font.family: Typography.family
        font.pixelSize: Math.min(parent.width, parent.height) * 0.26
        font.weight: Typography.weightBold
        color: Qt.rgba(1, 1, 1, 0.82)
    }
    Text {
        visible: placeholder.showTitle
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Dimensions.spacingMd
        horizontalAlignment: Text.AlignHCenter
        text: placeholder.title
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        font.family: Typography.family
        font.pixelSize: Math.max(Typography.caption, parent.width * 0.075)
        font.weight: Typography.weightMedium
        color: Qt.rgba(1, 1, 1, 0.6)
    }
}
