import QtQuick
import Nexora.Console

// Home: quick actions, "Continue playing" and "Favorites" rows, and a getting-started
// panel while the library is empty. Up/Down move between sections.
FocusScope {
    id: screen
    objectName: "homeScreen"
    required property AppController app
    required property LibraryController library

    signal navigate(string tab)   // "games" or "settings"

    readonly property bool noDirectories: library.directories.length === 0
    readonly property bool noGames: library.totalCount === 0
    readonly property var hints: actions.activeFocus
        ? [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Open" },
           { button: "L1 / R1", action: "Switch tab" }, { button: "Menu / F1", action: "Diagnostics" }]
        : [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Play" },
           { button: "Y / F", action: "Favorite" }, { button: "↑ ↓", action: "Sections" },
           { button: "L1 / R1", action: "Switch tab" }]

    // Focusable sections in visual order (rows without games are skipped).
    function sections() {
        const list = [actions]
        if (recentRow.hasGames) list.push(recentRow)
        if (favoritesRow.hasGames) list.push(favoritesRow)
        return list
    }
    function currentSection() {
        const list = sections()
        for (let i = 0; i < list.length; ++i) {
            if (list[i].activeFocus)
                return i
        }
        return -1
    }
    function moveSection(delta) {
        const list = sections()
        const next = currentSection() + delta
        if (next < 0 || next >= list.length)
            return false
        list[next].forceActiveFocus()
        return true
    }
    function focusDefault() {
        if (recentRow.hasGames) {
            recentRow.focus = true
        } else if (favoritesRow.hasGames) {
            favoritesRow.focus = true
        } else {
            (noDirectories ? settingsButton : noGames ? rescanButton : browseButton).focus = true
            actions.focus = true
        }
    }
    function ensureVisible(item) {
        const p = item.mapToItem(flick.contentItem, 0, 0)
        const top = p.y - Theme.spacing
        const bottom = p.y + item.height + Theme.spacing
        if (top < flick.contentY)
            flick.contentY = Math.max(0, top)
        else if (bottom > flick.contentY + flick.height)
            flick.contentY = Math.min(bottom - flick.height, Math.max(0, flick.contentHeight - flick.height))
    }

    Component.onCompleted: focusDefault()

    Keys.onDownPressed: moveSection(1)
    Keys.onUpPressed: (event) => { event.accepted = moveSection(-1) }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + Theme.spacing * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Behavior on contentY { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

        Column {
            id: column
            x: Theme.spacing * 2
            y: Theme.spacing * 1.5
            width: flick.width - Theme.spacing * 4
            spacing: Theme.spacing * 1.5

            Item {
                width: parent.width
                height: Math.max(heroText.height, actions.height)

                Column {
                    id: heroText
                    anchors.left: parent.left
                    anchors.right: actions.left
                    anchors.rightMargin: Theme.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        text: screen.noGames ? "Welcome to Nexora" : "Welcome back"
                        color: Theme.text
                        font.pixelSize: Theme.fontHero
                        font.bold: true
                    }
                    Text {
                        objectName: "homeSummary"
                        width: parent.width
                        elide: Text.ElideRight
                        text: (screen.library.totalCount === 1 ? "1 game in your library"
                               : screen.library.totalCount + " games in your library")
                              + (screen.library.favoriteGames.count > 0
                                 ? " · " + screen.library.favoriteGames.count + " favorite"
                                   + (screen.library.favoriteGames.count === 1 ? "" : "s") : "")
                              + " · " + screen.app.emulators.count + " emulator"
                              + (screen.app.emulators.count === 1 ? "" : "s")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                    }
                }

                FocusScope {
                    id: actions
                    objectName: "homeActions"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: actionRow.width
                    height: actionRow.height
                    onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)

                    Row {
                        id: actionRow
                        spacing: 12
                        NxButton {
                            id: browseButton
                            objectName: "browseGamesButton"
                            focus: true
                            glyph: "▦"
                            text: "Browse games"
                            kind: screen.noGames ? "normal" : "primary"
                            KeyNavigation.right: rescanButton
                            onClicked: screen.navigate("games")
                        }
                        NxButton {
                            id: rescanButton
                            objectName: "homeRescanButton"
                            glyph: "↻"
                            text: screen.library.scanning ? "Scanning…" : "Rescan library"
                            enabled: !screen.noDirectories
                            KeyNavigation.left: browseButton
                            KeyNavigation.right: settingsButton
                            onClicked: screen.library.refresh()
                        }
                        NxButton {
                            id: settingsButton
                            objectName: "homeSettingsButton"
                            glyph: "⚙"
                            text: screen.noDirectories ? "Add a game folder" : "Settings"
                            kind: screen.noDirectories ? "primary" : "normal"
                            KeyNavigation.left: rescanButton
                            onClicked: screen.navigate("settings")
                        }
                    }
                }
            }

            // Getting started / nothing found.
            Rectangle {
                id: emptyPanel
                objectName: "homeEmptyState"
                visible: screen.noGames
                width: parent.width
                height: emptyColumn.implicitHeight + Theme.spacing * 3
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border

                Column {
                    id: emptyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacing * 1.5
                    spacing: 10
                    Text {
                        width: parent.width
                        text: screen.library.scanning ? "Scanning your library…"
                            : screen.noDirectories ? "Let's add your games"
                            : "No games found yet"
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                        lineHeight: 1.2
                        text: screen.library.scanning
                              ? "Looking for game files in your library folders. This can take a moment."
                              : screen.noDirectories
                                ? "Nexora has no library folders yet. Choose “Add a game folder” above (or open Settings → Library folders), type the full path of the folder that holds your game files, for example /home/you/roms, and select Add. Nexora scans it and lists every game one of your emulators can run."
                                : "Your library folders (" + screen.library.directories.join(", ") + ") contain no files that a configured emulator can run. Check that the emulator definitions list the right file extensions, then rescan."
                    }
                    Text {
                        width: parent.width
                        visible: !screen.library.scanning && screen.app.emulators.count === 0
                        wrapMode: Text.Wrap
                        color: Theme.warning
                        font.pixelSize: Theme.fontSmall
                        text: "No emulators are configured either — add emulator definitions to "
                              + screen.app.configDirectory + " (see the Emulators tab)."
                    }
                }
            }

            GameRow {
                id: recentRow
                objectName: "recentRow"
                visible: !screen.noGames
                width: parent.width
                library: screen.library
                model: screen.library.recentGames
                title: "Continue playing"
                emptyText: "Games you play show up here. Pick one from Browse games to get started."
                onActivated: (gameId) => screen.library.launch(gameId)
                onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)
            }

            GameRow {
                id: favoritesRow
                objectName: "favoritesRow"
                visible: !screen.noGames
                width: parent.width
                library: screen.library
                model: screen.library.favoriteGames
                title: "Favorites"
                emptyText: "Press Y / F on any game to add it to your favorites."
                onActivated: (gameId) => screen.library.launch(gameId)
                onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)
            }
        }
    }
}
