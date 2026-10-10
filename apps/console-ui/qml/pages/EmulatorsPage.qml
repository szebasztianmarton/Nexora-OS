import QtQuick
import Nexora.Core

// PLACEHOLDER — replaced by the page implementation.
PageContainer {
    id: page
    title: qsTr("Emulators")
    defaultFocusItem: button
    hints: [{ action: "confirm", label: qsTr("Select") }]
    FocusButton { id: button; text: qsTr("Placeholder"); variant: "primary"; onClicked: Router.toast("Clicked", "success") }
}
