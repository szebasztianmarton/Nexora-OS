import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Nexora.Core

// Home dashboard: status line, Continue Playing (or onboarding), Recently
// Played, Favorites and Quick Access. Everything shown comes from real state.
//
// Focus order (Up/Down between sections, empty sections skipped):
//   hero / onboarding -> Recently Played -> Favorites -> Quick Access
// Rows remember their own position; Left at the left edge goes to the sidebar.
PageContainer {
    id: page

    padded: false
    title: ""

    // ---- state ---------------------------------------------------------------
    property string heroPath: ""
    property var heroGame: ({})
    readonly property bool hasHero: !!heroGame.path
    readonly property int gameCount: Library.model.count
    readonly property bool scanningFirst: Library.scanning && !Library.hasScanned
    readonly property bool hasDirectories: Settings.libraryDirectories.length > 0
    readonly property bool hasEmulators: Emulators.count > 0

    function refreshHero() {
        heroPath = Library.mostRecentPath()
        heroGame = heroPath !== "" ? Library.get(heroPath) : ({})
    }
    Component.onCompleted: {
        // Re-check executables (cheap file checks) so status is current
        // whenever Home is first shown.
        Emulators.validateAll()
        refreshHero()
    }
    Connections {
        target: Library
        function onStateChanged() { page.refreshHero() }
        function onFavoriteChanged() { page.refreshHero() }
        function onScanFinished() { page.refreshHero() }
    }
    Connections {
        target: Library.model
        function onModelReset() { page.refreshHero() }
        function onDataChanged() { page.refreshHero() }
        function onRowsRemoved() { page.refreshHero() }
    }

    readonly property string statusMessage: {
        if (scanningFirst) return qsTr("Scanning your library…")
        if (!hasEmulators) return qsTr("No emulator configured")
        if (!hasDirectories) return qsTr("No game folders added yet")
        if (gameCount === 0) return qsTr("No games in your library yet")
        return gameCount === 1 ? qsTr("Your gaming library — 1 game")
                               : qsTr("Your gaming library — %1 games").arg(gameCount)
    }

    // ---- sections & focus ------------------------------------------------------
    readonly property var sections: [topSection, recentRow, favoritesRow, quickRow]

    defaultFocusItem: topSection

    readonly property bool cardFocused: recentRow.activeFocus || favoritesRow.activeFocus
    hints: cardFocused
           ? [{ action: "confirm", label: qsTr("Details") }, { action: "secondary", label: qsTr("Favorite") },
              { action: "options", label: qsTr("More") }]
           : [{ action: "confirm", label: qsTr("Select") }]

    function focusSection(section) {
        if (section === topSection) {
            if (page.hasHero) {
                if (hero.launchItem.enabled) hero.forceActiveFocus()
                else hero.detailsItem.forceActiveFocus()
            } else {
                onboarding.forceActiveFocus()
            }
        } else {
            section.forceActiveFocus()
        }
    }
    function sectionIndexOfFocus() {
        for (var i = 0; i < sections.length; ++i)
            if (sections[i].activeFocus) return i
        return -1
    }
    function moveSection(step) {
        var i = sectionIndexOfFocus()
        if (i < 0) return false
        for (var j = i + step; j >= 0 && j < sections.length; j += step) {
            if (sections[j].visible) {
                focusSection(sections[j])
                return true
            }
        }
        return false
    }

    // Keep the focused element (and preferably its whole section) visible.
    function ensureVisible(item) {
        if (!item) return
        var section = null
        for (var i = 0; i < sections.length; ++i) {
            var s = sections[i]
            for (var p = item; p; p = p.parent)
                if (p === s) { section = s; break }
            if (section) break
        }
        var target = section || item
        var pos = target.mapToItem(column, 0, 0)
        var margin = Dimensions.spacingXl
        var top = pos.y + column.y - margin
        var bottom = pos.y + column.y + target.height + margin
        if (section === topSection) top = 0          // keep the status line in view
        var y = flick.contentY
        if (bottom > y + flick.height) y = bottom - flick.height
        if (top < y) y = top
        y = Math.max(0, Math.min(y, Math.max(0, flick.contentHeight - flick.height)))
        if (Math.abs(y - flick.contentY) < 1) return
        scrollAnim.stop()
        scrollAnim.to = y
        scrollAnim.start()
    }
    Connections {
        target: page.Window.window
        function onActiveFocusItemChanged() {
            var item = page.Window.window ? page.Window.window.activeFocusItem : null
            if (item && item !== page && page._isDescendant(item))
                page.ensureVisible(item)
        }
    }

    // Never leave focus on a section that just became hidden.
    function fixFocus() {
        var w = page.Window.window
        var item = w ? w.activeFocusItem : null
        if (!item || !page._isDescendant(item)) return
        var hidden = item === page || item === flick || item === column
        for (var p = item; p && p !== page && !hidden; p = p.parent)
            if (!p.visible) hidden = true
        if (hidden) focusSection(topSection)
    }
    onHasHeroChanged: Qt.callLater(fixFocus)
    Connections {
        target: recentModel
        function onCountChanged() { Qt.callLater(page.fixFocus) }
    }
    Connections {
        target: favoritesModel
        function onCountChanged() { Qt.callLater(page.fixFocus) }
    }

    GameFilterModel {
        id: recentModel
        sourceModel: Library.model
        playedOnly: true
        sortMode: "recent"
        excludePath: page.heroPath
    }
    GameFilterModel {
        id: favoritesModel
        sourceModel: Library.model
        favoritesOnly: true
        sortMode: "title-asc"
    }

    NumberAnimation {
        id: scrollAnim
        target: flick
        property: "contentY"
        duration: Motion.page
        easing.type: Motion.easing
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: column.y + column.height + Dimensions.pagePaddingV
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        Column {
            id: column
            x: Dimensions.pagePaddingH
            y: Dimensions.pagePaddingV
            width: flick.width - Dimensions.pagePaddingH * 2
            spacing: Dimensions.spacingXl

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Down) {
                    if (page.moveSection(1)) event.accepted = true
                    else event.accepted = true          // bottom of the page: stay
                } else if (event.key === Qt.Key_Up) {
                    if (page.moveSection(-1)) event.accepted = true
                    else { event.accepted = true; scrollAnim.stop(); scrollAnim.to = 0; scrollAnim.start() }
                }
            }

            // ---- (A) identity + status ---------------------------------------
            Column {
                id: statusBlock
                width: parent.width
                spacing: Dimensions.spacingSm

                Text {
                    text: qsTr("NEXORA OS")
                    font.family: Typography.family
                    font.pixelSize: Typography.caption
                    font.weight: Typography.weightSemiBold
                    font.letterSpacing: Dimensions.s(3)
                    color: Theme.accentPrimary
                }
                Text {
                    width: parent.width
                    text: page.statusMessage
                    elide: Text.ElideRight
                    font.family: Typography.family
                    font.pixelSize: Typography.pageTitle
                    font.weight: Typography.weightBold
                    color: Theme.textPrimary
                }
                Flow {
                    width: parent.width
                    spacing: Dimensions.spacingLg
                    // Controller
                    Row {
                        spacing: Dimensions.spacingXs
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "gamepad"
                            size: Dimensions.iconSizeSmall
                            color: Controllers.count > 0 ? Theme.statusSuccess : Theme.textMuted
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: {
                                if (Controllers.count > 0) {
                                    var c = Controllers.controllers[0]
                                    return Controllers.count === 1 ? qsTr("%1 connected").arg(c.name)
                                                                   : qsTr("%1 controllers connected").arg(Controllers.count)
                                }
                                return Controllers.available ? qsTr("No controller connected — keyboard ready")
                                                             : qsTr("Controller support unavailable — keyboard ready")
                            }
                            font.family: Typography.family
                            font.pixelSize: Typography.secondary
                            color: Theme.textSecondary
                        }
                    }
                    // Emulators
                    Row {
                        spacing: Dimensions.spacingXs
                        visible: page.hasEmulators
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "chip"
                            size: Dimensions.iconSizeSmall
                            color: Emulators.availableCount > 0 ? Theme.statusSuccess : Theme.statusWarning
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("%1 of %2 emulators ready").arg(Emulators.availableCount).arg(Emulators.count)
                            font.family: Typography.family
                            font.pixelSize: Typography.secondary
                            color: Theme.textSecondary
                        }
                    }
                    // Background rescan
                    Row {
                        spacing: Dimensions.spacingXs
                        visible: Library.scanning && Library.hasScanned
                        LoadingIndicator {
                            anchors.verticalCenter: parent.verticalCenter
                            size: Dimensions.iconSizeSmall
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Refreshing library…")
                            font.family: Typography.family
                            font.pixelSize: Typography.secondary
                            color: Theme.textSecondary
                        }
                    }
                }
            }

            // ---- (B) Continue Playing / onboarding -----------------------------
            FocusScope {
                id: topSection
                width: parent.width
                height: page.hasHero ? hero.implicitHeight : onboarding.implicitHeight

                HomeHero {
                    id: hero
                    anchors.fill: parent
                    visible: page.hasHero
                    focus: page.hasHero
                    game: page.heroGame
                }

                // Onboarding: guides to the next real step.
                FocusScope {
                    id: onboarding
                    anchors.left: parent.left
                    anchors.right: parent.right
                    visible: !page.hasHero
                    focus: !page.hasHero
                    implicitHeight: onboardingLayout.implicitHeight + Dimensions.spacingXl * 2

                    // step: "scanning" | "emulators" | "folders" | "empty" | "browse"
                    readonly property string step: page.scanningFirst ? "scanning"
                                                 : !page.hasEmulators ? "emulators"
                                                 : !page.hasDirectories ? "folders"
                                                 : page.gameCount === 0 ? "empty" : "browse"

                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.radiusLg
                        color: Theme.backgroundElevated
                        border.color: Theme.borderSubtle
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.backgroundElevated }
                            GradientStop { position: 1.0; color: Theme.backgroundSecondary }
                        }
                    }

                    RowLayout {
                        id: onboardingLayout
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Dimensions.spacingXl
                        spacing: Dimensions.spacingXl

                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            implicitWidth: Dimensions.s(88)
                            implicitHeight: implicitWidth
                            radius: width / 2
                            color: Theme.accentPrimarySoft
                            Icon {
                                anchors.centerIn: parent
                                visible: onboarding.step !== "scanning"
                                name: onboarding.step === "emulators" ? "chip"
                                    : onboarding.step === "folders" ? "folder-plus"
                                    : onboarding.step === "empty" ? "search" : "gamepad"
                                size: Dimensions.iconSizeLarge
                                color: Theme.accentPrimary
                            }
                            LoadingIndicator {
                                anchors.centerIn: parent
                                visible: onboarding.step === "scanning"
                                size: Dimensions.iconSizeLarge
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingSm
                            Text {
                                Layout.fillWidth: true
                                text: {
                                    switch (onboarding.step) {
                                    case "scanning": return qsTr("Looking for your games")
                                    case "emulators": return qsTr("Set up an emulator to start playing")
                                    case "folders": return qsTr("Add the folder where your games live")
                                    case "empty": return qsTr("No games were found yet")
                                    default: return qsTr("Pick something to play")
                                    }
                                }
                                wrapMode: Text.WordWrap
                                font.family: Typography.family
                                font.pixelSize: Typography.sectionTitle
                                font.weight: Typography.weightSemiBold
                                color: Theme.textPrimary
                            }
                            Text {
                                Layout.fillWidth: true
                                text: {
                                    switch (onboarding.step) {
                                    case "scanning": return qsTr("Nexora is indexing your library folders. This only takes a moment.")
                                    case "emulators": return qsTr("Nexora launches games through the emulators listed in emulators.json. Review the emulator setup to add one.")
                                    case "folders": return qsTr("Choose one or more folders with game files in Settings › Game Library. Nothing outside those folders is scanned.")
                                    case "empty": return qsTr("Your library folders don't contain files that a configured emulator can open. Check the folders or the supported file types.")
                                    default: return page.gameCount === 1
                                             ? qsTr("You haven't launched a game from Nexora yet. Your library has 1 game ready.")
                                             : qsTr("You haven't launched a game from Nexora yet. Your library has %1 games ready.").arg(page.gameCount)
                                    }
                                }
                                wrapMode: Text.WordWrap
                                font.family: Typography.family
                                font.pixelSize: Typography.body
                                color: Theme.textSecondary
                            }
                            Row {
                                Layout.topMargin: Dimensions.spacingMd
                                spacing: Dimensions.spacingMd
                                visible: onboarding.step !== "scanning"
                                FocusButton {
                                    id: onboardingPrimary
                                    focus: true
                                    variant: "primary"
                                    text: {
                                        switch (onboarding.step) {
                                        case "emulators": return qsTr("Open Emulators")
                                        case "folders": return qsTr("Add game folder")
                                        case "empty": return qsTr("Open Game Library")
                                        default: return qsTr("Browse library")
                                        }
                                    }
                                    iconName: onboarding.step === "emulators" ? "chip"
                                            : onboarding.step === "folders" ? "folder-plus" : "library"
                                    onClicked: {
                                        switch (onboarding.step) {
                                        case "emulators": Router.showSection("emulators"); break
                                        case "folders": Router.openSettings("library"); break
                                        default: Router.showSection("library"); break
                                        }
                                    }
                                    KeyNavigation.right: onboardingSecondary.visible ? onboardingSecondary : null
                                }
                                FocusButton {
                                    id: onboardingSecondary
                                    visible: onboarding.step === "empty"
                                    text: qsTr("Manage folders")
                                    iconName: "folder"
                                    onClicked: Router.openSettings("library")
                                    KeyNavigation.left: onboardingPrimary
                                }
                            }
                        }
                    }
                }
            }

            // ---- (C) Recently Played ------------------------------------------------
            GameRow {
                id: recentRow
                width: parent.width
                visible: count > 0
                title: qsTr("Recently played")
                model: recentModel
            }

            // ---- (D) Favorites --------------------------------------------------------
            GameRow {
                id: favoritesRow
                width: parent.width
                visible: count > 0
                title: qsTr("Favorites")
                model: favoritesModel
            }

            // ---- (E) Quick Access -------------------------------------------------------
            FocusScope {
                id: quickRow
                width: parent.width
                height: quickHeading.height + Dimensions.spacingMd + quickTiles.height

                Text {
                    id: quickHeading
                    text: qsTr("Quick access")
                    font.family: Typography.family
                    font.pixelSize: Typography.sectionTitle
                    font.weight: Typography.weightSemiBold
                    color: quickRow.activeFocus ? Theme.textPrimary : Theme.textSecondary
                }
                RowLayout {
                    id: quickTiles
                    anchors.top: quickHeading.bottom
                    anchors.topMargin: Dimensions.spacingMd
                    width: parent.width
                    spacing: Dimensions.spacingMd

                    HomeQuickTile {
                        id: tileLibrary
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        focus: true
                        label: qsTr("Game Library")
                        caption: page.gameCount === 1 ? qsTr("1 game") : qsTr("%1 games").arg(page.gameCount)
                        iconName: "library"
                        onClicked: Router.showSection("library")
                        KeyNavigation.right: tileEmulators
                    }
                    HomeQuickTile {
                        id: tileEmulators
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        label: qsTr("Emulators")
                        caption: page.hasEmulators ? qsTr("%1 of %2 ready").arg(Emulators.availableCount).arg(Emulators.count)
                                                   : qsTr("None configured")
                        iconName: "chip"
                        onClicked: Router.showSection("emulators")
                        KeyNavigation.left: tileLibrary
                        KeyNavigation.right: tileSettings
                    }
                    HomeQuickTile {
                        id: tileSettings
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        label: qsTr("Settings")
                        caption: qsTr("Display, controller, library")
                        iconName: "settings"
                        onClicked: Router.showSection("settings")
                        KeyNavigation.left: tileEmulators
                        KeyNavigation.right: tileSystem
                    }
                    HomeQuickTile {
                        id: tileSystem
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        label: qsTr("System")
                        caption: qsTr("Diagnostics and logs")
                        iconName: "activity"
                        onClicked: Router.openSettings("system")
                        KeyNavigation.left: tileSettings
                    }
                }
            }
        }
    }
}
