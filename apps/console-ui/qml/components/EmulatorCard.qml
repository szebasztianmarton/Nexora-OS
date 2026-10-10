import QtQuick
import QtQuick.Layouts

// One row of the Emulators list. Two focus stops:
//   body          Confirm -> activated (open details), Secondary -> launchRequested
//                 (only when launchable), Options -> optionsRequested,
//                 Right -> Launch button (when shown)
//   Launch button Confirm -> launchRequested, Left -> body
// Left on the body is NOT accepted so the Shell can move focus to the sidebar.
// Status is shown as a pill with icon + text, never colour alone.
FocusScope {
    id: card

    property string name: ""
    property string emulatorId: ""
    property string status: "unknown"
    property string statusText: ""
    property var platforms: []
    property var extensions: []
    property var validationMessages: []
    property int gameCount: 0
    property bool canLaunch: false
    property bool launchAllowed: canLaunch      // false while another process runs
    property bool current: false                // ListView.isCurrentItem

    signal activated()
    signal launchRequested()
    signal optionsRequested()
    signal clicked()

    readonly property bool launchFocused: launchButton.activeFocus
    readonly property bool bodyFocused: body.activeFocus
    readonly property bool highlighted: body.activeFocus || mouse.containsMouse

    function focusBody() { body.forceActiveFocus() }

    implicitHeight: content.implicitHeight + Dimensions.spacingLg * 2
    implicitWidth: Dimensions.s(900)

    Accessible.role: Accessible.Button
    Accessible.name: name + ", " + statusText

    Rectangle {
        id: background
        anchors.fill: parent
        radius: Dimensions.radiusLg
        color: card.highlighted ? Theme.backgroundHover : Theme.backgroundSecondary
        border.width: 1
        border.color: card.current && !card.activeFocus ? Theme.accentSecondary : Theme.borderSubtle
        Behavior on color { ColorAnimation { duration: Motion.focus } }
    }
    FocusFrame { shown: body.activeFocus; radiusBase: Dimensions.radiusLg }

    // Invisible focus target for the row body (keeps key handling separate
    // from the Launch button).
    Item {
        id: body
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
            if (Input.isConfirm(event)) {
                event.accepted = true
                if (!event.isAutoRepeat) card.activated()
            } else if (Input.isSecondary(event)) {
                event.accepted = true
                if (!event.isAutoRepeat && card.canLaunch && card.launchAllowed) card.launchRequested()
            } else if (Input.isOptions(event)) {
                event.accepted = true
                if (!event.isAutoRepeat) card.optionsRequested()
            } else if (event.key === Qt.Key_Right && launchButton.visible) {
                event.accepted = true
                launchButton.forceActiveFocus()
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: function(mouseEvent) {
            card.clicked()
            body.forceActiveFocus()
            if (mouseEvent.button === Qt.RightButton) card.optionsRequested()
            else card.activated()
        }
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.leftMargin: Dimensions.spacingLg
        anchors.rightMargin: Dimensions.spacingLg
        spacing: Dimensions.spacingLg

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Dimensions.s(64)
            implicitHeight: implicitWidth
            radius: Dimensions.radiusMd
            color: Theme.backgroundElevated
            border.color: Theme.borderSubtle
            Icon {
                anchors.centerIn: parent
                name: "chip"
                size: Dimensions.iconSize
                color: card.canLaunch ? Theme.accentPrimary : Theme.textMuted
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Dimensions.spacingSm

            Row {
                id: titleRow
                Layout.fillWidth: true
                spacing: Dimensions.spacingMd
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, titleRow.width - pill.width - titleRow.spacing)
                    text: card.name
                    elide: Text.ElideRight
                    font.family: Typography.family
                    font.pixelSize: Typography.cardTitle
                    font.weight: Typography.weightSemiBold
                    color: Theme.textPrimary
                }
                StatusIndicator {
                    id: pill
                    anchors.verticalCenter: parent.verticalCenter
                    status: card.status
                    text: card.statusText
                }
            }

            Text {
                Layout.fillWidth: true
                text: card._metaLine()
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                color: Theme.textSecondary
            }

            Text {
                Layout.fillWidth: true
                visible: !card.canLaunch && text !== ""
                text: card.status === "disabled"
                      ? qsTr("Disabled in emulators.json (\"enabled\": false)")
                      : (card.validationMessages && card.validationMessages.length > 0 ? card.validationMessages[0] : "")
                elide: Text.ElideMiddle
                font.family: Typography.family
                font.pixelSize: Typography.caption
                color: Theme.textMuted
            }
        }

        FocusButton {
            id: launchButton
            Layout.alignment: Qt.AlignVCenter
            visible: card.canLaunch
            small: true
            text: qsTr("Launch")
            iconName: "play"
            variant: activeFocus ? "primary" : "secondary"
            accessibleDescription: qsTr("Start %1 without a game").arg(card.name)
            onClicked: if (card.launchAllowed) card.launchRequested()
            Keys.onLeftPressed: function(event) { event.accepted = true; body.forceActiveFocus() }
            onVisibleChanged: if (!visible && activeFocus) body.forceActiveFocus()
        }
    }

    function _metaLine() {
        var parts = []
        if (platforms && platforms.length > 0)
            parts.push(platforms.join(", "))
        if (extensions && extensions.length > 0)
            parts.push(extensions.map(function(e) { return "." + e }).join(" "))
        else
            parts.push(qsTr("No file types"))
        parts.push(gameCount === 1 ? qsTr("1 game") : gameCount > 1 ? qsTr("%1 games").arg(gameCount)
                                                                    : qsTr("No games use it yet"))
        return parts.join("  ·  ")
    }
}
