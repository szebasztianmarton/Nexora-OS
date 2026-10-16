import QtQuick
import QtTest
import Nexora.Console

// Theme is a plain singleton (no backend needed): verifies the accessibility inputs
// (scale, highContrast, reducedMotion) actually drive the tokens screens read.
Item {
    width: 400
    height: 300

    TestCase {
        name: "Theme"
        when: windowShown

        // Theme is process-wide for this file's TestCase; leave it as found.
        function cleanup() {
            Theme.scale = 1.0
            Theme.highContrast = false
            Theme.reducedMotion = false
        }

        function test_scaleMultipliesTypographyAndSpacing() {
            compare(Theme.scale, 1.0)
            compare(Theme.fontBody, 17)
            compare(Theme.space4, 16)
            Theme.scale = 1.5
            compare(Theme.fontBody, Math.round(17 * 1.5))
            compare(Theme.space4, Math.round(16 * 1.5))
            compare(Theme.cardWidth, Math.round(230 * 1.5))
        }

        function test_highContrastSwitchesPalette() {
            verify(Theme.background !== "#000000")
            Theme.highContrast = true
            compare(Theme.background, "#000000")
            compare(Theme.text, "#ffffff")
            compare(Theme.focusRing, "#ffd400")
        }

        function test_reducedMotionZeroesAnimationTokens() {
            verify(Theme.animFast > 0)
            verify(Theme.animNormal > 0)
            verify(Theme.animSlow > 0)
            Theme.reducedMotion = true
            compare(Theme.animFast, 0)
            compare(Theme.animNormal, 0)
            compare(Theme.animSlow, 0)
        }

        function test_controlSizesScale() {
            compare(Theme.controlHeight, 48)
            compare(Theme.rowHeight, 60)
            Theme.scale = 1.5
            compare(Theme.controlHeight, 72)
            compare(Theme.chipHeight, Math.round(44 * 1.5))
            compare(Theme.iconLarge, 60)
        }

        // The game bar / quick menu palette follows high contrast too (it used to be hard-coded).
        function test_overlayPaletteFollowsHighContrast() {
            verify(Theme.overlaySurface !== "#000000")
            Theme.highContrast = true
            compare(Theme.overlaySurface, "#000000")
            compare(Theme.overlayText, "#ffffff")
            compare(Theme.overlayFocus, "#ffd400")
            compare(Theme.overlaySelected, "#ffd400")
        }

        function test_listMoveDurationFollowsReducedMotion() {
            verify(Theme.animMove > 0)
            Theme.reducedMotion = true
            compare(Theme.animMove, 0)
        }

        function test_existingPropertyNamesStillWork() {
            // Pre-existing screens bind Theme.spacing/Theme.radius/Theme.fontTiny.. etc;
            // the token rework must keep every one of them available.
            verify(Theme.spacing !== undefined)
            verify(Theme.radius !== undefined)
            verify(Theme.focusWidth !== undefined)
            verify(Theme.animFast !== undefined && Theme.animNormal !== undefined)
            verify(Theme.fontTiny !== undefined && Theme.fontSmall !== undefined && Theme.fontBody !== undefined
                   && Theme.fontLarge !== undefined && Theme.fontTitle !== undefined && Theme.fontHero !== undefined)
            verify(typeof Theme.capitalize === "function")
            verify(typeof Theme.statusColor === "function")
        }
    }
}
