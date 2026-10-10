import QtQuick
import QtQuick.Window

// Base for all modal dialogs. While open it traps focus (every key not
// handled inside is swallowed, so nothing behind reacts), Back cancels, and
// on close focus returns to the item that was focused before opening.
// Created dynamically by the Shell; destroys itself after closing.
FocusScope {
    id: modal

    property string title: ""
    property real panelWidth: Dimensions.s(640)
    property Item initialFocusItem: null
    property Item returnFocusItem: null
    default property alias panelContent: body.data
    readonly property real maxPanelHeight: height - Dimensions.safeMargin * 4
    signal rejected()
    signal closed()

    property bool _closing: false

    anchors.fill: parent
    z: 100
    focus: true

    function close() {
        if (_closing) return
        _closing = true
        closeAnim.start()
    }
    function reject() {
        rejected()
        close()
    }

    Component.onCompleted: {
        if (!returnFocusItem && Window.window) {
            returnFocusItem = Window.window.activeFocusItem
            // Opened from a modal that is closing (e.g. menu -> confirmation):
            // return to what that modal would have returned to instead.
            for (var p = returnFocusItem; p; p = p.parent) {
                if (p !== modal && p._closing === true && p.returnFocusItem !== undefined) {
                    returnFocusItem = p.returnFocusItem
                    break
                }
            }
        }
        openAnim.start()
        modal.forceActiveFocus()
        Qt.callLater(focusInitial)   // delegates (e.g. Repeater buttons) exist by then
    }
    function focusInitial() {
        if (!_closing && initialFocusItem) initialFocusItem.forceActiveFocus()
    }

    Keys.onPressed: function(event) {
        if (Input.isBack(event)) {
            if (!event.isAutoRepeat) modal.reject()
        }
        event.accepted = true      // focus/input containment
    }
    Keys.onReleased: function(event) { event.accepted = true }

    Rectangle {
        id: scrim
        anchors.fill: parent
        color: Theme.backgroundOverlay
        opacity: 0
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onClicked: modal.reject() }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(modal.panelWidth, modal.width - Dimensions.safeMargin * 4)
        height: Math.min(body.implicitHeight + Dimensions.spacingXl * 2, modal.maxPanelHeight)
        radius: Dimensions.radiusLg
        color: Theme.backgroundElevated
        border.color: Theme.borderStrong
        border.width: 1
        opacity: 0
        scale: 0.97
        MouseArea { anchors.fill: parent }  // swallow clicks on the panel

        Item {
            id: body
            anchors.fill: parent
            anchors.margins: Dimensions.spacingXl
            implicitHeight: childrenRect.height
        }
    }

    ParallelAnimation {
        id: openAnim
        NumberAnimation { target: scrim; property: "opacity"; to: 1; duration: Motion.dialog }
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Motion.dialog }
        NumberAnimation { target: panel; property: "scale"; to: 1; duration: Motion.dialog; easing.type: Motion.easing }
    }
    SequentialAnimation {
        id: closeAnim
        ParallelAnimation {
            NumberAnimation { target: scrim; property: "opacity"; to: 0; duration: Motion.dialog * 0.7 }
            NumberAnimation { target: panel; property: "opacity"; to: 0; duration: Motion.dialog * 0.7 }
        }
        ScriptAction {
            script: {
                var target = modal.returnFocusItem
                // Don't steal focus from a modal opened meanwhile (menu -> dialog).
                var current = modal.Window.window ? modal.Window.window.activeFocusItem : null
                var focusElsewhere = false
                for (var p = current; p; p = p.parent) {
                    if (p === modal) { focusElsewhere = false; break }
                    if (p._closing === false && p.returnFocusItem !== undefined) { focusElsewhere = true; break }
                }
                modal.closed()
                modal.focus = false
                modal.visible = false
                if (target && !focusElsewhere && typeof target.forceActiveFocus === "function")
                    target.forceActiveFocus()
                modal.destroy()
            }
        }
    }
}
