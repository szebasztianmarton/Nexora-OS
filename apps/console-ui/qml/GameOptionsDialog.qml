import QtQuick
import Nexora.Console

// Modal options for one game (Select / O on a game): play with a specific launch profile,
// choose which of the emulators that declare the file's extension launches it, toggle the
// favorite. Opened by Main.qml; focus returns to the game when it closes.
FocusScope {
    id: dialog
    objectName: "gameOptions"
    required property LibraryController library
    property GameSessionManager sessions: null

    property real gameId: -1
    property var details: ({})
    property var rows: []
    property Item returnFocus: null
    // Set by Main.qml: the window's scene, blurred behind the panel (glass blur only).
    property Item glassBackdrop: null

    readonly property var hints: nameKeyboard.visible ? nameKeyboard.hints
        : [{ button: "↑ ↓", action: "Select" }, { button: "A / Enter", action: "Choose" },
           { button: "B / Esc", action: "Close" }]

    visible: false
    z: 8

    function open(id, focusItem) {
        gameId = id
        returnFocus = focusItem
        reload()
        if (rows.length === 0)
            return
        optionList.currentIndex = 0
        visible = true
        optionList.forceActiveFocus()
    }
    function close() {
        visible = false
        const item = returnFocus
        returnFocus = null
        if (item && item.visible)
            item.forceActiveFocus()
    }
    // The session Play would resume: the game's on the emulator it launches with. One game can
    // have one per emulator; |suspended| is newest first.
    function quickResumeSession(suspended, id, emulatorId) {
        for (const s of suspended) {
            if (s.gameId === id && s.emulatorId === emulatorId)
                return s
        }
        return null
    }
    function reload() {
        details = library.gameDetails(gameId)
        const r = []
        if (!details.title) {
            rows = r
            return
        }
        const profiles = details.profiles
        if (details.present) {
            let defaultName = ""
            for (let i = 0; i < profiles.length; ++i) {
                if (profiles[i].isDefault)
                    defaultName = profiles[i].name
            }
            const saved = quickResumeSession(sessions ? sessions.suspended : [], gameId, details.emulatorId)
            r.push({ kind: "play", id: "", label: saved ? "Resume" : "Play",
                     detail: saved ? "From the Quick Resume state saved " + saved.savedAt
                           : defaultName.length > 0 ? "Default profile “" + defaultName + "”" : "Emulator defaults" })
            for (let i = 0; i < profiles.length; ++i)
                r.push({ kind: "profile", id: profiles[i].id, label: "Play with “" + profiles[i].name + "”",
                         detail: profiles[i].isDefault ? "Launch profile (default)" : "Launch profile" })
            if (profiles.length > 0)
                r.push({ kind: "profile", id: "", label: "Play with emulator defaults", detail: "No launch profile" })
        }
        const emulators = details.emulators
        if (emulators.length > 1) {
            r.push({ kind: "emulator", id: "", label: "Emulator: automatic", checked: details.automatic,
                     detail: "The first emulator that handles this file type" })
            for (let i = 0; i < emulators.length; ++i) {
                const e = emulators[i]
                r.push({ kind: "emulator", id: e.id, label: "Emulator: " + e.name,
                         checked: !details.automatic && e.selected,
                         detail: e.available ? (e.selected ? "Used for this game" : "Available") : e.reason })
            }
        }
        r.push({ kind: "favorite", id: "", label: details.favorite ? "Remove from favorites" : "Add to favorites",
                 detail: "" })
        r.push({ kind: "pin", id: "", label: details.pinned ? "Unpin from Home" : "Pin to Home",
                 detail: details.pinned ? "Shown in the Pinned row on Home" : "Adds it to the Pinned row on Home" })
        const member = library.collectionsOf(gameId)
        for (const c of library.collections) {
            const inIt = member.indexOf(c.id) >= 0
            r.push({ kind: "collection", id: c.id, label: "Collection: " + c.name, checked: inIt,
                     detail: inIt ? "In this collection · A / Enter removes it" : "A / Enter adds it" })
        }
        r.push({ kind: "newCollection", id: "", label: "New collection…",
                 detail: "Creates a collection with this game" })
        if (sessions) {
            const enabled = sessions.isQuickResumeEnabled(gameId)
            r.push({ kind: "quickResume", id: "", label: "Quick Resume: " + (enabled ? "on" : "off"),
                     detail: enabled ? "Suspend saves a state when the emulator supports it"
                                     : "This game always starts fresh and is never suspended" })
        }
        r.push({ kind: "close", id: "", label: "Close", detail: "" })
        rows = r
    }
    function activate(index) {
        const row = rows[index]
        if (!row)
            return
        const id = gameId
        switch (row.kind) {
        case "play":
            close()
            library.launch(id)
            break
        case "profile":
            close()
            library.launchWithProfile(id, row.id)
            break
        case "emulator":
            library.setGameEmulator(id, row.id)
            reload()
            optionList.currentIndex = Math.min(index, rows.length - 1)
            break
        case "quickResume":
            sessions.setQuickResumeEnabled(id, !sessions.isQuickResumeEnabled(id))
            reload()
            optionList.currentIndex = Math.min(index, rows.length - 1)
            break
        case "pin":
            library.setPinned(id, !details.pinned)
            reload()
            optionList.currentIndex = Math.min(index, rows.length - 1)
            break
        case "collection":
            library.setInCollection(id, row.id, !row.checked)
            reload()
            optionList.currentIndex = Math.min(index, rows.length - 1)
            break
        case "newCollection":
            nameKeyboard.open("New collection", "")
            break
        case "favorite":
            library.toggleFavorite(id)
            reload()
            optionList.currentIndex = Math.min(index, rows.length - 1)
            break
        default:
            close()
        }
    }

    // Glass: the window's shared modal scrim (Main.qml) dims the whole window, so none here.
    Rectangle {
        objectName: "gameOptionsScrim"
        anchors.fill: parent
        color: Theme.glassActive ? "transparent" : Theme.background
        opacity: Theme.glassActive ? 1.0 : 0.8
    }
    MouseArea { anchors.fill: parent; onClicked: dialog.close() }

    Accessible.role: Accessible.Dialog
    Accessible.name: dialog.details.title || "Game options"
    Accessible.description: dialog.details.path || ""

    Rectangle {
        id: panel
        objectName: "gameOptionsPanel"
        // Tallest the panel may get; the option list takes what the header leaves of it.
        readonly property real maxHeight: parent.height - Math.round(48 * Theme.scale)
        anchors.centerIn: parent
        width: Math.min(parent.width - Math.round(64 * Theme.scale), Math.round(620 * Theme.scale))
        height: Math.min(maxHeight, column.implicitHeight + Theme.spacing * 3)
        radius: Theme.glassActive ? Theme.radiusXL : Theme.radius
        color: Theme.glassActive ? "transparent" : Theme.surfaceFloating
        GlassSurface { anchors.fill: parent; z: -1; radius: panel.radius; dense: true; backdrop: dialog.glassBackdrop }
        border.color: Theme.border
        border.width: Theme.glassActive ? 0 : 1
        MouseArea { anchors.fill: parent } // keep clicks inside

        Column {
            id: column
            anchors.fill: parent
            anchors.margins: Theme.spacing * 1.5
            spacing: Theme.spacing * 0.75

            Text {
                textFormat: Text.PlainText
                objectName: "gameOptionsTitle"
                width: parent.width
                text: dialog.details.title || ""
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                text: (dialog.details.path || "") + (dialog.details.size ? "  ·  " + dialog.details.size : "")
                color: Theme.textMuted
                font.pixelSize: Theme.fontTiny
                elide: Text.ElideMiddle
            }
            Text {
                textFormat: Text.PlainText
                objectName: "gameOptionsSummary"
                width: parent.width
                wrapMode: Text.Wrap
                readonly property var emulators: dialog.details.emulators || []
                text: (emulators.length === 0 ? "No emulator handles this file type"
                       : emulators.length === 1 ? "Emulator: " + emulators[0].name
                                                  + (emulators[0].available ? "" : " — " + emulators[0].reason)
                       : emulators.length + " emulators handle this file type")
                      + "  ·  " + (dialog.details.playCount > 0
                                   ? "played " + dialog.details.playCount + "×, last " + dialog.details.lastPlayedText
                                   : "never played")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }

            ListView {
                id: optionList
                objectName: "gameOptionsList"
                width: parent.width
                // Never past the bottom of the panel: the rest scrolls inside the (clipped) list.
                height: Math.max(0, Math.min(contentHeight, Math.round(420 * Theme.scale),
                                             panel.maxHeight - Theme.spacing * 3 - y))
                spacing: Math.round(6 * Theme.scale)
                clip: true
                keyNavigationEnabled: true
                model: dialog.rows
                delegate: Rectangle {
                    id: option
                    required property var modelData
                    required property int index
                    width: optionList.width
                    // The 1.0-scale heights, or taller when the scaled text needs it.
                    height: Math.max(modelData.detail.length > 0 ? 56 : 44, optionContent.implicitHeight + Theme.space4)
                    radius: Theme.radiusSmall
                    readonly property bool current: ListView.isCurrentItem && optionList.activeFocus
                    color: current ? Theme.surfaceHover : Theme.surfaceRaised
                    border.width: current ? Theme.focusWidth : 1
                    border.color: current ? Theme.focusRing : Theme.border
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.label
                    Accessible.description: modelData.detail

                    Text {
                        textFormat: Text.PlainText
                        id: mark
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing * 0.75
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(22 * Theme.scale)
                        text: option.modelData.kind === "emulator" ? (option.modelData.checked ? "●" : "○")
                              : option.modelData.kind === "collection" ? (option.modelData.checked ? "☑" : "☐") : ""
                        color: Theme.accent
                        font.pixelSize: Theme.fontBody
                    }
                    Column {
                        id: optionContent
                        objectName: "gameOptionContent"
                        anchors.left: mark.right
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * Theme.scale)
                        Text {
                            textFormat: Text.PlainText
                            width: parent.width
                            text: option.modelData.label
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.bold: option.modelData.kind === "play"
                            elide: Text.ElideRight
                        }
                        Text {
                            textFormat: Text.PlainText
                            width: parent.width
                            visible: text.length > 0
                            text: option.modelData.detail
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontTiny
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { optionList.currentIndex = option.index; dialog.activate(option.index) }
                    }
                }
                Keys.onReturnPressed: dialog.activate(currentIndex)
                Keys.onEnterPressed: dialog.activate(currentIndex)
                Keys.onSpacePressed: dialog.activate(currentIndex)
            }
        }
    }

    // Modal: nothing reaches the screens or the tab bar while open.
    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Escape:
        case Qt.Key_Back:
        case Qt.Key_Backspace:
        case Qt.Key_Context1:
        case Qt.Key_O:
            dialog.close()
            break
        }
        event.accepted = true
    }

    // The game can disappear (rescan) or an emulator can start while the dialog is open.
    Connections {
        target: dialog.library
        enabled: dialog.visible
        function onContentChanged() {
            if (!dialog.library.gameDetails(dialog.gameId).title)
                dialog.close()
        }
    }

    OnScreenKeyboard {
        id: nameKeyboard
        objectName: "gameOptionsNameKeyboard"
        maxLength: 60
        onAccepted: (value) => {
            if (dialog.library.createCollection(value, dialog.gameId) > 0) {
                const keep = optionList.currentIndex
                dialog.reload()
                optionList.currentIndex = Math.min(keep, dialog.rows.length - 1)
            }
            optionList.forceActiveFocus()
        }
        onCancelled: optionList.forceActiveFocus()
    }
}
