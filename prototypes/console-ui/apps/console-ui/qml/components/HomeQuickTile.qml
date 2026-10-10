import QtQuick
import QtQuick.Layouts

// Quick Access tile on Home: icon, label and a short real-state caption.
FocusScope {
    id: tile

    property string label: ""
    property string caption: ""
    property string iconName: "info"
    signal clicked()

    implicitHeight: Dimensions.s(112)
    activeFocusOnTab: true
    scale: activeFocus ? 1.03 : 1
    Behavior on scale { NumberAnimation { duration: Motion.focus; easing.type: Motion.easing } }

    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.description: caption
    Accessible.onPressAction: tile.clicked()

    Keys.onPressed: function(event) {
        if (Input.isConfirm(event)) {
            event.accepted = true
            if (!event.isAutoRepeat) tile.clicked()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusLg
        color: tile.activeFocus || mouse.containsMouse ? Theme.backgroundHover : Theme.backgroundElevated
        border.color: Theme.borderSubtle
        Behavior on color { ColorAnimation { duration: Motion.focus } }
    }
    FocusFrame { shown: tile.activeFocus; radiusBase: Dimensions.radiusLg }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.spacingLg
        anchors.rightMargin: Dimensions.spacingLg
        spacing: Dimensions.spacingMd

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Dimensions.s(52)
            implicitHeight: implicitWidth
            radius: Dimensions.radiusMd
            color: tile.activeFocus ? Theme.accentPrimarySoft : Theme.backgroundSecondary
            Icon {
                anchors.centerIn: parent
                name: tile.iconName
                size: Dimensions.iconSize
                color: tile.activeFocus ? Theme.accentPrimary : Theme.textSecondary
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Dimensions.s(2)
            Text {
                Layout.fillWidth: true
                text: tile.label
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.body
                font.weight: Typography.weightSemiBold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: tile.caption
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.caption
                color: Theme.textMuted
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { tile.forceActiveFocus(); tile.clicked() }
    }
}
