import QtQuick
import Nexora.Console

// Branding plus global status: running emulator, library scan, root warning, clock. A docked
// bar in dark / high contrast; a floating glass bar in the glass theme (Main.qml places it).
Rectangle {
    id: header
    required property AppController app
    required property LibraryController library

    implicitHeight: Theme.headerHeight
    color: Theme.glassActive ? "transparent" : Theme.surface

    GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing * 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            radius: 8
            rotation: 45
            color: "transparent"
            border.color: Theme.accent
            border.width: 3
            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: 2
                color: Theme.accent
            }
        }
        Item { width: 6; height: 1 }
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
        height: 32
        width: Math.min(chipRow.implicitWidth + 28, 420)
        color: Theme.surfaceRaised
        border.color: tone
        border.width: 1
        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                id: dot
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
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
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, 360)
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
        spacing: 12

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
            label: header.app.activeEmulatorName + " · " + header.app.state
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
            Timer {
                interval: 10000
                running: true
                repeat: true
                onTriggered: {
                    clock.text = Qt.formatTime(new Date(), "hh:mm")
                    date.text = Qt.formatDate(new Date(), "ddd d MMM")
                }
            }
        }
    }
}
