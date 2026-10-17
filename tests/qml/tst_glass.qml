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

        // QtQuick.Effects exists from Qt 6.5; Qt.version is the runtime version.
        readonly property bool hasEffects: {
            const v = Qt.version.split(".")
            return Number(v[0]) > 6 || (Number(v[0]) === 6 && Number(v[1]) >= 5)
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

        function test_outsideGlassSurfaceIsHidden() {
            Theme.glass = false
            compare(plain.visible, false)
        }
    }
}
