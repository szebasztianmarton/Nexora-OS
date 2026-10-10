import QtQuick
import QtQuick.Window
import Nexora.Core

// Game Library: search, sort and filter the indexed games, browse them in a
// responsive grid and open details / launch from there.
PageContainer {
    id: page

    title: qsTr("Game Library")
    subtitle: {
        var total = Library.model.count
        if (!Library.hasScanned && Library.scanning) return qsTr("Scanning…")
        if (filter.filtersActive && total > 0)
            return (total === 1 ? qsTr("%1 of 1 game") : qsTr("%1 of %2 games")).arg(filter.count).arg(total)
        return total === 1 ? qsTr("1 game") : qsTr("%1 games").arg(total)
    }

    // ---- view state -----------------------------------------------------
    property string sortOverride: ""
    property bool favoritesFilter: false
    property bool artworkFilter: false
    property string emulatorFilter: ""
    readonly property string sortMode: sortOverride !== "" ? sortOverride : Settings.defaultSort
    readonly property bool anyChipFilter: favoritesFilter || artworkFilter || emulatorFilter !== ""

    readonly property bool loading: Library.scanning && !Library.hasScanned
    readonly property bool libraryEmpty: !loading && Library.model.count === 0
    readonly property bool noResults: !loading && Library.model.count > 0 && filter.count === 0
    readonly property bool showGrid: !loading && filter.count > 0
    readonly property bool hasDirectories: Settings.libraryDirectories.length > 0
    readonly property bool hasUsableEmulators: Emulators.supportedExtensions.length > 0

    // Last focused toolbar control, so Up from the grid returns to it.
    property Item lastToolbarItem: searchField

    defaultFocusItem: showGrid ? gameGrid : (stateView.visible && stateView.firstFocusable ? stateView.firstFocusable : searchField)

    readonly property bool gridFocused: gameGrid.activeFocus
    readonly property bool searchFocused: searchField.activeFocus
    hints: {
        if (searchFocused)
            return [{ action: "confirm", label: qsTr("Keyboard") }, { action: "back", label: qsTr("Done") }]
        if (gridFocused)
            return [{ action: "confirm", label: qsTr("Details") }, { action: "secondary", label: qsTr("Favorite") },
                    { action: "options", label: qsTr("More") }, { action: "search", label: qsTr("Search") }]
        return toolbar.visible ? [{ action: "confirm", label: qsTr("Select") }, { action: "search", label: qsTr("Search") }]
                               : [{ action: "confirm", label: qsTr("Select") }]
    }

    function clearAll() {
        searchField.text = ""
        favoritesFilter = false
        artworkFilter = false
        emulatorFilter = ""
    }

    function emulatorName(id) {
        var emu = Emulators.byId(id)
        return emu && emu.name ? emu.name : id
    }

    function openEmulatorFilter() {
        var actions = [{ id: "", label: qsTr("All emulators"), icon: emulatorFilter === "" ? "check" : "chip" }]
        for (var i = 0; i < Emulators.count; ++i) {
            var emu = Emulators.get(i)
            if (emu.gameCount > 0)
                actions.push({ id: emu.id, label: qsTr("%1 (%2)").arg(emu.name).arg(emu.gameCount),
                               icon: emu.id === emulatorFilter ? "check" : "chip" })
        }
        Router.openMenu({ title: qsTr("Filter by emulator"), actions: actions },
                        function(id) { page.emulatorFilter = id })
    }

    function showSkipped() {
        var list = Library.skippedEntries
        var shown = list.slice(0, 12)
        var message = shown.join("\n")
        if (list.length > shown.length)
            message += "\n" + qsTr("…and %1 more (see the log file)").arg(list.length - shown.length)
        Router.showError({
            title: qsTr("Some locations couldn't be scanned"),
            message: qsTr("Games in these locations are not shown. Check that the folders exist and are readable, then refresh."),
            details: message,
            kind: "warning",
            actions: [{ id: "close", label: qsTr("Close"), primary: true },
                      { id: "folders", label: qsTr("Manage folders") }]
        }, function(id) { if (id === "folders") Router.openSettings("library") })
    }

    function focusBelowToolbar() {
        if (warningBanner.visible) warningBanner.forceActiveFocus()
        else if (showGrid) gameGrid.focusGrid()
        else if (stateView.visible && stateView.firstFocusable) stateView.firstFocusable.forceActiveFocus()
    }

    // Search key jumps to the search field from anywhere on the page.
    Keys.onPressed: function(event) {
        if (Input.isSearch(event) && !event.isAutoRepeat && toolbar.visible) {
            event.accepted = true
            searchField.forceActiveFocus()
        }
    }

    GameFilterModel {
        id: filter
        sourceModel: Library.model
        searchText: searchField.text
        sortMode: page.sortMode
        favoritesOnly: page.favoritesFilter
        artworkOnly: page.artworkFilter
        emulatorFilter: page.emulatorFilter
    }

    headerActions: [
        FocusButton {
            id: refreshButton
            small: true
            text: Library.scanning ? qsTr("Scanning…") : qsTr("Refresh")
            iconName: "refresh"
            busy: Library.scanning
            onClicked: Library.refresh()
            KeyNavigation.down: searchField
        }
    ]

    // ---- toolbar ------------------------------------------------------------
    Row {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Dimensions.spacingSm
        visible: !page.loading && Library.model.count > 0
        height: visible ? implicitHeight : 0

        SearchField {
            id: searchField
            width: Math.min(Dimensions.s(420), toolbar.width * 0.34)
            placeholder: qsTr("Search games")
            KeyNavigation.right: chipAll
            KeyNavigation.up: refreshButton
            onLeaveRequested: page.focusBelowToolbar()
            onAccepted: page.focusBelowToolbar()
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = searchField
        }
        Item { width: Dimensions.spacingSm; height: 1 }
        OptionChip {
            id: chipAll
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("All")
            checked: !page.anyChipFilter
            onClicked: { page.favoritesFilter = false; page.artworkFilter = false; page.emulatorFilter = "" }
            KeyNavigation.left: searchField
            KeyNavigation.right: chipFavorites
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = chipAll
        }
        OptionChip {
            id: chipFavorites
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Favorites")
            iconName: "star"
            checked: page.favoritesFilter
            onClicked: page.favoritesFilter = !page.favoritesFilter
            KeyNavigation.left: chipAll
            KeyNavigation.right: chipArtwork
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = chipFavorites
        }
        OptionChip {
            id: chipArtwork
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("With artwork")
            iconName: "monitor"
            checked: page.artworkFilter
            onClicked: page.artworkFilter = !page.artworkFilter
            KeyNavigation.left: chipFavorites
            KeyNavigation.right: chipEmulator
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = chipArtwork
        }
        OptionChip {
            id: chipEmulator
            anchors.verticalCenter: parent.verticalCenter
            text: page.emulatorFilter === "" ? qsTr("Emulator") : page.emulatorName(page.emulatorFilter)
            iconName: "chip"
            checked: page.emulatorFilter !== ""
            onClicked: page.openEmulatorFilter()
            KeyNavigation.left: chipArtwork
            KeyNavigation.right: chipSort
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = chipEmulator
        }
        OptionChip {
            id: chipSort
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Sort: %1").arg(gameGrid.sortLabel(page.sortMode))
            iconName: "sort"
            onClicked: gameGrid.openSortMenu(page.sortMode, function(id) { page.sortOverride = id })
            KeyNavigation.left: chipEmulator
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = chipSort
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Down) {
                event.accepted = true
                page.focusBelowToolbar()
            } else if (event.key === Qt.Key_Up) {
                event.accepted = true
                refreshButton.forceActiveFocus()
            }
        }
    }

    // ---- partial scan warning ---------------------------------------------
    FocusScope {
        id: warningBanner
        anchors.top: toolbar.bottom
        anchors.topMargin: visible ? Dimensions.spacingMd : 0
        anchors.left: parent.left
        width: Math.min(parent.width, bannerRow.implicitWidth + Dimensions.spacingLg * 2)
        height: visible ? Dimensions.buttonHeightSmall : 0
        visible: !page.loading && Library.skippedEntries.length > 0
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: bannerText.text

        Keys.onPressed: function(event) {
            if (Input.isConfirm(event)) {
                event.accepted = true
                if (!event.isAutoRepeat) page.showSkipped()
            } else if (event.key === Qt.Key_Up) {
                event.accepted = true
                if (toolbar.visible) page.lastToolbarItem.forceActiveFocus()
                else refreshButton.forceActiveFocus()
            } else if (event.key === Qt.Key_Down) {
                event.accepted = true
                if (page.showGrid) gameGrid.focusGrid()
                else if (stateView.visible && stateView.firstFocusable) stateView.firstFocusable.forceActiveFocus()
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: warningBanner.activeFocus || bannerMouse.containsMouse
                   ? Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.18)
                   : Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.09)
            border.width: 1
            border.color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.45)
        }
        FocusFrame { shown: warningBanner.activeFocus; radiusBase: warningBanner.height / 2 }
        Row {
            id: bannerRow
            anchors.centerIn: parent
            spacing: Dimensions.spacingSm
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "warning"
                size: Dimensions.iconSizeSmall
                color: Theme.statusWarning
            }
            Text {
                id: bannerText
                anchors.verticalCenter: parent.verticalCenter
                text: Library.skippedEntries.length === 1 ? qsTr("1 location couldn't be scanned")
                      : qsTr("%1 locations couldn't be scanned").arg(Library.skippedEntries.length)
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                color: Theme.textPrimary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("View details")
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                font.weight: Typography.weightSemiBold
                color: Theme.statusWarning
            }
        }
        MouseArea {
            id: bannerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: { warningBanner.forceActiveFocus(); page.showSkipped() }
        }
    }

    // ---- grid -----------------------------------------------------------------
    GameGrid {
        id: gameGrid
        anchors.top: warningBanner.bottom
        anchors.topMargin: Dimensions.spacingSm
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // Bleed into the page padding so focus rings are not clipped.
        anchors.leftMargin: -Dimensions.cardGap / 2
        anchors.rightMargin: -Dimensions.cardGap / 2
        visible: page.showGrid
        model: filter
        upTarget: warningBanner.visible ? warningBanner : (toolbar.visible ? page.lastToolbarItem : refreshButton)
    }

    // ---- loading / empty / no results ----------------------------------------
    EmptyState {
        id: stateView
        anchors.top: warningBanner.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Dimensions.spacingXl
        visible: page.loading || page.libraryEmpty || page.noResults

        kind: page.loading ? "loading" : "empty"
        iconName: page.noResults ? "search"
                : !page.hasDirectories ? "folder-plus"
                : !page.hasUsableEmulators ? "chip" : "library"
        title: page.loading ? qsTr("Scanning your library…")
             : page.noResults ? qsTr("No games found")
             : qsTr("Your library is empty")
        message: {
            if (page.loading)
                return qsTr("Looking for games in your library folders.")
            if (page.noResults) {
                if (searchField.text !== "")
                    return page.anyChipFilter ? qsTr("Nothing matches \"%1\" with the current filters.").arg(searchField.text)
                                              : qsTr("Nothing in your library matches \"%1\".").arg(searchField.text)
                return qsTr("No games match the current filters.")
            }
            if (!page.hasDirectories)
                return qsTr("Add a folder that contains your games and Nexora will index it.")
            if (!page.hasUsableEmulators)
                return Emulators.count === 0
                        ? qsTr("No emulators are configured, so Nexora doesn't know which files are games. Set up an emulator first.")
                        : qsTr("None of the configured emulators is enabled, so no game files can be recognised.")
            return Settings.libraryDirectories.length === 1
                    ? qsTr("No supported game files were found in your library folder.")
                    : qsTr("No supported game files were found in your %1 library folders.").arg(Settings.libraryDirectories.length)
        }
        details: {
            if (page.libraryEmpty && page.hasDirectories && page.hasUsableEmulators)
                return qsTr("Supported file types: %1").arg(Emulators.supportedExtensions.map(function(e) { return "." + e }).join("  "))
            return ""
        }
        primaryText: {
            if (page.loading) return ""
            if (page.noResults)
                return searchField.text !== "" && page.anyChipFilter ? qsTr("Clear search and filters")
                     : searchField.text !== "" ? qsTr("Clear search") : qsTr("Clear filters")
            if (!page.hasDirectories) return qsTr("Add game folder")
            if (!page.hasUsableEmulators) return qsTr("Set up emulators")
            return qsTr("Refresh")
        }
        primaryIcon: page.noResults ? "close" : !page.hasDirectories ? "folder-plus"
                   : !page.hasUsableEmulators ? "chip" : "refresh"
        secondaryText: {
            if (page.loading || page.noResults) return ""
            if (!page.hasDirectories && !page.hasUsableEmulators) return qsTr("Set up emulators")
            if (page.hasDirectories && page.hasUsableEmulators) return qsTr("Manage folders")
            return ""
        }
        secondaryIcon: !page.hasDirectories ? "chip" : "folder"
        onPrimaryClicked: {
            if (page.noResults) { page.clearAll(); Qt.callLater(gameGrid.focusGrid); return }
            if (!page.hasDirectories) Router.openSettings("library")
            else if (!page.hasUsableEmulators) Router.showSection("emulators")
            else Library.refresh()
        }
        onSecondaryClicked: {
            if (!page.hasDirectories) Router.showSection("emulators")
            else Router.openSettings("library")
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Up) {
                event.accepted = true
                if (warningBanner.visible) warningBanner.forceActiveFocus()
                else if (toolbar.visible) page.lastToolbarItem.forceActiveFocus()
                else refreshButton.forceActiveFocus()
            }
        }
    }

    // When the visible state changes (e.g. scan finished, search emptied the
    // grid), never leave focus on a hidden item.
    onShowGridChanged: Qt.callLater(fixFocus)
    onLibraryEmptyChanged: Qt.callLater(fixFocus)
    onNoResultsChanged: Qt.callLater(fixFocus)
    function fixFocus() {
        var w = page.Window.window
        var item = w ? w.activeFocusItem : null
        if (!item || !page._isDescendant(item)) return
        var hidden = item === page
        for (var p = item; p && p !== page && !hidden; p = p.parent)
            if (!p.visible) hidden = true
        if (hidden && page.defaultFocusItem) page.defaultFocusItem.forceActiveFocus()
    }
}
