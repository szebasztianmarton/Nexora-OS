import QtQuick
import QtTest
import Nexora.Console

// Theme is a plain singleton (no backend needed): verifies the accessibility inputs
// (scale, highContrast, reducedMotion) actually drive the tokens screens read.
Item {
    width: 400
    height: 300

    function findText(item, text) {
        for (let i = 0; i < item.children.length; ++i) {
            const c = item.children[i]
            if (c.text === text && c.color !== undefined && c.font !== undefined)
                return c
            const found = findText(c, text)
            if (found)
                return found
        }
        return null
    }
    Backdrop { id: backdrop; anchors.fill: parent; z: -1 }
    Row {
        NxButton { id: primary; kind: "primary"; text: "Play" }
        NxButton { id: normal; text: "Other" }
    }

    TestCase {
        name: "Theme"
        when: windowShown

        // Theme is process-wide for this file's TestCase; leave it as found.
        function cleanup() {
            Theme.userScale = 1.0
            Theme.resolutionScale = 1.0
            Theme.highContrast = false
            Theme.glass = false
            Theme.reducedMotion = false
            Theme.reduceTransparency = false
            Theme.gpuEffects = false
            Theme.sessionActive = false
        }

        function test_scaleMultipliesTypographyAndSpacing() {
            compare(Theme.scale, 1.0)
            compare(Theme.fontBody, 17)
            compare(Theme.space4, 16)
            Theme.userScale = 1.5
            compare(Theme.fontBody, Math.round(17 * 1.5))
            compare(Theme.space4, Math.round(16 * 1.5))
            compare(Theme.cardWidth, Math.round(230 * 1.5))
        }

        // Theme.scale = user scale (Settings › Interface scale) x resolution factor
        // (Main.qml: max(1, window size / 1280x720)), so 1080p is not drawn with 720p sizes.
        function test_resolutionFactorMultipliesTheUserScale() {
            Theme.resolutionScale = 1.5
            compare(Theme.scale, 1.5)
            compare(Theme.fontBody, Math.round(17 * 1.5))
            compare(Theme.fontSmall, 21)
            Theme.userScale = 1.5
            compare(Theme.scale, 2.25)
            compare(Theme.controlHeight, Math.round(48 * 2.25))
        }

        // Focused primary buttons in high contrast: black text on the yellow accent (not white,
        // ~1.4:1) and the focus-ring colour as border.
        function test_highContrastPrimaryButtonIsReadable() {
            Theme.highContrast = true
            primary.forceActiveFocus()
            verify(primary.activeFocus)
            tryCompare(primary, "color", Theme.accent) // animated (Behavior on color)
            const label = findText(primary, "Play")
            verify(label !== null)
            compare(label.color, Theme.background)
            compare(primary.border.color, Theme.focusRing)
            normal.forceActiveFocus()
            compare(normal.border.color, Theme.focusRing)
        }

        // The modal scrim is a Theme token: black 40 % in glass, the background in high contrast.
        function test_modalScrimToken() {
            Theme.glass = true
            compare(Theme.modalScrim, Qt.rgba(0, 0, 0, 0.40))
            Theme.highContrast = true
            compare(Theme.modalScrim, Theme.background)
        }

        function test_highContrastSwitchesPalette() {
            verify(Theme.background !== "#000000")
            Theme.highContrast = true
            compare(Theme.background, "#000000")
            compare(Theme.text, "#ffffff")
            compare(Theme.focusRing, "#ffd400")
        }

        // Glass: translucent surfaces and a dense floating-panel tint; high contrast replaces
        // it completely (opaque black surfaces, no glows).
        function test_glassPaletteIsTranslucentAndYieldsToHighContrast() {
            compare(Theme.surface.a, 1.0)
            compare(Theme.glassActive, false)
            Theme.glass = true
            verify(Theme.glassActive)
            verify(Theme.surface.a < 0.2)
            verify(Theme.surfaceRaised.a > Theme.surface.a)
            verify(Theme.surfaceHover.a > Theme.surfaceRaised.a)
            verify(Theme.surfaceFloating.a >= 0.85)
            verify(Theme.overlaySurface.a >= 0.8)
            Theme.highContrast = true
            verify(!Theme.glassActive)
            compare(Theme.surface, "#000000")
            compare(Theme.surfaceFloating, "#000000")
            compare(Theme.overlaySurface, "#000000")
        }

        function test_backdropShowsGlowsOnlyInGlass() {
            const glows = backdrop.children[0]
            verify(!glows.visible)
            Theme.glass = true
            verify(glows.visible)
            compare(glows.children.length, 4) // three glows + vignette
            for (let i = 0; i < 3; ++i)
                tryCompare(glows.children[i], "status", Image.Ready)
            Theme.highContrast = true
            verify(!glows.visible)
            compare(backdrop.color, Theme.background)
        }

        // Theme.glassBlur is the one switch every glass surface reads (spec: "glassBlur").
        function test_glassBlurNeedsGlassGpuAndNoSession() {
            Theme.glass = true
            compare(Theme.glassBlur, false)          // no GPU yet
            Theme.gpuEffects = true
            compare(Theme.glassBlur, true)
            Theme.sessionActive = true
            compare(Theme.glassBlur, false)
            Theme.sessionActive = false
            Theme.reduceTransparency = true
            compare(Theme.glassBlur, false)
            Theme.reduceTransparency = false
            Theme.highContrast = true
            compare(Theme.glassBlur, false)
            Theme.highContrast = false
            Theme.glass = false
            compare(Theme.glassBlur, false)
        }

        function test_reduceTransparencyUsesDenseFills() {
            Theme.glass = true
            verify(Theme.glassFillRegular.a < 0.5)
            Theme.reduceTransparency = true
            compare(Theme.glassFillRegular, Theme.surfaceFloating)
            compare(Theme.glassFillClear, Theme.surfaceFloating)
        }

        function test_glassTokensScale() {
            compare(Theme.glassBlurRadius, 32)
            compare(Theme.radiusXL, 24)
            Theme.userScale = 1.5
            compare(Theme.glassBlurRadius, 48)
            compare(Theme.radiusXL, 36)
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
            Theme.userScale = 1.5
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
