import QtQuick
import Nexora.Console

// Game bar: a quick overlay in the style of the Xbox Game Bar. Guide (or Ctrl+G) opens it,
// also on top of a running emulator; Guide, Back or a click outside the widgets closes it.
//
// Widgets: now playing (session time, back to game, stop), performance (CPU with history,
// memory, load), audio and power. Left/Right move between widgets (and adjust the focused
// volume slider), Up/Down move inside a widget.
FocusScope {
    id: bar
    objectName: "gameBar"
    required property GameBarController gameBar
    required property AppController app
    required property SettingsController settings
    required property SystemController system

    // Dashboard navigation from the idle "now playing" widget (home, games, settings).
    signal navigate(string tab)

    readonly property int panelWidth: Math.max(280, Math.min(360, (width - Theme.spacing * 6) / 3))
    readonly property var hints: [{ button: "Guide / B", action: "Close" },
                                  { button: "A", action: "Select" },
                                  { button: "← →", action: "Widgets / volume" }]

    function focusDefault() {
        if (bar.app.busy)
            backToGame.forceActiveFocus()
        else
            homeButton.forceActiveFocus()
    }
    function formatDuration(seconds) {
        if (seconds < 0)
            return "—"
        const h = Math.floor(seconds / 3600)
        const m = Math.floor(seconds / 60) % 60
        const s = seconds % 60
        const mm = (h > 0 && m < 10 ? "0" : "") + m
        return (h > 0 ? h + ":" : "") + mm + ":" + (s < 10 ? "0" : "") + s
    }

    visible: gameBar.open || opacity > 0.01
    opacity: gameBar.open ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

    Accessible.role: Accessible.Dialog
    Accessible.name: "Game bar"

    Connections {
        target: bar.app
        // The focused widget content changes between idle and running; keep focus inside.
        function onBusyChanged() {
            if (bar.gameBar.open)
                Qt.callLater(bar.focusDefault)
        }
    }

    // Scrim: dims the dashboard or game underneath; a click on it closes the bar.
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.55
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onClicked: bar.gameBar.close()
    }

    Item {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.spacing * 1.5
        // Slides down from the top edge as it fades in, like the Xbox Game Bar.
        transform: Translate { y: (1 - bar.opacity) * -24 }

        // --- Widget menu: brand, clock, controller status ------------------------------
        Rectangle {
            id: menuBar
            objectName: "gameBarMenu"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: Math.min(parent.width, menuRow.implicitWidth + Theme.spacing * 3)
            height: 56
            radius: height / 2
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            MouseArea { anchors.fill: parent }

            Row {
                id: menuRow
                anchors.centerIn: parent
                spacing: Theme.spacing

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    radius: 6
                    rotation: 45
                    color: "transparent"
                    border.color: Theme.accent
                    border.width: 3
                    Rectangle {
                        anchors.centerIn: parent
                        width: 6
                        height: 6
                        radius: 2
                        color: Theme.accent
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "NEXORA BAR"
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                    font.letterSpacing: 3
                }
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 28; color: Theme.border }
                Text {
                    id: barClock
                    objectName: "gameBarClock"
                    anchors.verticalCenter: parent.verticalCenter
                    text: Qt.formatTime(new Date(), "hh:mm")
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                    Timer {
                        interval: 5000
                        running: bar.visible
                        repeat: true
                        triggeredOnStart: true
                        onTriggered: barClock.text = Qt.formatTime(new Date(), "hh:mm")
                    }
                }
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 1; height: 28; color: Theme.border }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, 260)
                    elide: Text.ElideRight
                    text: bar.app.inputDevices.length > 0 ? "Controller: " + bar.app.inputDevices[0]
                                                          : "No controller (" + bar.app.inputBackend + ")"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        // --- Widgets --------------------------------------------------------------------
        Row {
            id: widgets
            anchors.top: menuBar.bottom
            anchors.topMargin: Theme.spacing * 1.5
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing

            // Now playing -----------------------------------------------------------------
            Widget {
                objectName: "nowPlayingWidget"
                title: "Now playing"
                glyph: "▶"

                Text {
                    objectName: "gameBarTitle"
                    width: parent.width
                    elide: Text.ElideRight
                    text: bar.app.busy ? bar.app.activeEmulatorName : "No game running"
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                Row {
                    visible: bar.app.busy
                    spacing: Theme.spacing
                    Text {
                        text: Theme.capitalize(bar.app.state)
                        color: Theme.statusColor(bar.app.state)
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                    Text {
                        objectName: "sessionTime"
                        text: "⏱ " + bar.formatDuration(bar.gameBar.sessionSeconds)
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }
                }
                Text {
                    visible: !bar.app.busy
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Pick something to play from your library."
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
                Item { width: 1; height: 4 }

                // Running: back to the game / stop it.
                NxButton {
                    id: backToGame
                    objectName: "gameBarResume"
                    visible: bar.app.busy
                    width: parent.width
                    glyph: "▶"
                    kind: "primary"
                    text: "Back to game"
                    KeyNavigation.down: stopGame
                    KeyNavigation.right: volume
                    onClicked: bar.gameBar.close()
                }
                NxButton {
                    id: stopGame
                    objectName: "gameBarStop"
                    visible: bar.app.busy
                    enabled: bar.app.state !== "stopping"
                    width: parent.width
                    glyph: "■"
                    kind: "danger"
                    text: "Stop emulator"
                    requireConfirm: true
                    confirmText: "Press again to stop"
                    KeyNavigation.up: backToGame
                    KeyNavigation.right: volume
                    onClicked: bar.app.stop()
                }

                // Idle: jump to a dashboard tab.
                NxButton {
                    id: homeButton
                    objectName: "gameBarHome"
                    visible: !bar.app.busy
                    width: parent.width
                    glyph: "⌂"
                    text: "Home"
                    KeyNavigation.down: gamesButton
                    KeyNavigation.right: volume
                    onClicked: { bar.navigate("home"); bar.gameBar.close() }
                }
                NxButton {
                    id: gamesButton
                    objectName: "gameBarGames"
                    visible: !bar.app.busy
                    width: parent.width
                    glyph: "▦"
                    text: "Games"
                    KeyNavigation.up: homeButton
                    KeyNavigation.down: settingsButton
                    KeyNavigation.right: volume
                    onClicked: { bar.navigate("games"); bar.gameBar.close() }
                }
                NxButton {
                    id: settingsButton
                    objectName: "gameBarSettings"
                    visible: !bar.app.busy
                    width: parent.width
                    glyph: "⚙"
                    text: "Settings"
                    KeyNavigation.up: gamesButton
                    KeyNavigation.right: volume
                    onClicked: { bar.navigate("settings"); bar.gameBar.close() }
                }
            }

            // Performance -------------------------------------------------------------------
            Widget {
                objectName: "performanceWidget"
                title: "Performance"
                glyph: "◔"

                Row {
                    spacing: Theme.spacing
                    Text {
                        id: cpuText
                        objectName: "cpuValue"
                        text: bar.gameBar.cpuPercent >= 0 ? bar.gameBar.cpuPercent + "%" : "—"
                        color: Theme.text
                        font.pixelSize: Theme.fontHero
                        font.bold: true
                    }
                    Text {
                        anchors.baseline: cpuText.baseline
                        text: "CPU"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        font.letterSpacing: 1
                    }
                }

                // CPU history (last 30 s), newest on the right.
                Canvas {
                    id: sparkline
                    objectName: "cpuGraph"
                    width: parent.width
                    height: 64
                    property var values: bar.gameBar.cpuHistory
                    onValuesChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = String(Theme.surfaceRaised)
                        ctx.fillRect(0, 0, width, height)
                        const v = values
                        if (!v || v.length < 2)
                            return
                        const step = width / (bar.gameBar.historyLength - 1)
                        const x0 = width - (v.length - 1) * step
                        const yOf = (p) => height - 2 - (height - 4) * p / 100
                        ctx.beginPath()
                        ctx.moveTo(x0, height)
                        for (let i = 0; i < v.length; ++i)
                            ctx.lineTo(x0 + i * step, yOf(v[i]))
                        ctx.lineTo(width, height)
                        ctx.closePath()
                        ctx.fillStyle = String(Theme.accentSoft)
                        ctx.fill()
                        ctx.beginPath()
                        for (let j = 0; j < v.length; ++j) {
                            if (j === 0)
                                ctx.moveTo(x0, yOf(v[0]))
                            else
                                ctx.lineTo(x0 + j * step, yOf(v[j]))
                        }
                        ctx.strokeStyle = String(Theme.accent)
                        ctx.lineWidth = 2
                        ctx.stroke()
                    }
                }

                Item { width: 1; height: 4 }
                Meter {
                    objectName: "memoryMeter"
                    label: "Memory"
                    percent: bar.gameBar.memoryPercent
                    detail: bar.gameBar.memoryTotalMiB > 0
                            ? (bar.gameBar.memoryUsedMiB / 1024).toFixed(1) + " / "
                              + (bar.gameBar.memoryTotalMiB / 1024).toFixed(1) + " GiB"
                            : "Unavailable"
                }
                Text {
                    text: "Load (1 min): " + (bar.gameBar.load1 >= 0 ? bar.gameBar.load1.toFixed(2) : "—")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
            }

            // Audio & power -----------------------------------------------------------------
            Widget {
                objectName: "audioPowerWidget"
                title: "Audio & power"
                glyph: "♪"

                Rectangle {
                    id: volume
                    objectName: "gameBarVolume"
                    width: parent.width
                    height: 64
                    radius: 10
                    activeFocusOnTab: true
                    color: activeFocus ? Theme.surfaceHover : Theme.surfaceRaised
                    border.width: activeFocus ? Theme.focusWidth : 1
                    border.color: activeFocus ? Theme.accent : Theme.border
                    Accessible.role: Accessible.Slider
                    Accessible.name: "Volume"
                    Accessible.description: bar.settings.volume + "%"

                    function adjust(step) {
                        bar.settings.volume = Math.max(0, Math.min(100, bar.settings.volume + step))
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8
                        Row {
                            width: parent.width
                            Text {
                                width: parent.width - volumeValue.width
                                text: bar.settings.volume === 0 ? "Volume (muted)" : "Volume"
                                color: Theme.text
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }
                            Text {
                                id: volumeValue
                                objectName: "gameBarVolumeValue"
                                text: bar.settings.volume + "%"
                                color: Theme.text
                                font.pixelSize: Theme.fontSmall
                            }
                        }
                        Rectangle {
                            width: parent.width
                            height: 6
                            radius: 3
                            color: Theme.border
                            Rectangle {
                                width: parent.width * bar.settings.volume / 100
                                height: parent.height
                                radius: 3
                                color: Theme.accent
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: (mouse) => {
                            volume.forceActiveFocus()
                            bar.settings.volume = Math.round(100 * mouse.x / width / 5) * 5
                        }
                    }
                    Keys.onLeftPressed: adjust(-5)
                    Keys.onRightPressed: adjust(5)
                    KeyNavigation.down: rebootButton
                }
                Text {
                    visible: !bar.settings.volumeSupported
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Saved as a preference; applied on Nexora OS hardware."
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontTiny
                }
                Item { width: 1; height: 4 }
                NxButton {
                    id: rebootButton
                    objectName: "gameBarReboot"
                    width: parent.width
                    compact: true
                    glyph: "↻"
                    text: "Reboot"
                    enabled: bar.system.powerSupported
                    requireConfirm: true
                    confirmText: "Press again to reboot"
                    KeyNavigation.up: volume
                    KeyNavigation.down: powerButton
                    KeyNavigation.left: bar.app.busy ? backToGame : homeButton
                    onClicked: bar.system.reboot()
                }
                NxButton {
                    id: powerButton
                    objectName: "gameBarPowerOff"
                    width: parent.width
                    compact: true
                    glyph: "⏻"
                    kind: "danger"
                    text: "Power off"
                    enabled: bar.system.powerSupported
                    requireConfirm: true
                    confirmText: "Press again to power off"
                    KeyNavigation.up: rebootButton
                    KeyNavigation.left: bar.app.busy ? backToGame : homeButton
                    onClicked: bar.system.powerOff()
                }
                Text {
                    visible: !bar.system.powerSupported
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Power control is not available on this platform."
                    color: Theme.textFaint
                    font.pixelSize: Theme.fontTiny
                }
            }
        }
    }

    // A widget card: title strip plus a column of content.
    component Widget: Rectangle {
        id: widget
        property string title
        property string glyph
        default property alias contents: body.data

        width: bar.panelWidth
        height: body.implicitHeight + header.height + Theme.spacing * 2
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        clip: true
        // Clicks inside a widget never reach the scrim (which would close the bar).
        MouseArea { anchors.fill: parent }

        Rectangle {
            id: header
            width: parent.width
            height: 40
            color: Theme.surfaceRaised
            radius: Theme.radius
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.radius; color: parent.color }
            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                Text {
                    text: widget.glyph
                    color: Theme.accent
                    font.pixelSize: Theme.fontBody
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: widget.title.toUpperCase()
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontTiny
                    font.bold: true
                    font.letterSpacing: 2
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
        Column {
            id: body
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Theme.spacing
            spacing: 10
        }
    }

    // Labelled horizontal usage meter; percent < 0 means unknown.
    component Meter: Column {
        id: meter
        property string label
        property int percent: -1
        property string detail
        width: parent ? parent.width : 0
        spacing: 6
        Row {
            width: parent.width
            Text {
                width: parent.width - meterDetail.width
                text: meter.label
                color: Theme.text
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
            Text {
                id: meterDetail
                text: meter.detail
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }
        }
        Rectangle {
            width: parent.width
            height: 6
            radius: 3
            color: Theme.border
            Rectangle {
                width: parent.width * Math.max(0, meter.percent) / 100
                height: parent.height
                radius: 3
                color: meter.percent >= 90 ? Theme.danger
                     : meter.percent >= 75 ? Theme.warning : Theme.success
            }
        }
    }

    // Guide arrives as gameBarRequested (not a key); keyboard Back / Home close the bar.
    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Escape:
        case Qt.Key_Back:
        case Qt.Key_Backspace:
        case Qt.Key_HomePage:
            bar.gameBar.close()
            break
        }
        // Nothing (not even arrows at the edge of the widget row) reaches the dashboard or
        // the running overlay underneath.
        event.accepted = true
    }
}
