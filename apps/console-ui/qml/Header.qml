import QtQuick
import Nexora.Console

// Branding plus global status: running emulator, library scan, root warning, clock. A docked
// bar in dark / high contrast; a floating glass bar in the glass theme (Main.qml places it).
Rectangle {
    id: header
    required property AppController app
    required property LibraryController library
    property GameSessionManager sessions: null
    // The game's title while a session runs; the emulator's name for a plain emulator launch.
    readonly property string title: sessions !== null && sessions.active.title ? sessions.active.title
                                                                            : app.activeEmulatorName

    implicitHeight: Theme.headerHeight
    color: Theme.glassActive ? "transparent" : Theme.surface

    GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL }
    // Glass: the bar floats over the scrolling pages; presses on it (also between its controls)
    // must not reach a card scrolled underneath. Below the bar's own content in z.
    MouseArea {
        objectName: "headerClickShield"
        anchors.fill: parent
        z: -1
        enabled: Theme.glassActive
        hoverEnabled: false
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = false // scrolling still reaches the page
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing * 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.round(28 * Theme.scale)
            height: width
            radius: Theme.space2
            rotation: 45
            color: "transparent"
            border.color: Theme.accent
            border.width: Math.round(3 * Theme.scale)
            Rectangle {
                anchors.centerIn: parent
                width: Theme.space2
                height: width
                radius: Math.round(2 * Theme.scale)
                color: Theme.accent
            }
        }
        Item { width: Math.round(6 * Theme.scale); height: 1 }
        Text {
            textFormat: Text.PlainText
            anchors.verticalCenter: parent.verticalCenter
            text: "NEXORA"
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.bold: true
            font.letterSpacing: 4
        }
        Text {
            textFormat: Text.PlainText
            anchors.verticalCenter: parent.verticalCenter
            text: "OS"
            color: Theme.accent
            font.pixelSize: Theme.fontTitle
            font.letterSpacing: 4
        }
    }

    component Chip: Rectangle {
        id: chip
        property string label
        property color tone: Theme.textMuted
        property bool pulsing: false
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        radius: height / 2
        // 32 px at 100 %: grows with the scale so the text never overflows the chip.
        height: Math.round(32 * Theme.scale)
        width: Math.min(chipRow.implicitWidth + Math.round(28 * Theme.scale), Math.round(420 * Theme.scale))
        color: Theme.surfaceRaised
        border.color: tone
        border.width: 1
        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: Theme.space2
            Rectangle {
                id: dot
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.space2
                height: width
                radius: width / 2
                color: chip.tone
                SequentialAnimation on opacity {
                    // A continuous pulse, not a discrete transition: reducedMotion stops it
                    // outright rather than collapsing its duration to 0.
                    running: chip.pulsing && chip.visible && !Theme.reducedMotion
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.25; duration: 600 }
                    NumberAnimation { to: 1.0; duration: 600 }
                    onStopped: dot.opacity = 1.0
                }
            }
            Text {
                textFormat: Text.PlainText
                objectName: "chipLabel"
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, Math.round(360 * Theme.scale))
                text: chip.label
                color: Theme.text
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space3

        Chip {
            objectName: "rootChip"
            visible: header.app.runningAsRoot
            label: "Running as root"
            tone: Theme.danger
        }
        Chip {
            objectName: "scanChip"
            visible: header.library.scanning
            label: "Scanning library…"
            tone: Theme.accent
            pulsing: true
        }
        Chip {
            objectName: "stateChip"
            visible: header.app.busy
            label: header.title + " · " + Theme.capitalize(header.app.state)
            tone: Theme.statusColor(header.app.state)
            pulsing: header.app.state !== "running"
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            Text {
                textFormat: Text.PlainText
                id: clock
                objectName: "clock"
                anchors.right: parent.right
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                font.bold: true
                text: Qt.formatTime(new Date(), "hh:mm")
            }
            Text {
                textFormat: Text.PlainText
                id: date
                anchors.right: parent.right
                color: Theme.textFaint
                font.pixelSize: Theme.fontTiny
                text: Qt.formatDate(new Date(), "ddd d MMM")
            }
            // Whenever the header can be seen, during a session too: it stays on screen above the
            // running overlay.
            Timer {
                objectName: "headerClockTimer"
                interval: 10000
                running: header.visible
                triggeredOnStart: true
                repeat: true
                onTriggered: {
                    clock.text = Qt.formatTime(new Date(), "hh:mm")
                    date.text = Qt.formatDate(new Date(), "ddd d MMM")
                }
            }
        }
    }
}
