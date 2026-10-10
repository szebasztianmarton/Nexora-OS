import QtQuick
import QtQuick.Window

// Base for every Settings category panel: a vertically scrolling column of
// sections and rows. Focus moves between rows that expose
// `settingsFocusable: true` (see SettingsRow) with Up/Down; the focused row
// is always scrolled into view. Read-only stretches between rows (or a page
// without any focusable row) scroll with Up/Down, so all content is reachable
// with a controller. Left/Back are not handled here: the Settings page uses
// them to return to the category list.
FocusScope {
    id: root

    default property alias content: column.data
    property Item defaultFocusItem: null
    readonly property alias flickable: flick
    readonly property real scrollStep: Dimensions.s(160)

    function focusables() {
        var result = []
        function walk(item) {
            for (var i = 0; i < item.children.length; ++i) {
                var child = item.children[i]
                if (!child.visible) continue
                if (child.settingsFocusable === true) result.push(child)
                else if (child.settingsFocusable === undefined) walk(child)
            }
        }
        walk(column)
        return result
    }
    function focusFirst() {
        var target = defaultFocusItem && defaultFocusItem.visible && defaultFocusItem.enabled
                ? defaultFocusItem : null
        if (!target) {
            var list = focusables()
            target = list.length > 0 ? list[0] : null
        }
        flick.contentY = 0
        if (target) target.forceActiveFocus()
        else root.forceActiveFocus()
    }
    function _currentRow(list) {
        var item = root.Window.window ? root.Window.window.activeFocusItem : null
        for (var p = item; p && p !== root; p = p.parent) {
            var i = list.indexOf(p)
            if (i >= 0) return i
        }
        return -1
    }
    function _scrollBy(delta) {
        var maxY = Math.max(0, flick.contentHeight - flick.height)
        var y = Math.max(0, Math.min(maxY, flick.contentY + delta))
        if (y === flick.contentY) return false
        flick.contentY = y
        return true
    }
    // Next row in direction `dir`, unless it is far away and there is
    // read-only content to scroll through first.
    function _move(dir) {
        var list = focusables()
        var i = _currentRow(list)
        var next = i < 0 ? (dir > 0 ? 0 : list.length - 1) : i + dir
        if (next >= 0 && next < list.length) {
            var target = list[next]
            var p = target.mapToItem(flick.contentItem, 0, 0)
            var visibleTop = flick.contentY
            var visibleBottom = flick.contentY + flick.height
            var reachable = dir > 0 ? p.y + target.height <= visibleBottom + root.scrollStep
                                    : p.y >= visibleTop - root.scrollStep
            if (reachable || i < 0) {
                target.forceActiveFocus()
                return true
            }
            return _scrollBy(dir * root.scrollStep) || (target.forceActiveFocus(), true)
        }
        return _scrollBy(dir * root.scrollStep)
    }
    function ensureVisible(item) {
        if (!item) return
        var list = focusables()
        var row = item
        for (var p = item; p && p !== root; p = p.parent)
            if (list.indexOf(p) >= 0) { row = p; break }
        if (list.length > 0 && row === list[0] && row.mapToItem(flick.contentItem, 0, 0).y + row.height < flick.height) {
            flick.contentY = 0           // keep the first heading visible
            return
        }
        var pos = row.mapToItem(flick.contentItem, 0, 0)
        var topMargin = Dimensions.s(90)  // room for a section heading above the row
        var bottomMargin = Dimensions.spacingXl
        var maxY = Math.max(0, flick.contentHeight - flick.height)
        if (pos.y - topMargin < flick.contentY)
            flick.contentY = Math.max(0, pos.y - topMargin)
        else if (pos.y + row.height + bottomMargin > flick.contentY + flick.height)
            flick.contentY = Math.min(maxY, pos.y + row.height + bottomMargin - flick.height)
    }

    Keys.onUpPressed: function(event) { event.accepted = _move(-1) }
    Keys.onDownPressed: function(event) { event.accepted = _move(1) }

    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            var item = root.Window.window ? root.Window.window.activeFocusItem : null
            for (var p = item; p; p = p.parent)
                if (p === root) { root.ensureVisible(item); return }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: column.implicitHeight + Dimensions.spacingXl
        boundsBehavior: Flickable.StopAtBounds
        Behavior on contentY {
            enabled: !flick.dragging && !flick.flicking
            NumberAnimation { duration: Motion.focus; easing.type: Motion.easing }
        }

        Column {
            id: column
            // Leave room for focus rings that extend outside the rows.
            x: Dimensions.focusOffset + Dimensions.focusWidth * 2
            y: Dimensions.focusOffset + Dimensions.focusWidth * 2
            width: flick.width - x * 2 - scrollTrack.width - Dimensions.spacingSm
            spacing: Dimensions.spacingSm
        }
    }

    // Soft edge where content scrolls under the panel header.
    Rectangle {
        anchors.left: parent.left
        anchors.right: scrollTrack.left
        anchors.top: parent.top
        height: Dimensions.s(18)
        visible: flick.contentY > 1
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.backgroundSecondary }
            GradientStop { position: 1; color: "transparent" }
        }
    }

    // Scroll position indicator (only when the content overflows).
    Rectangle {
        id: scrollTrack
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Dimensions.s(4)
        radius: width / 2
        color: Theme.borderSubtle
        visible: flick.contentHeight > flick.height + 1
        Rectangle {
            width: parent.width
            radius: width / 2
            color: Theme.textMuted
            height: Math.max(Dimensions.s(40), parent.height * flick.height / Math.max(1, flick.contentHeight))
            y: (parent.height - height) * (flick.contentY / Math.max(1, flick.contentHeight - flick.height))
        }
    }
}
