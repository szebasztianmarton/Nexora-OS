import QtQuick
import QtQuick.Window
import Nexora.Core

// Favorites: the same grid as the library, limited to favorite games.
PageContainer {
    id: page

    title: qsTr("Favorites")
    subtitle: {
        if (loading) return qsTr("Scanning…")
        if (total === 0) return ""
        if (filter.searchText !== "")
            return (total === 1 ? qsTr("%1 of 1 favorite") : qsTr("%1 of %2 favorites")).arg(filter.count).arg(total)
        return total === 1 ? qsTr("1 favorite") : qsTr("%1 favorites").arg(total)
    }

    property string sortOverride: ""
    readonly property string sortMode: sortOverride !== "" ? sortOverride : Settings.defaultSort
    readonly property bool loading: Library.scanning && !Library.hasScanned
    readonly property int total: allFavorites.count
    readonly property bool empty: !loading && total === 0
    readonly property bool noResults: !loading && total > 0 && filter.count === 0
    readonly property bool showGrid: !loading && filter.count > 0
    property Item lastToolbarItem: searchField

    defaultFocusItem: showGrid ? gameGrid : (stateView.visible && stateView.firstFocusable ? stateView.firstFocusable : searchField)

    readonly property bool gridFocused: gameGrid.activeFocus
    readonly property bool searchFocused: searchField.activeFocus
    hints: {
        if (searchFocused)
            return [{ action: "confirm", label: qsTr("Keyboard") }, { action: "back", label: qsTr("Done") }]
        if (gridFocused)
            return [{ action: "confirm", label: qsTr("Details") }, { action: "secondary", label: qsTr("Unfavorite") },
                    { action: "options", label: qsTr("More") }, { action: "search", label: qsTr("Search") }]
        if (total > 0)
            return [{ action: "confirm", label: qsTr("Select") }, { action: "search", label: qsTr("Search") }]
        return [{ action: "confirm", label: qsTr("Select") }]
    }

    function focusBelowToolbar() {
        if (showGrid) gameGrid.focusGrid()
        else if (stateView.visible && stateView.firstFocusable) stateView.firstFocusable.forceActiveFocus()
    }

    Keys.onPressed: function(event) {
        if (Input.isSearch(event) && !event.isAutoRepeat && toolbar.visible) {
            event.accepted = true
            searchField.forceActiveFocus()
        }
    }

    // Unfiltered favorites (for the total and the empty state).
    GameFilterModel {
        id: allFavorites
        sourceModel: Library.model
        favoritesOnly: true
    }
    GameFilterModel {
        id: filter
        sourceModel: Library.model
        favoritesOnly: true
        searchText: searchField.text
        sortMode: page.sortMode
    }

    Row {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Dimensions.spacingMd
        visible: !page.loading && page.total > 0
        height: visible ? implicitHeight : 0

        SearchField {
            id: searchField
            width: Math.min(Dimensions.s(420), toolbar.width * 0.4)
            placeholder: qsTr("Search favorites")
            KeyNavigation.right: chipSort
            onLeaveRequested: page.focusBelowToolbar()
            onAccepted: page.focusBelowToolbar()
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = searchField
        }
        OptionChip {
            id: chipSort
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Sort: %1").arg(gameGrid.sortLabel(page.sortMode))
            iconName: "sort"
            onClicked: gameGrid.openSortMenu(page.sortMode, function(id) { page.sortOverride = id })
            KeyNavigation.left: searchField
            onActiveFocusChanged: if (activeFocus) page.lastToolbarItem = chipSort
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Down) {
                event.accepted = true
                page.focusBelowToolbar()
            }
        }
    }

    GameGrid {
        id: gameGrid
        anchors.top: toolbar.bottom
        anchors.topMargin: toolbar.visible ? Dimensions.spacingSm : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: -Dimensions.cardGap / 2
        anchors.rightMargin: -Dimensions.cardGap / 2
        visible: page.showGrid
        model: filter
        upTarget: toolbar.visible ? page.lastToolbarItem : null
    }

    EmptyState {
        id: stateView
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Dimensions.spacingXl
        visible: page.loading || page.empty || page.noResults
        kind: page.loading ? "loading" : "empty"
        iconName: page.noResults ? "search" : "star"
        title: page.loading ? qsTr("Scanning your library…")
             : page.noResults ? qsTr("No favorites found")
             : qsTr("No favorites yet")
        message: {
            if (page.loading) return ""
            if (page.noResults) return qsTr("Nothing in your favorites matches \"%1\".").arg(searchField.text)
            if (Library.model.count === 0)
                return qsTr("Your library is empty. Once games are indexed you can mark the ones you play most as favorites.")
            // Re-evaluated when the input device changes.
            var key = (Controllers.lastInputDevice, Controllers.controllerType, Controllers.buttonLabel("secondary"))
            return qsTr("Press %1 on a game card, or select Favorite on a game's details page. Favorites appear here and on Home.").arg(key)
        }
        primaryText: page.loading ? "" : page.noResults ? qsTr("Clear search") : qsTr("Browse library")
        primaryIcon: page.noResults ? "close" : "library"
        onPrimaryClicked: {
            if (page.noResults) { searchField.text = ""; Qt.callLater(gameGrid.focusGrid) }
            else Router.showSection("library")
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Up && toolbar.visible) {
                event.accepted = true
                page.lastToolbarItem.forceActiveFocus()
            }
        }
    }

    onShowGridChanged: Qt.callLater(fixFocus)
    onEmptyChanged: Qt.callLater(fixFocus)
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
