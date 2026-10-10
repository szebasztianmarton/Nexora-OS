import QtQuick
import QtQuick.Layouts
import Nexora.Core

// Controller-friendly folder picker (modal). Lists folders only, never files,
// and never scans anything. Built on DirectoryBrowser.
//
// Usage (works with or without the Shell / Router — the dialog fills its parent
// and traps focus; it restores focus and destroys itself when closed):
//
//   Component { id: folderDialogComponent; FolderBrowserDialog {} }
//   ...
//   var dlg = folderDialogComponent.createObject(someFullScreenItem, { startPath: "" })
//   dlg.folderChosen.connect(function(path) { /* already added to the library */ })
//
// In the Shell context pass any item that covers the window (e.g. the page
// root or the Shell's modal layer). With `addToLibrary: true` (default)
// "Use this folder" calls Settings.addLibraryDirectory(path); a non-empty error
// string is shown inline and the dialog stays open. With `addToLibrary: false`
// the chosen path is only reported through folderChosen(path).
//
// Keys: Up/Down move, Confirm opens a folder, Secondary (F / West) goes up one
// level, Left/Right switch between the locations and folder lists, Down past
// the end reaches the buttons, Back cancels.
ModalBase {
    id: dialog

    property string startPath: ""
    property bool addToLibrary: true
    property string confirmText: qsTr("Use this folder")
    signal folderChosen(string path)

    title: qsTr("Choose a game folder")
    panelWidth: Dimensions.s(1120)
    initialFocusItem: folderList.count > 0 ? folderList : useButton

    property string _message: ""
    property string _messageKind: "error"

    DirectoryBrowser { id: browser }

    Component.onCompleted: {
        if (startPath !== "") browser.open(startPath)
    }

    function _enter(name) {
        _message = ""
        if (browser.enter(name)) folderList.currentIndex = 0
        _afterNavigate()
    }
    function _up() {
        if (!browser.canGoUp) return
        var leaving = browser.currentPath.substring(browser.currentPath.lastIndexOf("/") + 1)
        _message = ""
        if (browser.up()) folderList.currentIndex = Math.max(0, browser.entries.indexOf(leaving))
        _afterNavigate()
    }
    function _openLocation(path) {
        _message = ""
        if (browser.open(path)) folderList.currentIndex = 0
        _afterNavigate()
    }
    function _afterNavigate() {
        // Never leave focus on an empty list.
        if (folderList.activeFocus && folderList.count === 0) useButton.forceActiveFocus()
    }
    function _use() {
        var path = browser.currentPath
        if (path === "") return
        if (addToLibrary) {
            var error = Settings.addLibraryDirectory(path)
            if (error !== "") {
                _messageKind = "error"
                _message = error
                return
            }
        }
        close()
        folderChosen(path)
    }

    ColumnLayout {
        // Inset so focus rings are not clipped by the panel body.
        x: Dimensions.focusOffset * 2
        y: Dimensions.focusOffset * 2
        width: parent.width - Dimensions.focusOffset * 4
        height: Math.min(Dimensions.s(780), dialog.maxPanelHeight - Dimensions.spacingXl * 2) - Dimensions.focusOffset * 4
        spacing: Dimensions.spacingMd
        // Secondary = up one level from anywhere in the dialog (buttons included).
        Keys.onPressed: function(event) {
            if (Input.isSecondary(event)) {
                event.accepted = true
                if (!event.isAutoRepeat) dialog._up()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingMd
            Icon { name: "folder"; color: Theme.accentPrimary; size: Dimensions.iconSize }
            Text {
                Layout.fillWidth: true
                text: dialog.title
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.sectionTitle
                font.weight: Typography.weightSemiBold
                color: Theme.textPrimary
            }
        }

        // Current path
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pathText.implicitHeight + Dimensions.spacingSm * 2
            radius: Dimensions.radiusSm
            color: Theme.backgroundSecondary
            border.color: Theme.borderSubtle
            Text {
                id: pathText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Dimensions.spacingMd
                text: browser.currentPath || qsTr("No folder selected")
                elide: Text.ElideLeft
                font.family: "monospace"
                font.pixelSize: Typography.secondary
                color: Theme.textPrimary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Dimensions.spacingLg

            // Locations
            ColumnLayout {
                Layout.preferredWidth: Dimensions.s(300)
                Layout.maximumWidth: Dimensions.s(300)
                Layout.fillHeight: true
                spacing: Dimensions.spacingSm
                ListLabel { text: qsTr("Locations") }
                ListView {
                    id: locationList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: browser.locations
                    keyNavigationEnabled: true
                    spacing: Dimensions.spacingXs
                    topMargin: Dimensions.focusOffset
                    bottomMargin: Dimensions.focusOffset
                    leftMargin: Dimensions.focusOffset
                    rightMargin: Dimensions.focusOffset
                    delegate: BrowserEntry {
                        required property var modelData
                        required property int index
                        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                        text: modelData.name
                        iconName: modelData.name === "File system" ? "monitor" : "folder"
                        active: ListView.isCurrentItem && ListView.view.activeFocus
                        selected: browser.currentPath === modelData.path
                        onClicked: { locationList.currentIndex = index; locationList.forceActiveFocus(); dialog._openLocation(modelData.path) }
                    }
                    Keys.onPressed: function(event) {
                        if (Input.isConfirm(event)) {
                            event.accepted = true
                            if (!event.isAutoRepeat && currentIndex >= 0)
                                dialog._openLocation(browser.locations[currentIndex].path)
                        } else if (event.key === Qt.Key_Right) {
                            event.accepted = true
                            if (folderList.count > 0) folderList.forceActiveFocus()
                            else useButton.forceActiveFocus()
                        } else if (Input.isSecondary(event)) {
                            event.accepted = true
                            if (!event.isAutoRepeat) dialog._up()
                        } else if (event.key === Qt.Key_Down && currentIndex >= count - 1) {
                            event.accepted = true
                            useButton.forceActiveFocus()
                        }
                    }
                }
            }

            // Sub-folders
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Dimensions.spacingSm
                ListLabel {
                    text: browser.entries.length === 1 ? qsTr("1 folder") : qsTr("%1 folders").arg(browser.entries.length)
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ListView {
                        id: folderList
                        anchors.fill: parent
                        clip: true
                        model: browser.entries
                        keyNavigationEnabled: true
                        spacing: Dimensions.spacingXs
                        highlightMoveDuration: Motion.card
                        highlightRangeMode: ListView.ApplyRange
                        preferredHighlightBegin: Dimensions.s(40)
                        preferredHighlightEnd: height - Dimensions.s(40)
                        topMargin: Dimensions.focusOffset
                        bottomMargin: Dimensions.focusOffset
                        leftMargin: Dimensions.focusOffset
                        rightMargin: Dimensions.focusOffset
                        delegate: BrowserEntry {
                            required property string modelData
                            required property int index
                            width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                            text: modelData
                            iconName: "folder"
                            showChevron: true
                            active: ListView.isCurrentItem && ListView.view.activeFocus
                            onClicked: { folderList.currentIndex = index; folderList.forceActiveFocus(); dialog._enter(modelData) }
                        }
                        Keys.onPressed: function(event) {
                            if (Input.isConfirm(event)) {
                                event.accepted = true
                                if (!event.isAutoRepeat && currentIndex >= 0) dialog._enter(browser.entries[currentIndex])
                            } else if (Input.isSecondary(event)) {
                                event.accepted = true
                                if (!event.isAutoRepeat) dialog._up()
                            } else if (event.key === Qt.Key_Left) {
                                event.accepted = true
                                locationList.forceActiveFocus()
                            } else if (event.key === Qt.Key_Down && currentIndex >= count - 1) {
                                event.accepted = true
                                useButton.forceActiveFocus()
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - Dimensions.spacingLg * 2
                        visible: folderList.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: qsTr("No sub-folders here. You can use this folder or go up.")
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: Theme.textMuted
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: dialog._message !== "" ? dialog._message : browser.error
            wrapMode: Text.Wrap
            font.family: Typography.family
            font.pixelSize: Typography.secondary
            color: Theme.statusError
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingMd
            FocusButton {
                id: upButton
                text: qsTr("Up one level")
                iconName: "chevron-up"
                enabled: browser.canGoUp
                onClicked: dialog._up()
                KeyNavigation.right: useButton
                KeyNavigation.up: folderList.count > 0 ? folderList : locationList
            }
            Item { Layout.fillWidth: true }
            FocusButton {
                id: cancelButton
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: dialog.reject()
                KeyNavigation.left: upButton.enabled ? upButton : null
                KeyNavigation.right: useButton
                KeyNavigation.up: folderList.count > 0 ? folderList : locationList
            }
            FocusButton {
                id: useButton
                text: dialog.confirmText
                iconName: "check"
                variant: "primary"
                enabled: browser.currentPath !== ""
                onClicked: dialog._use()
                KeyNavigation.left: cancelButton
                KeyNavigation.up: folderList.count > 0 ? folderList : locationList
            }
        }

        ControllerHints {
            Layout.fillWidth: true
            hints: {
                var h = []
                if (folderList.activeFocus) h.push({ action: "confirm", label: qsTr("Open folder") })
                else if (locationList.activeFocus) h.push({ action: "confirm", label: qsTr("Go to location") })
                else h.push({ action: "confirm", label: qsTr("Select") })
                if (browser.canGoUp) h.push({ action: "secondary", label: qsTr("Up one level") })
                h.push({ action: "back", label: qsTr("Cancel") })
                return h
            }
        }
    }

    component ListLabel: Text {
        Layout.fillWidth: true
        font.family: Typography.family
        font.pixelSize: Typography.caption
        font.weight: Typography.weightSemiBold
        color: Theme.textMuted
    }

    component BrowserEntry: Item {
        id: entry
        property string text: ""
        property string iconName: "folder"
        property bool active: false
        property bool selected: false
        property bool showChevron: false
        signal clicked()
        height: Dimensions.buttonHeightSmall
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusSm
            color: entry.active ? Theme.backgroundHover : entry.selected ? Theme.accentPrimarySoft : "transparent"
        }
        FocusFrame { shown: entry.active; radiusBase: Dimensions.radiusSm }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.spacingMd
            anchors.rightMargin: Dimensions.spacingMd
            spacing: Dimensions.spacingSm
            Icon {
                name: entry.iconName
                size: Dimensions.iconSizeSmall
                color: entry.active || entry.selected ? Theme.accentPrimary : Theme.textSecondary
            }
            Text {
                Layout.fillWidth: true
                text: entry.text
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.body
                font.weight: entry.active ? Typography.weightSemiBold : Typography.weightRegular
                color: Theme.textPrimary
            }
            Icon {
                visible: entry.showChevron
                name: "chevron-right"
                size: Dimensions.iconSizeSmall
                color: Theme.textMuted
            }
        }
        MouseArea { anchors.fill: parent; hoverEnabled: true; onClicked: entry.clicked() }
    }
}
