import QtQuick

// Status pill: icon + text, coloured by status. Never relies on colour alone.
// status keys: available/success, warning, disabled, unknown, missing,
// not-executable, invalid/error, info, neutral
Rectangle {
    id: pill
    property string status: "neutral"
    property string text: ""

    readonly property color tone: Theme.statusColor(status)
    readonly property string iconName: {
        switch (status) {
        case "available": case "success": return "success"
        case "missing": case "not-executable": case "invalid": case "error": return "error"
        case "warning": case "disabled": return "warning"
        case "unknown": return "clock"
        default: return "info"
        }
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: text

    implicitHeight: Dimensions.s(34)
    implicitWidth: row.implicitWidth + Dimensions.spacingMd * 2
    radius: height / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.12)
    border.width: 1
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.45)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Dimensions.spacingXs
        Icon {
            name: pill.iconName
            size: Dimensions.iconSizeSmall * 0.9
            color: pill.tone
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: pill.text
            anchors.verticalCenter: parent.verticalCenter
            font.family: Typography.family
            font.pixelSize: Typography.caption
            font.weight: Typography.weightSemiBold
            color: pill.tone
        }
    }
}
