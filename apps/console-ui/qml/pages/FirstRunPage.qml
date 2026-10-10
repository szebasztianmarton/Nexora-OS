import QtQuick
import QtQuick.Layouts
import Nexora.Core

// First-run setup (full screen, no Shell/Router available). Five steps:
//   1 Welcome  2 Controller  3 Emulators  4 Game library  5 Finish
// Nothing here is required: every step can be continued or skipped, and all
// settings remain available later in Settings. Back goes to the previous step
// (does nothing on step 1). Main.qml persists completion on finished().
FocusScope {
    id: root

    signal finished()

    property int step: 0
    readonly property int stepCount: 5
    readonly property var stepNames: [qsTr("Welcome"), qsTr("Controller"), qsTr("Emulators"), qsTr("Game library"), qsTr("Finish")]
    readonly property var steps: [welcomeStep, controllerStep, emulatorsStep, libraryStep, finishStep]
    readonly property bool dialogOpen: _dialog !== null
    property var _dialog: null
    property string libraryMessage: ""
    property string emulatorMessage: ""

    function goTo(index) {
        controllerStep.testing = false
        step = Math.max(0, Math.min(stepCount - 1, index))
        Qt.callLater(focusStep)
    }
    function next() { goTo(step + 1) }
    function previous() { if (step > 0) goTo(step - 1) }
    function focusStep() {
        var s = steps[step]
        if (s && s.defaultItem) s.defaultItem.forceActiveFocus()
    }

    function openFolderBrowser() {
        if (_dialog) return
        libraryMessage = ""
        var dlg = folderDialogComponent.createObject(root, {})
        if (!dlg) { console.warn("Nexora: failed to create folder dialog", folderDialogComponent.errorString()); return }
        _dialog = dlg
        dlg.folderChosen.connect(function(path) { root.libraryMessage = qsTr("Added %1").arg(path) })
        dlg.closed.connect(function() { root._dialog = null })
    }

    Component { id: folderDialogComponent; FolderBrowserDialog {} }

    // Main.qml gives the root active focus; hand it to the current step.
    onActiveFocusChanged: if (activeFocus && !dialogOpen) Qt.callLater(focusStep)
    Component.onCompleted: Qt.callLater(focusStep)

    Keys.onPressed: function(event) {
        if (Input.isBack(event)) {
            event.accepted = true
            if (!event.isAutoRepeat) root.previous()
        }
    }

    // ---- background ----------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: Theme.backgroundPrimary
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.accentSecondarySoft }
                GradientStop { position: 0.55; color: "transparent" }
            }
            opacity: 0.6
        }
        Rectangle {
            width: parent.width * 0.6; height: width
            radius: width / 2
            x: -width * 0.35; y: parent.height - height * 0.4
            color: Theme.accentPrimary
            opacity: 0.03
        }
    }

    // ---- step indicator ------------------------------------------------------
    Item {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Dimensions.safeMargin + Dimensions.spacingLg
        anchors.leftMargin: Dimensions.pagePaddingH
        anchors.rightMargin: Dimensions.pagePaddingH
        height: Math.max(indicator.implicitHeight, smallLogo.implicitHeight)

        NexoraLogo {
            id: smallLogo
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            markSize: Dimensions.s(34)
            visible: !Dimensions.compact
            opacity: root.step === 0 ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: Motion.page } }
        }

        Row {
            id: indicator
            anchors.centerIn: parent
            spacing: Dimensions.spacingSm
            Accessible.role: Accessible.ProgressBar
            Accessible.name: qsTr("Step %1 of %2: %3").arg(root.step + 1).arg(root.stepCount).arg(root.stepNames[root.step])
            Repeater {
                model: root.stepCount
                delegate: Row {
                    id: stepItem
                    required property int index
                    readonly property bool done: index < root.step
                    readonly property bool current: index === root.step
                    spacing: Dimensions.spacingSm
                    Rectangle {
                        visible: stepItem.index > 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: Dimensions.s(36); height: Math.max(1, Dimensions.s(2))
                        color: stepItem.done || stepItem.current ? Theme.accentPrimary : Theme.borderStrong
                        opacity: 0.7
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Dimensions.s(34); height: width; radius: width / 2
                        color: stepItem.current ? Theme.accentPrimary : stepItem.done ? Theme.accentPrimarySoft : Theme.backgroundElevated
                        border.width: 1
                        border.color: stepItem.current || stepItem.done ? Theme.accentPrimary : Theme.borderStrong
                        Text {
                            anchors.centerIn: parent
                            visible: !stepItem.done
                            text: stepItem.index + 1
                            font.family: Typography.family
                            font.pixelSize: Typography.caption
                            font.weight: Typography.weightBold
                            color: stepItem.current ? Theme.onAccent : Theme.textSecondary
                        }
                        Icon {
                            anchors.centerIn: parent
                            visible: stepItem.done
                            name: "check"
                            size: Dimensions.iconSizeSmall * 0.85
                            color: Theme.accentPrimary
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.stepNames[stepItem.index]
                        visible: stepItem.current || !Dimensions.compact
                        font.family: Typography.family
                        font.pixelSize: Typography.secondary
                        font.weight: stepItem.current ? Typography.weightSemiBold : Typography.weightRegular
                        color: stepItem.current ? Theme.textPrimary : stepItem.done ? Theme.textSecondary : Theme.textMuted
                    }
                }
            }
        }
    }

    // ---- panel ---------------------------------------------------------------
    Rectangle {
        id: panel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: (topBar.height - hints.height) / 2
        width: Math.min(Dimensions.s(1180), parent.width - Dimensions.pagePaddingH * 2)
        height: Math.min(stepColumn.implicitHeight + Dimensions.spacingXl * 2,
                         parent.height - topBar.height - hints.height - Dimensions.safeMargin * 2 - Dimensions.spacingXl * 2)
        radius: Dimensions.radiusLg
        color: Qt.rgba(Theme.backgroundSecondary.r, Theme.backgroundSecondary.g, Theme.backgroundSecondary.b, 0.92)
        border.color: Theme.borderSubtle
        border.width: 1
        clip: true

        Column {
            id: stepColumn
            x: Dimensions.spacingXl + Dimensions.spacingMd
            y: Dimensions.spacingXl
            width: parent.width - x * 2

            // 1 ---- Welcome
            StepScope {
                id: welcomeStep
                index: 0
                defaultItem: welcomeContinue
                ColumnLayout {
                    width: parent.width
                    spacing: Dimensions.spacingLg
                    NexoraLogo {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: Dimensions.spacingMd
                        markSize: Dimensions.s(96)
                    }
                    StepTitle {
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Welcome to Nexora")
                        font.pixelSize: Typography.display
                    }
                    StepText {
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("A console-style home for the games and emulators you already have. Nexora organises your own game files and starts them with your installed emulators — it doesn't provide or download any games.")
                    }
                    StepText {
                        horizontalAlignment: Text.AlignHCenter
                        muted: true
                        text: qsTr("A few quick checks follow. Everything is optional and can be changed later in Settings.")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Dimensions.spacingLg
                        Item { Layout.fillWidth: true }
                        FocusButton {
                            id: welcomeContinue
                            variant: "primary"
                            text: qsTr("Continue")
                            iconName: "chevron-right"
                            onClicked: root.next()
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // 2 ---- Controller
            StepScope {
                id: controllerStep
                index: 1
                property bool testing: false
                property var pressed: ({})
                property var seen: ({})
                defaultItem: testing ? testArea : controllerNav.continueButton

                onTestingChanged: {
                    pressed = ({}); seen = ({})
                    Qt.callLater(root.focusStep)
                }
                Connections {
                    target: Controllers
                    enabled: controllerStep.testing
                    function onButtonEvent(button, isPressed) {
                        var p = Object.assign({}, controllerStep.pressed)
                        p[button] = isPressed
                        controllerStep.pressed = p
                        if (isPressed) {
                            var s = Object.assign({}, controllerStep.seen)
                            s[button] = true
                            controllerStep.seen = s
                        }
                    }
                }
                Connections {
                    target: Controllers
                    function onCountChanged() { if (Controllers.count === 0) controllerStep.testing = false }
                }

                ColumnLayout {
                    width: parent.width
                    spacing: Dimensions.spacingLg
                    StepHeading { iconName: "gamepad"; text: qsTr("Controller") }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingMd
                        StatusIndicator {
                            status: Controllers.count > 0 ? "success" : (Controllers.available ? "neutral" : "warning")
                            text: Controllers.count > 0
                                  ? (Controllers.count === 1 ? qsTr("1 controller connected") : qsTr("%1 controllers connected").arg(Controllers.count))
                                  : Controllers.available ? qsTr("No controller detected") : qsTr("Controller support unavailable")
                        }
                        Item { Layout.fillWidth: true }
                    }
                    StepText {
                        visible: Controllers.count > 0
                        text: {
                            var names = []
                            var list = Controllers.controllers
                            for (var i = 0; i < list.length; ++i) names.push(list[i].name)
                            return names.join("\n")
                        }
                    }
                    StepText {
                        visible: Controllers.count === 0
                        text: qsTr("No controller detected — you can use the keyboard (arrows, Enter, Esc). Connect a controller at any time; it is picked up automatically.")
                    }
                    StepText {
                        visible: !Controllers.available && Controllers.unavailableReason !== ""
                        muted: true
                        text: Controllers.unavailableReason
                    }

                    // Optional live input test
                    FocusScope {
                        id: testArea
                        Layout.fillWidth: true
                        visible: controllerStep.testing
                        implicitHeight: testColumn.implicitHeight + Dimensions.spacingLg * 2
                        Keys.onPressed: function(event) {
                            // Swallow translated button presses so testing doesn't navigate.
                            event.accepted = true
                            if (Input.isBack(event) && !event.isAutoRepeat) controllerStep.testing = false
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: Dimensions.radiusMd
                            color: Theme.backgroundElevated
                            border.color: testArea.activeFocus ? Theme.accentPrimary : Theme.borderSubtle
                        }
                        ColumnLayout {
                            id: testColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Dimensions.spacingLg
                            spacing: Dimensions.spacingMd
                            StepText {
                                text: qsTr("Press buttons on your controller — they light up here. Press B / Circle (or Esc) to finish the test.")
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingSm
                                Repeater {
                                    model: [
                                        { id: "south", label: qsTr("A / Cross") }, { id: "east", label: qsTr("B / Circle") },
                                        { id: "west", label: qsTr("X / Square") }, { id: "north", label: qsTr("Y / Triangle") },
                                        { id: "dpup", label: qsTr("D-pad up") }, { id: "dpdown", label: qsTr("D-pad down") },
                                        { id: "dpleft", label: qsTr("D-pad left") }, { id: "dpright", label: qsTr("D-pad right") },
                                        { id: "leftshoulder", label: qsTr("LB / L1") }, { id: "rightshoulder", label: qsTr("RB / R1") },
                                        { id: "start", label: qsTr("Start") }, { id: "back", label: qsTr("Select / Back") },
                                        { id: "leftstick", label: qsTr("Left stick") }, { id: "rightstick", label: qsTr("Right stick") },
                                        { id: "guide", label: qsTr("Guide") }
                                    ]
                                    delegate: Rectangle {
                                        id: chip
                                        required property var modelData
                                        readonly property bool down: !!controllerStep.pressed[modelData.id]
                                        readonly property bool wasSeen: !!controllerStep.seen[modelData.id]
                                        implicitWidth: chipRow.implicitWidth + Dimensions.spacingMd * 2
                                        implicitHeight: Dimensions.s(40)
                                        radius: height / 2
                                        color: down ? Theme.accentPrimary : wasSeen ? Theme.accentPrimarySoft : Theme.backgroundSecondary
                                        border.color: down || wasSeen ? Theme.accentPrimary : Theme.borderSubtle
                                        Row {
                                            id: chipRow
                                            anchors.centerIn: parent
                                            spacing: Dimensions.spacingXs
                                            Icon {
                                                visible: chip.wasSeen
                                                anchors.verticalCenter: parent.verticalCenter
                                                name: "check"
                                                size: Dimensions.iconSizeSmall * 0.8
                                                color: chip.down ? Theme.onAccent : Theme.accentPrimary
                                            }
                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: chip.modelData.label
                                                font.family: Typography.family
                                                font.pixelSize: Typography.caption
                                                font.weight: Typography.weightSemiBold
                                                color: chip.down ? Theme.onAccent : Theme.textPrimary
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    StepNav {
                        id: controllerNav
                        visible: !controllerStep.testing
                        showSkip: false
                        FocusButton {
                            id: testButton
                            visible: Controllers.count > 0
                            text: qsTr("Test buttons")
                            iconName: "gamepad"
                            onClicked: controllerStep.testing = true
                            KeyNavigation.left: controllerNav.backButton
                            KeyNavigation.right: controllerNav.continueButton
                        }
                    }
                }
            }

            // 3 ---- Emulators
            StepScope {
                id: emulatorsStep
                index: 2
                defaultItem: emulatorsNav.continueButton
                ColumnLayout {
                    width: parent.width
                    spacing: Dimensions.spacingLg
                    StepHeading { iconName: "chip"; text: qsTr("Emulators") }
                    StepText {
                        text: qsTr("Nexora starts games with emulators already installed on this system. They are listed in a configuration file that you edit with a text editor — nothing has to be installed now.")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingMd
                        StatusIndicator {
                            status: Emulators.loadError !== "" ? "error"
                                  : Emulators.availableCount > 0 ? "success"
                                  : Emulators.count > 0 ? "warning" : "neutral"
                            text: Emulators.loadError !== "" ? qsTr("Configuration couldn't be read")
                                : !Emulators.configExists ? qsTr("No configuration file yet")
                                : qsTr("%1 configured · %2 available").arg(Emulators.count).arg(Emulators.availableCount)
                        }
                        Item { Layout.fillWidth: true }
                    }
                    // Compact list of configured emulators
                    Column {
                        Layout.fillWidth: true
                        visible: Emulators.count > 0
                        spacing: Dimensions.spacingXs
                        Repeater {
                            model: Emulators
                            delegate: RowLayout {
                                required property string name
                                required property string status
                                required property string statusText
                                required property int index
                                visible: index < 5
                                width: parent.width
                                spacing: Dimensions.spacingMd
                                Text {
                                    Layout.fillWidth: true
                                    text: name
                                    elide: Text.ElideRight
                                    font.family: Typography.family
                                    font.pixelSize: Typography.body
                                    color: Theme.textPrimary
                                }
                                StatusIndicator { status: parent.status; text: parent.statusText }
                            }
                        }
                        StepText {
                            visible: Emulators.count > 5
                            muted: true
                            text: qsTr("and %1 more").arg(Emulators.count - 5)
                        }
                    }
                    StepText {
                        visible: Emulators.loadError !== ""
                        text: Emulators.loadError
                        color: Theme.statusError
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: pathColumn.implicitHeight + Dimensions.spacingMd * 2
                        radius: Dimensions.radiusSm
                        color: Theme.backgroundElevated
                        border.color: Theme.borderSubtle
                        ColumnLayout {
                            id: pathColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Dimensions.spacingMd
                            spacing: Dimensions.spacingXs
                            StepText { muted: true; text: qsTr("Configuration file (emulators.json)"); font.pixelSize: Typography.caption }
                            Text {
                                Layout.fillWidth: true
                                text: Emulators.configPath
                                wrapMode: Text.WrapAnywhere
                                font.family: "monospace"
                                font.pixelSize: Typography.secondary
                                color: Theme.textPrimary
                            }
                        }
                    }
                    StepText {
                        visible: root.emulatorMessage !== ""
                        muted: true
                        text: root.emulatorMessage
                    }
                    StepNav {
                        id: emulatorsNav
                        showSkip: false
                        FocusButton {
                            id: emuReload
                            text: qsTr("Reload")
                            iconName: "refresh"
                            onClicked: {
                                Emulators.reload()
                                root.emulatorMessage = Emulators.loadError !== "" ? qsTr("Reloaded — the file still has a problem.")
                                                     : !Emulators.configExists ? qsTr("Reloaded — no emulators.json found yet.")
                                                     : qsTr("Reloaded at %1.").arg(Qt.formatTime(new Date(), Qt.locale().timeFormat(Locale.ShortFormat)))
                            }
                            KeyNavigation.left: emulatorsNav.backButton
                            KeyNavigation.right: emulatorsNav.continueButton
                        }
                    }
                }
            }

            // 4 ---- Game library
            StepScope {
                id: libraryStep
                index: 3
                defaultItem: addFolderButton
                readonly property var dirs: Settings.libraryDirectories
                ColumnLayout {
                    width: parent.width
                    spacing: Dimensions.spacingLg
                    StepHeading { iconName: "library"; text: qsTr("Game library") }
                    StepText {
                        text: qsTr("Choose the folders that contain your game files. Nexora only organises files you supply — it never provides or downloads games, and it only looks inside the folders you add.")
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: dirColumn.implicitHeight + Dimensions.spacingMd * 2
                        radius: Dimensions.radiusSm
                        color: Theme.backgroundElevated
                        border.color: Theme.borderSubtle
                        ColumnLayout {
                            id: dirColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Dimensions.spacingMd
                            spacing: Dimensions.spacingXs
                            StepText {
                                visible: libraryStep.dirs.length === 0
                                muted: true
                                text: qsTr("No folders added yet.")
                            }
                            Repeater {
                                model: libraryStep.dirs.slice(0, 4)
                                delegate: RowLayout {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    spacing: Dimensions.spacingSm
                                    Icon { name: "folder"; size: Dimensions.iconSizeSmall; color: Theme.accentPrimary }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData
                                        elide: Text.ElideMiddle
                                        font.family: "monospace"
                                        font.pixelSize: Typography.secondary
                                        color: Theme.textPrimary
                                    }
                                }
                            }
                            StepText {
                                visible: libraryStep.dirs.length > 4
                                muted: true
                                text: qsTr("and %1 more").arg(libraryStep.dirs.length - 4)
                            }
                        }
                    }
                    StepText {
                        visible: libraryStep.dirs.length > 0
                        muted: true
                        text: Library.scanning ? qsTr("Scanning folders…")
                              : Library.model.count === 1 ? qsTr("1 game found so far.")
                              : qsTr("%1 games found so far. Only files matching a configured emulator are listed.").arg(Library.model.count)
                    }
                    StepText {
                        visible: root.libraryMessage !== ""
                        text: root.libraryMessage
                        color: Theme.statusSuccess
                    }
                    StepNav {
                        id: libraryNav
                        showSkip: libraryStep.dirs.length === 0
                        FocusButton {
                            id: addFolderButton
                            text: qsTr("Add folder")
                            iconName: "folder-plus"
                            onClicked: root.openFolderBrowser()
                            KeyNavigation.left: libraryNav.backButton
                            KeyNavigation.right: libraryNav.showSkip ? libraryNav.skipButton : libraryNav.continueButton
                        }
                    }
                }
            }

            // 5 ---- Finish
            StepScope {
                id: finishStep
                index: 4
                defaultItem: startButton
                ColumnLayout {
                    width: parent.width
                    spacing: Dimensions.spacingLg
                    StepHeading { iconName: "success"; text: qsTr("You're all set") }
                    StepText { text: qsTr("Here's what Nexora found. Anything missing can be set up later.") }

                    SummaryRow {
                        label: qsTr("Controller")
                        ok: Controllers.count > 0
                        value: Controllers.count > 0
                               ? (Controllers.count === 1 ? qsTr("1 detected") : qsTr("%1 detected").arg(Controllers.count))
                               : qsTr("None")
                        note: Controllers.count > 0 ? "" : qsTr("The keyboard works everywhere. Controllers connect automatically; see Settings › Controller.")
                    }
                    SummaryRow {
                        label: qsTr("Emulators")
                        ok: Emulators.availableCount > 0
                        value: Emulators.availableCount > 0
                               ? qsTr("%1 available").arg(Emulators.availableCount)
                               : qsTr("None available")
                        note: Emulators.availableCount > 0 ? ""
                              : qsTr("Add emulators to emulators.json, then reload them in the Emulators section.")
                    }
                    SummaryRow {
                        label: qsTr("Game library")
                        ok: Settings.libraryDirectories.length > 0
                        value: Settings.libraryDirectories.length === 1 ? qsTr("1 folder")
                             : Settings.libraryDirectories.length > 1 ? qsTr("%1 folders").arg(Settings.libraryDirectories.length)
                             : qsTr("No folders")
                        note: Settings.libraryDirectories.length > 0 ? "" : qsTr("Add game folders later in Settings › Game Library.")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Dimensions.spacingMd
                        spacing: Dimensions.spacingMd
                        FocusButton {
                            id: finishBack
                            text: qsTr("Back")
                            iconName: "chevron-left"
                            variant: "ghost"
                            onClicked: root.previous()
                            KeyNavigation.right: startButton
                        }
                        Item { Layout.fillWidth: true }
                        FocusButton {
                            id: startButton
                            variant: "primary"
                            text: qsTr("Start Gaming")
                            iconName: "play"
                            onClicked: root.finished()
                            KeyNavigation.left: finishBack
                        }
                    }
                }
            }
        }
    }

    ControllerHints {
        id: hints
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Dimensions.pagePaddingH
        anchors.rightMargin: Dimensions.pagePaddingH
        anchors.bottomMargin: Dimensions.safeMargin
        visible: !root.dialogOpen
        hints: {
            if (controllerStep.testing)
                return [{ action: "back", label: qsTr("End test") }]
            var h = [{ action: "confirm", label: qsTr("Select") }]
            if (root.step > 0) h.push({ action: "back", label: qsTr("Previous step") })
            return h
        }
    }

    // ---- inline components -----------------------------------------------------
    component StepScope: FocusScope {
        property int index: 0
        property Item defaultItem: null
        width: parent ? parent.width : 0
        height: visible && children.length > 0 ? children[0].implicitHeight : 0
        visible: root.step === index
        focus: visible
    }
    component StepTitle: Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.family: Typography.family
        font.pixelSize: Typography.pageTitle
        font.weight: Typography.weightBold
        color: Theme.textPrimary
    }
    component StepHeading: RowLayout {
        id: heading
        property string iconName: "info"
        property string text: ""
        Layout.fillWidth: true
        spacing: Dimensions.spacingMd
        Rectangle {
            implicitWidth: Dimensions.s(56); implicitHeight: implicitWidth
            radius: Dimensions.radiusMd
            color: Theme.accentPrimarySoft
            Icon { anchors.centerIn: parent; name: heading.iconName; color: Theme.accentPrimary; size: Dimensions.iconSize }
        }
        Text {
            Layout.fillWidth: true
            text: heading.text
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.pageTitle
            font.weight: Typography.weightBold
            color: Theme.textPrimary
        }
    }
    component StepText: Text {
        property bool muted: false
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.family: Typography.family
        font.pixelSize: muted ? Typography.secondary : Typography.body
        color: muted ? Theme.textMuted : Theme.textSecondary
        lineHeight: 1.15
    }
    // Back / [extra buttons] / Skip / Continue. Extra buttons go in the middle.
    component StepNav: RowLayout {
        id: nav
        property bool showSkip: false
        property string continueText: qsTr("Continue")
        readonly property alias backButton: navBack
        readonly property alias skipButton: navSkip
        readonly property alias continueButton: navContinue
        default property alias extra: extraRow.data
        Layout.fillWidth: true
        Layout.topMargin: Dimensions.spacingMd
        spacing: Dimensions.spacingMd
        FocusButton {
            id: navBack
            text: qsTr("Back")
            iconName: "chevron-left"
            variant: "ghost"
            onClicked: root.previous()
            KeyNavigation.right: extraRow.children.length > 0 && extraRow.children[0].visible ? extraRow.children[0]
                               : (navSkip.visible ? navSkip : navContinue)
        }
        Row {
            id: extraRow
            spacing: Dimensions.spacingMd
        }
        Item { Layout.fillWidth: true }
        FocusButton {
            id: navSkip
            visible: nav.showSkip
            text: qsTr("Skip")
            variant: "ghost"
            onClicked: root.next()
            KeyNavigation.left: extraRow.children.length > 0 && extraRow.children[extraRow.children.length - 1].visible
                                ? extraRow.children[extraRow.children.length - 1] : navBack
            KeyNavigation.right: navContinue
        }
        FocusButton {
            id: navContinue
            variant: "primary"
            text: nav.continueText
            iconName: "chevron-right"
            onClicked: root.next()
            KeyNavigation.left: navSkip.visible ? navSkip
                              : (extraRow.children.length > 0 && extraRow.children[extraRow.children.length - 1].visible
                                 ? extraRow.children[extraRow.children.length - 1] : navBack)
        }
    }
    component SummaryRow: Rectangle {
        id: summary
        property string label: ""
        property string value: ""
        property string note: ""
        property bool ok: false
        Layout.fillWidth: true
        implicitHeight: summaryColumn.implicitHeight + Dimensions.spacingMd * 2
        radius: Dimensions.radiusMd
        color: Theme.backgroundElevated
        border.color: Theme.borderSubtle
        ColumnLayout {
            id: summaryColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Dimensions.spacingLg
            spacing: Dimensions.spacingXs
            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingMd
                Text {
                    Layout.fillWidth: true
                    text: summary.label
                    font.family: Typography.family
                    font.pixelSize: Typography.body
                    font.weight: Typography.weightSemiBold
                    color: Theme.textPrimary
                }
                StatusIndicator { status: summary.ok ? "success" : "neutral"; text: summary.value }
            }
            Text {
                Layout.fillWidth: true
                visible: summary.note !== ""
                text: summary.note
                wrapMode: Text.WordWrap
                font.family: Typography.family
                font.pixelSize: Typography.secondary
                color: Theme.textMuted
            }
        }
    }
}
