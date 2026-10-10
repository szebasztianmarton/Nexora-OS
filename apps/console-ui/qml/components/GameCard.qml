import QtQuick

// Portrait game card used by the library grid, favorites and home rows.
// States: default, focused (accent ring + slight scale), selected (persistent
// violet outline when focus is elsewhere), favorite (star badge).
// Keys: Confirm -> activated, Secondary (F / West) -> favoriteToggled,
//       Options (M / Start) -> optionsRequested.
Item {
    id: card

    property string title: ""
    property string artworkUrl: ""
    property string subtitle: ""          // e.g. emulator or platform; omitted when empty
    property bool favorite: false
    property bool selected: false
    property bool showSubtitle: true

    signal activated()
    signal favoriteToggled()
    signal optionsRequested()
    signal clicked()                      // pointer selection; views update currentIndex

    readonly property bool focused: activeFocus
    implicitWidth: Dimensions.rowCardWidth
    implicitHeight: implicitWidth * Dimensions.cardAspect + Dimensions.cardTitleHeight

    z: focused ? 2 : 0
    scale: focused ? Dimensions.focusScale : 1
    Behavior on scale { NumberAnimation { duration: Motion.card; easing.type: Motion.easing } }

    Accessible.role: Accessible.Button
    Accessible.name: title + (favorite ? qsTr(", favorite") : "")
    Accessible.onPressAction: card.activated()

    Keys.onPressed: function(event) {
        if (event.isAutoRepeat && !(event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                    || event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
            event.accepted = true
            return
        }
        if (Input.isConfirm(event)) { event.accepted = true; card.activated() }
        else if (Input.isSecondary(event)) { event.accepted = true; card.favoriteToggled() }
        else if (Input.isOptions(event)) { event.accepted = true; card.optionsRequested() }
    }

    Item {
        id: art
        width: parent.width
        height: width * Dimensions.cardAspect

        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusSm
            color: Theme.backgroundElevated
            clip: true
            Artwork {
                anchors.fill: parent
                source: card.artworkUrl
                title: card.title
                showPlaceholderTitle: false
            }
        }
        Rectangle {
            // favorite badge
            visible: card.favorite
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Dimensions.spacingSm
            width: Dimensions.s(34); height: width
            radius: width / 2
            color: Theme.backgroundOverlay
            Icon {
                anchors.centerIn: parent
                name: "star-filled"
                size: Dimensions.iconSizeSmall
                color: Theme.statusWarning
            }
        }
        Rectangle {
            // persistent selection outline (distinct from focus)
            anchors.fill: parent
            anchors.margins: -Dimensions.s(2)
            radius: Dimensions.radiusSm + Dimensions.s(2)
            color: "transparent"
            border.width: Dimensions.s(2)
            border.color: Theme.accentSecondary
            visible: card.selected && !card.focused
        }
        FocusFrame { shown: card.focused; radiusBase: Dimensions.radiusSm }
    }

    Column {
        anchors.top: art.bottom
        anchors.topMargin: Dimensions.spacingSm
        width: parent.width
        spacing: Dimensions.s(2)
        Text {
            width: parent.width
            text: card.title
            elide: Text.ElideRight
            maximumLineCount: 1
            font.family: Typography.family
            font.pixelSize: Typography.cardTitle * 0.9
            font.weight: Typography.weightMedium
            color: card.focused ? Theme.textPrimary : Theme.textSecondary
        }
        Text {
            width: parent.width
            visible: card.showSubtitle && card.subtitle !== ""
            text: card.subtitle
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.caption
            color: Theme.textMuted
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function(mouse) {
            card.clicked()
            card.forceActiveFocus()
            if (mouse.button === Qt.RightButton) card.optionsRequested()
        }
        onDoubleClicked: card.activated()
    }
}
