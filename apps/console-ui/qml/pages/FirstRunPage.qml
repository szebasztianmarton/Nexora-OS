import QtQuick

// PLACEHOLDER — replaced by the first-run implementation.
FocusScope {
    signal finished()
    Rectangle { anchors.fill: parent; color: Theme.backgroundPrimary }
    FocusButton { anchors.centerIn: parent; focus: true; text: qsTr("Start Gaming"); variant: "primary"; onClicked: parent.finished() }
}
