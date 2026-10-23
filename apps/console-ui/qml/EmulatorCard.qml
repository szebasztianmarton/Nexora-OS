import QtQuick
import Nexora.Console

Rectangle {
    id: card
    required property int index
    required property string emulatorId
    required property string name
    required property string status
    required property string statusText
    required property string detail
    required property bool available
    required property var profiles
    required property string extensions

    readonly property bool selected: activeFocus || ListView.isCurrentItem && ListView.view.activeFocus
    readonly property int enabledProfileCount: {
        let n = 0
        for (let i = 0; i < profiles.length; ++i)
            n += profiles[i].enabled ? 1 : 0
        return n
    }

    signal activated()

    Accessible.role: Accessible.Button
    Accessible.name: name + ", " + statusText
    Accessible.description: "Starts the emulator without a game"
    Accessible.onPressAction: activated()

    width: Math.round(300 * Theme.scale)
    height: Math.round(360 * Theme.scale)
    radius: Theme.radius
    GlassSurface {
        anchors.fill: parent
        z: -1
        variant: "clear"
        allowBlur: false
        interactive: true
        active: card.selected // the ListView holds the focus, not the card
        scale: 1.0 // the card already lifts itself (selected: 1.04) and has its own focus ring
        radius: card.radius
    }
    color: selected ? Theme.surfaceHover : Theme.surfaceRaised
    opacity: available ? 1.0 : 0.65
    border.width: selected ? Theme.focusWidth : 1
    border.color: selected ? Theme.focusRing : Theme.border
    scale: selected && !Theme.reducedMotion ? 1.04 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast } }

    // The list caps the card's height to the screen (720p at a large interface scale), so the
    // content shares out what is there: the unavailable reason first, then the file types, and
    // the art takes what is left (hidden when that is too little to show the monogram).
    FontMetrics {
        id: smallMetrics
        font.pixelSize: Theme.fontSmall
    }
    readonly property real lineHeight: Math.ceil(smallMetrics.lineSpacing)
    // Room under the status pill for the files text, the detail text and the art.
    readonly property real textRoom: content.height - nameText.height - statusPill.height - 2 * content.spacing

    Column {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.spacing * 1.25
        spacing: Theme.space3
        clip: true // last resort: nothing draws outside the card even at the smallest height

        Rectangle {
            objectName: "emulatorArt"
            width: parent.width
            height: Math.max(0, Math.min(Math.max(Math.round(90 * Theme.scale), card.height * 0.38),
                                         content.height - nameText.height - statusPill.height - filesText.height
                                         - (detailText.visible ? detailText.height + content.spacing : 0)
                                         - 3 * content.spacing))
            visible: height >= Theme.iconLarge
            radius: Theme.radius - Theme.space1
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.artColor(card.emulatorId, 0.30) }
                GradientStop { position: 1.0; color: Theme.surface }
            }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                text: card.name.length > 0 ? card.name.charAt(0).toUpperCase() : "?"
                color: Theme.artInk
                font.pixelSize: Math.max(Theme.fontTiny, Math.min(Math.round(64 * Theme.scale), Math.round(parent.height * 0.7)))
                font.bold: true
            }
        }

        Text {
            id: nameText
            textFormat: Text.PlainText
            width: parent.width
            text: card.name
            color: Theme.text
            font.pixelSize: Theme.fontLarge
            font.bold: true
            elide: Text.ElideRight
        }

        Rectangle {
            id: statusPill
            objectName: "statusPill"
            height: Math.round(28 * Theme.scale)
            width: pillLabel.implicitWidth + Theme.space5
            radius: height / 2
            color: "transparent"
            border.color: Theme.statusColor(card.status)
            border.width: 2
            Text {
                textFormat: Text.PlainText
                id: pillLabel
                anchors.centerIn: parent
                text: card.statusText
                color: Theme.statusColor(card.status)
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
        }

        Text {
            id: filesText
            objectName: "emulatorFiles"
            textFormat: Text.PlainText
            width: parent.width
            text: (card.extensions.length > 0 ? "Files: " + card.extensions : "No game file types")
                  + (card.enabledProfileCount > 0
                     ? "\n" + card.enabledProfileCount + " launch profile" + (card.enabledProfileCount === 1 ? "" : "s")
                     : "")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            maximumLineCount: Math.max(1, Math.min(3, Math.floor(
                (card.textRoom - (detailText.visible ? detailText.height + content.spacing : 0)) / card.lineHeight)))
            elide: Text.ElideRight
        }

        Text {
            id: detailText
            textFormat: Text.PlainText
            width: parent.width
            objectName: "emulatorDetail"
            text: card.detail
            visible: text.length > 0
            color: card.status === "failed" || card.status === "unavailable" ? Theme.danger : Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            // Keeps one line (and the gap) for the files text above it.
            maximumLineCount: Math.max(1, Math.min(3, Math.floor(
                (card.textRoom - content.spacing - card.lineHeight) / card.lineHeight)))
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            card.ListView.view.currentIndex = card.index
            card.ListView.view.forceActiveFocus()
        }
        onDoubleClicked: card.activated()
    }
}
