import QtQuick
import QtQuick.Layouts

// Centered explanation for empty / unavailable / error / loading states with
// up to two actions. kind: "empty" | "unavailable" | "error" | "loading"
FocusScope {
    id: state

    property string kind: "empty"
    property string iconName: kind === "error" ? "error" : kind === "unavailable" ? "warning" : "library"
    property string title: ""
    property string message: ""
    property string details: ""            // optional technical detail line (muted)
    property string primaryText: ""
    property string primaryIcon: ""
    property string secondaryText: ""
    property string secondaryIcon: ""
    signal primaryClicked()
    signal secondaryClicked()

    readonly property Item firstFocusable: primaryButton.visible ? primaryButton : (secondaryButton.visible ? secondaryButton : null)
    implicitHeight: column.implicitHeight
    implicitWidth: column.implicitWidth

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        width: Math.min(parent.width, Dimensions.s(720))
        spacing: Dimensions.spacingMd

        Item {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: Dimensions.s(88); implicitHeight: implicitWidth
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Theme.backgroundElevated
                border.color: Theme.borderSubtle
                visible: state.kind !== "loading"
            }
            Icon {
                anchors.centerIn: parent
                visible: state.kind !== "loading"
                name: state.iconName
                size: Dimensions.iconSizeLarge
                color: state.kind === "error" ? Theme.statusError
                     : state.kind === "unavailable" ? Theme.statusWarning : Theme.accentPrimary
            }
            LoadingIndicator {
                anchors.centerIn: parent
                visible: state.kind === "loading"
                size: Dimensions.iconSizeLarge
            }
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: Dimensions.spacingSm
            horizontalAlignment: Text.AlignHCenter
            text: state.title
            wrapMode: Text.WordWrap
            font.family: Typography.family
            font.pixelSize: Typography.sectionTitle
            font.weight: Typography.weightSemiBold
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            horizontalAlignment: Text.AlignHCenter
            text: state.message
            wrapMode: Text.WordWrap
            font.family: Typography.family
            font.pixelSize: Typography.body
            color: Theme.textSecondary
            lineHeight: 1.15
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            horizontalAlignment: Text.AlignHCenter
            text: state.details
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 4
            elide: Text.ElideRight
            font.family: Typography.family
            font.pixelSize: Typography.caption
            color: Theme.textMuted
        }
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Dimensions.spacingMd
            spacing: Dimensions.spacingMd
            FocusButton {
                id: primaryButton
                visible: state.primaryText !== ""
                focus: visible
                text: state.primaryText
                iconName: state.primaryIcon
                variant: "primary"
                onClicked: state.primaryClicked()
                KeyNavigation.right: secondaryButton.visible ? secondaryButton : null
            }
            FocusButton {
                id: secondaryButton
                visible: state.secondaryText !== ""
                focus: visible && !primaryButton.visible
                text: state.secondaryText
                iconName: state.secondaryIcon
                onClicked: state.secondaryClicked()
                KeyNavigation.left: primaryButton.visible ? primaryButton : null
            }
        }
    }
}
