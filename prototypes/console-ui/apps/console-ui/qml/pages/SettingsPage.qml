import QtQuick
import QtQuick.Window
import Nexora.Core

// Settings: category list on the left, the selected category on the right.
//   category list: Up/Down switch the panel immediately, Right/Confirm move
//                  focus into the panel, Left goes to the sidebar and Back is
//                  left to the Shell (previous section).
//   panel:         Up/Down between rows, Back (or Left on a row that does not
//                  use Left itself) returns to the category list.
PageContainer {
    id: page
    title: qsTr("Settings")

    readonly property var categories: [
        { key: "general", label: qsTr("General"), icon: "settings",
          description: qsTr("Interface scale, sorting, motion and defaults") },
        { key: "display", label: qsTr("Display"), icon: "monitor",
          description: qsTr("Screen information and full-screen mode") },
        { key: "audio", label: qsTr("Audio"), icon: "speaker",
          description: qsTr("Sound output") },
        { key: "controller", label: qsTr("Controller"), icon: "gamepad",
          description: qsTr("Connected controllers, input test and button mapping") },
        { key: "library", label: qsTr("Game Library"), icon: "library",
          description: qsTr("Game folders, scanning and removed games") },
        { key: "emulators", label: qsTr("Emulator Configuration"), icon: "chip",
          description: qsTr("Configuration file, emulator status and launch behaviour") },
        { key: "system", label: qsTr("System & Diagnostics"), icon: "activity",
          description: qsTr("Versions, logs and recent errors") },
        { key: "about", label: qsTr("About Nexora OS"), icon: "info",
          description: qsTr("Version, licences and acknowledgements") }
    ]
    readonly property var current: categories[Math.max(0, categoryList.currentIndex)]
    readonly property bool panelFocused: panel.activeFocus

    // Row currently focused inside the panel (for the hint bar).
    property Item focusedRow: null
    Connections {
        target: page.Window.window
        function onActiveFocusItemChanged() {
            var item = page.Window.window ? page.Window.window.activeFocusItem : null
            var found = null
            for (var p = item; p; p = p.parent) {
                if (p === panel) break
                if (!found && p.settingsHint !== undefined) found = p
            }
            page.focusedRow = p === panel ? found : null
        }
    }

    defaultFocusItem: categoryList
    hints: {
        if (!panelFocused)
            return [{ action: "confirm", label: qsTr("Open") }]
        var list = []
        if (focusedRow && focusedRow.settingsFocusable && focusedRow.settingsHint)
            list.push({ action: "confirm", label: focusedRow.settingsHint })
        list.push({ action: "back", label: focusedRow && focusedRow.settingsBackHint
                                           ? focusedRow.settingsBackHint : qsTr("Categories") })
        return list
    }

    function indexOfKey(key) {
        for (var i = 0; i < categories.length; ++i)
            if (categories[i].key === key) return i
        return -1
    }
    // Called by the Shell through Router.openSettings(category).
    function openCategory(key) {
        var i = indexOfKey(key)
        if (i < 0) { categoryList.forceActiveFocus(); return }
        categoryList.currentIndex = i
        categoryList.positionViewAtIndex(i, ListView.Contain)
        focusPanel()
    }
    // Moves focus into the panel unless it has nothing to focus or scroll
    // (e.g. Audio), in which case the category list keeps focus.
    function focusPanel() {
        var item = loader.item
        if (!item) return
        if (item.focusables && item.focusables().length === 0
                && item.flickable && item.flickable.contentHeight <= item.flickable.height) {
            categoryList.forceActiveFocus()
            return
        }
        if (item.focusFirst) item.focusFirst()
        else panel.forceActiveFocus()
    }
    function focusCategories() {
        categoryList.forceActiveFocus()
    }

    // ---- category list -------------------------------------------------
    Rectangle {
        id: listBackground
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Dimensions.compact ? Dimensions.s(330) : Dimensions.s(380)
        radius: Dimensions.radiusLg
        color: Theme.backgroundSecondary
        border.color: Theme.borderSubtle
        border.width: 1

        ListView {
            id: categoryList
            anchors.fill: parent
            anchors.margins: Dimensions.spacingMd
            model: page.categories
            focus: true
            clip: true
            spacing: Dimensions.spacingXs
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            currentIndex: 0
            boundsBehavior: Flickable.StopAtBounds
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Settings categories")

            Keys.onPressed: function(event) {
                if (Input.isConfirm(event) || event.key === Qt.Key_Right) {
                    event.accepted = true
                    if (!event.isAutoRepeat || event.key === Qt.Key_Right) page.focusPanel()
                }
            }

            delegate: Item {
                id: entry
                required property var modelData
                required property int index
                readonly property bool selected: ListView.isCurrentItem
                readonly property bool focused: selected && categoryList.activeFocus
                width: categoryList.width
                height: Dimensions.navItemHeight
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.label
                Accessible.checkable: true
                Accessible.checked: selected

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Dimensions.focusOffset
                    radius: Dimensions.radiusMd
                    color: entry.focused ? Theme.backgroundHover
                         : entry.selected ? Theme.accentPrimarySoft
                         : (mouse.containsMouse ? Theme.backgroundElevated : "transparent")
                    FocusFrame { shown: entry.focused }
                    Rectangle {
                        // selected indicator (shape, not only colour)
                        visible: entry.selected
                        width: Dimensions.s(4)
                        height: parent.height * 0.5
                        radius: width / 2
                        anchors.left: parent.left
                        anchors.leftMargin: Dimensions.s(4)
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.accentPrimary
                    }
                    Icon {
                        id: entryIcon
                        anchors.left: parent.left
                        anchors.leftMargin: Dimensions.spacingMd + Dimensions.s(4)
                        anchors.verticalCenter: parent.verticalCenter
                        name: entry.modelData.icon
                        size: Dimensions.iconSizeSmall
                        color: entry.selected ? Theme.accentPrimary : Theme.textSecondary
                    }
                    Text {
                        anchors.left: entryIcon.right
                        anchors.leftMargin: Dimensions.spacingMd
                        anchors.right: chevron.left
                        anchors.rightMargin: Dimensions.spacingXs
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.label
                        elide: Text.ElideRight
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        font.weight: entry.selected ? Typography.weightSemiBold : Typography.weightMedium
                        color: entry.selected || entry.focused ? Theme.textPrimary : Theme.textSecondary
                    }
                    Icon {
                        id: chevron
                        anchors.right: parent.right
                        anchors.rightMargin: Dimensions.spacingSm
                        anchors.verticalCenter: parent.verticalCenter
                        name: "chevron-right"
                        size: Dimensions.iconSizeSmall * 0.8
                        color: Theme.textMuted
                        visible: entry.focused
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        categoryList.currentIndex = entry.index
                        categoryList.forceActiveFocus()
                    }
                }
            }
        }
    }

    // ---- content panel ---------------------------------------------------
    Rectangle {
        id: panelBackground
        anchors.left: listBackground.right
        anchors.leftMargin: Dimensions.spacingLg
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        radius: Dimensions.radiusLg
        color: Theme.backgroundSecondary
        border.width: 1
        border.color: page.panelFocused ? Theme.borderStrong : Theme.borderSubtle

        Column {
            id: panelHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Dimensions.spacingLg
            spacing: Dimensions.s(4)
            Text {
                width: parent.width
                text: page.current.label
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.sectionTitle
                font.weight: Typography.weightSemiBold
                color: Theme.textPrimary
            }
            Text {
                width: parent.width
                text: page.current.description
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                color: Theme.textSecondary
            }
        }

        FocusScope {
            id: panel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: panelHeader.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: Dimensions.spacingLg - Dimensions.focusOffset - Dimensions.focusWidth * 2
            anchors.rightMargin: Dimensions.spacingSm
            anchors.topMargin: Dimensions.spacingMd - Dimensions.focusOffset - Dimensions.focusWidth * 2
            anchors.bottomMargin: Dimensions.spacingSm

            // Local unwinding: Back (or an unused Left) returns to the list.
            Keys.onPressed: function(event) {
                if (Input.isBack(event) || event.key === Qt.Key_Left) {
                    event.accepted = true
                    if (!event.isAutoRepeat) page.focusCategories()
                }
            }

            Loader {
                id: loader
                anchors.fill: parent
                focus: true
                sourceComponent: {
                    switch (page.current.key) {
                    case "general": return generalPage
                    case "display": return displayPage
                    case "audio": return audioPage
                    case "controller": return controllerPage
                    case "library": return libraryPage
                    case "emulators": return emulatorPage
                    case "system": return systemPage
                    default: return aboutPage
                    }
                }
            }
        }
    }

    Component { id: generalPage; GeneralSettingsPage {} }
    Component { id: displayPage; DisplaySettingsPage {} }
    Component { id: audioPage; AudioSettingsPage {} }
    Component { id: controllerPage; ControllerSettingsPage {} }
    Component { id: libraryPage; LibrarySettingsPage {} }
    Component { id: emulatorPage; EmulatorSettingsPage {} }
    Component { id: systemPage; SystemDiagnosticsPage {} }
    Component { id: aboutPage; AboutPage {} }
}
