import QtQuick
import Nexora.Console

// Modal options for one game (Select / O on a game): play with a specific launch profile,
// choose which of the emulators that declare the file's extension launches it, toggle the
// favorite. Opened by Main.qml; focus returns to the game when it closes.
FocusScope {
    id: dialog
    objectName: "gameOptions"
    required property LibraryController library

    property real gameId: -1
    property var details: ({})
    property var rows: []
    property Item returnFocus: null

    readonly property var hints: [{ button: "↑ ↓", action: "Select" }, { button: "A / Enter", action: "Choose" },
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
            r.push({ kind: "play", id: "", label: "Play",
                     detail: defaultName.length > 0 ? "Default profile “" + defaultName + "”" : "Emulator defaults" })
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
        case "favorite":
            library.toggleFavorite(id)
            reload()
            optionList.currentIndex = Math.min(index, rows.length - 1)
            break
        default:
            close()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
        opacity: 0.8
    }
    MouseArea { anchors.fill: parent; onClicked: dialog.close() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 620)
        height: Math.min(parent.height - 48, column.implicitHeight + Theme.spacing * 3)
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        MouseArea { anchors.fill: parent } // keep clicks inside

        Column {
            id: column
            anchors.fill: parent
            anchors.margins: Theme.spacing * 1.5
            spacing: Theme.spacing * 0.75

            Text {
                objectName: "gameOptionsTitle"
                width: parent.width
                text: dialog.details.title || ""
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: (dialog.details.path || "") + (dialog.details.size ? "  ·  " + dialog.details.size : "")
                color: Theme.textMuted
                font.pixelSize: Theme.fontTiny
                elide: Text.ElideMiddle
            }
            Text {
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
                height: Math.min(contentHeight, 420)
                spacing: 6
                clip: true
                keyNavigationEnabled: true
                model: dialog.rows
                delegate: Rectangle {
                    id: option
                    required property var modelData
                    required property int index
                    width: optionList.width
                    height: modelData.detail.length > 0 ? 56 : 44
                    radius: 10
                    readonly property bool current: ListView.isCurrentItem && optionList.activeFocus
                    color: current ? Theme.surfaceHover : Theme.surfaceRaised
                    border.width: current ? Theme.focusWidth : 1
                    border.color: current ? Theme.accent : Theme.border
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.label
                    Accessible.description: modelData.detail

                    Text {
                        id: mark
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing * 0.75
                        anchors.verticalCenter: parent.verticalCenter
                        width: 22
                        text: option.modelData.kind === "emulator" ? (option.modelData.checked ? "●" : "○") : ""
                        color: Theme.accent
                        font.pixelSize: Theme.fontBody
                    }
                    Column {
                        anchors.left: mark.right
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            width: parent.width
                            text: option.modelData.label
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.bold: option.modelData.kind === "play"
                            elide: Text.ElideRight
                        }
                        Text {
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
}
