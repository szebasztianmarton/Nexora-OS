import QtQuick
import QtQuick.Layouts
import Nexora.Core

// Read-only view of the end of the application log (Settings › System &
// Diagnostics › View log). Opens scrolled to the newest line.
// Up/Down scroll by line, Page Up/Page Down (LB/RB) by page, Confirm
// reloads, Back closes.
ModalBase {
    id: viewer
    property int maxLines: 200
    property var lines: []

    title: qsTr("Application log")
    panelWidth: Dimensions.s(1500)
    initialFocusItem: logList

    function reload() {
        lines = Diagnostics.logTail(maxLines)
        Qt.callLater(function() { logList.positionViewAtEnd() })
    }
    Component.onCompleted: reload()

    ColumnLayout {
        width: parent.width
        height: viewer.maxPanelHeight - Dimensions.spacingXl * 2
        spacing: Dimensions.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingMd
            Icon { name: "terminal"; size: Dimensions.iconSize; color: Theme.accentPrimary }
            Column {
                Layout.fillWidth: true
                spacing: Dimensions.s(2)
                Text {
                    width: parent.width
                    text: viewer.title
                    elide: Text.ElideRight
                    font.family: Typography.family
                    font.pixelSize: Typography.sectionTitle
                    font.weight: Typography.weightSemiBold
                    color: Theme.textPrimary
                }
                Text {
                    width: parent.width
                    text: qsTr("Last %n line(s) of %1", "", viewer.lines.length).arg(Diagnostics.logFilePath)
                    elide: Text.ElideMiddle
                    font.family: Typography.family
                    font.pixelSize: Typography.caption
                    color: Theme.textMuted
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Dimensions.radiusMd
            color: Theme.backgroundPrimary
            border.width: logList.activeFocus ? Dimensions.focusWidth : 1
            border.color: logList.activeFocus ? Theme.focusRing : Theme.borderSubtle

            Text {
                anchors.centerIn: parent
                visible: viewer.lines.length === 0
                text: Diagnostics.logFilePath !== "" ? qsTr("The log is empty or could not be read.")
                                                      : qsTr("File logging is not active in this session.")
                font.family: Typography.family
                font.pixelSize: Typography.body
                color: Theme.textSecondary
            }

            ListView {
                id: logList
                anchors.fill: parent
                anchors.margins: Dimensions.spacingMd
                clip: true
                focus: true
                model: viewer.lines
                boundsBehavior: Flickable.StopAtBounds
                readonly property real lineStep: Dimensions.s(26)
                function scrollBy(delta) {
                    var maxY = Math.max(originY, originY + contentHeight - height)
                    contentY = Math.max(originY, Math.min(maxY, contentY + delta))
                }
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Up) { scrollBy(-lineStep); event.accepted = true }
                    else if (event.key === Qt.Key_Down) { scrollBy(lineStep); event.accepted = true }
                    else if (Input.isPrevTab(event)) { scrollBy(-height * 0.9); event.accepted = true }
                    else if (Input.isNextTab(event)) { scrollBy(height * 0.9); event.accepted = true }
                    else if (event.key === Qt.Key_Home) { positionViewAtBeginning(); event.accepted = true }
                    else if (event.key === Qt.Key_End) { positionViewAtEnd(); event.accepted = true }
                    else if (Input.isConfirm(event)) {
                        event.accepted = true
                        if (!event.isAutoRepeat) viewer.reload()
                    }
                }
                delegate: Text {
                    required property string modelData
                    width: logList.width - Dimensions.s(10)
                    text: modelData
                    wrapMode: Text.WrapAnywhere
                    font.family: "monospace"
                    font.pixelSize: Typography.caption
                    color: modelData.indexOf("[ERROR]") >= 0 || modelData.indexOf("[FATAL]") >= 0 ? Theme.statusError
                         : modelData.indexOf("[WARN]") >= 0 ? Theme.statusWarning
                         : Theme.textSecondary
                }
                Rectangle {
                    anchors.right: parent.right
                    width: Dimensions.s(4)
                    radius: width / 2
                    visible: logList.contentHeight > logList.height
                    height: Math.max(Dimensions.s(30), logList.height * logList.visibleArea.heightRatio)
                    y: logList.height * logList.visibleArea.yPosition
                    color: Theme.textMuted
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingLg
            Text {
                Layout.fillWidth: true
                text: qsTr("Up/Down scroll · %1/%2 page · %3 reload · %4 close")
                      .arg(Controllers.buttonLabel("prevTab")).arg(Controllers.buttonLabel("nextTab"))
                      .arg(Controllers.buttonLabel("confirm")).arg(Controllers.buttonLabel("back"))
                elide: Text.ElideRight
                font.family: Typography.family
                font.pixelSize: Typography.caption
                color: Theme.textMuted
            }
            FocusButton {
                id: closeButton
                text: qsTr("Close")
                small: true
                onClicked: viewer.reject()
            }
        }
    }
}
