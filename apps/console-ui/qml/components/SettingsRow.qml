import QtQuick
import QtQuick.Layouts

// Base for the rows used by the Settings category pages: label + optional
// description on the left, a trailing slot on the right. The whole row is a
// single focus target (no horizontal focus movement inside rows), so Up/Down
// always moves between rows. `interactive: false` rows are read-only and are
// skipped by SettingsCategoryPage's focus traversal.
FocusScope {
    id: row

    property string label: ""
    property string description: ""
    property bool interactive: true
    // Hint label shown in the bottom bar for Confirm while this row is focused.
    property string settingsHint: qsTr("Select")
    readonly property bool settingsFocusable: interactive && enabled && visible
    property bool danger: false
    property real trailingMaxWidth: width * 0.55
    default property alias trailing: trailingSlot.data
    readonly property alias trailingItem: trailingSlot
    signal activated()

    width: parent ? parent.width : implicitWidth
    implicitWidth: Dimensions.s(600)
    implicitHeight: Math.max(Dimensions.rowHeight, textColumn.implicitHeight + Dimensions.spacingMd * 2,
                             trailingSlot.height + Dimensions.spacingMd * 2)
    activeFocusOnTab: interactive
    opacity: enabled ? 1 : 0.5

    Accessible.role: interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: label
    Accessible.description: description
    Accessible.onPressAction: if (interactive) row.activated()

    // Never leave focus on a row that just became read-only or hidden.
    onSettingsFocusableChanged: {
        if (settingsFocusable || !activeFocus) return
        for (var p = parent; p; p = p.parent) {
            if (typeof p.focusFirst === "function") { p.focusFirst(); return }
        }
    }

    Keys.onPressed: function(event) {
        if (row.interactive && Input.isConfirm(event)) {
            event.accepted = true
            if (!event.isAutoRepeat) row.activated()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusMd
        color: row.activeFocus ? Theme.backgroundHover
             : (row.interactive && mouse.containsMouse ? Theme.backgroundElevated : Theme.backgroundSecondary)
        border.width: 1
        border.color: row.activeFocus ? Theme.borderStrong : Theme.borderSubtle
        Behavior on color { ColorAnimation { duration: Motion.focus } }
    }
    FocusFrame { shown: row.activeFocus }

    ColumnLayout {
        id: textColumn
        anchors.left: parent.left
        anchors.leftMargin: Dimensions.spacingLg
        anchors.right: trailingSlot.left
        anchors.rightMargin: Dimensions.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        spacing: Dimensions.s(3)
        Text {
            Layout.fillWidth: true
            text: row.label
            visible: text !== ""
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.body
            font.weight: Typography.weightMedium
            color: row.danger ? Theme.statusError : Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            text: row.description
            visible: text !== ""
            wrapMode: Text.WordWrap
            font.family: Typography.family
            font.pixelSize: Typography.caption
            color: Theme.textMuted
            lineHeight: 1.1
        }
    }

    Item {
        id: trailingSlot
        anchors.right: parent.right
        anchors.rightMargin: Dimensions.spacingLg
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(childrenRect.width, row.trailingMaxWidth)
        height: childrenRect.height
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: row.interactive
        enabled: row.interactive
        cursorShape: Qt.PointingHandCursor
        onClicked: { row.forceActiveFocus(); row.activated() }
    }
}
