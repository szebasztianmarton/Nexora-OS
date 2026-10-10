import QtQuick
import Nexora.Core

// Responsive grid of GameCards shared by the Game Library and Favorites pages
// so both behave identically.
//   - columns = floor(width / (cardMinWidth + gap)), clamped to 3..8; cards
//     fill the cell width
//   - D-pad moves between cards; keys that cannot move further (Up on the
//     first row, Left on the first column) are NOT accepted so the page /
//     shell can move focus (toolbar, sidebar)
//   - Confirm opens details, Secondary toggles favorite, Options opens the
//     game menu (all through GameActions)
//   - the card whose details were opened last stays `selected`
//   - the current game is kept (by path) across rescans, sorting and filtering
FocusScope {
    id: root

    property alias model: grid.model
    property alias currentIndex: grid.currentIndex
    readonly property int count: grid.count
    readonly property int columns: Math.max(3, Math.min(8, Math.floor((width + Dimensions.cardGap) / (Dimensions.cardMinWidth + Dimensions.cardGap))))
    readonly property real cellWidth: Math.floor(width / columns)
    readonly property real cardWidth: cellWidth - Dimensions.cardGap
    // Path of the game whose details were opened last (persistent selection).
    property string selectedPath: ""
    // Path of the current card; used to restore the position after model changes.
    property string currentPath: ""
    // Item focused when Up is pressed on the first row (e.g. the toolbar).
    property Item upTarget: null

    // Sort options shared by the pages that use the grid.
    readonly property var sortOptions: [
        { id: "title-asc", label: qsTr("A–Z") },
        { id: "title-desc", label: qsTr("Z–A") },
        { id: "recent", label: qsTr("Recently played") },
        { id: "added", label: qsTr("Recently added") }
    ]
    function sortLabel(mode) {
        for (var i = 0; i < sortOptions.length; ++i)
            if (sortOptions[i].id === mode) return sortOptions[i].label
        return sortOptions[0].label
    }
    function openSortMenu(current, onChosen) {
        Router.openMenu({
            title: qsTr("Sort by"),
            actions: sortOptions.map(function(o) {
                return { id: o.id, label: o.label, icon: o.id === current ? "check" : "sort" }
            })
        }, onChosen)
    }

    function focusGrid() {
        if (grid.count === 0) return
        if (grid.currentIndex < 0) grid.currentIndex = 0
        grid.forceActiveFocus()
    }

    function _setCurrent(index) {
        if (index < 0 || index >= grid.count) return
        grid.currentIndex = index
        grid.positionViewAtIndex(index, GridView.Contain)
        _rememberCurrent()
    }
    function _rememberCurrent() {
        var game = grid.model && grid.currentIndex >= 0 ? grid.model.get(grid.currentIndex) : null
        if (game && game.path) currentPath = game.path
    }
    function _restoreCurrent() {
        if (!grid.model || grid.count === 0) return
        var index = currentPath !== "" ? grid.model.indexOfPath(currentPath) : -1
        if (index < 0) index = Math.min(Math.max(grid.currentIndex, 0), grid.count - 1)
        grid.currentIndex = index
        grid.positionViewAtIndex(index, GridView.Contain)
        _rememberCurrent()
    }

    Connections {
        target: grid.model
        ignoreUnknownSignals: true
        function onModelReset() { Qt.callLater(root._restoreCurrent) }
        function onLayoutChanged() { Qt.callLater(root._restoreCurrent) }
        function onRowsRemoved() { Qt.callLater(root._restoreCurrent) }
        function onRowsInserted() { Qt.callLater(root._restoreCurrent) }
    }

    // Positioning while the page was hidden (e.g. under a details page) may
    // not have applied; make sure the current card is visible on (re)entry.
    onActiveFocusChanged: if (activeFocus && grid.currentIndex >= 0) grid.positionViewAtIndex(grid.currentIndex, GridView.Contain)

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Up && root.upTarget && root.upTarget.visible && root.upTarget.enabled) {
            event.accepted = true
            root.upTarget.forceActiveFocus()
        }
    }

    GridView {
        id: grid
        anchors.fill: parent
        focus: true
        clip: true
        // Room for the focus ring + focus scale of the first row.
        topMargin: Dimensions.focusOffset + Dimensions.focusWidth * 2 + Dimensions.s(6)
        bottomMargin: Dimensions.spacingLg
        cellWidth: root.cellWidth
        cellHeight: Math.round(root.cardWidth * Dimensions.cardAspect + Dimensions.cardTitleHeight + Dimensions.cardGap)
        keyNavigationEnabled: false      // handled below so edge keys propagate
        boundsBehavior: Flickable.StopAtBounds
        highlightFollowsCurrentItem: false
        cacheBuffer: cellHeight * 2
        currentIndex: 0

        Keys.onPressed: function(event) {
            var cols = root.columns
            var i = grid.currentIndex
            var n = grid.count
            if (n === 0) return
            switch (event.key) {
            case Qt.Key_Left:
                if (i % cols > 0) { event.accepted = true; root._setCurrent(i - 1) }
                break
            case Qt.Key_Right:
                if (i % cols < cols - 1 && i + 1 < n) { event.accepted = true; root._setCurrent(i + 1) }
                else event.accepted = true   // nothing to the right: stay put
                break
            case Qt.Key_Up:
                if (i >= cols) { event.accepted = true; root._setCurrent(i - cols) }
                break
            case Qt.Key_Down:
                event.accepted = true
                if (Math.floor(i / cols) < Math.floor((n - 1) / cols))
                    root._setCurrent(Math.min(i + cols, n - 1))
                break
            case Qt.Key_Home:
                event.accepted = true; root._setCurrent(0)
                break
            case Qt.Key_End:
                event.accepted = true; root._setCurrent(n - 1)
                break
            }
        }

        delegate: FocusScope {
            id: cell
            required property int index
            required property var model
            width: grid.cellWidth
            height: grid.cellHeight
            // Focus follows the view's current item.
            focus: GridView.isCurrentItem

            GameCard {
                id: card
                x: Math.round(Dimensions.cardGap / 2)
                width: root.cardWidth
                height: root.cardWidth * Dimensions.cardAspect + Dimensions.cardTitleHeight
                focus: true
                title: cell.model.title
                artworkUrl: cell.model.artworkUrl
                favorite: cell.model.favorite
                subtitle: cell.model.emulatorName || qsTr("No emulator")
                selected: root.selectedPath !== "" && cell.model.path === root.selectedPath
                onClicked: { grid.currentIndex = cell.index; root._rememberCurrent() }
                onActivated: {
                    grid.currentIndex = cell.index
                    root._rememberCurrent()
                    root.selectedPath = cell.model.path
                    GameActions.openDetails(cell.model.path)
                }
                onFavoriteToggled: GameActions.toggleFavorite(cell.model.path)
                onOptionsRequested: {
                    grid.currentIndex = cell.index
                    root._rememberCurrent()
                    GameActions.openMenu(cell.model.path)
                }
            }
        }
    }
}
