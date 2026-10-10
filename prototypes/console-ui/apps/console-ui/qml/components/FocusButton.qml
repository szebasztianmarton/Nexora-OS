import QtQuick
import QtQuick.Layouts

// Primary interactive button. Controller/keyboard: Confirm activates (auto-
// repeat is ignored so a held button never triggers twice). Mouse: click.
// variant: "primary" (accent fill) | "secondary" (elevated) | "ghost" | "danger"
FocusScope {
    id: control

    property string text: ""
    property string iconName: ""
    property string variant: "secondary"
    property bool small: false
    property bool busy: false
    property string accessibleDescription: ""
    signal clicked()

    readonly property bool highlighted: activeFocus || mouse.containsMouse
    readonly property bool _filled: variant === "primary"

    implicitHeight: small ? Dimensions.buttonHeightSmall : Dimensions.buttonHeight
    implicitWidth: Math.max(small ? Dimensions.s(96) : Dimensions.controlMinWidth,
                            row.implicitWidth + (small ? Dimensions.spacingMd : Dimensions.spacingLg) * 2)
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.45
    scale: activeFocus ? 1.03 : 1
    Behavior on scale { NumberAnimation { duration: Motion.focus; easing.type: Motion.easing } }

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: accessibleDescription
    Accessible.onPressAction: control._activate()

    property double _lastActivation: 0
    function _activate() {
        if (!enabled || busy)
            return
        var now = Date.now()
        if (now - _lastActivation < 300)
            return
        _lastActivation = now
        clicked()
    }

    Keys.onPressed: function(event) {
        if (Input.isConfirm(event)) {
            event.accepted = true
            if (!event.isAutoRepeat)
                control._activate()
        }
    }

    Rectangle {
        id: background
        anchors.fill: parent
        radius: Dimensions.radiusMd
        color: {
            if (control._filled)
                return control.highlighted ? Theme.accentHover : Theme.accentPrimary
            if (control.variant === "ghost")
                return control.highlighted ? Theme.backgroundHover : "transparent"
            if (control.variant === "danger")
                return control.highlighted ? Qt.rgba(1, 0.45, 0.5, 0.22) : Qt.rgba(1, 0.45, 0.5, 0.12)
            return control.highlighted ? Theme.backgroundHover : Theme.backgroundElevated
        }
        border.width: control._filled || control.variant === "ghost" ? 0 : 1
        border.color: control.variant === "danger" ? Theme.statusError : Theme.borderSubtle
        Behavior on color { ColorAnimation { duration: Motion.focus } }
    }

    FocusFrame { shown: control.activeFocus; radiusBase: Dimensions.radiusMd }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Dimensions.spacingSm

        LoadingIndicator {
            visible: control.busy
            size: Dimensions.iconSizeSmall
            color: control._filled ? Theme.onAccent : Theme.accentPrimary
        }
        Icon {
            visible: control.iconName !== "" && !control.busy
            name: control.iconName
            size: control.small ? Dimensions.iconSizeSmall : Dimensions.iconSize * 0.9
            color: label.color
        }
        Text {
            id: label
            text: control.text
            visible: text !== ""
            font.family: Typography.family
            font.pixelSize: control.small ? Typography.secondary : Typography.button
            font.weight: control._filled ? Typography.weightSemiBold : Typography.weightMedium
            color: control._filled ? Theme.onAccent
                 : control.variant === "danger" ? Theme.statusError
                 : (control.highlighted ? Theme.textPrimary : Theme.textSecondary)
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { control.forceActiveFocus(); control._activate() }
    }
}
