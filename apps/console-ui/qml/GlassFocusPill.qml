import QtQuick
import Nexora.Console

// One glass capsule per container that slides and resizes onto the focused item (≈ SwiftUI
// glassEffectID morphing). Put it in the container below its items and point |target| at
// the focused one. Jumps under reduced motion; the focus ring stays drawn by the items.
// Invisible outside the glass theme.
Item {
    id: pill
    property Item target: null
    // False until the pill has been placed on a target; the first placement snaps.
    property bool hasPlaced: false
    // Corner radius: a capsule by default (tabs); list rows set their own row radius.
    property real radius: height / 2
    readonly property rect area: {
        void target
        if (target) {
            // Re-evaluate when the target or any container up to the pill's parent moves
            // (a centred Row shifting its tabs, a scrolled list).
            for (let p = target; p && p !== parent; p = p.parent) { void p.x; void p.y; void p.width; void p.height }
        }
        return target && parent ? target.mapToItem(parent, 0, 0, target.width, target.height) : Qt.rect(0, 0, 0, 0)
    }
    x: area.x
    y: area.y
    width: area.width
    height: area.height
    visible: target !== null && Theme.glassActive
    z: -1
    onTargetChanged: if (target === null) hasPlaced = false
    // Set after the first bindings settled on a real target, so later moves animate.
    onAreaChanged: if (target !== null && !hasPlaced && area.width > 0) Qt.callLater(() => { if (pill.target !== null) pill.hasPlaced = true })

    Behavior on x { enabled: !Theme.reducedMotion && pill.hasPlaced; NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on y { enabled: !Theme.reducedMotion && pill.hasPlaced; NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on width { enabled: !Theme.reducedMotion && pill.hasPlaced; NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on height { enabled: !Theme.reducedMotion && pill.hasPlaced; NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }

    GlassSurface {
        anchors.fill: parent
        radius: pill.radius
        interactive: true
        active: true
        allowBlur: false
        // The morph is the motion: no lift, so the pill never overhangs a full-width row.
        scale: 1.0
    }
}
