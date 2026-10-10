import QtQuick
import Nexora.Core

// Details for one game: artwork, title, emulator, file information and the
// actions that apply to it. Only real data from the library is shown.
PageContainer {
    id: page

    property string gamePath: ""
    property var game: ({})
    readonly property bool exists: !!game.path
    readonly property var emulator: exists && game.emulatorId ? Emulators.byId(game.emulatorId) : ({})
    readonly property bool hasEmulator: exists && !!game.emulatorId && !!emulator.id
    // Launcher.busy is deliberately not part of this: the launch overlay owns
    // focus while busy and restores it to this button afterwards, which must
    // not be disabled in between. Router.launchGame refuses a second launch.
    readonly property bool canLaunch: hasEmulator && emulator.canLaunch

    title: ""
    defaultFocusItem: !exists ? goneState.firstFocusable
                    : canLaunch ? launchButton
                    : !hasEmulator ? chooseButton : favoriteButton

    readonly property bool actionsFocused: actionRow.activeFocus
    hints: exists
           ? [{ action: "confirm", label: qsTr("Select") }, { action: "secondary", label: qsTr("Favorite") },
              { action: "options", label: qsTr("More") }, { action: "back", label: qsTr("Back") }]
           : [{ action: "confirm", label: qsTr("Select") }, { action: "back", label: qsTr("Back") }]

    function reload() {
        var g = Library.get(gamePath)
        game = g
    }
    Component.onCompleted: {
        reload()
        if (game.emulatorId) Emulators.validate(game.emulatorId)   // refresh status (file checks only)
    }
    onGamePathChanged: reload()

    Connections {
        target: Library.model
        function onDataChanged() { page.reload() }
        function onModelReset() { page.reload() }
        function onRowsRemoved() { page.reload() }
        function onRowsInserted() { page.reload() }
    }
    Connections {
        target: Library
        function onFavoriteChanged(path, favorite) { if (path === page.gamePath) page.reload() }
        function onStateChanged() { page.reload() }
    }
    Connections {
        target: Emulators
        function onStatusChanged() { page.reload() }
        function onReloaded() { page.reload() }
    }

    // If the focused control disappears (game removed), move focus.
    onExistsChanged: Qt.callLater(function() { if (page.defaultFocusItem && !page.exists) page.defaultFocusItem.forceActiveFocus() })

    function folderOf(g) {
        var p = String(g.path || ""), f = String(g.fileName || "")
        return p.length > f.length ? p.substring(0, p.length - f.length - 1) : p
    }
    function launchBlockedReason() {
        if (!exists) return ""
        if (!game.emulatorId) return qsTr("No emulator handles .%1 files yet. Choose one to launch this game.").arg(game.extension)
        if (!emulator.id) return qsTr("The assigned emulator \"%1\" is no longer configured. Choose another one.").arg(game.emulatorId)
        if (!emulator.canLaunch) return qsTr("%1 can't be launched: %2").arg(emulator.name).arg(emulator.statusText)
        return ""
    }
    function openMore() {
        Router.openMenu({
            title: game.title,
            actions: [
                { id: "file", label: qsTr("File information"), icon: "file" },
                { id: "emulator", label: qsTr("Change emulator"), icon: "chip" },
                { id: "remove", label: qsTr("Remove from library"), icon: "trash", destructive: true }
            ]
        }, function(id) {
            if (id === "file") GameActions.showFileInfo(page.gamePath)
            else if (id === "emulator") GameActions.chooseEmulator(page.gamePath)
            else if (id === "remove") GameActions.confirmRemove(page.gamePath, function() { leaveTimer.start() })
        })
    }

    // Leave after "Remove from library" once the confirmation dialog has
    // finished closing (it restores focus into this page when it closes).
    Timer {
        id: leaveTimer
        interval: Motion.dialog + 120
        onTriggered: Router.back()
    }

    Keys.onPressed: function(event) {
        if (!page.exists) return
        if (Input.isSecondary(event)) {
            event.accepted = true
            if (!event.isAutoRepeat) GameActions.toggleFavorite(page.gamePath)
        } else if (Input.isOptions(event)) {
            event.accepted = true
            if (!event.isAutoRepeat) page.openMore()
        }
    }

    // ---- game no longer available ----------------------------------------
    EmptyState {
        id: goneState
        anchors.fill: parent
        anchors.bottomMargin: Dimensions.spacingXxl
        visible: !page.exists
        kind: "unavailable"
        iconName: "warning"
        title: qsTr("This game is no longer in your library")
        message: qsTr("It may have been removed, moved or renamed, or its folder is no longer configured.")
        details: page.gamePath
        primaryText: qsTr("Back")
        primaryIcon: "chevron-left"
        onPrimaryClicked: Router.back()
    }

    // ---- details --------------------------------------------------------------
    Flickable {
        id: flick
        anchors.fill: parent
        visible: page.exists
        clip: true
        contentWidth: width
        contentHeight: layout.height + Dimensions.spacingXl
        boundsBehavior: Flickable.StopAtBounds

        Item {
            id: layout
            x: Dimensions.focusOffset * 3
            y: Dimensions.focusOffset * 3
            width: flick.width - Dimensions.focusOffset * 6
            height: Math.max(artFrame.height, info.height)

            // Artwork (portrait 2:3; landscape art switches to a 16:9 frame)
            Rectangle {
                id: artFrame
                readonly property real baseHeight: Math.min(Dimensions.s(600), Math.max(Dimensions.s(300), flick.height - Dimensions.spacingXl))
                // Frame shape follows the artwork's orientation. Updated deferred:
                // resizing reloads the (sourceSize'd) image, which would otherwise
                // feed back into this binding.
                property bool landscape: false
                Connections {
                    target: art
                    function onLandscapeChanged() { Qt.callLater(function() { artFrame.landscape = art.landscape }) }
                }
                width: landscape ? Math.min(baseHeight * 1.2, layout.width * 0.42) : baseHeight / Dimensions.cardAspect
                height: landscape ? width * 9 / 16 : baseHeight
                radius: Dimensions.radiusLg
                color: Theme.backgroundElevated
                border.color: Theme.borderSubtle
                clip: true
                Artwork {
                    id: art
                    anchors.fill: parent
                    source: page.game.artworkUrl || ""
                    title: page.game.title || ""
                }
            }

            Column {
                id: info
                x: artFrame.width + Dimensions.spacingXxl
                width: layout.width - x
                spacing: Dimensions.spacingMd

                Text {
                    width: parent.width
                    text: qsTr("Game details")
                    font.family: Typography.family
                    font.pixelSize: Typography.caption
                    font.weight: Typography.weightSemiBold
                    font.letterSpacing: Dimensions.s(1.5)
                    font.capitalization: Font.AllUppercase
                    color: Theme.textMuted
                }
                Text {
                    width: parent.width
                    text: page.game.title || ""
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    lineHeight: 1.05
                    font.family: Typography.family
                    font.pixelSize: Typography.display * 0.82
                    font.weight: Typography.weightBold
                    color: Theme.textPrimary
                }

                // Emulator + favorite summary
                Flow {
                    width: parent.width
                    spacing: Dimensions.spacingSm
                    StatusIndicator {
                        visible: page.hasEmulator
                        status: page.emulator.status || "unknown"
                        text: (page.emulator.name || "") + (page.emulator.canLaunch ? "" : " — " + (page.emulator.statusText || ""))
                    }
                    StatusIndicator {
                        visible: page.exists && !page.hasEmulator
                        status: "warning"
                        text: qsTr("No emulator assigned")
                    }
                    StatusIndicator {
                        visible: !!page.game.platform
                        status: "neutral"
                        text: page.game.platform || ""
                    }
                    Rectangle {
                        visible: !!page.game.favorite
                        height: Dimensions.s(34)
                        width: favRow.implicitWidth + Dimensions.spacingMd * 2
                        radius: height / 2
                        color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.12)
                        border.width: 1
                        border.color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.45)
                        Row {
                            id: favRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingXs
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "star-filled"
                                size: Dimensions.iconSizeSmall * 0.9
                                color: Theme.statusWarning
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Favorite")
                                font.family: Typography.family
                                font.pixelSize: Typography.caption
                                font.weight: Typography.weightSemiBold
                                color: Theme.statusWarning
                            }
                        }
                    }
                }

                // Actions
                Item { width: 1; height: Dimensions.spacingXs }
                Flow {
                    id: actionRow
                    width: parent.width
                    spacing: Dimensions.spacingMd
                    FocusButton {
                        id: launchButton
                        text: Launcher.busy ? qsTr("Running…") : qsTr("Launch")
                        iconName: "play"
                        variant: "primary"
                        enabled: page.canLaunch
                        onClicked: Router.launchGame(page.gamePath)
                        KeyNavigation.right: chooseButton.visible ? chooseButton : favoriteButton
                    }
                    FocusButton {
                        id: chooseButton
                        visible: !page.hasEmulator
                        text: qsTr("Choose emulator")
                        iconName: "chip"
                        variant: page.hasEmulator ? "secondary" : "primary"
                        onClicked: GameActions.chooseEmulator(page.gamePath)
                        KeyNavigation.right: favoriteButton
                    }
                    FocusButton {
                        id: favoriteButton
                        text: page.game.favorite ? qsTr("Unfavorite") : qsTr("Favorite")
                        iconName: page.game.favorite ? "star-filled" : "star"
                        onClicked: GameActions.toggleFavorite(page.gamePath)
                        KeyNavigation.left: chooseButton.visible ? chooseButton : (launchButton.enabled ? launchButton : null)
                        KeyNavigation.right: emulatorButton.visible ? emulatorButton : moreButton
                    }
                    FocusButton {
                        id: emulatorButton
                        visible: page.hasEmulator
                        text: qsTr("Change emulator")
                        iconName: "chip"
                        onClicked: GameActions.chooseEmulator(page.gamePath)
                        KeyNavigation.left: favoriteButton
                        KeyNavigation.right: moreButton
                    }
                    FocusButton {
                        id: moreButton
                        text: qsTr("More")
                        iconName: "more"
                        accessibleDescription: qsTr("More actions")
                        onClicked: page.openMore()
                        KeyNavigation.left: emulatorButton.visible ? emulatorButton : favoriteButton
                    }
                }
                // Why Launch is unavailable
                Row {
                    width: parent.width
                    visible: page.launchBlockedReason() !== ""
                    spacing: Dimensions.spacingSm
                    Icon {
                        name: "warning"
                        size: Dimensions.iconSizeSmall
                        color: Theme.statusWarning
                    }
                    Text {
                        width: parent.width - Dimensions.iconSizeSmall - parent.spacing
                        text: page.launchBlockedReason()
                        wrapMode: Text.WordWrap
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: Theme.textSecondary
                    }
                }

                // Information
                Item { width: 1; height: Dimensions.spacingSm }
                Rectangle {
                    width: parent.width
                    height: infoGrid.height + Dimensions.spacingLg * 2
                    radius: Dimensions.radiusLg
                    color: Theme.backgroundSecondary
                    border.color: Theme.borderSubtle

                    Column {
                        id: infoGrid
                        x: Dimensions.spacingLg
                        y: Dimensions.spacingLg
                        width: parent.width - Dimensions.spacingLg * 2
                        spacing: Dimensions.spacingSm

                        Repeater {
                            model: {
                                var g = page.game
                                if (!g.path) return []
                                var played = GameActions.formatRelative(g.lastPlayed)
                                var rows = [
                                    { label: qsTr("Emulator"), value: page.hasEmulator ? page.emulator.name + (g.emulatorOverridden ? qsTr(" (chosen manually)") : qsTr(" (automatic)")) : qsTr("None") }
                                ]
                                if (g.platform) rows.push({ label: qsTr("Platform"), value: g.platform })
                                rows.push({ label: qsTr("Last played"), value: played !== "" ? played + " · " + Qt.formatTime(g.lastPlayed, Qt.locale().timeFormat(Locale.ShortFormat)) : qsTr("Never launched from Nexora") })
                                var added = GameActions.formatRelative(g.firstSeen)
                                if (added !== "") rows.push({ label: qsTr("Added to library"), value: added })
                                rows.push({ label: qsTr("File name"), value: g.fileName })
                                rows.push({ label: qsTr("Type"), value: "." + g.extension })
                                rows.push({ label: qsTr("Size"), value: GameActions.formatSize(g.fileSize) })
                                rows.push({ label: qsTr("Folder"), value: page.folderOf(g) })
                                return rows
                            }
                            delegate: Item {
                                id: infoRow
                                required property var modelData
                                required property int index
                                width: infoGrid.width
                                height: Math.max(labelText.implicitHeight, valueText.implicitHeight)
                                Text {
                                    id: labelText
                                    width: Dimensions.s(200)
                                    text: infoRow.modelData.label
                                    font.family: Typography.family
                                    font.pixelSize: Typography.secondary
                                    color: Theme.textMuted
                                }
                                Text {
                                    id: valueText
                                    anchors.left: labelText.right
                                    anchors.leftMargin: Dimensions.spacingMd
                                    anchors.right: parent.right
                                    text: infoRow.modelData.value
                                    wrapMode: Text.WrapAnywhere
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                    font.family: Typography.family
                                    font.pixelSize: Typography.secondary
                                    color: Theme.textPrimary
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
