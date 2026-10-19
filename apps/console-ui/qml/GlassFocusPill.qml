import QtQuick
import Nexora.Console

// One glass capsule per container that slides and resizes onto the focused item (≈ SwiftUI
// glassEffectID morphing). Put it in the container below its items and point |target| at
// the focused one. Jumps under reduced motion; the focus ring stays drawn by the items.
// Invisible outside the glass theme.
Item {
    id: pill
    property Item target: null
    readonly property bool moving: xAnim.running || wAnim.running
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

    Behavior on x { enabled: !Theme.reducedMotion; NumberAnimation { id: xAnim; duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on y { enabled: !Theme.reducedMotion; NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on width { enabled: !Theme.reducedMotion; NumberAnimation { id: wAnim; duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on height { enabled: !Theme.reducedMotion; NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }

    GlassSurface {
        anchors.fill: parent
        radius: height / 2
        interactive: true
        active: true
        allowBlur: false
    }
}
