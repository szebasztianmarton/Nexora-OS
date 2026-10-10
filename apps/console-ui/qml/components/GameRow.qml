import QtQuick
import Nexora.Core

// Titled horizontal row of game cards (Home: Recently Played, Favorites).
// Left/Right move between cards; Up/Down leave the row (handled by the page).
// Confirm opens details, Secondary toggles favorite, Options opens the menu.
FocusScope {
    id: row

    property string title: ""
    property alias model: list.model
    property alias currentIndex: list.currentIndex
    readonly property int count: list.count
    property real cardWidth: Dimensions.rowCardWidth
    property string subtitleRole: "emulatorName"

    implicitHeight: heading.height + Dimensions.spacingMd + list.height

    Text {
        id: heading
        text: row.title
        font.family: Typography.family
        font.pixelSize: Typography.sectionTitle
        font.weight: Typography.weightSemiBold
        color: row.activeFocus ? Theme.textPrimary : Theme.textSecondary
    }

    ListView {
        id: list
        focus: true
        anchors.top: heading.bottom
        anchors.topMargin: Dimensions.spacingMd
        width: parent.width
        height: row.cardWidth * Dimensions.cardAspect + Dimensions.cardTitleHeight + Dimensions.focusOffset * 2
        orientation: ListView.Horizontal
        spacing: Dimensions.cardGap
        clip: false
        keyNavigationEnabled: true
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: Motion.card
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: Dimensions.focusOffset * 2
        preferredHighlightEnd: width - row.cardWidth - Dimensions.focusOffset * 2
        cacheBuffer: row.cardWidth * 4

        delegate: GameCard {
            required property int index
            required property var model
            y: Dimensions.focusOffset
            width: row.cardWidth
            title: model.title
            artworkUrl: model.artworkUrl
            favorite: model.favorite
            subtitle: model[row.subtitleRole] || ""
            onClicked: list.currentIndex = index
            onActivated: GameActions.openDetails(model.path)
            onFavoriteToggled: GameActions.toggleFavorite(model.path)
            onOptionsRequested: GameActions.openMenu(model.path)
        }
    }
}
