import QtQuick
import QtQuick.Controls
import Nexora.Console

// Games: search, filter chips (+ grid/list toggle), system filter chips, scan progress with
// Cancel, the sort selector and the game grid. Accept on a game opens its details.
// Focus rows (top to bottom): [search, filter chips] → system chips → scan Cancel (while
// scanning) → sort → grid. Up from the top row reaches the tab bar.
FocusScope {
    id: screen
    objectName: "gamesScreen"
    required property AppController app
    required property LibraryController library
    // Height the floating glass bars cover at the top / bottom of the page (set by Main.qml;
    // 0 when the bars are docked).
    property real chromeTop: 0
    property real chromeBottom: 0
    // Glass: the grid runs to the bottom of the window, under the scan summary and the hint
    // bar; the current card stays above both. Docked: the grid stops above the summary.
    readonly property bool floatingChrome: chromeBottom > 0

    signal navigate(string tab)
    signal optionsRequested(real gameId)
    signal detailsRequested(real gameId)

    readonly property alias grid: grid
    readonly property real currentGameId: grid.currentGameId
    readonly property bool searchActive: searchField.activeFocus
    property bool listMode: false
    readonly property var sortOrders: ["title", "recent", "emulator", "added"]
    readonly property var hints: searchField.activeFocus
        ? [{ button: "Type", action: "Search" }, { button: "Enter / ↓", action: "Results" },
           { button: "B / Esc", action: "Leave search" }]
        : sortSelector.activeFocus
          ? [{ button: "← →", action: "Change sort" }, { button: "↓", action: "Games" },
             { button: "↑", action: "Filters" }, { button: "L1 / R1", action: "Switch tab" }]
          : emptyAction.activeFocus
            ? [{ button: "A / Enter", action: emptyAction.text }, { button: "↑", action: "Search & sort" },
               { button: "L1 / R1", action: "Switch tab" }]
          : chips.activeFocus || systemChips.activeFocus
            ? [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Apply" },
               { button: "↑ ↓", action: "Rows" }, { button: "L1 / R1", action: "Switch tab" }]
          : cancelScanButton.activeFocus
            ? [{ button: "A / Enter", action: "Cancel scan" }, { button: "↑ ↓", action: "Rows" }]
            : [{ button: "↑ ↓ ← →", action: "Move" }, { button: "A / Enter", action: "Details" },
               { button: "Y / F", action: "Favorite" }, { button: "Select / O", action: "Options" },
               { button: "X / “/”", action: "Search" },
               { button: "L1 / R1", action: "Switch tab" }]

    // Systems that actually have games (the catalog lists every supported one).
    property int systemsRevision: 0
    readonly property var systemsWithGames: {
        void systemsRevision
        const list = []
        const model = library.systems
        for (let i = 0; i < model.count; ++i) {
            const s = model.get(i)
            if (s.gameCount > 0 && s.systemId.length > 0)
                list.push(s)
        }
        return list
    }
    Connections {
        target: screen.library
        function onContentChanged() { screen.systemsRevision += 1 }
    }

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }
    function focusResults() {
        if (grid.count > 0)
            grid.forceActiveFocus()
        else if (emptyAction.visible)
            emptyAction.forceActiveFocus()
        else
            sortSelector.forceActiveFocus()
    }
    function focusDefault() {
        if (grid.count > 0)
            grid.focus = true
        else if (emptyAction.visible)
            emptyAction.focus = true
        else
            searchField.focus = true
    }
    function cycleSort(step) {
        const i = Math.max(0, sortOrders.indexOf(library.sortOrder))
        library.sortOrder = sortOrders[(i + step + sortOrders.length) % sortOrders.length]
    }

    // Focus rows in visual order; hidden rows are skipped.
    function focusRows() {
        const rows = [toolbar]
        if (systemChips.visible)
            rows.push(systemChips)
        if (cancelScanButton.visible)
            rows.push(cancelScanButton)
        rows.push(sortSelector)
        rows.push(resultsRow)
        return rows
    }
    function currentRow() {
        const rows = focusRows()
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].activeFocus)
                return i
        }
        return grid.activeFocus || emptyAction.activeFocus ? rows.length - 1 : -1
    }
    function focusRow(item) {
        if (item === toolbar)
            (chips.lastFocused >= 0 ? chips : searchField).forceActiveFocus()
        else if (item === systemChips)
            systemChips.focusChip(systemChips.lastFocused >= 0 ? systemChips.lastFocused : systemChips.selectedIndex())
        else if (item === resultsRow)
            focusResults()
        else
            item.forceActiveFocus()
    }
    function moveRow(delta) {
        const rows = focusRows()
        const next = currentRow() + delta
        if (currentRow() < 0 || next < 0 || next >= rows.length)
            return false
        focusRow(rows[next])
        return true
    }

    Component.onCompleted: focusDefault()

    Keys.onUpPressed: (event) => { event.accepted = moveRow(-1) }
    Keys.onDownPressed: (event) => { event.accepted = moveRow(1) }

    // --- Row 1: search + filters --------------------------------------------------------
    FocusScope {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.space4 + screen.chromeTop
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        height: Theme.controlHeight

        TextField {
            id: searchField
            objectName: "searchField"
            anchors.left: parent.left
            anchors.right: chips.left
            anchors.rightMargin: Theme.spacing
            height: parent.height
            text: screen.library.searchText
            onTextEdited: screen.library.searchText = text
            placeholderText: "Search games  (X / “/”)"
            placeholderTextColor: Theme.textFaint
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.text
            font.pixelSize: Theme.fontBody
            leftPadding: Theme.space6 + Theme.space3
            rightPadding: Theme.space4
            verticalAlignment: TextInput.AlignVCenter
            background: Rectangle {
                radius: height / 2
                color: searchField.activeFocus ? Theme.surfaceHover : Theme.surfaceRaised
                border.width: searchField.activeFocus ? Theme.focusWidth : 1
                border.color: searchField.activeFocus ? Theme.focusRing : Theme.border
                Text {
                    textFormat: Text.PlainText
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space4
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⌕"
                    color: searchField.activeFocus ? Theme.accent : Theme.textMuted
                    font.pixelSize: Math.round(22 * Theme.scale)
                }
            }

            Keys.onEscapePressed: screen.focusResults()
            Keys.onBackPressed: screen.focusResults()
            Keys.onReturnPressed: screen.focusResults()
            Keys.onEnterPressed: screen.focusResults()
            Keys.onRightPressed: (event) => {
                if (cursorPosition === length && selectedText.length === 0)
                    chips.focusChip(chips.lastFocused >= 0 ? chips.lastFocused : 0)
                else
                    event.accepted = false
            }
        }

        FocusScope {
            id: chips
            objectName: "filterChips"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: chipRow.width
            height: chipRow.height
            property int lastFocused: -1
            readonly property var filters: {
                const list = [{ key: "all", title: "All" }, { key: "favorites", title: "★ Favorites" },
                              { key: "recent", title: "Recent" }]
                if (screen.library.missingCount > 0 || screen.library.filter === "missing")
                    list.push({ key: "missing", title: "⚠ Missing (" + screen.library.missingCount + ")" })
                // Cards that do not fit (large interface scale) show as the list as well.
                list.push({ key: "view", title: grid.effectiveListMode ? "☰ List" : "▦ Grid" })
                return list
            }
            function focusChip(i) {
                const item = chipRepeater.itemAt(Math.max(0, Math.min(i, chipRepeater.count - 1)))
                if (item)
                    item.forceActiveFocus()
            }

            Row {
                id: chipRow
                spacing: Theme.space2
                Repeater {
                    id: chipRepeater
                    model: chips.filters
                    delegate: Rectangle {
                        id: chip
                        required property var modelData
                        required property int index
                        readonly property bool isView: modelData.key === "view"
                        readonly property bool selected: !isView && screen.library.filter === modelData.key
                        objectName: isView ? "viewToggle" : "filter_" + modelData.key
                        activeFocusOnTab: true
                        height: Theme.chipHeight
                        width: chipLabel.implicitWidth + Theme.space6
                        radius: height / 2
                        color: selected ? Theme.accentSoft : (chipMouse.containsMouse ? Theme.surfaceHover : Theme.surfaceRaised)
                        border.width: activeFocus ? Theme.focusWidth : 1
                        border.color: activeFocus ? Theme.focusRing : (selected ? Theme.accent : Theme.border)
                        onActiveFocusChanged: if (activeFocus) chips.lastFocused = index

                        Accessible.role: isView ? Accessible.Button : Accessible.RadioButton
                        Accessible.name: isView ? "View: " + (grid.effectiveListMode ? "list" : "grid") : modelData.title
                        Accessible.description: isView ? "Switch between grid and list" : "Filter"
                        Accessible.checkable: !isView
                        Accessible.checked: selected
                        Accessible.onPressAction: apply()
                        Text {
                            textFormat: Text.PlainText
                            id: chipLabel
                            anchors.centerIn: parent
                            text: chip.modelData.title
                            color: chip.selected ? Theme.text : Theme.textMuted
                            font.pixelSize: Theme.fontSmall + 1
                            font.bold: chip.selected
                        }
                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { chip.forceActiveFocus(); chip.apply() }
                        }
                        function apply() {
                            if (isView)
                                screen.listMode = !screen.listMode
                            else
                                screen.library.filter = modelData.key
                        }
                        Keys.onReturnPressed: apply()
                        Keys.onEnterPressed: apply()
                        Keys.onSpacePressed: apply()
                        Keys.onLeftPressed: index > 0 ? chips.focusChip(index - 1) : searchField.forceActiveFocus()
                        Keys.onRightPressed: if (index < chips.filters.length - 1) chips.focusChip(index + 1)
                    }
                }
            }
        }
    }

    // --- Row 2: system filter chips (only systems that have games) ----------------------------
    FocusScope {
        id: systemChips
        objectName: "systemChips"
        anchors.top: toolbar.bottom
        anchors.topMargin: visible ? Theme.space3 : 0
        anchors.left: toolbar.left
        anchors.right: toolbar.right
        height: visible ? Theme.chipHeight : 0
        visible: entries.length > 2 || screen.library.systemFilter.length > 0
        property int lastFocused: -1
        // All systems, every system with games, "Other" (games without a recognised
        // system), and the active filter even when it has no games (opened from Systems).
        readonly property var entries: {
            void screen.systemsRevision
            const list = [{ systemId: "", shortName: "All systems", gameCount: screen.library.totalCount }]
                         .concat(screen.systemsWithGames)
            const otherFilter = screen.library.otherSystemFilter
            const model = screen.library.systems
            let active = null
            for (let i = 0; i < model.count; ++i) {
                const s = model.get(i)
                if (s.systemId.length === 0 && s.gameCount > 0)
                    list.push({ systemId: otherFilter, shortName: "Other", gameCount: s.gameCount })
                else if (s.systemId === screen.library.systemFilter)
                    active = s
            }
            const filter = screen.library.systemFilter
            if (filter.length > 0 && !list.some(e => e.systemId === filter))
                list.push(filter === otherFilter ? { systemId: filter, shortName: "Other", gameCount: 0 }
                          : { systemId: filter, shortName: active ? active.shortName : filter,
                              gameCount: active ? active.gameCount : 0 })
            return list
        }
        function selectedIndex() {
            for (let i = 0; i < entries.length; ++i) {
                if (entries[i].systemId === screen.library.systemFilter)
                    return i
            }
            return 0
        }
        function focusChip(i) {
            systemList.currentIndex = Math.max(0, Math.min(i, systemList.count - 1))
            systemList.forceActiveFocus()
        }

        ListView {
            id: systemList
            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: Theme.space2
            clip: true
            model: systemChips.entries
            keyNavigationEnabled: true
            highlightMoveDuration: Theme.animMove
            boundsBehavior: Flickable.StopAtBounds
            onCurrentIndexChanged: if (activeFocus) systemChips.lastFocused = currentIndex
            onActiveFocusChanged: if (activeFocus) systemChips.lastFocused = currentIndex
            delegate: Rectangle {
                id: sysChip
                required property var modelData
                required property int index
                readonly property bool selected: screen.library.systemFilter === modelData.systemId
                readonly property bool current: ListView.isCurrentItem && systemList.activeFocus
                objectName: "systemChip_" + (modelData.systemId.length === 0 ? "all"
                                             : modelData.systemId === screen.library.otherSystemFilter ? "other"
                                             : modelData.systemId)
                height: systemList.height
                width: sysLabel.implicitWidth + Theme.space6
                radius: height / 2
                color: selected ? Theme.accentSoft : Theme.surfaceRaised
                border.width: current ? Theme.focusWidth : 1
                border.color: current ? Theme.focusRing : (selected ? Theme.accent : Theme.border)
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.shortName
                Accessible.checkable: true
                Accessible.checked: selected
                Text {
                    textFormat: Text.PlainText
                    id: sysLabel
                    anchors.centerIn: parent
                    text: sysChip.modelData.shortName + "  " + sysChip.modelData.gameCount
                    color: sysChip.selected ? Theme.text : Theme.textMuted
                    font.pixelSize: Theme.fontSmall + 1
                    font.bold: sysChip.selected
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        systemChips.focusChip(sysChip.index)
                        screen.library.systemFilter = sysChip.modelData.systemId
                    }
                }
            }
            Keys.onReturnPressed: screen.library.systemFilter = systemChips.entries[currentIndex].systemId
            Keys.onEnterPressed: screen.library.systemFilter = systemChips.entries[currentIndex].systemId
        }
    }

    // --- Row 3: scan progress + Cancel (while scanning) --------------------------------------
    Rectangle {
        id: scanBanner
        objectName: "scanProgress"
        anchors.top: systemChips.bottom
        anchors.topMargin: visible ? Theme.space3 : 0
        anchors.left: toolbar.left
        anchors.right: toolbar.right
        visible: screen.library.scanning
        height: visible ? Theme.controlHeight + Theme.space2 : 0
        radius: Theme.radiusSmall
        color: Theme.surface
        border.color: Theme.border
        Text {
            textFormat: Text.PlainText
            objectName: "scanProgressText"
            anchors.left: parent.left
            anchors.leftMargin: Theme.space4
            anchors.right: cancelScanButton.left
            anchors.rightMargin: Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideMiddle
            text: "Scanning" + (screen.library.scanCurrentRoot.length > 0 ? " " + screen.library.scanCurrentRoot : "")
                  + "…  " + screen.library.scanFilesSeen + (screen.library.scanFilesSeen === 1 ? " file" : " files") + " seen"
            color: Theme.text
            font.pixelSize: Theme.fontSmall + 1
        }
        NxButton {
            id: cancelScanButton
            objectName: "cancelScanButton"
            visible: screen.library.scanning
            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            compact: true
            text: "Cancel scan"
            onClicked: screen.library.cancelScan()
        }
    }

    // --- Row 4: result count + sort ------------------------------------------------------
    Item {
        id: infoRow
        anchors.top: scanBanner.bottom
        anchors.topMargin: Theme.space2
        anchors.left: toolbar.left
        anchors.right: toolbar.right
        height: Theme.controlHeightCompact

        Text {
            textFormat: Text.PlainText
            id: resultCount
            objectName: "resultCount"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: grid.count === screen.library.totalCount
                  ? grid.count + (grid.count === 1 ? " game" : " games")
                  : grid.count + " of " + screen.library.totalCount + " games"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall + 1
        }

        Text {
            textFormat: Text.PlainText
            objectName: "selectedDetail"
            anchors.left: resultCount.right
            anchors.leftMargin: Theme.space5
            anchors.right: sortSelector.left
            anchors.rightMargin: Theme.spacing
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            readonly property var game: grid.activeFocus ? grid.currentItem : null
            text: !game ? ""
                : !game.present ? "⚠  " + game.title + ": the game file is missing"
                : !game.available ? "⚠  " + game.title + ": " + game.unavailableReason : ""
            color: game && !game.present ? Theme.warning : Theme.danger
            font.pixelSize: Theme.fontSmall
        }

        Rectangle {
            id: sortSelector
            objectName: "sortSelector"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            activeFocusOnTab: true
            height: Theme.controlHeightCompact - 2
            width: sortRow.implicitWidth + Theme.space5 + Theme.space1
            radius: height / 2
            color: activeFocus ? Theme.surfaceHover : "transparent"
            border.width: activeFocus ? Theme.focusWidth : 1
            border.color: activeFocus ? Theme.focusRing : Theme.border

            Accessible.role: Accessible.ComboBox
            Accessible.name: "Sort by " + Theme.sortLabel(screen.library.sortOrder)
            Accessible.description: "Change sort order"
            Accessible.onPressAction: screen.cycleSort(1)

            Row {
                id: sortRow
                anchors.centerIn: parent
                spacing: Theme.space2
                Text {
                    textFormat: Text.PlainText
                    text: "Sort"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    textFormat: Text.PlainText
                    text: "‹"
                    color: sortSelector.activeFocus ? Theme.accent : Theme.textFaint
                    font.pixelSize: Theme.fontLarge
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "sortValue"
                    width: Math.round(130 * Theme.scale)
                    horizontalAlignment: Text.AlignHCenter
                    text: Theme.sortLabel(screen.library.sortOrder)
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall + 1
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    textFormat: Text.PlainText
                    text: "›"
                    color: sortSelector.activeFocus ? Theme.accent : Theme.textFaint
                    font.pixelSize: Theme.fontLarge
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    sortSelector.forceActiveFocus()
                    screen.cycleSort(mouse.button === Qt.RightButton ? -1 : 1)
                }
            }
            Keys.onLeftPressed: screen.cycleSort(-1)
            Keys.onRightPressed: screen.cycleSort(1)
            Keys.onReturnPressed: screen.cycleSort(1)
            Keys.onEnterPressed: screen.cycleSort(1)
        }
    }

    // --- Results (grid / list, or the loading / empty states) ---------------------------------
    Item {
        id: resultsRow
        anchors.top: infoRow.bottom
        anchors.topMargin: Theme.space1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: screen.floatingChrome ? parent.bottom : summary.top
        anchors.bottomMargin: screen.floatingChrome ? 0 : Theme.space1
        anchors.leftMargin: Theme.space6 - Theme.space2
        anchors.rightMargin: Theme.space6 - Theme.space2

        GameGrid {
            id: grid
            objectName: "gameGrid"
            anchors.fill: parent
            visible: count > 0
            focus: true
            library: screen.library
            model: screen.library.games
            listMode: screen.listMode
            chromeBottom: !screen.floatingChrome ? 0
                          : summaryBacking.visible ? screen.height - summaryBacking.y + Theme.space1
                          : screen.chromeBottom
            onDetailsRequested: (gameId) => screen.detailsRequested(gameId)
            onOptionsRequested: (gameId) => screen.optionsRequested(gameId)
        }

        Column {
            id: loading
            objectName: "gamesLoading"
            anchors.centerIn: parent
            visible: screen.library.scanning && grid.count === 0
            spacing: Theme.spacing
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.round(56 * Theme.scale)
                height: width
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: Theme.border
                    border.width: Math.round(5 * Theme.scale)
                }
                Rectangle {
                    width: Math.round(14 * Theme.scale)
                    height: width
                    radius: width / 2
                    color: Theme.accent
                    x: parent.width / 2 - width / 2
                    y: -width / 4
                }
                RotationAnimation on rotation {
                    // A continuous spin, not a discrete transition: reducedMotion stops it
                    // outright rather than collapsing its duration to 0.
                    running: loading.visible && !Theme.reducedMotion
                    from: 0
                    to: 360
                    duration: 1100
                    loops: Animation.Infinite
                }
            }
            Text {
                textFormat: Text.PlainText
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Scanning your library…"
                color: Theme.text
                font.pixelSize: Theme.fontLarge
            }
        }

        Column {
            id: empty
            objectName: "gamesEmpty"
            anchors.centerIn: parent
            width: Math.min(parent.width - Theme.space6 * 3, 720 * Theme.scale)
            visible: !screen.library.scanning && grid.count === 0
            spacing: Theme.spacing
            readonly property string mode: screen.library.totalCount === 0 && screen.library.filter !== "missing"
                ? (screen.library.directories.length === 0 ? "noDirectories" : "noGames")
                : (screen.library.searchText.length > 0 ? "noResults"
                   : screen.library.systemFilter.length > 0 ? "noSystem"
                   : screen.library.filter === "favorites" ? "noFavorites"
                   : screen.library.filter === "missing" ? "noMissing" : "noRecent")

            Text {
                textFormat: Text.PlainText
                objectName: "gamesEmptyTitle"
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: empty.mode === "noDirectories" ? "No library folders yet"
                    : empty.mode === "noGames" ? "No games found"
                    : empty.mode === "noResults" ? "No results for “" + screen.library.searchText + "”"
                    : empty.mode === "noSystem" ? "No games for this system with the current filter"
                    : empty.mode === "noFavorites" ? "No favorites yet"
                    : empty.mode === "noMissing" ? "No missing games"
                    : "Nothing played yet"
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: empty.mode === "noDirectories"
                      ? "Nexora looks for games in the folders you add. Open Settings › Library, type the full path "
                        + "of the folder that holds your game files (for example /home/you/roms) and select Add folder. "
                        + "Every file one of your emulators can run shows up here."
                    : empty.mode === "noGames"
                      ? "None of the files in your library folders match an emulator's file extensions."
                    : empty.mode === "noResults"
                      ? "Try a shorter search or a different filter."
                    : empty.mode === "noSystem"
                      ? "Choose “All systems” or another filter."
                    : empty.mode === "noFavorites"
                      ? "Press Y / F on a game to mark it as a favorite."
                    : empty.mode === "noMissing"
                      ? "Every game file in the library is where it should be."
                      : "Games you launch will appear here."
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
            }
            NxButton {
                id: emptyAction
                objectName: "gamesEmptyAction"
                anchors.horizontalCenter: parent.horizontalCenter
                kind: "primary"
                text: empty.mode === "noDirectories" ? "Add a game folder"
                    : empty.mode === "noGames" ? "Rescan library"
                    : empty.mode === "noResults" ? "Clear search"
                    : "Show all games"
                onClicked: {
                    if (empty.mode === "noDirectories") {
                        screen.navigate("settings")
                    } else if (empty.mode === "noGames") {
                        screen.library.refresh()
                    } else {
                        if (empty.mode === "noResults")
                            screen.library.searchText = ""
                        screen.library.systemFilter = ""
                        screen.library.filter = "all"
                        Qt.callLater(screen.focusResults)
                    }
                }
            }
        }
    }

    // Glass: cards scroll under the summary, so it sits on a dense pill of its own.
    Rectangle {
        id: summaryBacking
        visible: screen.floatingChrome && summary.text.length > 0
        x: summary.x - Theme.space3
        y: summary.y - Theme.space1
        width: Math.min(summary.implicitWidth, summary.width) + Theme.space3 * 2
        height: summary.height + Theme.space1 * 2
        radius: height / 2
        color: Theme.surfaceFloating
        border.color: Theme.glassRim
    }
    Text {
        textFormat: Text.PlainText
        id: summary
        objectName: "scanSummary"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        anchors.bottomMargin: Theme.space2 + screen.chromeBottom
        text: screen.library.lastScanSummary
        color: Theme.textFaint
        font.pixelSize: Theme.fontTiny
        elide: Text.ElideRight
    }
}
