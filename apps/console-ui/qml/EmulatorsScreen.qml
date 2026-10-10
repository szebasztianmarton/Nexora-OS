import QtQuick
import Nexora.Console

// Emulators: a horizontal row of cards navigated with arrows / D-pad. Accept launches the
// emulator; one with launch profiles first asks which profile to use.
FocusScope {
    id: screen
    objectName: "emulatorsScreen"
    required property AppController app

    readonly property alias currentIndex: list.currentIndex
    readonly property bool pickerOpen: picker.visible
    readonly property var hints: picker.visible
        ? [{ button: "↑ ↓", action: "Select profile" }, { button: "A / Enter", action: "Launch" },
           { button: "B / Esc", action: "Cancel" }]
        : [{ button: "← →", action: "Select" }, { button: "A / Enter", action: "Launch" },
           { button: "L1 / R1", action: "Switch tab" }, { button: "Menu / F1", action: "Diagnostics" }]

    function focusDefault() { list.focus = true }

    function enabledProfiles(profiles) {
        const result = []
        for (let i = 0; i < profiles.length; ++i) {
            if (profiles[i].enabled)
                result.push(profiles[i])
        }
        return result
    }

    function activate(index) {
        if (index < 0)
            return
        const item = list.itemAtIndex(index)
        if (!item)
            return
        const profiles = enabledProfiles(item.profiles)
        if (profiles.length === 0) {
            screen.app.launch(item.emulatorId)
            return
        }
        picker.open(item.emulatorId, item.name, profiles)
    }

    Text {
        id: title
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: Theme.spacing * 1.5
        anchors.leftMargin: Theme.spacing * 2
        text: "Emulators"
        color: Theme.text
        font.pixelSize: Theme.fontHero
        font.bold: true
    }

    Text {
        id: statusLine
        objectName: "statusLine"
        visible: list.count > 0
        anchors.top: title.bottom
        anchors.left: title.left
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 2
        anchors.topMargin: 6
        text: screen.app.statusMessage.length > 0 ? screen.app.statusMessage
              : list.count > 0 ? "Launch an emulator without a game, e.g. to configure it." : ""
        color: screen.app.statusIsError ? Theme.danger : Theme.textMuted
        font.pixelSize: Theme.fontBody
        elide: Text.ElideRight
    }

    ListView {
        id: list
        objectName: "emulatorList"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: statusLine.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.spacing
        anchors.bottomMargin: Theme.spacing
        leftMargin: Theme.spacing * 2
        rightMargin: Theme.spacing * 2
        orientation: ListView.Horizontal
        spacing: Theme.spacing * 1.5
        clip: false
        focus: true
        visible: count > 0
        keyNavigationEnabled: true
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 120
        preferredHighlightBegin: Theme.spacing * 2
        preferredHighlightEnd: width - 300 - Theme.spacing * 2
        highlightRangeMode: ListView.ApplyRange
        model: screen.app.emulators

        delegate: EmulatorCard {
            height: Math.min(380, list.height - 24)
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            onActivated: screen.activate(index)
        }

        Keys.onReturnPressed: screen.activate(currentIndex)
        Keys.onEnterPressed: screen.activate(currentIndex)
        Keys.onSpacePressed: screen.activate(currentIndex)
    }

    EmptyState {
        objectName: "emptyState"
        app: screen.app
        anchors.centerIn: parent
        visible: list.count === 0
    }

    // --- Profile picker -----------------------------------------------------------------
    FocusScope {
        id: picker
        objectName: "profilePicker"
        anchors.fill: parent
        visible: false
        z: 5

        property string emulatorId
        property string emulatorName
        property var choices: []

        function open(id, name, profiles) {
            emulatorId = id
            emulatorName = name
            const c = [{ id: "", name: "Default", detail: "Emulator defaults" }]
            for (let i = 0; i < profiles.length; ++i)
                c.push({ id: profiles[i].id, name: profiles[i].name, detail: "Profile “" + profiles[i].id + "”" })
            choices = c
            profileList.currentIndex = 0
            visible = true
            profileList.forceActiveFocus()
        }
        function close() {
            visible = false
            list.forceActiveFocus()
        }
        function launchCurrent() {
            const choice = choices[profileList.currentIndex]
            const id = emulatorId
            close()
            if (choice.id.length > 0)
                screen.app.launchProfile(id, choice.id)
            else
                screen.app.launch(id)
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.background
            opacity: 0.8
        }
        MouseArea { anchors.fill: parent; onClicked: picker.close() }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 520)
            height: Math.min(parent.height - 48, pickerColumn.implicitHeight + Theme.spacing * 3)
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            MouseArea { anchors.fill: parent } // keep clicks inside

            Column {
                id: pickerColumn
                anchors.fill: parent
                anchors.margins: Theme.spacing * 1.5
                spacing: Theme.spacing
                Text {
                    text: "Launch " + picker.emulatorName
                    color: Theme.text
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    width: parent.width
                    elide: Text.ElideRight
                }
                Text {
                    text: "Choose a launch profile"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
                ListView {
                    id: profileList
                    objectName: "profileList"
                    width: parent.width
                    height: Math.min(contentHeight, 360)
                    spacing: 8
                    clip: true
                    keyNavigationEnabled: true
                    model: picker.choices
                    delegate: Rectangle {
                        id: choice
                        required property var modelData
                        required property int index
                        width: profileList.width
                        height: 60
                        radius: 10
                        readonly property bool current: ListView.isCurrentItem && profileList.activeFocus
                        color: current ? Theme.surfaceHover : Theme.surfaceRaised
                        border.width: current ? Theme.focusWidth : 1
                        border.color: current ? Theme.accent : Theme.border
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacing
                            spacing: 2
                            Text {
                                text: choice.modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                font.bold: true
                            }
                            Text {
                                text: choice.modelData.detail
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontTiny
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { profileList.currentIndex = choice.index; picker.launchCurrent() }
                        }
                    }
                    Keys.onReturnPressed: picker.launchCurrent()
                    Keys.onEnterPressed: picker.launchCurrent()
                }
            }
        }

        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Escape:
            case Qt.Key_Back:
            case Qt.Key_Backspace:
                picker.close()
                event.accepted = true
                break
            case Qt.Key_Left:
            case Qt.Key_Right:
            case Qt.Key_Up:
            case Qt.Key_Down:
                event.accepted = true // stay inside the picker
                break
            }
        }
    }

    onVisibleChanged: if (!visible && picker.visible) { picker.visible = false; list.focus = true }
}
