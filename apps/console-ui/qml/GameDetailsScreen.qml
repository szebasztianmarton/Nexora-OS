import QtQuick
import Nexora.Console

// Details of one game, opened with Accept on a game tile (Games, Home rows, Favorites,
// Recent). Shows cover art (or the placeholder), system, emulator, play history and notes,
// with the actions Play, Favorite, emulator choice, launch options, Edit details, system
// choice and Remove from library. Back closes it and focus returns to the tile it was opened
// from. Modal: tab switching and the other global keys do not reach the pages underneath.
//
// Removing only hides the library entry (LibraryController.removeGame); the game file itself
// is never touched, and the confirmation says so.
FocusScope {
    id: details
    objectName: "gameDetails"
    required property AppController app
    required property LibraryController library
    // Height the floating glass bars cover at the top / bottom of the page (set by Main.qml;
    // 0 when the bars are docked).
    property real chromeTop: 0
    property real chromeBottom: 0

    property real gameId: -1
    property var info: ({})
    property Item returnFocus: null
    readonly property bool hasGame: info.title !== undefined
    readonly property bool overlayOpen: confirmRemove.visible || editor.visible

    readonly property var hints: editor.visible ? editor.hints
        : confirmRemove.visible ? [{ button: "← →", action: "Choose" }, { button: "A / Enter", action: "Confirm" },
                                   { button: "B / Esc", action: "Cancel" }]
        : [{ button: "↑ ↓", action: "Move" }, { button: "← →", action: "Change" },
           { button: "A / Enter", action: "Select" }, { button: "Y / F", action: "Favorite" },
           { button: "B / Esc", action: "Back" }]

    // Asks Main.qml for the launch-options dialog (launch profiles of the chosen emulator).
    signal optionsRequested(real gameId)
    signal closed()

    visible: false
    z: 7

    function open(id, focusItem) {
        gameId = id
        returnFocus = focusItem
        reload()
        visible = true
        if (hasGame)
            actions.focusFirst()
        else
            backButton.forceActiveFocus()
    }
    function close() {
        visible = false
        const item = returnFocus
        returnFocus = null
        if (item && item.visible)
            item.forceActiveFocus()
        closed()
    }
    function reload() {
        info = library.gameDetails(gameId)
    }
    // Focus target after the overlays on top of the details close (see Main.restoreFocus).
    function focusDefault() {
        if (!actions.focusCurrent())
            backButton.forceActiveFocus()
    }

    // "Automatic" + every emulator that declares the file's extension.
    function emulatorChoices() {
        const list = [{ id: "", name: "Automatic" }]
        const emulators = info.emulators || []
        for (const e of emulators)
            list.push({ id: e.id, name: e.name + (e.available ? "" : " (unavailable)") })
        return list
    }
    function currentEmulatorChoice() {
        return info.automatic ? "" : (info.emulatorId || "")
    }
    function cycleEmulator(step) {
        const list = emulatorChoices()
        if (list.length < 2)
            return
        let i = list.findIndex(c => c.id === currentEmulatorChoice())
        i = ((Math.max(0, i) + step) % list.length + list.length) % list.length
        library.setGameEmulator(gameId, list[i].id)
        reload()
    }
    // "Automatic" (detected) + every system the file extension could belong to.
    function systemChoices() {
        const list = [{ id: "", name: "Automatic" }]
        const ids = info.systemCandidates || []
        for (const id of ids) {
            const name = library.systemName(id)
            list.push({ id: id, name: name.length > 0 ? name : id })
        }
        return list
    }
    function cycleSystem(step) {
        const list = systemChoices()
        if (list.length < 2)
            return
        let i = list.findIndex(c => c.id === (info.systemId || ""))
        i = ((Math.max(0, i) + step) % list.length + list.length) % list.length
        library.setGameSystem(gameId, list[i].id)
        reload()
    }
    function formatIso(iso) {
        if (!iso)
            return ""
        const d = new Date(iso)
        return isNaN(d.getTime()) ? iso : d.toLocaleString(Qt.locale(), Locale.ShortFormat)
    }

    Connections {
        target: details.library
        enabled: details.visible
        function onContentChanged() { details.reload() }
    }

    Backdrop { anchors.fill: parent }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

    // --- Error state: the game is gone (removed elsewhere, library reopened) -------------------
    Column {
        objectName: "gameDetailsError"
        visible: !details.hasGame
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space6 * 2, 640 * Theme.scale)
        spacing: Theme.space4
        Text {
            textFormat: Text.PlainText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "This game is no longer in the library"
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "It may have been removed or its library folder was taken out. Press Back to return."
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
        }
        NxButton {
            id: backButton
            objectName: "gameDetailsBack"
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Back"
            onClicked: details.close()
        }
    }

    // --- Content ----------------------------------------------------------------------------
    Item {
        visible: details.hasGame
        anchors.fill: parent
        anchors.margins: Theme.space6
        anchors.topMargin: Theme.space6 + details.chromeTop
        anchors.bottomMargin: Theme.space6 + details.chromeBottom

        // Left: cover + facts.
        Column {
            id: left
            anchors.left: parent.left
            anchors.top: parent.top
            width: Math.min(parent.width * 0.36, 420 * Theme.scale)
            spacing: Theme.space4

            Rectangle {
                id: cover
                objectName: "gameDetailsCover"
                width: parent.width
                height: Math.min(width * 0.75, (details.height - details.chromeTop - details.chromeBottom) * 0.42)
                radius: Theme.radiusLarge
                clip: true
                opacity: details.info.present ? 1.0 : 0.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.artColor(details.info.title || "", 0.34) }
                    GradientStop { position: 1.0; color: Theme.artColor((details.info.title || "") + "~", 0.16) }
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: Theme.initials(details.info.title || "")
                    color: Theme.artInk
                    font.pixelSize: Math.min(cover.height * 0.4, 96 * Theme.scale)
                    font.bold: true
                    font.letterSpacing: 3
                }
                Rectangle {
                    objectName: "gameDetailsMissing"
                    visible: details.hasGame && !details.info.present
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: Theme.space3
                    height: Math.round(28 * Theme.scale)
                    width: missingLabel.implicitWidth + Theme.space4
                    radius: height / 2
                    color: Theme.badgeScrim
                    border.color: Theme.warning
                    Text {
                        textFormat: Text.PlainText
                        id: missingLabel
                        anchors.centerIn: parent
                        text: "⚠ File missing"
                        color: Theme.warning
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }
            }

            Grid {
                width: parent.width
                columns: 2
                columnSpacing: Theme.space4
                rowSpacing: Theme.space2
                component Fact: Text {
                    textFormat: Text.PlainText
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
                component Value: Text {
                    textFormat: Text.PlainText
                    width: parent ? parent.width * 0.6 : 0
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                }
                Fact { text: "Last played" }
                Value {
                    objectName: "gameDetailsLastPlayed"
                    text: details.info.lastPlayedText ? details.info.lastPlayedText : "Never"
                }
                Fact { text: "Times played" }
                Value { objectName: "gameDetailsPlayCount"; text: String(details.info.playCount || 0) }
                Fact { text: "Added" }
                Value { text: details.formatIso(details.info.addedAt) }
                Fact { text: "File" }
                Value { text: details.info.path || "" }
            }
        }

        // Right: title, notes, actions.
        Flickable {
            id: flick
            anchors.left: left.right
            anchors.leftMargin: Theme.space6
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            contentWidth: width
            contentHeight: rightColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Behavior on contentY { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

            function ensureVisible(item) {
                if (!item)
                    return
                const p = item.mapToItem(rightColumn, 0, 0)
                if (p.y < contentY)
                    contentY = Math.max(0, p.y - Theme.space4)
                else if (p.y + item.height > contentY + height)
                    contentY = Math.min(p.y + item.height - height + Theme.space4, Math.max(0, contentHeight - height))
            }

            Column {
                id: rightColumn
                width: flick.width
                spacing: Theme.space3

                Text {
                    textFormat: Text.PlainText
                    objectName: "gameDetailsTitle"
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: details.info.title || ""
                    color: Theme.text
                    font.pixelSize: Theme.fontHero
                    font.bold: true
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "gameDetailsSubtitle"
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: (details.info.systemName ? details.info.systemName : "Unknown system")
                          + "  ·  " + (details.info.emulatorName ? details.info.emulatorName : "No emulator")
                          + (details.info.favorite ? "  ·  ★ Favorite" : "")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontBody
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "gameDetailsNotes"
                    width: parent.width
                    visible: (details.info.notes || "").length > 0
                    wrapMode: Text.Wrap
                    text: details.info.notes || ""
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.italic: true
                }

                NavColumn {
                    id: actions
                    objectName: "gameDetailsActions"
                    width: parent.width
                    onActiveFocusChanged: if (activeFocus) flick.ensureVisible(lastFocused)
                    onLastFocusedChanged: flick.ensureVisible(lastFocused)

                    SettingRow {
                        id: launchRow
                        objectName: "detailsLaunch"
                        readonly property bool canLaunch: details.info.present === true
                                                          && (details.info.emulatorId || "").length > 0
                        label: "Play"
                        description: !details.info.present ? "The game file is missing"
                                   : canLaunch ? "Starts " + (details.info.emulatorName || "the emulator")
                                   : "No emulator handles this file type"
                        actionText: canLaunch ? "Play" : "Unavailable"
                        labelColor: canLaunch ? Theme.text : Theme.textMuted
                        onAccepted: if (canLaunch && !details.app.busy) details.library.launch(details.gameId)
                    }
                    SettingRow {
                        objectName: "detailsFavorite"
                        label: "Favorite"
                        value: details.info.favorite ? "★ Yes" : "No"
                        valueColor: details.info.favorite ? Theme.favorite : Theme.textMuted
                        adjustable: true
                        onDecrement: { details.library.toggleFavorite(details.gameId); details.reload() }
                        onIncrement: { details.library.toggleFavorite(details.gameId); details.reload() }
                        onAccepted: { details.library.toggleFavorite(details.gameId); details.reload() }
                    }
                    SettingRow {
                        objectName: "detailsEmulator"
                        label: "Emulator"
                        readonly property var choices: details.emulatorChoices()
                        value: details.info.automatic
                               ? "Automatic" + (details.info.emulatorName ? " (" + details.info.emulatorName + ")" : "")
                               : (details.info.emulatorName || "—")
                        // "Automatic" plus one emulator start the same thing: nothing to choose.
                        description: choices.length > 2 ? "← → choose which emulator starts this game"
                                   : choices.length > 1 ? "The only emulator that declares this file type"
                                   : "No emulator declares this file type"
                        adjustable: choices.length > 2
                        onDecrement: details.cycleEmulator(-1)
                        onIncrement: details.cycleEmulator(1)
                    }
                    SettingRow {
                        objectName: "detailsOptions"
                        visible: details.info.present === true
                        label: "Launch options"
                        description: "Launch profiles of the selected emulator"
                        actionText: "Open"
                        onAccepted: details.optionsRequested(details.gameId)
                    }
                    SettingRow {
                        objectName: "detailsEdit"
                        label: "Edit details"
                        description: "Title and notes (the game file is not renamed)"
                        actionText: "Edit"
                        onAccepted: editor.open(details.gameId)
                    }
                    SettingRow {
                        objectName: "detailsSystem"
                        label: "System"
                        readonly property var choices: details.systemChoices()
                        value: details.info.systemName ? details.info.systemName : "Unknown"
                        description: choices.length > 2 ? "← → change the system this game belongs to"
                                                        : "Detected from the file type"
                        adjustable: choices.length > 2
                        onDecrement: details.cycleSystem(-1)
                        onIncrement: details.cycleSystem(1)
                    }
                    SettingRow {
                        objectName: "detailsRemove"
                        label: "Remove from library"
                        description: "Hides this entry; the game file stays where it is"
                        actionText: "Remove"
                        labelColor: Theme.danger
                        onAccepted: confirmRemove.open()
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirmRemove
        chromeTop: details.chromeTop
        chromeBottom: details.chromeBottom
        objectName: "confirmRemoveGame"
        title: "Remove “" + (details.info.title || "") + "” from the library?"
        message: "The game file is not deleted. It stays in its folder and can be brought back with "
                 + "Settings › Library › Restore removed games."
        confirmText: "Remove"
        dangerous: true
        onConfirmed: {
            if (details.library.removeGame(details.gameId))
                details.close()
        }
    }

    MetadataEditor {
        id: editor
        chromeTop: details.chromeTop
        chromeBottom: details.chromeBottom
        library: details.library
        onSaved: details.reload()
    }

    // Global keys stop here while the details are open (modal); Back closes.
    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Escape:
        case Qt.Key_Back:
        case Qt.Key_Backspace:
            details.close()
            break
        case Qt.Key_Favorites:
            if (details.hasGame) {
                details.library.toggleFavorite(details.gameId)
                details.reload()
            }
            break
        case Qt.Key_F:
            if (details.hasGame && event.modifiers === Qt.NoModifier) {
                details.library.toggleFavorite(details.gameId)
                details.reload()
            }
            break
        default:
            break
        }
        event.accepted = true
    }
}
