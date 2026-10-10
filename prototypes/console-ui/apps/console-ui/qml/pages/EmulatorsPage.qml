import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Nexora.Core

// Emulators section: read-only view of emulators.json with live status.
// Focus graph:
//   header [Reload] <-> [Validate all]      Down -> warning banner / list / empty state
//   warning banner (skipped entries)        Up -> header, Down -> list
//   list rows (EmulatorCard)                Up on first row -> banner / header
//   empty / error state buttons             Up -> header
// Left at the leftmost control is not accepted (Shell moves to the sidebar).
PageContainer {
    id: page

    title: qsTr("Emulators")
    subtitle: page.hasError || !Emulators.configExists ? ""
              : qsTr("%1 configured · %2 available").arg(Emulators.count).arg(Emulators.availableCount)

    readonly property bool hasError: Emulators.loadError !== ""
    readonly property bool hasEntries: !hasError && Emulators.count > 0
    readonly property bool hasSkipped: Emulators.skippedEntries.length > 0
    readonly property Item currentCard: list.currentItem

    defaultFocusItem: hasEntries ? list : (emptyState.firstFocusable || reloadButton)

    hints: {
        if (list.activeFocus && currentCard) {
            if (currentCard.launchFocused)
                return [{ action: "confirm", label: qsTr("Launch") }]
            var h = [{ action: "confirm", label: qsTr("Details") }]
            if (currentCard.canLaunch && !Launcher.busy)
                h.push({ action: "secondary", label: qsTr("Launch") })
            h.push({ action: "options", label: qsTr("More") })
            return h
        }
        return [{ action: "confirm", label: qsTr("Select") }]
    }

    // When the visible section changes (reload turned the list into an error
    // state or back), never leave focus on a hidden item.
    onHasEntriesChanged: Qt.callLater(page._fixFocus)
    function _fixFocus() {
        var item = page.Window.window ? page.Window.window.activeFocusItem : null
        var inPage = item && page._isDescendant(item)
        if (!inPage) return
        for (var p = item; p && p !== page; p = p.parent)
            if (!p.visible) { page.defaultFocusItem.forceActiveFocus(); return }
    }

    function reload() {
        Emulators.reload()
        if (Emulators.loadError !== "")
            Router.toast(qsTr("emulators.json could not be read"), "error")
        else if (!Emulators.configExists)
            Router.toast(qsTr("No emulators.json found"), "warning")
        else
            Router.toast(qsTr("Reloaded: %1 configured, %2 available").arg(Emulators.count).arg(Emulators.availableCount),
                         Emulators.skippedEntries.length > 0 ? "warning" : "success")
    }

    function validateAll() {
        Emulators.validateAll()
        Router.toast(qsTr("Checked %1 emulators: %2 available").arg(Emulators.count).arg(Emulators.availableCount), "info")
    }

    function openDetails(id) { Router.push("EmulatorDetailsPage", { emulatorId: id }) }

    function openMenu(row) {
        var emu = Emulators.get(row)
        if (!emu.id) return
        Router.openMenu({
            title: emu.name,
            actions: [
                { id: "details", label: qsTr("Open details"), icon: "info" },
                { id: "launch", label: qsTr("Launch emulator"), icon: "play", enabled: emu.canLaunch && !Launcher.busy },
                { id: "validate", label: qsTr("Validate"), icon: "refresh" }
            ]
        }, function(action) {
            if (action === "details") page.openDetails(emu.id)
            else if (action === "launch") Router.launchEmulator(emu.id)
            else if (action === "validate") {
                Emulators.validate(emu.id)
                var now = Emulators.byId(emu.id)
                Router.toast(qsTr("%1: %2").arg(now.name).arg(now.statusText), now.canLaunch ? "success" : "warning")
            }
        })
    }

    function showHelp() {
        Router.showError({
            kind: "info",
            title: qsTr("Configuring emulators"),
            message: qsTr("Nexora reads emulators from %1. Create or edit that file with a text editor, then choose Reload. "
                          + "Nexora does not download or install emulators — point \"executable\" at one you already have. "
                          + "\"{game}\" is replaced by the game file; \"extensions\" decides which games use the emulator.")
                     .arg(Emulators.configPath),
            details: '{"version": 1, "emulators": [{\n'
                     + '  "id": "myemu", "name": "My Emulator",\n'
                     + '  "executable": "/usr/bin/my-emulator",\n'
                     + '  "arguments": ["--fullscreen", "{game}"],\n'
                     + '  "extensions": ["iso"], "platforms": ["My Platform"]\n'
                     + '}]}',
            actions: [{ id: "close", label: qsTr("Close"), primary: true }]
        })
    }

    function showSkipped() {
        var entries = Emulators.skippedEntries
        Router.showError({
            kind: "warning",
            title: entries.length === 1 ? qsTr("1 entry was skipped") : qsTr("%1 entries were skipped").arg(entries.length),
            message: qsTr("These entries in emulators.json could not be used and are not listed. Fix them in the file, then choose Reload.")
                     + "\n\n• " + entries.join("\n• "),
            details: Emulators.configPath,
            actions: [{ id: "close", label: qsTr("Close"), primary: true }, { id: "reload", label: qsTr("Reload") }]
        }, function(id) { if (id === "reload") page.reload() })
    }

    headerActions: [
        FocusButton {
            id: reloadButton
            small: true
            text: qsTr("Reload configuration")
            iconName: "refresh"
            onClicked: page.reload()
            KeyNavigation.right: validateButton.visible ? validateButton : null
            Keys.onDownPressed: function(event) { event.accepted = page._focusBelowHeader() }
        },
        FocusButton {
            id: validateButton
            small: true
            visible: page.hasEntries
            text: qsTr("Validate all")
            iconName: "check"
            onClicked: page.validateAll()
            KeyNavigation.left: reloadButton
            Keys.onDownPressed: function(event) { event.accepted = page._focusBelowHeader() }
        }
    ]

    function _focusBelowHeader() {
        if (banner.visible) { banner.forceActiveFocus(); return true }
        if (page.hasEntries) { list.forceActiveFocus(); return true }
        if (emptyState.visible && emptyState.firstFocusable) { emptyState.firstFocusable.forceActiveFocus(); return true }
        return false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Dimensions.spacingMd

        // ---- partial-load warning ---------------------------------------
        Rectangle {
            id: banner
            Layout.fillWidth: true
            visible: page.hasSkipped && !page.hasError
            implicitHeight: bannerRow.implicitHeight + Dimensions.spacingMd * 2
            radius: Dimensions.radiusMd
            color: activeFocus || bannerMouse.containsMouse ? Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.18)
                                                            : Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.10)
            border.color: Qt.rgba(Theme.statusWarning.r, Theme.statusWarning.g, Theme.statusWarning.b, 0.5)
            border.width: 1
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: bannerText.text
            FocusFrame { shown: banner.activeFocus }
            Keys.onPressed: function(event) {
                if (Input.isConfirm(event)) {
                    event.accepted = true
                    if (!event.isAutoRepeat) page.showSkipped()
                } else if (event.key === Qt.Key_Up) {
                    event.accepted = true
                    reloadButton.forceActiveFocus()
                } else if (event.key === Qt.Key_Down) {
                    event.accepted = true
                    if (page.hasEntries) list.forceActiveFocus()
                    else if (emptyState.firstFocusable) emptyState.firstFocusable.forceActiveFocus()
                }
            }
            RowLayout {
                id: bannerRow
                anchors.fill: parent
                anchors.leftMargin: Dimensions.spacingLg
                anchors.rightMargin: Dimensions.spacingLg
                spacing: Dimensions.spacingMd
                Icon { name: "warning"; color: Theme.statusWarning; size: Dimensions.iconSizeSmall }
                Text {
                    id: bannerText
                    Layout.fillWidth: true
                    text: Emulators.skippedEntries.length === 1
                          ? qsTr("1 entry in emulators.json was skipped because it is invalid.")
                          : qsTr("%1 entries in emulators.json were skipped because they are invalid.").arg(Emulators.skippedEntries.length)
                    elide: Text.ElideRight
                    font.family: Typography.family
                    font.pixelSize: Typography.secondary
                    color: Theme.textPrimary
                }
                Text {
                    text: qsTr("Show entries")
                    font.family: Typography.family
                    font.pixelSize: Typography.secondary
                    font.weight: Typography.weightSemiBold
                    color: Theme.statusWarning
                }
                Icon { name: "chevron-right"; color: Theme.statusWarning; size: Dimensions.iconSizeSmall }
            }
            MouseArea {
                id: bannerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { banner.forceActiveFocus(); page.showSkipped() }
            }
        }

        // ---- emulator list ----------------------------------------------
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.hasEntries
            model: Emulators
            clip: true
            focus: page.hasEntries
            spacing: Dimensions.spacingMd
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: Motion.card
            highlightRangeMode: ListView.ApplyRange
            preferredHighlightBegin: Dimensions.focusOffset * 3
            preferredHighlightEnd: height - Dimensions.focusOffset * 3
            topMargin: Dimensions.focusOffset * 2
            bottomMargin: Dimensions.focusOffset * 2
            leftMargin: Dimensions.focusOffset * 2
            rightMargin: Dimensions.focusOffset * 2
            currentIndex: 0
            Keys.priority: Keys.AfterItem
            Keys.onUpPressed: function(event) {
                // Only reached when the list could not move further up.
                event.accepted = true
                if (banner.visible) banner.forceActiveFocus()
                else reloadButton.forceActiveFocus()
            }
            onCountChanged: if (currentIndex < 0 && count > 0) currentIndex = 0

            delegate: EmulatorCard {
                id: delegateCard
                required property int index
                required property var model
                width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                emulatorId: model.id
                name: model.name
                status: model.status
                statusText: model.statusText
                platforms: model.platforms
                extensions: model.extensions
                validationMessages: model.validationMessages
                gameCount: model.gameCount
                canLaunch: model.canLaunch
                launchAllowed: !Launcher.busy
                current: ListView.isCurrentItem
                focus: ListView.isCurrentItem
                onClicked: ListView.view.currentIndex = index
                onActivated: page.openDetails(model.id)
                onLaunchRequested: Router.launchEmulator(model.id)
                onOptionsRequested: page.openMenu(index)
            }

            footer: Item {
                width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                height: footerText.implicitHeight + Dimensions.spacingLg * 2
                Text {
                    id: footerText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.Wrap
                    text: qsTr("Read from %1. Editing in the UI is not supported in this release — edit the file and choose Reload. "
                               + "An emulator can be available even when no games use it yet; games are matched by file extension.")
                          .arg(Emulators.configPath)
                    font.family: Typography.family
                    font.pixelSize: Typography.caption
                    color: Theme.textMuted
                }
            }
        }

        // ---- empty / error states ---------------------------------------
        EmptyState {
            id: emptyState
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !page.hasEntries
            kind: page.hasError ? "error" : "empty"
            iconName: page.hasError ? "error" : "chip"
            title: page.hasError ? qsTr("Emulator configuration couldn't be read")
                 : !Emulators.configExists ? qsTr("No emulators configured")
                 : qsTr("No usable emulators in the configuration")
            message: page.hasError
                     ? qsTr("Nothing was loaded. Fix the file in a text editor and choose Reload. Your games are not affected.")
                     : !Emulators.configExists
                       ? qsTr("Nexora reads emulators you already have installed from a JSON file. Create %1 with an \"emulators\" list "
                              + "(id, name, executable, arguments, extensions), then choose Reload.").arg(Emulators.configPath)
                       : qsTr("%1 exists but contains no valid emulator entries. Check the file, then choose Reload.").arg(Emulators.configPath)
            details: page.hasError ? Emulators.loadError + "\n" + Emulators.configPath : ""
            primaryText: qsTr("Reload")
            primaryIcon: "refresh"
            secondaryText: qsTr("Open help")
            secondaryIcon: "info"
            onPrimaryClicked: page.reload()
            onSecondaryClicked: page.showHelp()
            Keys.onUpPressed: function(event) {
                event.accepted = true
                if (banner.visible) banner.forceActiveFocus()
                else reloadButton.forceActiveFocus()
            }
        }
    }
}
