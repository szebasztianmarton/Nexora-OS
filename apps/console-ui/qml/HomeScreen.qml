import QtQuick
import Nexora.Console

// Home: quick actions, status cards (controllers, system health, updates), the rows
// "Continue playing", "Favorites", "Recently added" and "Systems", and a getting-started
// panel while the library is empty. Up/Down move between sections, Left/Right inside one;
// Accept on a game opens its details.
FocusScope {
    id: screen
    objectName: "homeScreen"
    required property AppController app
    required property LibraryController library
    required property UpdateController updates

    signal navigate(string tab)   // "games", "settings", "system"
    signal optionsRequested(real gameId)
    signal detailsRequested(real gameId)
    signal systemChosen(string systemId)
    signal settingsSectionRequested(string section)
    signal updatesRequested()

    readonly property bool noDirectories: library.directories.length === 0
    readonly property bool noGames: library.totalCount === 0
    readonly property var hints: actions.activeFocus || statusCards.activeFocus || systemsRow.activeFocus
        ? [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Open" },
           { button: "↑ ↓", action: "Sections" },
           { button: "L1 / R1", action: "Switch tab" }, { button: "Menu / F1", action: "System" }]
        : [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Details" },
           { button: "Y / F", action: "Favorite" }, { button: "Select / O", action: "Options" },
           { button: "↑ ↓", action: "Sections" },
           { button: "L1 / R1", action: "Switch tab" }]

    // --- Status card data ----------------------------------------------------------------------
    readonly property int controllerCount: app.input ? app.input.devices.length : app.inputDevices.length
    property int healthRevision: 0
    readonly property int unavailableEmulators: {
        void healthRevision
        let n = 0
        for (let i = 0; i < healthProbe.count; ++i) {
            const item = healthProbe.itemAt(i)
            if (item && item.health !== "available")
                ++n
        }
        return n
    }
    readonly property int issueCount: unavailableEmulators + app.configIssues.length + library.issues.length
                                      + library.missingCount
    readonly property bool updateAvailable: updates.supported
                                            && (updates.state === "available" || updates.state === "readytoinstall")
    // One invisible item per emulator, only to read its health role.
    Repeater {
        id: healthProbe
        model: screen.app.emulators
        onCountChanged: screen.healthRevision += 1
        delegate: Item {
            required property string health
            visible: false
            onHealthChanged: screen.healthRevision += 1
        }
    }

    // Installed systems (with games) for the Systems row.
    property int systemsRevision: 0
    readonly property var systemsWithGames: {
        void systemsRevision
        const list = []
        const model = library.systems
        for (let i = 0; i < model.count; ++i) {
            const s = model.get(i)
            if (s.gameCount > 0)
                list.push(s)
        }
        return list
    }
    Connections {
        target: screen.library
        function onContentChanged() { screen.systemsRevision += 1 }
    }

    // Focusable sections in visual order (rows without content are skipped).
    function sections() {
        const list = [actions, statusCards]
        if (recentRow.hasGames) list.push(recentRow)
        if (favoritesRow.hasGames) list.push(favoritesRow)
        if (addedRow.hasGames) list.push(addedRow)
        if (systemsRow.visible) list.push(systemsRow)
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
            x: Theme.space6
            y: Theme.space5
            width: flick.width - Theme.space6 * 2
            spacing: Theme.space5

            Item {
                width: parent.width
                height: Math.max(heroText.height, actions.height)

                Column {
                    id: heroText
                    anchors.left: parent.left
                    anchors.right: actions.left
                    anchors.rightMargin: Theme.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space1
                    Text {
                        textFormat: Text.PlainText
                        text: screen.noGames ? "Welcome to Nexora" : "Welcome back"
                        color: Theme.text
                        font.pixelSize: Theme.fontHero
                        font.bold: true
                    }
                    Text {
                        textFormat: Text.PlainText
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
                        spacing: Theme.space3
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

            // Status cards: controllers, system health, updates. Accept opens the related page.
            FocusScope {
                id: statusCards
                objectName: "statusCards"
                width: parent.width
                height: cardRow.height
                property int current: 0
                onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)
                readonly property var cards: [
                    { key: "controllers", title: "Controllers",
                      value: screen.controllerCount === 0 ? "None connected"
                             : screen.controllerCount === 1 ? "1 connected" : screen.controllerCount + " connected",
                      detail: screen.controllerCount === 0 ? "Keyboard works too · input: " + screen.app.inputBackend
                                                           : "Input: " + screen.app.inputBackend,
                      tone: screen.controllerCount > 0 ? Theme.success : Theme.textMuted },
                    { key: "health", title: "System health",
                      value: screen.issueCount === 0 ? "No problems found"
                             : screen.issueCount === 1 ? "1 issue" : screen.issueCount + " issues",
                      detail: screen.issueCount === 0 ? "Emulators, configuration and library are OK"
                              : [screen.unavailableEmulators > 0 ? screen.unavailableEmulators + " emulator(s) unavailable" : "",
                                 screen.app.configIssues.length > 0 ? screen.app.configIssues.length + " config issue(s)" : "",
                                 screen.library.issues.length > 0 ? screen.library.issues.length + " library issue(s)" : "",
                                 screen.library.missingCount > 0 ? screen.library.missingCount + " missing game(s)" : ""]
                                .filter(t => t.length > 0).join(" · "),
                      tone: screen.issueCount === 0 ? Theme.success : Theme.warning },
                    // The result of the last attempt comes first: a rolled-back update is
                    // reported as such (the previous version is running), never as "OK".
                    { key: "updates", title: "Updates",
                      value: screen.updates.state === "recoveryrequired" ? "Update rolled back"
                             : screen.updates.state === "completed" ? "Update installed"
                             : !screen.updates.supported ? "Not on this build"
                             : screen.updateAvailable ? "Update " + screen.updates.availableVersion + " available"
                             : screen.updates.state === "idle" ? "No update selected"
                             : Theme.capitalize(screen.updates.state),
                      detail: screen.updates.state === "recoveryrequired"
                              ? "Still running " + screen.updates.currentVersion + " · open for details"
                              : "Installed " + screen.updates.currentVersion,
                      tone: screen.updates.state === "recoveryrequired" ? Theme.danger
                            : screen.updates.state === "completed" ? Theme.success
                            : screen.updateAvailable ? Theme.accent : Theme.textMuted }
                ]
                function activate(i) {
                    const key = cards[i].key
                    if (key === "controllers")
                        screen.settingsSectionRequested("Controllers")
                    else if (key === "health")
                        screen.navigate("system")
                    else
                        screen.updatesRequested()
                }
                Row {
                    id: cardRow
                    spacing: Theme.space4
                    Repeater {
                        model: statusCards.cards
                        delegate: Rectangle {
                            id: card
                            required property var modelData
                            required property int index
                            readonly property bool selected: statusCards.activeFocus && statusCards.current === index
                            objectName: "statusCard_" + modelData.key
                            width: (statusCards.width - Theme.space4 * 2) / 3
                            height: cardColumn.implicitHeight + Theme.space4 * 2
                            radius: Theme.radius
                            color: selected ? Theme.surfaceHover : Theme.surface
                            border.width: selected ? Theme.focusWidth : 1
                            border.color: selected ? Theme.focusRing : Theme.border
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.title + ": " + modelData.value
                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.margins: Theme.space3
                                width: Theme.space1
                                radius: width / 2
                                color: card.modelData.tone
                            }
                            Column {
                                id: cardColumn
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Theme.space4
                                anchors.leftMargin: Theme.space5 + Theme.space1
                                spacing: 2
                                Text {
                                    textFormat: Text.PlainText
                                    text: card.modelData.title
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSmall
                                }
                                Text {
                                    objectName: "statusValue_" + card.modelData.key
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: card.modelData.value
                                    textFormat: Text.PlainText // may carry an update bundle's version
                                    color: Theme.text
                                    font.pixelSize: Theme.fontLarge
                                    font.bold: true
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: card.modelData.detail
                                    color: Theme.textFaint
                                    font.pixelSize: Theme.fontTiny
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    statusCards.current = card.index
                                    statusCards.forceActiveFocus()
                                    statusCards.activate(card.index)
                                }
                            }
                        }
                    }
                }
                Keys.onLeftPressed: current = Math.max(0, current - 1)
                Keys.onRightPressed: current = Math.min(cards.length - 1, current + 1)
                Keys.onReturnPressed: activate(current)
                Keys.onEnterPressed: activate(current)
            }

            // Getting started / nothing found.
            Rectangle {
                id: emptyPanel
                objectName: "homeEmptyState"
                visible: screen.noGames
                width: parent.width
                height: emptyColumn.implicitHeight + Theme.space5 * 2
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border

                Column {
                    id: emptyColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space5
                    spacing: Theme.space3
                    Text {
                        textFormat: Text.PlainText
                        width: parent.width
                        text: screen.library.scanning ? "Scanning your library…"
                            : screen.noDirectories ? "Let's add your games"
                            : "No games found yet"
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                    }
                    Text {
                        textFormat: Text.PlainText
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
                        textFormat: Text.PlainText
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
                onActivated: (gameId) => screen.detailsRequested(gameId)
                onOptionsRequested: (gameId) => screen.optionsRequested(gameId)
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
                onActivated: (gameId) => screen.detailsRequested(gameId)
                onOptionsRequested: (gameId) => screen.optionsRequested(gameId)
                onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)
            }

            GameRow {
                id: addedRow
                objectName: "recentlyAddedRow"
                visible: !screen.noGames
                width: parent.width
                library: screen.library
                model: screen.library.recentlyAdded
                title: "Recently added"
                emptyText: "Newly found games show up here after a library scan."
                onActivated: (gameId) => screen.detailsRequested(gameId)
                onOptionsRequested: (gameId) => screen.optionsRequested(gameId)
                onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)
            }

            // Installed systems; Accept opens Games filtered to that system.
            FocusScope {
                id: systemsRow
                objectName: "homeSystemsRow"
                visible: screen.systemsWithGames.length > 0
                width: parent.width
                height: systemsHeading.height + Theme.space3 + systemsList.height
                onActiveFocusChanged: if (activeFocus) screen.ensureVisible(this)
                Text {
                    textFormat: Text.PlainText
                    id: systemsHeading
                    text: "Systems"
                    color: systemsRow.activeFocus ? Theme.text : Theme.textMuted
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
                ListView {
                    id: systemsList
                    objectName: "homeSystemsList"
                    anchors.top: systemsHeading.bottom
                    anchors.topMargin: Theme.space3
                    width: parent.width
                    height: Math.round(96 * Theme.scale)
                    focus: true
                    orientation: ListView.Horizontal
                    spacing: Theme.space4
                    clip: true
                    model: screen.systemsWithGames
                    keyNavigationEnabled: true
                    highlightMoveDuration: Theme.animMove
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: Rectangle {
                        id: sysCard
                        required property var modelData
                        required property int index
                        readonly property bool selected: ListView.isCurrentItem && systemsList.activeFocus
                        width: Math.round(200 * Theme.scale)
                        height: systemsList.height
                        radius: Theme.radius
                        color: selected ? Theme.surfaceHover : Theme.surfaceRaised
                        border.width: selected ? Theme.focusWidth : 1
                        border.color: selected ? Theme.focusRing : Theme.border
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.name
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.space4
                            spacing: 2
                            Text {
                                textFormat: Text.PlainText
                                width: parent.width
                                elide: Text.ElideRight
                                text: sysCard.modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                font.bold: true
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: sysCard.modelData.gameCount === 1 ? "1 game" : sysCard.modelData.gameCount + " games"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSmall
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: screen.systemChosen(sysCard.modelData.systemId)
                        }
                    }
                    Keys.onReturnPressed: if (currentIndex >= 0) screen.systemChosen(screen.systemsWithGames[currentIndex].systemId)
                    Keys.onEnterPressed: if (currentIndex >= 0) screen.systemChosen(screen.systemsWithGames[currentIndex].systemId)
                }
            }
        }
    }
}
