import QtQuick
import Nexora.Core

// Live controller input test for Settings › Controller.
// While the Settings › Controller page is open, every physical button and
// axis reported by ControllerManager lights up here (raw SDL input, so it
// works for every controller type). Because controller buttons also drive
// navigation, an explicit test mode is available: press Confirm on the tester
// to capture all input (nothing navigates), press Back twice to stop.
// The last LOGICAL action received (from keyboard or controller) is shown too.
FocusScope {
    id: tester

    property bool testing: false
    property var pressed: ({})
    property var axes: ({ leftx: 0, lefty: 0, rightx: 0, righty: 0, lefttrigger: 0, righttrigger: 0 })
    property string lastAction: ""
    property string lastButton: ""
    property double _lastBack: 0
    property bool _backArmed: false

    readonly property bool settingsFocusable: visible && enabled
    readonly property string settingsHint: testing ? "" : qsTr("Start test")
    readonly property string settingsBackHint: testing ? qsTr("Press twice to stop test") : ""

    readonly property string padType: Controllers.count > 0 && Controllers.controllers[0].type
                                      ? Controllers.controllers[0].type : Controllers.controllerType
    function faceLabel(button) {
        var map = {
            xbox: { south: "A", east: "B", west: "X", north: "Y" },
            playstation: { south: "✕", east: "○", west: "□", north: "△" },
            nintendo: { south: "B", east: "A", west: "Y", north: "X" }
        }
        var m = map[padType]
        return m ? m[button] : { south: "S", east: "E", west: "W", north: "N" }[button]
    }

    width: parent ? parent.width : implicitWidth
    implicitWidth: Dimensions.s(760)
    implicitHeight: header.height + pad.height * pad.scale + Dimensions.spacingLg * 3
    activeFocusOnTab: true
    onActiveFocusChanged: if (!activeFocus) { testing = false; _backArmed = false }
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Controller input test")

    Connections {
        target: Controllers
        enabled: tester.visible
        function onButtonEvent(button, isPressed) {
            var p = Object.assign({}, tester.pressed)
            p[button] = isPressed
            tester.pressed = p
            if (isPressed) tester.lastButton = button
        }
        function onAxisEvent(axis, value) {
            var a = Object.assign({}, tester.axes)
            a[axis] = value
            tester.axes = a
        }
    }

    function actionName(event) {
        if (Input.isConfirm(event)) return qsTr("Confirm")
        if (Input.isBack(event)) return qsTr("Back")
        if (Input.isSecondary(event)) return qsTr("Secondary")
        if (Input.isSearch(event)) return qsTr("Search")
        if (Input.isOptions(event)) return qsTr("Options")
        if (Input.isPrevTab(event)) return qsTr("Previous tab")
        if (Input.isNextTab(event)) return qsTr("Next tab")
        switch (event.key) {
        case Qt.Key_Up: return qsTr("Up")
        case Qt.Key_Down: return qsTr("Down")
        case Qt.Key_Left: return qsTr("Left")
        case Qt.Key_Right: return qsTr("Right")
        }
        return ""
    }

    Timer { id: backTimer; interval: 2000; onTriggered: tester._backArmed = false }

    Keys.onPressed: function(event) {
        if (!testing) {
            if (Input.isConfirm(event)) {
                event.accepted = true
                if (!event.isAutoRepeat) { testing = true; lastAction = "" }
            }
            return
        }
        event.accepted = true          // capture everything while testing
        var name = actionName(event)
        if (name !== "") lastAction = name
        if (Input.isBack(event) && !event.isAutoRepeat) {
            if (_backArmed) { testing = false; _backArmed = false; backTimer.stop() }
            else { _backArmed = true; backTimer.restart() }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusMd
        color: tester.activeFocus ? Theme.backgroundHover : Theme.backgroundSecondary
        border.width: tester.testing ? Dimensions.focusWidth : 1
        border.color: tester.testing ? Theme.accentSecondary : Theme.borderSubtle
    }
    FocusFrame { shown: tester.activeFocus && !tester.testing }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Dimensions.spacingLg
        height: Math.max(statusColumn.implicitHeight, modePill.height)
        Column {
            id: statusColumn
            anchors.left: parent.left
            anchors.right: modePill.left
            anchors.rightMargin: Dimensions.spacingMd
            spacing: Dimensions.s(3)
            Text {
                width: parent.width
                text: tester.testing ? qsTr("Test mode — navigation is paused")
                                     : qsTr("Input test")
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.body
                font.weight: Typography.weightMedium
                color: Theme.textPrimary
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: tester._backArmed ? qsTr("Press Back again to stop testing.")
                     : tester.testing ? qsTr("Press any button. Press Back twice to stop testing.")
                     : qsTr("Buttons and sticks light up as you use them. Press Confirm here to start a test mode in which buttons do not navigate.")
                font.family: Typography.family
                font.pixelSize: Typography.caption
                color: tester._backArmed ? Theme.statusWarning : Theme.textMuted
            }
            Text {
                width: parent.width
                visible: tester.testing
                text: qsTr("Last action received: %1").arg(tester.lastAction !== "" ? tester.lastAction : qsTr("none yet"))
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                font.weight: Typography.weightSemiBold
                color: Theme.accentPrimary
            }
        }
        Rectangle {
            id: modePill
            anchors.right: parent.right
            anchors.top: parent.top
            height: Dimensions.buttonHeightSmall
            width: pillText.implicitWidth + Dimensions.spacingLg * 2
            radius: Dimensions.radiusMd
            color: tester.testing ? Theme.accentSecondarySoft : (tester.activeFocus ? Theme.accentPrimary : Theme.backgroundElevated)
            border.width: 1
            border.color: tester.testing ? Theme.accentSecondary : Theme.borderStrong
            Text {
                id: pillText
                anchors.centerIn: parent
                text: tester.testing ? qsTr("Testing") : qsTr("Start test")
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                font.weight: Typography.weightSemiBold
                color: tester.testing ? Theme.textPrimary : (tester.activeFocus ? Theme.onAccent : Theme.textPrimary)
            }
        }
    }

    // ---- controller drawing (reference 760 x 250) --------------------------
    component PadButton: Rectangle {
        property string button: ""
        property string text: ""
        readonly property bool down: tester.pressed[button] === true
        width: Dimensions.s(44); height: width
        radius: width / 2
        color: down ? Theme.accentPrimary : Theme.backgroundElevated
        border.width: 1
        border.color: down ? Theme.accentHover : Theme.borderStrong
        Text {
            anchors.centerIn: parent
            text: parent.text
            font.family: Typography.family
            font.pixelSize: Typography.caption
            font.weight: Typography.weightSemiBold
            color: parent.down ? Theme.onAccent : Theme.textSecondary
        }
    }
    component Stick: Rectangle {
        property string button: ""
        property real ax: 0
        property real ay: 0
        readonly property bool down: tester.pressed[button] === true
        width: Dimensions.s(92); height: width
        radius: width / 2
        color: Theme.backgroundPrimary
        border.width: Math.max(1, Dimensions.s(2))
        border.color: down ? Theme.accentPrimary : Theme.borderStrong
        Rectangle {
            width: parent.width * 0.38; height: width
            radius: width / 2
            x: (parent.width - width) / 2 + parent.ax * (parent.width - width) / 2
            y: (parent.height - height) / 2 + parent.ay * (parent.height - height) / 2
            color: parent.down ? Theme.accentPrimary
                 : (Math.abs(parent.ax) > 0.15 || Math.abs(parent.ay) > 0.15 ? Theme.accentSecondary : Theme.textMuted)
        }
    }
    component Trigger: Item {
        property string label: ""
        property real value: 0
        property string shoulder: ""
        readonly property bool shoulderDown: tester.pressed[shoulder] === true
        width: Dimensions.s(150); height: Dimensions.s(58)
        Rectangle {
            id: shoulderRect
            width: parent.width; height: Dimensions.s(26)
            radius: Dimensions.radiusSm
            color: parent.shoulderDown ? Theme.accentPrimary : Theme.backgroundElevated
            border.width: 1
            border.color: Theme.borderStrong
            Text {
                anchors.centerIn: parent
                text: parent.parent.label
                font.family: Typography.family
                font.pixelSize: Typography.caption
                font.weight: Typography.weightSemiBold
                color: parent.parent.shoulderDown ? Theme.onAccent : Theme.textSecondary
            }
        }
        Rectangle {
            anchors.top: shoulderRect.bottom
            anchors.topMargin: Dimensions.s(8)
            width: parent.width; height: Dimensions.s(14)
            radius: height / 2
            color: Theme.backgroundPrimary
            border.width: 1
            border.color: Theme.borderSubtle
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, parent.parent.value))
                height: parent.height
                radius: height / 2
                color: Theme.accentSecondary
            }
        }
    }

    Item {
        id: pad
        anchors.top: header.bottom
        anchors.topMargin: Dimensions.spacingLg
        anchors.horizontalCenter: parent.horizontalCenter
        width: Dimensions.s(760)
        height: Dimensions.s(250)
        scale: Math.min(1, (tester.width - Dimensions.spacingLg * 2) / width)
        transformOrigin: Item.Top

        Trigger { x: Dimensions.s(40); y: 0; label: qsTr("LB · LT"); shoulder: "leftshoulder"; value: tester.axes.lefttrigger }
        Trigger { x: parent.width - width - Dimensions.s(40); y: 0; label: qsTr("RB · RT"); shoulder: "rightshoulder"; value: tester.axes.righttrigger }

        // D-pad
        Item {
            x: Dimensions.s(40); y: Dimensions.s(84)
            width: Dimensions.s(140); height: width
            PadButton { button: "dpup"; text: "▲"; radius: Dimensions.radiusSm; x: (parent.width - width) / 2; y: 0 }
            PadButton { button: "dpdown"; text: "▼"; radius: Dimensions.radiusSm; x: (parent.width - width) / 2; y: parent.height - height }
            PadButton { button: "dpleft"; text: "◀"; radius: Dimensions.radiusSm; x: 0; y: (parent.height - height) / 2 }
            PadButton { button: "dpright"; text: "▶"; radius: Dimensions.radiusSm; x: parent.width - width; y: (parent.height - height) / 2 }
        }
        Stick { button: "leftstick"; x: Dimensions.s(214); y: Dimensions.s(152); ax: tester.axes.leftx; ay: tester.axes.lefty }

        // Centre buttons
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Dimensions.s(84)
            spacing: Dimensions.spacingMd
            PadButton { button: "back"; text: qsTr("Back"); width: Dimensions.s(70); height: Dimensions.s(32); radius: height / 2 }
            PadButton { button: "guide"; text: qsTr("Home"); width: Dimensions.s(70); height: Dimensions.s(32); radius: height / 2 }
            PadButton { button: "start"; text: qsTr("Start"); width: Dimensions.s(70); height: Dimensions.s(32); radius: height / 2 }
        }

        Stick { button: "rightstick"; x: parent.width - width - Dimensions.s(214); y: Dimensions.s(152); ax: tester.axes.rightx; ay: tester.axes.righty }
        // Face buttons
        Item {
            x: parent.width - width - Dimensions.s(40); y: Dimensions.s(80)
            width: Dimensions.s(148); height: width
            PadButton { button: "north"; text: tester.faceLabel("north"); x: (parent.width - width) / 2; y: 0 }
            PadButton { button: "south"; text: tester.faceLabel("south"); x: (parent.width - width) / 2; y: parent.height - height }
            PadButton { button: "west"; text: tester.faceLabel("west"); x: 0; y: (parent.height - height) / 2 }
            PadButton { button: "east"; text: tester.faceLabel("east"); x: parent.width - width; y: (parent.height - height) / 2 }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: tester.forceActiveFocus()
    }
}
