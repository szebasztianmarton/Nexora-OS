import QtQuick
import QtQuick.Layouts
import Nexora.Core

// "Continue Playing" hero on Home: the most recently played game with large
// artwork, its emulator, when it was last played and Launch / Details.
// Portrait artwork sits in a 2:3 frame; landscape artwork gets a 16:9 frame.
// The backdrop is a faint, cropped copy of the artwork (or the placeholder
// gradient) fading into the surface colour.
FocusScope {
    id: hero

    property var game: ({})
    readonly property Item launchItem: launchButton
    readonly property Item detailsItem: detailsButton

    function focusFirst() {
        if (launchButton.enabled) launchButton.forceActiveFocus()
        else detailsButton.forceActiveFocus()
    }

    implicitHeight: Dimensions.s(400)

    readonly property var _colors: Theme.placeholderColors(game.title || "")

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Dimensions.radiusLg
        color: Theme.backgroundElevated
        border.color: hero.activeFocus ? Theme.borderStrong : Theme.borderSubtle
        clip: true

        // Placeholder-derived tint
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            opacity: 0.55
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: hero._colors[1] }
                GradientStop { position: 1.0; color: hero._colors[0] }
            }
        }
        // Faint artwork backdrop on the right
        Image {
            id: backdrop
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: parent.width * 0.6
            source: hero.game.artworkUrl || ""
            visible: status === Image.Ready
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: Math.ceil(width / 3)   // low resolution on purpose: soft look
            smooth: true
            opacity: 0.22
        }
        // Fade from the surface colour into the backdrop
        Rectangle {
            anchors.fill: backdrop
            visible: backdrop.visible
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.backgroundElevated }
                GradientStop { position: 0.6; color: Qt.rgba(Theme.backgroundElevated.r, Theme.backgroundElevated.g, Theme.backgroundElevated.b, 0.35) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Dimensions.spacingXl
        spacing: Dimensions.spacingXl

        Rectangle {
            id: artFrame
            Layout.fillHeight: true
            // Deferred: resizing reloads the sourceSize'd image (binding feedback).
            property bool landscape: false
            Connections {
                target: art
                function onLandscapeChanged() { Qt.callLater(function() { artFrame.landscape = art.landscape }) }
            }
            Layout.preferredWidth: landscape ? height * 16 / 9 : height / Dimensions.cardAspect
            Layout.maximumWidth: hero.width * 0.45
            radius: Dimensions.radiusMd
            color: Theme.backgroundSecondary
            clip: true
            Artwork {
                id: art
                anchors.fill: parent
                source: hero.game.artworkUrl || ""
                title: hero.game.title || ""
                showPlaceholderTitle: false
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Dimensions.spacingSm

            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                text: qsTr("Continue playing")
                font.family: Typography.family
                font.pixelSize: Typography.caption
                font.weight: Typography.weightSemiBold
                font.letterSpacing: Dimensions.s(1.5)
                font.capitalization: Font.AllUppercase
                color: Theme.accentPrimary
            }
            Text {
                Layout.fillWidth: true
                text: hero.game.title || ""
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                lineHeight: 1.05
                font.family: Typography.family
                font.pixelSize: Dimensions.compact ? Typography.pageTitle : Typography.display * 0.8
                font.weight: Typography.weightBold
                color: Theme.textPrimary
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingMd
                Row {
                    spacing: Dimensions.spacingXs
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "chip"
                        size: Dimensions.iconSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: hero.game.emulatorName || qsTr("No emulator assigned")
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: hero.game.emulatorName ? Theme.textSecondary : Theme.statusWarning
                    }
                }
                Row {
                    spacing: Dimensions.spacingXs
                    visible: lastPlayedText.text !== ""
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "clock"
                        size: Dimensions.iconSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        id: lastPlayedText
                        anchors.verticalCenter: parent.verticalCenter
                        readonly property string rel: GameActions.formatRelative(hero.game.lastPlayed)
                        text: rel !== "" ? qsTr("Last played %1").arg(/^[A-Z][a-z]+$/.test(rel) ? rel.toLowerCase() : rel) : ""
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        color: Theme.textSecondary
                    }
                }
                Item { Layout.fillWidth: true }
            }
            Row {
                Layout.topMargin: Dimensions.spacingLg
                spacing: Dimensions.spacingMd
                FocusButton {
                    id: launchButton
                    focus: true
                    text: Launcher.busy ? qsTr("Running…") : qsTr("Launch")
                    iconName: "play"
                    variant: "primary"
                    // Not disabled while Launcher.busy: the launch overlay covers the
                    // UI and restores focus to this button afterwards (a disabled
                    // button would lose it). Router.launchGame refuses a 2nd launch.
                    onClicked: Router.launchGame(hero.game.path)
                    KeyNavigation.right: detailsButton
                }
                FocusButton {
                    id: detailsButton
                    text: qsTr("Details")
                    iconName: "info"
                    onClicked: GameActions.openDetails(hero.game.path)
                    KeyNavigation.left: launchButton.enabled ? launchButton : null
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
