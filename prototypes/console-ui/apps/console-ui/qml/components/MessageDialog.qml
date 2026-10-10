import QtQuick
import QtQuick.Layouts

// Error / information dialog: what happened, what is affected, what to do
// next. Technical details are shown muted and truncated; logs hold the rest.
ModalBase {
    id: dialog
    property string kind: "error"          // error | warning | info
    property string message: ""
    property string details: ""
    property var actions: [{ id: "close", label: qsTr("Close"), primary: true }]
    signal actionTriggered(string actionId)
    panelWidth: Dimensions.s(720)
    initialFocusItem: buttons.count > 0 ? buttons.itemAt(0) : null

    ColumnLayout {
        width: parent.width
        spacing: Dimensions.spacingMd
        RowLayout {
            spacing: Dimensions.spacingMd
            Icon {
                name: dialog.kind === "info" ? "info" : dialog.kind === "warning" ? "warning" : "error"
                size: Dimensions.iconSizeLarge * 0.85
                color: dialog.kind === "info" ? Theme.statusInformation
                     : dialog.kind === "warning" ? Theme.statusWarning : Theme.statusError
            }
            Text {
                Layout.fillWidth: true
                text: dialog.title
                wrapMode: Text.WordWrap
                font.family: Typography.family
                font.pixelSize: Typography.sectionTitle
                font.weight: Typography.weightSemiBold
                color: Theme.textPrimary
            }
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: dialog.message
            wrapMode: Text.Wrap
            font.family: Typography.family
            font.pixelSize: Typography.body
            color: Theme.textSecondary
            lineHeight: 1.15
        }
        Rectangle {
            Layout.fillWidth: true
            visible: dialog.details !== ""
            implicitHeight: detailsText.implicitHeight + Dimensions.spacingMd * 2
            radius: Dimensions.radiusSm
            color: Theme.backgroundSecondary
            border.color: Theme.borderSubtle
            Text {
                id: detailsText
                anchors.fill: parent
                anchors.margins: Dimensions.spacingMd
                text: dialog.details
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 6
                elide: Text.ElideRight
                font.family: "monospace"
                font.pixelSize: Typography.caption
                color: Theme.textMuted
            }
        }
        Row {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: Dimensions.spacingLg
            spacing: Dimensions.spacingMd
            Repeater {
                id: buttons
                model: dialog.actions
                delegate: FocusButton {
                    required property var modelData
                    required property int index
                    text: modelData.label
                    variant: modelData.primary ? "primary" : "secondary"
                    onClicked: { dialog.close(); dialog.actionTriggered(modelData.id) }
                    KeyNavigation.left: index > 0 ? buttons.itemAt(index - 1) : null
                    KeyNavigation.right: index < buttons.count - 1 ? buttons.itemAt(index + 1) : null
                }
            }
        }
    }
}
