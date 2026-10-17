import QtQuick
import Nexora.Console

// Step-by-step introduction shown on the first start (until ui.tutorialCompleted is set)
// and again from Settings → "Show tutorial". Left/Right choose a button, Accept activates
// it, Back returns to the previous step. Finishing or skipping marks the tutorial done.
FocusScope {
    id: tutorial
    objectName: "tutorialOverlay"
    required property LibraryController library

    // Emitted when the tutorial is finished or skipped; |openSettings| asks the window to
    // open the Settings tab (on finish while no library folder exists yet).
    signal done(bool openSettings)

    readonly property bool noDirectories: library.directories.length === 0
    readonly property var steps: [
        { glyph: "✦", title: "Welcome to Nexora",
          body: "Nexora brings your emulators and games together in one place you can use "
                + "entirely with a controller.\n\nThis short tour shows the basics. It takes "
                + "less than a minute, and you can skip it at any time.",
          controls: [] },
        { glyph: "✥", title: "Moving around",
          body: "Use the D-pad or the left stick to move between items. The highlighted item "
                + "has a blue outline.\n\nOn a keyboard, the arrow keys, Enter and Esc do the same.",
          controls: [{ button: "D-pad / ↑ ↓ ← →", action: "Move" },
                     { button: "A / Enter", action: "Select" },
                     { button: "B / Esc", action: "Back" }] },
        { glyph: "▤", title: "Tabs",
          body: "Nexora has five tabs: Home, Games, Emulators, Settings and Diagnostics.\n\n"
                + "Switch tabs with the shoulder buttons, or press Up at the top of a page to "
                + "reach the tab bar.",
          controls: [{ button: "L1 / R1", action: "Previous / next tab" },
                     { button: "PgUp / PgDn", action: "Same on a keyboard" },
                     { button: "Start / F1", action: "Diagnostics" }] },
        { glyph: "+", title: "Add your games",
          body: "Nexora finds games in the library folders you choose. Add one under "
                + "Settings → Library folders; the folder is scanned automatically.\n\n"
                + "Games are matched to your emulators by file extension. Nexora does not "
                + "include any games or BIOS files.",
          controls: [] },
        { glyph: "▶", title: "Playing",
          body: "Select a game to start it in its emulator. Mark the games you love as "
                + "favorites; they appear on Home together with your recently played games.\n\n"
                + "To leave a game, quit the emulator, or hold Select + Start for one second.",
          controls: [{ button: "A / Enter", action: "Play" },
                     { button: "Y / F", action: "Favorite" },
                     { button: "X / “/”", action: "Search" },
                     { button: "Select + Start (hold)", action: "Return to Nexora" }] },
        { glyph: "✓", title: "You're all set",
          body: tutorial.noDirectories
                ? "Next, add the folder that holds your games. Get started takes you there.\n\n"
                  + "You can watch this tour again at any time from Settings → Show tutorial."
                : "Your library is ready. Have fun!\n\n"
                  + "You can watch this tour again at any time from Settings → Show tutorial.",
          controls: [] }
    ]
    property int step: 0
    readonly property bool lastStep: step === steps.length - 1
    readonly property var hints: [{ button: "← →", action: "Choose" }, { button: "A / Enter", action: "Select" },
                                  { button: "B / Esc", action: "Previous step" }]

    // Starts again from the first step with the main button focused.
    function restart() {
        step = 0
        nextButton.forceActiveFocus()
    }
    function next() {
        if (lastStep) {
            done(noDirectories)
            return
        }
        ++step
        nextButton.forceActiveFocus()
    }
    function previous() {
        if (step === 0)
            return
        --step
        if (step === 0)
            nextButton.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
        opacity: 0.94
    }
    // Swallow pointer input to the screens underneath.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(tutorial.width - Theme.spacing * 4, 760)
        height: content.implicitHeight + Theme.spacing * 4
        radius: Theme.radius * 1.5
        color: Theme.surface
        border.color: Theme.border

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.spacing * 2
            spacing: Theme.spacing

            Row {
                width: parent.width
                spacing: Theme.spacing
                Rectangle {
                    width: 56
                    height: 56
                    radius: 28
                    color: Theme.accentSoft
                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: tutorial.steps[tutorial.step].glyph
                        color: Theme.text
                        font.pixelSize: Theme.fontTitle
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        textFormat: Text.PlainText
                        objectName: "tutorialProgress"
                        text: "STEP " + (tutorial.step + 1) + " OF " + tutorial.steps.length
                        color: Theme.accent
                        font.pixelSize: Theme.fontTiny
                        font.bold: true
                        font.letterSpacing: 2
                    }
                    Text {
                        textFormat: Text.PlainText
                        objectName: "tutorialTitle"
                        text: tutorial.steps[tutorial.step].title
                        color: Theme.text
                        font.pixelSize: Theme.fontHero
                        font.bold: true
                    }
                }
            }

            Text {
                textFormat: Text.PlainText
                width: parent.width
                text: tutorial.steps[tutorial.step].body
                wrapMode: Text.Wrap
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                lineHeight: 1.15
            }

            // Controls introduced by this step.
            Flow {
                width: parent.width
                visible: tutorial.steps[tutorial.step].controls.length > 0
                spacing: Theme.spacing
                Repeater {
                    model: tutorial.steps[tutorial.step].controls
                    delegate: Row {
                        id: control
                        required property var modelData
                        spacing: 8
                        Rectangle {
                            height: 30
                            width: Math.max(30, keyLabel.implicitWidth + 18)
                            radius: 6
                            color: Theme.surfaceRaised
                            border.color: Theme.border
                            Text {
                                textFormat: Text.PlainText
                                id: keyLabel
                                anchors.centerIn: parent
                                text: control.modelData.button
                                color: Theme.text
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }
                        }
                        Text {
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                            text: control.modelData.action
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.spacing / 2 }

            Item {
                width: parent.width
                height: nextButton.height

                // Progress dots.
                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Repeater {
                        model: tutorial.steps.length
                        delegate: Rectangle {
                            required property int index
                            width: index === tutorial.step ? 24 : 8
                            height: 8
                            radius: 4
                            color: index <= tutorial.step ? Theme.accent : Theme.border
                            Behavior on width { NumberAnimation { duration: Theme.animNormal } }
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    spacing: 12
                    NxButton {
                        id: skipButton
                        objectName: "tutorialSkip"
                        visible: !tutorial.lastStep
                        compact: true
                        text: "Skip tutorial"
                        onClicked: tutorial.done(false)
                        Keys.onRightPressed: (backButton.visible ? backButton : nextButton).forceActiveFocus()
                    }
                    NxButton {
                        id: backButton
                        objectName: "tutorialBack"
                        visible: tutorial.step > 0
                        compact: true
                        glyph: "‹"
                        text: "Back"
                        onClicked: tutorial.previous()
                        Keys.onLeftPressed: if (skipButton.visible) skipButton.forceActiveFocus()
                        Keys.onRightPressed: nextButton.forceActiveFocus()
                    }
                    NxButton {
                        id: nextButton
                        objectName: "tutorialNext"
                        focus: true
                        compact: true
                        kind: "primary"
                        text: tutorial.lastStep ? "Get started" : "Next"
                        onClicked: tutorial.next()
                        Keys.onLeftPressed: {
                            if (backButton.visible)
                                backButton.forceActiveFocus()
                            else if (skipButton.visible)
                                skipButton.forceActiveFocus()
                        }
                    }
                }
            }
        }
    }

    // Back steps back; tab switches, search, menu and the rest do nothing here.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back || event.key === Qt.Key_Backspace)
            tutorial.previous()
        event.accepted = true
    }
}
