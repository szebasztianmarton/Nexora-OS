import QtQuick
import QtQuick.Window

// Base for every page. Provides the standard padding, an optional header,
// controller hints and focus memory:
//   - `defaultFocusItem` receives focus the first time the page is entered
//   - the last focused descendant is remembered and restored by restoreFocus()
//     (after returning from a detail page, closing a dialog, or re-entering
//     from the sidebar)
// Content goes into the default property and is laid out below the header.
FocusScope {
    id: page

    property string title: ""
    property string subtitle: ""
    property bool showHeader: title !== ""
    property bool padded: true
    // [{ action: "confirm"|"back"|"secondary"|"search"|"options"|"prevTab"|"nextTab", label: "..." }]
    property var hints: []
    property Item defaultFocusItem: null
    property Item lastFocusedItem: null
    // Items rendered on the right side of the header (buttons, counters).
    property alias headerActions: headerActionsRow.data
    default property alias content: contentArea.data
    readonly property alias contentItem: contentArea

    // Called by the shell when the page becomes visible / is returned to.
    function restoreFocus() {
        var target = lastFocusedItem && lastFocusedItem.visible && lastFocusedItem.enabled
                ? lastFocusedItem : defaultFocusItem
        if (target)
            target.forceActiveFocus()
        else
            page.forceActiveFocus()
    }

    function _isDescendant(item) {
        for (var p = item; p; p = p.parent)
            if (p === page) return true
        return false
    }

    Connections {
        target: page.Window.window
        function onActiveFocusItemChanged() {
            var item = page.Window.window ? page.Window.window.activeFocusItem : null
            if (item && item !== page && page._isDescendant(item))
                page.lastFocusedItem = item
        }
    }

    PageHeader {
        id: header
        visible: page.showHeader
        title: page.title
        subtitle: page.subtitle
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: page.padded ? Dimensions.pagePaddingV : 0
        anchors.leftMargin: page.padded ? Dimensions.pagePaddingH : 0
        anchors.rightMargin: page.padded ? Dimensions.pagePaddingH : 0
        Row {
            id: headerActionsRow
            parent: header.actionsSlot
            spacing: Dimensions.spacingSm
        }
    }

    Item {
        id: contentArea
        anchors.top: page.showHeader ? header.bottom : parent.top
        anchors.topMargin: page.showHeader ? Dimensions.spacingLg : (page.padded ? Dimensions.pagePaddingV : 0)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: page.padded ? Dimensions.pagePaddingH : 0
        anchors.rightMargin: page.padded ? Dimensions.pagePaddingH : 0
        anchors.bottomMargin: page.padded ? Dimensions.spacingMd : 0
    }
}
