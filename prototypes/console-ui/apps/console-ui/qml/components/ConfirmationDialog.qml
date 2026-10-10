import QtQuick
import QtQuick.Layouts

// Confirmation for meaningful persistent or destructive actions. Focus starts
// on Cancel for destructive actions so an accidental Confirm press is safe.
ModalBase {
    id: dialog
    property string message: ""
    property string confirmText: qsTr("Confirm")
    property string cancelText: qsTr("Cancel")
    property bool destructive: false
    signal accepted()

    initialFocusItem: destructive ? cancelButton : confirmButton

    ColumnLayout {
        width: parent.width
        spacing: Dimensions.spacingMd
        Text {
            Layout.fillWidth: true
            text: dialog.title
            wrapMode: Text.WordWrap
            font.family: Typography.family
            font.pixelSize: Typography.sectionTitle
            font.weight: Typography.weightSemiBold
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: dialog.message
            wrapMode: Text.WordWrap
            font.family: Typography.family
            font.pixelSize: Typography.body
            color: Theme.textSecondary
            lineHeight: 1.15
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: Dimensions.spacingLg
            spacing: Dimensions.spacingMd
            FocusButton {
                id: cancelButton
                text: dialog.cancelText
                variant: "ghost"
                onClicked: dialog.reject()
                KeyNavigation.right: confirmButton
            }
            FocusButton {
                id: confirmButton
                text: dialog.confirmText
                variant: dialog.destructive ? "danger" : "primary"
                onClicked: { dialog.accepted(); dialog.close() }
                KeyNavigation.left: cancelButton
            }
        }
    }
}
