import QtQuick
import Nexora.Console

// Home: games saved for Quick Resume (GameSessionManager.suspended), newest first.
// Accept resumes (a session that cannot be resumed explains why and offers a fresh start);
// Select / O asks to remove the saved state. Hidden while there is nothing to resume.
FocusScope {
    id: row
    property GameSessionManager sessions: null
    readonly property var entries: sessions ? sessions.suspended : []
    readonly property bool hasSessions: entries.length > 0
    readonly property alias currentIndex: list.currentIndex

    signal removeRequested(string sessionId, string title, string savedAt)

    visible: hasSessions
    implicitHeight: heading.height + 10 + cardHeight + 24
    readonly property real cardWidth: Math.round(Theme.cardWidth * 1.5)
    readonly property real cardHeight: Math.round(116 * Theme.scale)

    function current() {
        return list.currentIndex >= 0 && list.currentIndex < entries.length ? entries[list.currentIndex] : null
    }
    function resumeCurrent() {
        const entry = current()
        if (entry && sessions)
            sessions.resume(entry.id)
    }

    Row {
        id: heading
        spacing: 12
        Text {
            id: headingLabel
            textFormat: Text.PlainText
            text: "Quick Resume"
            color: row.activeFocus ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontLarge
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            anchors.baseline: headingLabel.baseline
            text: row.entries.length
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
        }
    }

    ListView {
        id: list
        objectName: "quickResumeList"
        anchors.top: heading.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        height: row.cardHeight + 24
        focus: true
        orientation: ListView.Horizontal
        spacing: Theme.spacing
        leftMargin: 8
        rightMargin: 8
        topMargin: 12
        clip: false
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animMove
        preferredHighlightBegin: 8
        preferredHighlightEnd: width - row.cardWidth - 8
        highlightRangeMode: ListView.ApplyRange
        boundsBehavior: Flickable.StopAtBounds
        model: row.entries
        onCountChanged: if (count > 0 && (currentIndex < 0 || currentIndex >= count)) currentIndex = 0

        delegate: Rectangle {
            id: card
            required property var modelData
            required property int index
            readonly property bool selected: ListView.isCurrentItem && list.activeFocus
            readonly property bool resumable: modelData.problem === ""
            objectName: "quickResumeCard_" + index
            width: row.cardWidth
            height: row.cardHeight
            radius: Theme.radius
            color: selected ? Theme.surfaceHover : Theme.surfaceRaised
            border.width: selected ? Theme.focusWidth : 1
            border.color: selected ? Theme.focusRing : Theme.border
            scale: selected && !Theme.reducedMotion ? 1.04 : 1.0
            z: selected ? 2 : 1
            Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutQuad } }

            Accessible.role: Accessible.Button
            Accessible.name: "Resume " + modelData.title
            Accessible.description: resumable ? "Saved " + modelData.savedAt : modelData.problem
            Accessible.onPressAction: { list.currentIndex = index; row.resumeCurrent() }

            Column {
                anchors.fill: parent
                anchors.margins: Theme.space3
                spacing: Theme.space1
                Text {
                    width: parent.width
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    text: card.modelData.title
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.bold: true
                }
                Text {
                    width: parent.width
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    text: "Saved " + card.modelData.savedAt + " · " + Theme.formatBytes(card.modelData.bytes)
                          + " · " + card.modelData.emulatorName
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    width: parent.width
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    text: !card.resumable ? card.modelData.problem
                          : card.modelData.old ? "Older than your Quick Resume age limit — consider removing it"
                          : card.modelData.note
                    color: !card.resumable || card.modelData.old ? Theme.warning : Theme.textFaint
                    font.pixelSize: Theme.fontSmall
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: { list.currentIndex = card.index; list.forceActiveFocus() }
                onDoubleClicked: row.resumeCurrent()
            }
        }

        Keys.onPressed: (event) => {
            const entry = row.current()
            if (!entry)
                return
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                row.resumeCurrent()
                event.accepted = true
            } else if (event.key === Qt.Key_Context1 || (event.key === Qt.Key_O && event.modifiers === Qt.NoModifier)) {
                row.removeRequested(entry.id, entry.title, entry.savedAt)
                event.accepted = true
            }
        }
    }
}
