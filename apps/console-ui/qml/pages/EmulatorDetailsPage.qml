import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Nexora.Core

// Read-only details of one emulators.json entry with live status.
// Focus graph (Up/Down walk the stops in order, Left/Right inside a stop):
//   actions row [Launch] [Validate again] [Reload]
//   launch profiles (one stop per profile, read-only)
//   games row (GameRow; Confirm opens game details)
// The Flickable scrolls so the focused stop is always visible. If the
// emulator disappears after a reload an EmptyState with Back replaces it.
PageContainer {
    id: page

    property string emulatorId: ""
    property var emu: ({})
    readonly property bool exists: !!emu.id

    title: exists ? emu.name : qsTr("Emulator")
    subtitle: exists ? qsTr("Emulator · id \"%1\"").arg(emu.id) : ""
    defaultFocusItem: exists ? (launchButton.visible ? launchButton : validateButton) : missingState.firstFocusable

    hints: {
        if (!exists || actionsRow.activeFocus)
            return [{ action: "confirm", label: qsTr("Select") }]
        if (gamesRow.activeFocus)
            return [{ action: "confirm", label: qsTr("Game details") },
                    { action: "secondary", label: qsTr("Favorite") },
                    { action: "options", label: qsTr("More") }]
        return []
    }

    function refresh() {
        var wasExisting = exists
        emu = emulatorId !== "" ? Emulators.byId(emulatorId) : ({})
        if (wasExisting !== exists)
            Qt.callLater(page._fixFocus)
    }
    onEmulatorIdChanged: refresh()
    Component.onCompleted: refresh()

    function _fixFocus() {
        var w = page.Window.window
        var item = w ? w.activeFocusItem : null
        if (!item || !page._isDescendant(item)) return
        for (var p = item; p && p !== page; p = p.parent)
            if (!p.visible) { if (page.defaultFocusItem) page.defaultFocusItem.forceActiveFocus(); return }
    }

    Connections {
        target: Emulators
        function onDataChanged() { page.refresh() }
        function onModelReset() { page.refresh() }
        function onStatusChanged() { page.refresh() }
        function onReloaded() { page.refresh() }
    }

    function validateAgain() {
        Emulators.validate(emulatorId)
        refresh()
        if (!exists) return
        Router.toast(qsTr("%1: %2").arg(emu.name).arg(emu.statusText), emu.canLaunch ? "success" : "warning")
    }
    function reload() {
        Emulators.reload()
        refresh()
        if (Emulators.loadError !== "")
            Router.toast(qsTr("emulators.json could not be read"), "error")
        else if (!exists)
            Router.toast(qsTr("This emulator is no longer in the configuration"), "warning")
        else
            Router.toast(qsTr("Reloaded · %1").arg(emu.statusText), emu.canLaunch ? "success" : "warning")
    }

    // ---- helpers ----------------------------------------------------------
    function _list(value) { return value ? Array.prototype.slice.call(value) : [] }
    function _problemTitle() {
        switch (emu.status) {
        case "disabled": return qsTr("This emulator is disabled")
        case "missing": return qsTr("The executable was not found")
        case "not-executable": return qsTr("The executable can't be run")
        case "invalid": return qsTr("The configuration entry is invalid")
        default: return qsTr("This emulator can't be launched right now")
        }
    }

    // Ordered focus stops; Up/Down move between the visible ones.
    readonly property var focusStops: {
        var stops = [actionsRow]
        for (var i = 0; i < profileRepeater.count; ++i)
            stops.push(profileRepeater.itemAt(i))
        stops.push(gamesRow)
        return stops
    }
    function _moveFocus(delta) {
        var w = page.Window.window
        var item = w ? w.activeFocusItem : null
        var stops = focusStops.filter(function(s) { return s && s.visible })
        var current = -1
        for (var i = 0; i < stops.length && current < 0; ++i)
            for (var p = item; p; p = p.parent)
                if (p === stops[i]) { current = i; break }
        var next = current + delta
        if (current < 0 || next < 0 || next >= stops.length) {
            // Nothing further: keep scrolling the page so all content is reachable.
            if (delta > 0) flick.contentY = Math.max(0, flick.contentHeight - flick.height)
            else flick.contentY = 0
            return
        }
        var target = stops[next]
        if (target === actionsRow) actionsRow.focusFirst()
        else target.forceActiveFocus()
    }

    // Keeps the focused control inside the viewport.
    Connections {
        target: page.Window.window
        function onActiveFocusItemChanged() { Qt.callLater(page._ensureVisible) }
    }
    function _ensureVisible() {
        var w = page.Window.window
        var item = w ? w.activeFocusItem : null
        if (!item || !flick.visible) return
        var inside = false
        for (var p = item; p; p = p.parent) if (p === flick.contentItem) { inside = true; break }
        if (!inside) return
        // Scroll to the whole stop (e.g. the game row incl. its heading), not only the button.
        var stop = item
        var stops = focusStops
        for (var q = item; q; q = q.parent) if (stops.indexOf(q) >= 0) { stop = q; break }
        var margin = Dimensions.spacingLg
        var pos = stop.mapToItem(flick.contentItem, 0, 0)
        var top = pos.y - margin
        var bottom = pos.y + stop.height + margin
        if (stop === actionsRow) top = 0
        if (top < flick.contentY)
            flick.contentY = Math.max(0, top)
        else if (bottom > flick.contentY + flick.height)
            flick.contentY = Math.min(Math.max(0, flick.contentHeight - flick.height), bottom - flick.height)
    }

    // ---- emulator missing --------------------------------------------------
    EmptyState {
        id: missingState
        anchors.fill: parent
        visible: !page.exists
        kind: "unavailable"
        iconName: "warning"
        title: qsTr("This emulator is no longer configured")
        message: Emulators.loadError !== ""
                 ? qsTr("emulators.json could not be read, so no emulators are loaded right now.")
                 : qsTr("No entry with id \"%1\" exists in emulators.json any more. It may have been removed or renamed.").arg(page.emulatorId)
        details: Emulators.loadError !== "" ? Emulators.loadError : Emulators.configPath
        primaryText: qsTr("Back")
        primaryIcon: "chevron-left"
        secondaryText: qsTr("Reload")
        secondaryIcon: "refresh"
        onPrimaryClicked: Router.back()
        onSecondaryClicked: page.reload()
    }

    // ---- details -----------------------------------------------------------
    Flickable {
        id: flick
        anchors.fill: parent
        visible: page.exists
        clip: true
        contentWidth: width
        contentHeight: column.implicitHeight + Dimensions.spacingXl
        boundsBehavior: Flickable.StopAtBounds
        Behavior on contentY { NumberAnimation { duration: Motion.card; easing.type: Motion.easing } }

        FocusScope {
            id: contentScope
            width: flick.width
            height: column.implicitHeight
            focus: true
            Keys.onUpPressed: function(event) { event.accepted = true; page._moveFocus(-1) }
            Keys.onDownPressed: function(event) { event.accepted = true; page._moveFocus(1) }

            ColumnLayout {
                id: column
                x: Dimensions.focusOffset * 2
                y: Dimensions.focusOffset * 2
                width: parent.width - Dimensions.focusOffset * 4
                spacing: Dimensions.spacingLg

                // Status + actions
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingLg
                    StatusIndicator {
                        status: page.emu.status || "unknown"
                        text: page.emu.statusText || qsTr("Unknown")
                    }
                    Text {
                        Layout.fillWidth: true
                        text: {
                            var n = page.emu.gameCount || 0
                            return n === 1 ? qsTr("1 game uses this emulator")
                                 : n > 1 ? qsTr("%1 games use this emulator").arg(n)
                                 : qsTr("No games use this emulator yet")
                        }
                        elide: Text.ElideRight
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: Theme.textSecondary
                    }
                }

                FocusScope {
                    id: actionsRow
                    Layout.fillWidth: true
                    implicitHeight: actionButtons.implicitHeight
                    focus: true
                    function focusFirst() { (launchButton.visible ? launchButton : validateButton).forceActiveFocus() }
                    Row {
                        id: actionButtons
                        spacing: Dimensions.spacingMd
                        FocusButton {
                            id: launchButton
                            visible: !!page.emu.canLaunch
                            focus: visible
                            variant: "primary"
                            iconName: "play"
                            text: qsTr("Launch emulator")
                            accessibleDescription: qsTr("Starts the emulator without a game")
                            onClicked: Router.launchEmulator(page.emulatorId)
                            KeyNavigation.right: validateButton
                        }
                        FocusButton {
                            id: validateButton
                            focus: !launchButton.visible
                            iconName: "check"
                            text: qsTr("Validate again")
                            onClicked: page.validateAgain()
                            KeyNavigation.left: launchButton.visible ? launchButton : null
                            KeyNavigation.right: reloadButton
                        }
                        FocusButton {
                            id: reloadButton
                            iconName: "refresh"
                            text: qsTr("Reload configuration")
                            onClicked: page.reload()
                            KeyNavigation.left: validateButton
                        }
                    }
                }

                // Why it can't launch
                DetailsPanel {
                    Layout.fillWidth: true
                    visible: page.exists && !page.emu.canLaunch
                    tone: page.emu.status === "disabled" ? Theme.statusWarning : Theme.statusError
                    iconName: page.emu.status === "disabled" ? "warning" : "error"
                    heading: page._problemTitle()
                    body: {
                        var msgs = page._list(page.emu.validationMessages)
                        if (page.emu.status === "disabled" && msgs.length === 0)
                            msgs = [qsTr("The entry has \"enabled\": false.")]
                        var text = msgs.length > 0 ? "• " + msgs.join("\n• ") : ""
                        return text + (text !== "" ? "\n\n" : "")
                               + qsTr("Fix the entry in emulators.json, then choose Reload. Games assigned to it can't start until then.")
                    }
                }

                // Read-only notice
                DetailsPanel {
                    Layout.fillWidth: true
                    tone: Theme.statusInformation
                    iconName: "info"
                    heading: qsTr("Read-only")
                    body: qsTr("Configuration is read from %1. Editing in the UI is not supported in this release.").arg(Emulators.configPath)
                }

                // Configuration
                SectionTitle { text: qsTr("Configuration") }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: configGrid.implicitHeight + Dimensions.spacingLg * 2
                    radius: Dimensions.radiusLg
                    color: Theme.backgroundSecondary
                    border.color: Theme.borderSubtle
                    GridLayout {
                        id: configGrid
                        anchors.fill: parent
                        anchors.margins: Dimensions.spacingLg
                        columns: 2
                        columnSpacing: Dimensions.spacingXl
                        rowSpacing: Dimensions.spacingMd

                        KeyLabel { text: qsTr("Executable") }
                        ValueText { text: page.emu.executable || qsTr("Not set"); mono: !!page.emu.executable }

                        KeyLabel { text: qsTr("Arguments") }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXs
                            Flow {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXs
                                visible: page._list(page.emu.arguments).length > 0
                                Repeater {
                                    model: page._list(page.emu.arguments)
                                    delegate: ArgChip { required property string modelData; text: modelData }
                                }
                            }
                            ValueText {
                                text: page._list(page.emu.arguments).length === 0
                                      ? qsTr("None — the game file is passed as the only argument")
                                      : page._list(page.emu.arguments).indexOf("{game}") >= 0
                                        ? qsTr("{game} is replaced by the game file. Arguments are passed directly, never through a shell.")
                                        : qsTr("No {game} placeholder: the game file is appended as the last argument.")
                                muted: true
                            }
                        }

                        KeyLabel { text: qsTr("Working directory") }
                        ValueText { text: page.emu.workingDirectory || qsTr("Not set (inherits Nexora's working directory)")
                                    mono: !!page.emu.workingDirectory; muted: !page.emu.workingDirectory }

                        KeyLabel { text: qsTr("Enabled") }
                        ValueText { text: page.emu.enabled ? qsTr("Yes") : qsTr("No") }

                        KeyLabel { text: qsTr("Platforms") }
                        ValueText { text: page._list(page.emu.platforms).length > 0 ? page._list(page.emu.platforms).join(", ")
                                                                                    : qsTr("Not specified in the configuration")
                                    muted: page._list(page.emu.platforms).length === 0 }

                        KeyLabel { text: qsTr("File types") }
                        ValueText { text: page._list(page.emu.extensions).length > 0
                                          ? page._list(page.emu.extensions).map(function(e) { return "." + e }).join("  ")
                                          : qsTr("None — no games will be matched automatically")
                                    mono: page._list(page.emu.extensions).length > 0
                                    muted: page._list(page.emu.extensions).length === 0 }
                    }
                }

                // Launch profiles
                SectionTitle {
                    text: qsTr("Launch profiles")
                    visible: profileRepeater.count > 0
                }
                Text {
                    Layout.fillWidth: true
                    visible: profileRepeater.count > 0
                    wrapMode: Text.Wrap
                    text: qsTr("Profiles replace the default arguments when a game is launched with them. Shown for reference.")
                    font.family: Typography.family
                    font.pixelSize: Typography.caption
                    color: Theme.textMuted
                }
                Repeater {
                    id: profileRepeater
                    model: page._list(page.emu.profiles)
                    delegate: Rectangle {
                        id: profile
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: profileColumn.implicitHeight + Dimensions.spacingMd * 2
                        radius: Dimensions.radiusMd
                        color: activeFocus ? Theme.backgroundHover : Theme.backgroundSecondary
                        border.color: Theme.borderSubtle
                        activeFocusOnTab: true
                        Accessible.role: Accessible.StaticText
                        Accessible.name: modelData.name
                        FocusFrame { shown: profile.activeFocus }
                        MouseArea { anchors.fill: parent; onClicked: profile.forceActiveFocus() }
                        ColumnLayout {
                            id: profileColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Dimensions.spacingLg
                            spacing: Dimensions.spacingSm
                            RowLayout {
                                spacing: Dimensions.spacingMd
                                Text {
                                    text: profile.modelData.name || profile.modelData.id
                                    font.family: Typography.family
                                    font.pixelSize: Typography.body
                                    font.weight: Typography.weightSemiBold
                                    color: Theme.textPrimary
                                }
                                Text {
                                    text: qsTr("id \"%1\"").arg(profile.modelData.id)
                                    font.family: Typography.family
                                    font.pixelSize: Typography.caption
                                    color: Theme.textMuted
                                }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXs
                                Repeater {
                                    model: page._list(profile.modelData.arguments)
                                    delegate: ArgChip { required property string modelData; text: modelData }
                                }
                            }
                        }
                    }
                }

                // Games
                GameFilterModel {
                    id: gamesModel
                    sourceModel: Library.model
                    emulatorFilter: page.emulatorId
                    sortMode: "title-asc"
                }
                GameRow {
                    id: gamesRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    visible: gamesModel.count > 0
                    title: qsTr("Games using this emulator (%1)").arg(gamesModel.count)
                    model: gamesModel
                    subtitleRole: "platform"
                    cardWidth: Dimensions.s(170)
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: gamesModel.count === 0
                    spacing: Dimensions.spacingXs
                    SectionTitle { text: qsTr("Games") }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("No games use this emulator yet. Games are matched by file type (%1) or assigned manually from a game's details.")
                              .arg(page._list(page.emu.extensions).length > 0
                                   ? page._list(page.emu.extensions).map(function(e) { return "." + e }).join(" ")
                                   : qsTr("none configured"))
                              + (page.emu.canLaunch ? " " + qsTr("The emulator itself can still be launched.") : "")
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }

    // ---- small inline components ---------------------------------------
    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: Dimensions.spacingSm
        font.family: Typography.family
        font.pixelSize: Typography.sectionTitle
        font.weight: Typography.weightSemiBold
        color: Theme.textPrimary
    }
    component KeyLabel: Text {
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: Dimensions.s(220)
        font.family: Typography.family
        font.pixelSize: Typography.secondary
        color: Theme.textMuted
    }
    component ValueText: Text {
        property bool mono: false
        property bool muted: false
        Layout.fillWidth: true
        wrapMode: mono ? Text.WrapAnywhere : Text.Wrap
        font.family: mono ? "monospace" : Typography.family
        font.pixelSize: mono ? Typography.secondary : Typography.body
        color: muted ? Theme.textMuted : Theme.textPrimary
    }
    component ArgChip: Rectangle {
        id: chipRoot
        property alias text: chipText.text
        readonly property bool placeholder: text === "{game}"
        implicitWidth: chipText.implicitWidth + Dimensions.spacingMd * 2
        implicitHeight: chipText.implicitHeight + Dimensions.spacingXs * 2
        radius: Dimensions.radiusSm
        color: placeholder ? Theme.accentPrimarySoft : Theme.backgroundElevated
        border.color: placeholder ? Theme.accentPrimary : Theme.borderSubtle
        Text {
            id: chipText
            anchors.centerIn: parent
            font.family: "monospace"
            font.pixelSize: Typography.secondary
            color: chipRoot.placeholder ? Theme.accentPrimary : Theme.textPrimary
        }
    }
    component DetailsPanel: Rectangle {
        id: panelRoot
        property color tone: Theme.statusInformation
        property string iconName: "info"
        property string heading: ""
        property string body: ""
        implicitHeight: panelRow.implicitHeight + Dimensions.spacingLg * 2
        radius: Dimensions.radiusLg
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.08)
        border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.45)
        border.width: 1
        RowLayout {
            id: panelRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Dimensions.spacingLg
            spacing: Dimensions.spacingMd
            Icon {
                Layout.alignment: Qt.AlignTop
                name: panelRoot.iconName
                color: panelRoot.tone
                size: Dimensions.iconSize
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXs
                Text {
                    Layout.fillWidth: true
                    text: panelRoot.heading
                    wrapMode: Text.Wrap
                    font.family: Typography.family
                    font.pixelSize: Typography.body
                    font.weight: Typography.weightSemiBold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: panelRoot.body
                    wrapMode: Text.Wrap
                    font.family: Typography.family
                    font.pixelSize: Typography.secondary
                    color: Theme.textSecondary
                    lineHeight: 1.1
                }
            }
        }
    }
}
