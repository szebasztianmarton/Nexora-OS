import QtQuick
import QtTest
import Nexora.Console

// GlassSurface: flat layers under the software backend, a silent fallback when the GPU file
// cannot load (Qt < 6.5 has no QtQuick.Effects), the interactive fill step and GlassGroup.
Item {
    id: root
    width: 640
    height: 360

    Item {
        id: scene
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: "#203060" }
    }
    GlassSurface { id: plain; x: 20; y: 20; width: 200; height: 80; backdrop: scene }
    GlassSurface { id: prominent; variant: "prominent"; x: 20; y: 120; width: 200; height: 80 }
    GlassGroup {
        id: group
        anchors.fill: parent
        backdrop: scene
        GlassSurface { id: grouped; x: 300; y: 20; width: 200; height: 80 }
    }
    GlassSurface { id: cyclic; anchors.fill: scene; backdrop: root }
    GlassSurface { id: selfRef; x: 300; y: 120; width: 100; height: 50 }
    GlassGroup { id: cyclicGroup; x: 420; y: 120; width: 100; height: 50 }
    GlassSurface { id: densePlain; dense: true; x: 20; y: 220; width: 100; height: 50 }
    Item {
        id: host
        x: 0; y: 0; width: 640; height: 360
        ConfirmDialog { id: dialog }
        SettingRow { id: row; label: "Row" }
        Toast { id: toast }
        EmulatorCard {
            id: card
            index: 0
            emulatorId: "e"
            name: "Emu"
            status: "ready"
            statusText: "Ready"
            detail: ""
            available: true
            profiles: []
            extensions: ""
        }
    }

    // First descendant whose type name starts with |name| (e.g. "GlassSurface"), or null.
    function findType(item, name) {
        for (let i = 0; i < item.children.length; ++i) {
            const c = item.children[i]
            if (String(c).startsWith(name))
                return c
            const found = findType(c, name)
            if (found)
                return found
        }
        return null
    }

    TestCase {
        name: "Glass"
        when: windowShown

        function init() { Theme.glass = true }
        function cleanup() {
            Theme.glass = false
            Theme.gpuEffects = false
            Theme.reducedMotion = false
            plain.interactive = false
            plain.active = false
        }

        function test_softwareBackendIsFlat() {
            compare(Theme.glassBlur, false)
            compare(plain.blurred, false)
            compare(plain.fillColor, Theme.glassFillRegular)
        }

        // QtQuick.Effects exists from Qt 6.5: probe whether the module can be imported.
        property bool hasEffects: false
        function initTestCase() {
            try {
                Qt.createQmlObject('import QtQuick; import QtQuick.Effects; Item {}', root, "probe").destroy()
                hasEffects = true
            } catch (e) {
                hasEffects = false
            }
        }

        // Without QtQuick.Effects every GPU Loader (plain, grouped and the group's shared pass)
        // logs one engine warning; that is the only thing allowed to be printed.
        function expectMissingEffectsWarnings(count) {
            if (hasEffects)
                return
            for (let i = 0; i < count; ++i)
                ignoreWarning(/module "QtQuick\.Effects" is not installed/)
        }

        // Forcing the switch on: with QtQuick.Effects (Qt >= 6.5) the GPU file loads, without it
        // the surface quietly stays flat. Under the software backend MultiEffect cannot render,
        // so only the loader outcome is checked.
        function test_gpuPathLoadsOrFallsBackSilently() {
            expectMissingEffectsWarnings(3)
            Theme.gpuEffects = true
            compare(Theme.glassBlur, true)
            if (hasEffects) {
                tryCompare(plain, "state", "gpu")
                compare(plain.blurred, true)
            } else {
                wait(100)
                compare(plain.state, "flat")
                compare(plain.blurred, false)
                compare(findChild(plain, "flat").visible, true)
            }
            verify(plain.fillColor === Theme.glassFillRegular)
        }

        function test_interactiveActiveRaisesFill() {
            plain.interactive = true
            plain.active = true
            compare(plain.fillColor, Theme.glassFillRegularActive)
        }

        function test_prominentUsesAccentFill() {
            compare(prominent.fillColor, Theme.glassFillProminent)
        }

        function test_groupProvidesBackdrop() {
            compare(grouped.effectiveBackdrop, scene)
        }

        // A surface must never blur an ancestor of itself (it would sample its own output).
        function test_ancestorBackdropIsRejected() {
            expectMissingEffectsWarnings(3)
            Theme.gpuEffects = true
            compare(cyclic.effectiveBackdrop, null)
            compare(cyclic.blurred, false)
        }

        function test_selfBackdropIsRejected() {
            selfRef.backdrop = selfRef
            compare(selfRef.effectiveBackdrop, null)
            selfRef.backdrop = null
        }

        function test_groupRejectsItselfAsBackdrop() {
            expectMissingEffectsWarnings(3)
            cyclicGroup.backdrop = cyclicGroup
            Theme.gpuEffects = true
            compare(cyclicGroup.backdropUsable, false)
            compare(cyclicGroup.blurredSource, null)
            cyclicGroup.backdrop = null
        }

        // Floating panels and cards draw GlassSurface; cards never take the GPU path.
        function test_panelsAndCardsUseGlassSurface() {
            expectMissingEffectsWarnings(3)
            Theme.gpuEffects = true
            const panelGlass = findType(dialog, "GlassSurface")
            verify(panelGlass !== null)
            compare(panelGlass.dense, true)
            const rowGlass = findType(row, "GlassSurface")
            verify(rowGlass !== null)
            compare(rowGlass.variant, "clear")
            compare(rowGlass.allowBlur, false)
            const cardGlass = findType(card, "GlassSurface")
            verify(cardGlass !== null)
            compare(cardGlass.variant, "clear")
            compare(cardGlass.allowBlur, false)
            compare(rowGlass.blurred, false)
        }

        // The panel box itself is transparent in glass (GlassSurface is its background) and solid
        // otherwise; the rim comes from GlassSurface.
        function test_panelFillFollowsTheTheme() {
            const panel = findChild(dialog, "confirmDialogPanel")
            verify(panel !== null)
            compare(panel.color.a, 0)
            Theme.glass = false
            compare(String(panel.color), String(Theme.surfaceFloating))
            Theme.glass = true
            Theme.highContrast = true
            compare(String(panel.color), String(Theme.surfaceFloating))
            Theme.highContrast = false
        }

        // In-page dialogs keep their box between the floating bars.
        function test_dialogPanelStaysInsideChromeInsets() {
            dialog.chromeTop = 60
            dialog.chromeBottom = 40
            const panel = findChild(dialog, "confirmDialogPanel")
            verify(panel.y >= 60)
            verify(panel.y + panel.height <= dialog.height - 40)
            dialog.chromeTop = 0
            dialog.chromeBottom = 0
            fuzzyCompare(panel.y, (dialog.height - panel.height) / 2, 0.5)
        }

        // The glass rim replaces a panel's own plain border; high contrast keeps it.
        function test_glassPanelDropsItsPlainBorder() {
            compare(toast.border.width, 0)
            Theme.highContrast = true
            compare(toast.border.width, 1)
            Theme.highContrast = false
        }

        function test_rimIsGlassRim() {
            compare(plain.rimColor, Theme.glassRim)
        }

        // Dense floating panels keep today's solid fill on the flat path.
        function test_denseFillIsSurfaceFloatingWhenFlat() {
            compare(densePlain.blurred, false)
            compare(densePlain.fillColor, Theme.surfaceFloating)
            compare(plain.fillColor, Theme.glassFillRegular)
        }

        function test_outsideGlassSurfaceIsHidden() {
            Theme.glass = false
            compare(plain.visible, false)
        }
    }
}
