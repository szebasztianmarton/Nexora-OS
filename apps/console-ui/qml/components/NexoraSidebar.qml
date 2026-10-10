import QtQuick
import Nexora.Core

// Primary navigation. Up/Down moves between sections (the page follows the
// selection immediately); Right or Confirm moves focus into the page.
FocusScope {
    id: sidebar

    property string currentSection: "home"
    property bool compact: false
    signal sectionRequested(string section)
    signal enterContent()

    readonly property var items: [
        { section: "home", label: qsTr("Home"), icon: "home" },
        { section: "library", label: qsTr("Game Library"), icon: "library" },
        { section: "favorites", label: qsTr("Favorites"), icon: "star" },
        { section: "emulators", label: qsTr("Emulators"), icon: "chip" },
        { section: "settings", label: qsTr("Settings"), icon: "settings" }
    ]

    implicitWidth: compact ? Dimensions.sidebarCompactWidth : Dimensions.sidebarWidth
    Behavior on implicitWidth { NumberAnimation { duration: Motion.page; easing.type: Motion.easing } }

    // Focus the entry for the active section (used when entering the sidebar).
    function focusActive() {
        for (var i = 0; i < repeater.count; ++i) {
            if (items[i].section === currentSection) {
                repeater.itemAt(i).forceActiveFocus()
                return
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundSecondary
        Rectangle {
            anchors.right: parent.right
            width: 1; height: parent.height
            color: Theme.borderSubtle
        }
    }

    NexoraLogo {
        id: logo
        showWordmark: !sidebar.compact
        anchors.top: parent.top
        anchors.topMargin: Dimensions.pagePaddingV
        anchors.horizontalCenter: sidebar.compact ? parent.horizontalCenter : undefined
        anchors.left: sidebar.compact ? undefined : parent.left
        anchors.leftMargin: Dimensions.spacingLg + Dimensions.s(6)
    }

    Column {
        id: navColumn
        anchors.top: logo.bottom
        anchors.topMargin: Dimensions.spacingXl
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Dimensions.spacingMd
        spacing: Dimensions.spacingXs

        Repeater {
            id: repeater
            model: sidebar.items
            delegate: NavItem {
                required property var modelData
                required property int index
                width: navColumn.width
                label: modelData.label
                iconName: modelData.icon
                compact: sidebar.compact
                active: sidebar.currentSection === modelData.section
                focus: active
                onActivated: { sidebar.sectionRequested(modelData.section); sidebar.enterContent() }
                onActiveFocusChanged: if (activeFocus && !active) sidebar.sectionRequested(modelData.section)
                Keys.onUpPressed: function(event) {
                    if (index > 0) repeater.itemAt(index - 1).forceActiveFocus()
                }
                Keys.onDownPressed: function(event) {
                    if (index < repeater.count - 1) repeater.itemAt(index + 1).forceActiveFocus()
                }
                Keys.onRightPressed: sidebar.enterContent()
                Keys.onPressed: function(event) {
                    if (Input.isConfirm(event)) {
                        event.accepted = true
                        if (!event.isAutoRepeat) sidebar.enterContent()
                    }
                }
            }
        }
    }

    // Bottom status: controller connection + clock (real values only).
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Dimensions.spacingLg
        spacing: Dimensions.spacingSm

        Row {
            spacing: Dimensions.spacingSm
            anchors.horizontalCenter: sidebar.compact ? parent.horizontalCenter : undefined
            Icon {
                name: "gamepad"
                size: Dimensions.iconSizeSmall
                anchors.verticalCenter: parent.verticalCenter
                color: Controllers.count > 0 ? Theme.statusSuccess : Theme.textMuted
            }
            Text {
                visible: !sidebar.compact
                anchors.verticalCenter: parent.verticalCenter
                text: !Controllers.available ? qsTr("Controller support off")
                    : Controllers.count > 0 ? qsTr("%n controller(s)", "", Controllers.count)
                    : qsTr("No controller")
                font.family: Typography.family
                font.pixelSize: Typography.caption
                color: Theme.textMuted
            }
        }
        Text {
            id: clock
            anchors.horizontalCenter: sidebar.compact ? parent.horizontalCenter : undefined
            font.family: Typography.family
            font.pixelSize: sidebar.compact ? Typography.caption : Typography.secondary
            font.weight: Typography.weightMedium
            color: Theme.textSecondary
            text: Qt.formatTime(new Date(), "hh:mm")
            Timer {
                interval: 15000; running: true; repeat: true
                onTriggered: clock.text = Qt.formatTime(new Date(), "hh:mm")
            }
        }
    }
}
