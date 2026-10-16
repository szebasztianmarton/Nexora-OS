import QtQuick
import Nexora.Console

// A vertical list of focus stops for controller/keyboard use: Up/Down move between the
// visible, enabled children that take focus (activeFocusOnTab: SettingRow, NxButton, ...);
// everything else (headings, info text) is skipped. Up on the first stop / Down on the last
// one is not accepted, so the key bubbles to whatever contains the column.
FocusScope {
    id: nav
    default property alias content: column.data
    property alias spacing: column.spacing
    // The stop that last had focus; focusCurrent() returns to it.
    property Item lastFocused: null

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    function stops() {
        const list = []
        const kids = column.children
        for (let i = 0; i < kids.length; ++i) {
            const c = kids[i]
            if (c.visible && c.enabled && c.activeFocusOnTab)
                list.push(c)
        }
        return list
    }
    function currentIndex() {
        const list = stops()
        for (let i = 0; i < list.length; ++i) {
            if (list[i].activeFocus)
                return i
        }
        return -1
    }
    function focusFirst() {
        const list = stops()
        if (list.length > 0)
            list[0].forceActiveFocus()
        return list.length > 0
    }
    function focusLast() {
        const list = stops()
        if (list.length > 0)
            list[list.length - 1].forceActiveFocus()
        return list.length > 0
    }
    function focusCurrent() {
        if (lastFocused && lastFocused.visible && lastFocused.enabled && stops().indexOf(lastFocused) >= 0) {
            lastFocused.forceActiveFocus()
            return true
        }
        return focusFirst()
    }
    function move(delta) {
        const list = stops()
        const i = currentIndex()
        const next = i + delta
        if (i < 0 || next < 0 || next >= list.length)
            return false
        list[next].forceActiveFocus()
        return true
    }

    Column {
        id: column
        width: nav.width
        spacing: Theme.space2
    }

    onActiveFocusChanged: {
        if (!activeFocus)
            return
        const i = currentIndex()
        if (i >= 0)
            lastFocused = stops()[i]
    }
    Connections {
        target: nav.Window.window
        function onActiveFocusItemChanged() {
            const i = nav.activeFocus ? nav.currentIndex() : -1
            if (i >= 0)
                nav.lastFocused = nav.stops()[i]
        }
    }

    Keys.onUpPressed: (event) => { event.accepted = move(-1) }
    Keys.onDownPressed: (event) => { event.accepted = move(1) }
}
