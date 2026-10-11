import QtQuick
import Nexora.Console

// One game: placeholder cover art, title, emulator, favorite star and availability.
// Used by the Home rows and the Games grid; the view decides whether it is |selected|.
Item {
    id: tile
    property real gameId: 0
    property string title
    property string emulatorName
    property bool favorite: false
    property string lastPlayed
    property bool available: true
    property string unavailableReason
    property bool selected: false

    signal pressed()        // single click: select
    signal activated()      // double click: launch
    signal favoriteClicked()

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: (available ? emulatorName : unavailableReason) + (favorite ? ", favorite" : "")
    Accessible.onPressAction: pressed()

    readonly property int infoHeight: 70

    z: selected ? 2 : 1
    scale: selected ? 1.05 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutQuad } }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radius
        color: tile.selected ? Theme.surfaceHover : Theme.surfaceRaised
        border.width: tile.selected ? Theme.focusWidth : 1
        border.color: tile.selected ? Theme.accent : Theme.border
        clip: true

        Rectangle {
            id: art
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: tile.selected ? Theme.focusWidth : 1
            height: parent.height - tile.infoHeight - anchors.margins
            radius: Theme.radius - 3
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.artColor(tile.title, 0.34) }
                GradientStop { position: 1.0; color: Theme.artColor(tile.title + "~", 0.16) }
            }
            opacity: tile.available ? 1.0 : 0.45

            // Square off the bottom corners against the info area.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: Theme.artColor(tile.title + "~", 0.16)
            }

            Text {
                anchors.centerIn: parent
                text: Theme.initials(tile.title)
                color: Qt.rgba(1, 1, 1, 0.85)
                font.pixelSize: Math.min(art.height * 0.42, 64)
                font.bold: true
                font.letterSpacing: 2
            }
        }

        Rectangle {
            id: unavailableBadge
            visible: !tile.available
            anchors.left: art.left
            anchors.top: art.top
            anchors.margins: 8
            height: 24
            width: badgeLabel.implicitWidth + 16
            radius: 12
            color: Qt.rgba(0, 0, 0, 0.6)
            border.color: Theme.danger
            Text {
                id: badgeLabel
                anchors.centerIn: parent
                text: "Unavailable"
                color: Theme.danger
                font.pixelSize: Theme.fontTiny
                font.bold: true
            }
        }

        Rectangle {
            objectName: "favoriteBadge"
            visible: tile.favorite
            anchors.right: art.right
            anchors.top: art.top
            anchors.margins: 8
            width: 30
            height: 30
            radius: 15
            color: Qt.rgba(0, 0, 0, 0.55)
            Text {
                anchors.centerIn: parent
                text: "★"
                color: Theme.favorite
                font.pixelSize: 18
            }
            MouseArea { anchors.fill: parent; onClicked: tile.favoriteClicked() }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 10
            spacing: 2

            Text {
                width: parent.width
                text: tile.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: tile.available ? (tile.emulatorName.length > 0 ? tile.emulatorName : "Unknown emulator")
                                     : (tile.unavailableReason.length > 0 ? tile.unavailableReason : "No emulator available")
                color: tile.available ? Theme.textMuted : Theme.danger
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: tile.lastPlayed.length > 0 ? "Played " + tile.lastPlayed : "Never played"
                color: Theme.textFaint
                font.pixelSize: Theme.fontTiny
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: tile.pressed()
        onDoubleClicked: tile.activated()
    }
}
