import QtQuick

// Heading above a group of settings rows, with an optional explanation.
Column {
    id: section
    property string title: ""
    property string description: ""
    width: parent ? parent.width : implicitWidth
    spacing: Dimensions.s(4)
    topPadding: Dimensions.spacingMd

    Text {
        width: parent.width
        text: section.title
        elide: Text.ElideRight
        font.family: Typography.family
        font.pixelSize: Typography.cardTitle
        font.weight: Typography.weightSemiBold
        color: Theme.textPrimary
    }
    Text {
        width: parent.width
        visible: text !== ""
        text: section.description
        wrapMode: Text.WordWrap
        font.family: Typography.family
        font.pixelSize: Typography.secondary
        color: Theme.textSecondary
        lineHeight: 1.1
    }
}
