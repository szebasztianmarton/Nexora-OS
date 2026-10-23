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
    property bool present: true      // false: the game file is missing
    property string systemShortName
    property string unavailableReason
    property bool selected: false

    signal pressed()        // single click: select
    signal activated()      // double click: launch
    signal favoriteClicked()

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: (!present ? "file missing, " : "") + (available ? emulatorName : unavailableReason)
                            + (favorite ? ", favorite" : "")
    Accessible.onPressAction: pressed()

    readonly property real infoHeight: Math.round(70 * Theme.scale)

    z: selected ? 2 : 1
    scale: selected && !Theme.reducedMotion ? 1.05 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutQuad } }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radius
        color: tile.selected ? Theme.surfaceHover : Theme.surfaceRaised
        border.width: tile.selected ? Theme.focusWidth : 1
        border.color: tile.selected ? Theme.focusRing : Theme.border
        clip: true

        // Glass: the info plate under the art.
        GlassSurface {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: tile.infoHeight
            z: -1
            variant: "clear"
            allowBlur: false
            interactive: true
            active: tile.selected
            scale: 1.0 // the tile already lifts itself (selected: 1.05)
            radius: Theme.radius
        }

        Rectangle {
            id: art
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: tile.selected ? Theme.focusWidth : 1
            height: parent.height - tile.infoHeight - anchors.margins
            radius: Theme.radius - Math.round(3 * Theme.scale)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.artColor(tile.title, 0.34) }
                GradientStop { position: 1.0; color: Theme.artColor(tile.title + "~", 0.16) }
            }
            opacity: tile.available && tile.present ? 1.0 : 0.45

            // Square off the bottom corners against the info area.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: Theme.artColor(tile.title + "~", 0.16)
            }

            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: Theme.initials(tile.title)
                color: Theme.artInk
                font.pixelSize: Math.min(art.height * 0.42, 64 * Theme.scale)
                font.bold: true
                font.letterSpacing: 2
            }
        }

        Rectangle {
            objectName: "missingBadge"
            visible: !tile.present
            anchors.left: art.left
            anchors.top: art.top
            anchors.margins: Theme.space2
            height: Math.round(24 * Theme.scale)
            width: missingLabel.implicitWidth + Theme.space4
            radius: height / 2
            color: Theme.badgeScrim
            border.color: Theme.warning
            Text {
                textFormat: Text.PlainText
                id: missingLabel
                anchors.centerIn: parent
                text: "File missing"
                color: Theme.warning
                font.pixelSize: Theme.fontTiny
                font.bold: true
            }
        }
        Rectangle {
            objectName: "systemBadge"
            visible: tile.systemShortName.length > 0 && tile.present
            anchors.left: art.left
            anchors.bottom: art.bottom
            anchors.margins: Theme.space2
            height: Math.round(22 * Theme.scale)
            width: systemLabel.implicitWidth + Theme.space3
            radius: Theme.space1
            color: Theme.badgeScrim
            Text {
                textFormat: Text.PlainText
                id: systemLabel
                anchors.centerIn: parent
                text: tile.systemShortName
                color: Theme.artInk
                font.pixelSize: Theme.fontTiny
                font.bold: true
            }
        }
        Rectangle {
            id: unavailableBadge
            visible: !tile.available && tile.present
            anchors.left: art.left
            anchors.top: art.top
            anchors.margins: Theme.space2
            height: Math.round(24 * Theme.scale)
            width: badgeLabel.implicitWidth + Theme.space4
            radius: height / 2
            color: Theme.badgeScrim
            border.color: Theme.danger
            Text {
                textFormat: Text.PlainText
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
            anchors.margins: Theme.space2
            width: Math.round(30 * Theme.scale)
            height: width
            radius: width / 2
            color: Theme.badgeScrim
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: "★"
                color: Theme.favorite
                font.pixelSize: Theme.fontBody
            }
            MouseArea { anchors.fill: parent; onClicked: tile.favoriteClicked() }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            anchors.bottomMargin: Math.round(10 * Theme.scale)
            spacing: Math.round(2 * Theme.scale)

            Text {
                objectName: "gameTileTitle"
                textFormat: Text.PlainText
                width: parent.width
                text: tile.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                text: tile.available ? (tile.emulatorName.length > 0 ? tile.emulatorName : "Unknown emulator")
                                     : (tile.unavailableReason.length > 0 ? tile.unavailableReason : "No emulator available")
                color: tile.available ? Theme.textMuted : Theme.danger
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
            Text {
                textFormat: Text.PlainText
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
