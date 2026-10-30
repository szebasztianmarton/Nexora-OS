import QtQuick
import Nexora.Console

// Game bar, laid out like the Xbox Game Bar: a row of square widget buttons at the top left,
// the selected widget's panel underneath, and status icons with a large clock at the top
// right. Guide (or Ctrl+G) opens it, also on top of a running emulator; Guide, Back or a
// click outside closes it.
//
// Navigation: Left/Right choose a widget button (its panel follows), Down enters the panel,
// Up returns to the buttons. Inside a panel Left/Right move along the row of tiles (and adjust
// the volume slider).
FocusScope {
    id: bar
    objectName: "gameBar"
    required property GameBarController gameBar
    required property AppController app
    required property LibraryController library
    required property SettingsController settings
    required property SystemController system
    property GameSessionManager sessions: null
    // The game's title while a session runs; the emulator's name for a plain emulator launch.
    readonly property string title: sessions !== null && sessions.active.title ? sessions.active.title
                                                                            : app.activeEmulatorName

    // Dashboard navigation from the Home and Settings panels (home, games, settings).
    signal navigate(string tab)

    readonly property var tabs: [{ key: "home", icon: "home", title: "Home" },
                                 { key: "settings", icon: "gear", title: "Quick settings" },
                                 { key: "audio", icon: "speaker", title: "Audio" },
                                 { key: "performance", icon: "chart", title: "Performance" }]
    property int currentTab: 0
    readonly property string currentKey: tabs[currentTab].key
    readonly property var hints: confirmReboot.visible || confirmPowerOff.visible
        ? confirmReboot.hints
        : [{ button: "Guide / B", action: "Close" },
           { button: "A", action: "Select" },
           { button: "← → ↑ ↓", action: "Move" }]

    // Game Bar palette (Theme.overlay*): graphite surfaces and light selected buttons, or the
    // high-contrast scheme when that theme is active.
    readonly property color barSurface: Theme.overlaySurface
    readonly property color barTile: Theme.overlayTile
    readonly property color barTileHover: Theme.overlayTileHover
    readonly property color barSelected: Theme.overlaySelected
    readonly property color barInk: Theme.overlayInk
    readonly property color barLine: Theme.overlayLine
    readonly property color xboxGreen: Theme.overlayAccent

    function tabButton(index) {
        return tabRepeater.itemAt(index)
    }
    // Opening: the Home button has focus, like the Xbox Game Bar.
    function reset() {
        currentTab = 0
        focusDefault()
    }
    function focusDefault() {
        const button = tabButton(currentTab)
        if (button)
            button.forceActiveFocus()
    }
    function focusPanel() {
        switch (currentKey) {
        case "home": homeActions.focusFirst(); break
        case "settings": settingsActions.focusFirst(); break
        case "audio": volume.forceActiveFocus(); break
        default: break // performance has nothing to operate
        }
    }
    function inPanel() {
        for (let p = Window.activeFocusItem; p; p = p.parent) {
            if (p === panel)
                return true
        }
        return false
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
        // Running and idle Home panels hold different tiles; keep focus where it was.
        function onBusyChanged() {
            if (!bar.gameBar.open)
                return
            const wasInPanel = bar.inPanel()
            Qt.callLater(() => {
                if (!bar.gameBar.open)
                    return // closed meanwhile, e.g. by launching a game from it
                if (wasInPanel && bar.currentKey === "home")
                    homeActions.focusFirst()
                else if (!bar.inPanel())
                    bar.focusDefault()
            })
        }
    }

    // Scrim over the whole window (header and hint bar included): the game stays visible
    // behind the bar; a click on it closes the bar.
    Rectangle {
        anchors.fill: parent
        color: Theme.overlayScrim
        opacity: 0.65
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onClicked: bar.gameBar.close()
    }

    HintBar {
        objectName: "gameBarHints"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: bar.settings.showHints
        color: Theme.overlayFooter // covers the dashboard's own hint bar underneath
        hints: bar.hints
    }

    Item {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.space5
        // Drops in from the top edge as it fades in.
        transform: Translate { y: (1 - bar.opacity) * -16 }

        // --- Widget buttons (top left) ---------------------------------------------------
        Row {
            id: tabRow
            objectName: "gameBarTabs"
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: Math.round(14 * Theme.scale)

            Repeater {
                id: tabRepeater
                model: bar.tabs
                delegate: Rectangle {
                    id: tabButton
                    required property var modelData
                    required property int index
                    readonly property bool selected: bar.currentTab === index
                    readonly property bool hovered: tabMouse.containsMouse

                    objectName: "barTab_" + modelData.key
                    width: Math.round(84 * Theme.scale)
                    height: width
                    radius: Math.round(8 * Theme.scale)
                    activeFocusOnTab: true
                    color: selected ? bar.barSelected : (hovered ? bar.barTileHover : bar.barTile)
                    border.width: activeFocus ? Theme.focusWidth : 1
                    border.color: activeFocus ? Theme.overlayFocus : (selected ? bar.barSelected : bar.barLine)
                    scale: activeFocus && !Theme.reducedMotion ? 1.06 : 1.0
                    Behavior on scale { NumberAnimation { duration: Theme.animFast } }
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }

                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData.title
                    Accessible.selected: selected

                    onActiveFocusChanged: if (activeFocus) bar.currentTab = index

                    BarIcon {
                        anchors.centerIn: parent
                        width: Math.round(38 * Theme.scale)
                        height: width
                        name: tabButton.modelData.icon
                        color: tabButton.selected ? bar.barInk : Theme.overlayTextSoft
                    }
                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: tabButton.forceActiveFocus()
                    }

                    Keys.onLeftPressed: if (index > 0) bar.tabButton(index - 1).forceActiveFocus()
                    Keys.onRightPressed: if (index < bar.tabs.length - 1) bar.tabButton(index + 1).forceActiveFocus()
                    Keys.onDownPressed: bar.focusPanel()
                    Keys.onReturnPressed: bar.focusPanel()
                    Keys.onEnterPressed: bar.focusPanel()
                    Keys.onSpacePressed: bar.focusPanel()
                }
            }
            Text {
                textFormat: Text.PlainText
                anchors.verticalCenter: parent.verticalCenter
                text: "›"
                color: Theme.overlayTextSoft
                font.pixelSize: Math.round(34 * Theme.scale)
            }
        }

        // --- Status (top right) ----------------------------------------------------------
        Row {
            id: statusRow
            objectName: "gameBarStatus"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: Theme.space1
            spacing: Math.round(18 * Theme.scale)

            BarIcon {
                objectName: "controllerStatus"
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(34 * Theme.scale)
                height: width
                name: "gamepad"
                color: bar.app.inputDevices.length > 0 ? Theme.overlayText : Theme.overlayDisabled
                Accessible.role: Accessible.StaticText
                Accessible.name: bar.app.inputDevices.length > 0 ? "Controller connected" : "No controller"
            }
            BarIcon {
                objectName: "networkStatus"
                anchors.verticalCenter: parent.verticalCenter
                visible: bar.gameBar.network !== "none"
                width: Math.round(34 * Theme.scale)
                height: width
                name: bar.gameBar.network === "wifi" ? "wifi" : "ethernet"
                color: Theme.overlayText
                Accessible.role: Accessible.StaticText
                Accessible.name: bar.gameBar.network === "wifi" ? "Wi-Fi connected" : "Wired network connected"
            }
            Row {
                objectName: "batteryStatus"
                anchors.verticalCenter: parent.verticalCenter
                visible: bar.gameBar.batteryPercent >= 0
                spacing: Math.round(6 * Theme.scale)
                Accessible.role: Accessible.StaticText
                Accessible.name: "Battery: " + bar.gameBar.batteryPercent + "%" + (bar.gameBar.batteryCharging ? ", charging" : "")
                BarIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.round(38 * Theme.scale)
                    height: width
                    name: "battery"
                    level: bar.gameBar.batteryPercent
                    charging: bar.gameBar.batteryCharging
                    color: bar.gameBar.batteryPercent >= 0 && bar.gameBar.batteryPercent <= 15
                           && !bar.gameBar.batteryCharging ? Theme.danger : Theme.overlayText
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.verticalCenter: parent.verticalCenter
                    text: bar.gameBar.batteryPercent + "%"
                    color: Theme.overlayText
                    font.pixelSize: Theme.fontTitle
                }
            }
            Text {
                textFormat: Text.PlainText
                id: barClock
                objectName: "gameBarClock"
                anchors.verticalCenter: parent.verticalCenter
                text: Qt.formatTime(new Date(), "hh:mm")
                color: Theme.overlayText
                font.pixelSize: Math.round(56 * Theme.scale)
                font.bold: true
                Accessible.role: Accessible.Clock
                Accessible.name: text
                Timer {
                    interval: 5000
                    running: bar.visible
                    repeat: true
                    triggeredOnStart: true
                    onTriggered: barClock.text = Qt.formatTime(new Date(), "hh:mm")
                }
            }
        }

        // --- Panel of the selected widget ------------------------------------------------
        Rectangle {
            id: panel
            objectName: "gameBarPanel"
            anchors.left: tabRow.left
            anchors.top: tabRow.bottom
            anchors.topMargin: Math.round(22 * Theme.scale)
            width: Math.min(Math.round(640 * Theme.scale), content.width)
            height: panelBody.implicitHeight + 2 * panelBody.anchors.margins
            radius: Theme.radius
            color: bar.barSurface
            border.color: bar.barLine
            border.width: 1
            // Glass: the highlight and rim over the overlay colours (no blur).
            GlassSurface { anchors.fill: parent; variant: "clear"; allowBlur: false; radius: panel.radius }
            // Clicks inside the panel never reach the scrim (which would close the bar).
            MouseArea { anchors.fill: parent }

            Item {
                id: panelBody
                anchors.fill: parent
                anchors.margins: Theme.space6
                implicitHeight: home.visible ? home.implicitHeight
                              : quick.visible ? quick.implicitHeight
                              : audio.visible ? audio.implicitHeight
                              : performance.implicitHeight

                // Home: the game in focus, then a row of tiles ---------------------------
                Column {
                    id: home
                    objectName: "homePanel"
                    visible: bar.currentKey === "home"
                    width: parent.width
                    spacing: Math.round(28 * Theme.scale)

                    Row {
                        id: gameRow
                        spacing: Math.round(22 * Theme.scale)
                        Rectangle {
                            id: artwork
                            objectName: "gameBarArtwork"
                            width: Math.round(100 * Theme.scale)
                            height: width
                            radius: Math.round(8 * Theme.scale)
                            clip: true
                            color: bar.app.busy ? Theme.artColor(bar.title, 0.32) : Theme.overlayArtEmpty
                            Text {
                                textFormat: Text.PlainText
                                visible: bar.app.busy
                                anchors.centerIn: parent
                                text: Theme.initials(bar.title)
                                color: Theme.overlayText
                                font.pixelSize: Math.round(38 * Theme.scale)
                                font.bold: true
                                font.italic: true
                            }
                            // Idle: the Nexora mark.
                            Rectangle {
                                visible: !bar.app.busy
                                anchors.centerIn: parent
                                width: Math.round(40 * Theme.scale)
                                height: width
                                radius: Math.round(10 * Theme.scale)
                                rotation: 45
                                color: "transparent"
                                border.color: Theme.accent
                                border.width: Math.round(5 * Theme.scale)
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: Math.round(11 * Theme.scale)
                                    height: width
                                    radius: Math.round(3 * Theme.scale)
                                    color: Theme.accent
                                }
                            }
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space2
                            Text {
                                textFormat: Text.PlainText
                                objectName: "gameBarTitle"
                                width: Math.min(implicitWidth, home.width - artwork.width - gameRow.spacing)
                                elide: Text.ElideRight
                                text: bar.app.busy ? bar.title : "No game running"
                                color: Theme.overlayText
                                font.pixelSize: Math.round(26 * Theme.scale)
                                font.bold: true
                            }
                            Row {
                                visible: bar.app.busy
                                spacing: Math.round(14 * Theme.scale)
                                Text {
                                    textFormat: Text.PlainText
                                    text: Theme.capitalize(bar.app.state)
                                    color: Theme.statusColor(bar.app.state)
                                    font.pixelSize: Theme.fontBody
                                    font.bold: true
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    objectName: "sessionTime"
                                    text: "Playing for " + bar.formatDuration(bar.gameBar.sessionSeconds)
                                    color: Theme.overlayTextMuted
                                    font.pixelSize: Theme.fontBody
                                }
                            }
                            Text {
                                textFormat: Text.PlainText
                                visible: !bar.app.busy
                                text: "Nexora OS " + bar.app.version
                                color: Theme.overlayTextMuted
                                font.pixelSize: Theme.fontBody
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: bar.barLine }

                    TileRow {
                        id: homeActions
                        objectName: "homeActions"

                        // Running: back to the game, stop it.
                        ActionTile {
                            objectName: "gameBarResume"
                            visible: bar.app.busy
                            icon: "play"
                            label: "Back to game"
                            tone: bar.xboxGreen
                            onActivated: bar.gameBar.close()
                        }
                        ActionTile {
                            objectName: "gameBarStop"
                            visible: bar.app.busy
                            // Dimmed, not disabled, while stopping: a disabled tile would drop focus.
                            opacity: bar.app.state === "stopping" ? 0.4 : 1.0
                            icon: "stop"
                            label: "Stop emulator"
                            tone: Theme.overlayDanger
                            requireConfirm: true
                            confirmLabel: "Press again to stop"
                            onActivated: if (bar.app.state !== "stopping") bar.app.stop()
                        }
                        // Idle: jump to a dashboard tab.
                        ActionTile {
                            objectName: "gameBarHome"
                            visible: !bar.app.busy
                            icon: "home"
                            label: "Nexora home"
                            tone: Theme.accentSoft
                            onActivated: { bar.navigate("home"); bar.gameBar.close() }
                        }
                        ActionTile {
                            objectName: "gameBarGames"
                            visible: !bar.app.busy
                            icon: "grid"
                            label: "Game library"
                            tone: bar.xboxGreen
                            onActivated: { bar.navigate("games"); bar.gameBar.close() }
                        }
                        // Recently played games, launchable when nothing runs.
                        Repeater {
                            model: bar.library.recentGames
                            delegate: ActionTile {
                                required property int index
                                required property real gameId
                                required property string title
                                required property bool available
                                objectName: "gameBarRecent_" + index
                                visible: index < 4
                                enabled: available && !bar.app.busy
                                artTitle: title
                                // Disabled while a game runs (no focus, so no label shown then).
                                label: "Play " + title
                                onActivated: {
                                    if (bar.library.launch(gameId))
                                        bar.gameBar.close()
                                }
                            }
                        }
                    }
                    Text {
                        objectName: "homeBusyHint"
                        textFormat: Text.PlainText
                        visible: bar.app.busy && bar.library.recentGames.count > 0
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: "Recent games can be started once the running game is stopped."
                        color: Theme.overlayTextFaint
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Quick settings ------------------------------------------------------------
                Column {
                    id: quick
                    objectName: "settingsPanel"
                    visible: bar.currentKey === "settings"
                    width: parent.width
                    spacing: Math.round(22 * Theme.scale)

                    PanelTitle { text: "Quick settings" }
                    TileRow {
                        id: settingsActions
                        ActionTile {
                            objectName: "gameBarSettings"
                            enabled: !bar.app.busy
                            icon: "gear"
                            label: "All settings" // disabled while a game runs: explained underneath
                            tone: Theme.accentSoft
                            onActivated: { bar.navigate("settings"); bar.gameBar.close() }
                        }
                        ActionTile {
                            objectName: "gameBarReboot"
                            enabled: bar.system.powerSupported
                            icon: "reboot"
                            label: "Restart"
                            onActivated: confirmReboot.open()
                        }
                        ActionTile {
                            objectName: "gameBarPowerOff"
                            enabled: bar.system.powerSupported
                            icon: "power"
                            label: "Power off"
                            tone: Theme.overlayDanger
                            onActivated: confirmPowerOff.open()
                        }
                    }
                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: (bar.app.inputDevices.length > 0 ? "Controller: " + bar.app.inputDevices.join(", ")
                                                               : "No controller connected (" + bar.app.inputBackend + ")")
                              + (bar.system.powerSupported ? "" : "\nPower control is not available on this platform.")
                              + (bar.app.busy ? "\nAll settings opens once the running game is stopped." : "")
                        color: Theme.overlayTextFaint
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Audio ---------------------------------------------------------------------
                Column {
                    id: audio
                    objectName: "audioPanel"
                    visible: bar.currentKey === "audio"
                    width: parent.width
                    spacing: Math.round(22 * Theme.scale)

                    PanelTitle { text: "Audio" }
                    Rectangle {
                        id: volume
                        objectName: "gameBarVolume"
                        width: parent.width
                        height: Math.round(76 * Theme.scale)
                        radius: Math.round(8 * Theme.scale)
                        activeFocusOnTab: true
                        color: activeFocus ? bar.barTileHover : bar.barTile
                        border.width: activeFocus ? Theme.focusWidth : 1
                        border.color: activeFocus ? Theme.overlayFocus : bar.barLine
                        Accessible.role: Accessible.Slider
                        Accessible.name: "Volume"
                        Accessible.description: bar.settings.volume + "%"

                        function adjust(step) {
                            bar.settings.volume = Math.max(0, Math.min(100, bar.settings.volume + step))
                        }

                        Row {
                            anchors.fill: parent
                            id: volumeRow
                            anchors.leftMargin: Math.round(18 * Theme.scale)
                            anchors.rightMargin: Math.round(18 * Theme.scale)
                            spacing: Math.round(18 * Theme.scale)
                            BarIcon {
                                id: volumeIcon
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.round(30 * Theme.scale)
                                height: width
                                name: "speaker"
                                color: bar.settings.volume === 0 ? Theme.overlayDisabled : Theme.overlayText
                            }
                            Rectangle {
                                id: track
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - volumeIcon.width - volumeValue.width - 2 * parent.spacing
                                height: Math.round(6 * Theme.scale)
                                radius: height / 2
                                color: Theme.overlayTrack
                                Rectangle {
                                    width: parent.width * bar.settings.volume / 100
                                    height: parent.height
                                    radius: height / 2
                                    color: Theme.overlayText
                                }
                                Rectangle {
                                    x: parent.width * bar.settings.volume / 100 - width / 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Math.round(18 * Theme.scale)
                                    height: width
                                    radius: width / 2
                                    color: Theme.overlayText
                                }
                            }
                            Text {
                                textFormat: Text.PlainText
                                id: volumeValue
                                objectName: "gameBarVolumeValue"
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.round(56 * Theme.scale)
                                horizontalAlignment: Text.AlignRight
                                text: bar.settings.volume + "%"
                                color: Theme.overlayText
                                font.pixelSize: Theme.fontLarge
                                font.bold: true
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: (mouse) => {
                                volume.forceActiveFocus()
                                const p = (mouse.x - track.x - volumeRow.anchors.leftMargin) / track.width
                                bar.settings.volume = Math.max(0, Math.min(100, Math.round(p * 20) * 5))
                            }
                        }
                        Keys.onLeftPressed: adjust(-5)
                        Keys.onRightPressed: adjust(5)
                        Keys.onUpPressed: bar.focusDefault()
                    }
                    Text {
                        textFormat: Text.PlainText
                        visible: !bar.settings.volumeSupported
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: "Saved as a preference; applied on Nexora OS hardware."
                        color: Theme.overlayTextFaint
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // Performance ---------------------------------------------------------------
                Column {
                    id: performance
                    objectName: "performancePanel"
                    visible: bar.currentKey === "performance"
                    width: parent.width
                    spacing: Math.round(18 * Theme.scale)

                    PanelTitle { text: "Performance" }
                    Row {
                        spacing: Theme.space3
                        Text {
                            textFormat: Text.PlainText
                            id: cpuText
                            objectName: "cpuValue"
                            text: bar.gameBar.cpuPercent >= 0 ? bar.gameBar.cpuPercent + "%" : "—"
                            color: Theme.overlayText
                            font.pixelSize: Math.round(44 * Theme.scale)
                            font.bold: true
                        }
                        Text {
                            textFormat: Text.PlainText
                            anchors.baseline: cpuText.baseline
                            text: "CPU"
                            color: Theme.overlayTextMuted
                            font.pixelSize: Theme.fontBody
                            font.bold: true
                            font.letterSpacing: 1
                        }
                    }
                    // CPU history (last 30 s), newest on the right.
                    Canvas {
                        id: sparkline
                        objectName: "cpuGraph"
                        width: parent.width
                        height: Math.round(90 * Theme.scale)
                        property var values: bar.gameBar.cpuHistory
                        onValuesChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = String(bar.barTile)
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
                            ctx.fillStyle = Qt.rgba(Theme.overlayAccent.r, Theme.overlayAccent.g, Theme.overlayAccent.b, 0.45)
                            ctx.fill()
                            ctx.beginPath()
                            for (let j = 0; j < v.length; ++j) {
                                if (j === 0)
                                    ctx.moveTo(x0, yOf(v[0]))
                                else
                                    ctx.lineTo(x0 + j * step, yOf(v[j]))
                            }
                            ctx.strokeStyle = Theme.overlayGraph
                            ctx.lineWidth = 2
                            ctx.stroke()
                        }
                    }
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
                        textFormat: Text.PlainText
                        text: "Load (1 min): " + (bar.gameBar.load1 >= 0 ? bar.gameBar.load1.toFixed(2) : "—")
                        color: Theme.overlayTextMuted
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }
        }
    }

    // Small upper-case heading inside a panel.
    component PanelTitle: Text {
        textFormat: Text.PlainText
        color: Theme.overlayTextMuted
        font.pixelSize: Theme.fontSmall
        font.bold: true
        font.letterSpacing: 2
        font.capitalization: Font.AllUppercase
    }

    // A row of tiles; Left/Right move between the visible, enabled ones, Up goes back to the
    // widget buttons. The label of the focused tile is shown underneath.
    component TileRow: Column {
        id: tileRow
        default property alias tiles: row.data
        property Item current: null // the focused tile, if any
        width: parent ? parent.width : 0
        spacing: Math.round(14 * Theme.scale)

        function focusables() {
            const list = []
            for (let i = 0; i < row.children.length; ++i) {
                const c = row.children[i]
                if (c.visible && c.enabled && c.activeFocusOnTab)
                    list.push(c)
            }
            return list
        }
        function focusFirst() {
            const list = focusables()
            if (list.length > 0)
                list[0].forceActiveFocus()
        }
        function move(from, step) {
            const list = focusables()
            const i = list.indexOf(from)
            if (i >= 0 && i + step >= 0 && i + step < list.length)
                list[i + step].forceActiveFocus()
        }

        Row {
            id: row
            spacing: Math.round(18 * Theme.scale)
        }
        Text {
            textFormat: Text.PlainText
            objectName: "tileLabel"
            width: parent.width
            height: implicitHeight
            elide: Text.ElideRight
            readonly property Item tile: tileRow.current
            text: tile ? (tile.armed ? tile.confirmLabel : tile.label) : " "
            color: tile && tile.armed ? Theme.overlayDangerSoft : Theme.overlayTextSoft
            font.pixelSize: Theme.fontBody
        }
    }

    // Square tile: an icon on a coloured background, or placeholder cover art for a game.
    component ActionTile: Rectangle {
        id: tile
        property string icon
        property string label
        property string artTitle
        property color tone: bar.barTile
        property bool requireConfirm: false
        property string confirmLabel: "Press again to confirm"
        property bool armed: false
        signal activated()

        function activate() {
            if (!enabled)
                return
            if (requireConfirm && !armed) {
                armed = true
                disarm.restart()
                return
            }
            armed = false
            activated()
        }

        width: Math.round(92 * Theme.scale)
        height: width
        radius: Math.round(8 * Theme.scale)
        clip: true
        activeFocusOnTab: true
        opacity: enabled ? 1.0 : 0.4
        color: armed ? Theme.overlayArmed : (artTitle.length > 0 ? Theme.artColor(artTitle, 0.3) : tone)
        border.width: activeFocus ? Theme.focusWidth : 0
        border.color: Theme.overlayFocus
        scale: activeFocus && !Theme.reducedMotion ? 1.06 : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.animFast } }

        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: activate()

        onActiveFocusChanged: {
            const row = parent ? parent.parent : null
            if (activeFocus) {
                row.current = tile
            } else {
                armed = false
                if (row && row.current === tile)
                    row.current = null
            }
        }
        Timer {
            id: disarm
            interval: 4000
            onTriggered: tile.armed = false
        }

        BarIcon {
            visible: tile.artTitle.length === 0
            anchors.centerIn: parent
            width: Theme.iconLarge
            height: width
            name: tile.icon
            color: Theme.overlayText
        }
        Text {
            textFormat: Text.PlainText
            visible: tile.artTitle.length > 0
            anchors.centerIn: parent
            text: Theme.initials(tile.artTitle)
            color: Theme.overlayText
            font.pixelSize: Theme.fontHero
            font.bold: true
            font.italic: true
        }
        MouseArea {
            anchors.fill: parent
            onClicked: { tile.forceActiveFocus(); tile.activate() }
        }

        Keys.onReturnPressed: activate()
        Keys.onEnterPressed: activate()
        Keys.onSpacePressed: activate()
        Keys.onLeftPressed: parent.parent.move(tile, -1)
        Keys.onRightPressed: parent.parent.move(tile, 1)
        Keys.onUpPressed: bar.focusDefault()
    }

    // Labelled horizontal usage meter; percent < 0 means unknown.
    component Meter: Column {
        id: meter
        property string label
        property int percent: -1
        property string detail
        width: parent ? parent.width : 0
        spacing: Theme.space2

        Accessible.role: Accessible.StaticText
        Accessible.name: meter.label + ": " + (meter.percent >= 0 ? meter.percent + "%" : "Unknown") + (meter.detail ? ", " + meter.detail : "")

        Row {
            width: parent.width
            Text {
                textFormat: Text.PlainText
                width: parent.width - meterDetail.width
                text: meter.label
                color: Theme.overlayText
                font.pixelSize: Theme.fontBody
                font.bold: true
            }
            Text {
                textFormat: Text.PlainText
                id: meterDetail
                text: meter.detail
                color: Theme.overlayTextMuted
                font.pixelSize: Theme.fontBody
            }
        }
        Rectangle {
            width: parent.width
            height: Math.round(6 * Theme.scale)
            radius: height / 2
            color: Theme.overlayTrack
            Rectangle {
                width: parent.width * Math.max(0, meter.percent) / 100
                height: parent.height
                radius: height / 2
                color: meter.percent >= 90 ? Theme.danger : meter.percent >= 75 ? Theme.warning : Theme.overlayGraph
            }
        }
    }

    // Restart / Power off ask first (default Cancel), like everywhere else in Nexora.
    ConfirmDialog {
        id: confirmReboot
        objectName: "confirmGameBarReboot"
        title: "Restart Nexora?"
        message: "The system will restart. A running game is saved for Quick Resume first when its emulator supports it; otherwise it is closed and unsaved progress may be lost."
        confirmText: "Restart"
        dangerous: true
        onConfirmed: bar.system.reboot()
    }
    ConfirmDialog {
        id: confirmPowerOff
        objectName: "confirmGameBarPowerOff"
        title: "Power off?"
        message: "The system will shut down. A running game is saved for Quick Resume first when its emulator supports it; otherwise it is closed and unsaved progress may be lost."
        confirmText: "Power off"
        dangerous: true
        onConfirmed: bar.system.powerOff()
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
        // Nothing (not even arrows at the edge of a row) reaches the dashboard or the
        // running overlay underneath.
        event.accepted = true
    }
}
