# Liquid Glass Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade the console UI's default `glass` theme to Apple-style Liquid Glass: real backdrop blur on GPUs with an automatic blur-free fallback, floating glass chrome, glass panels, a morphing focus pill and glass button styles.

**Architecture:** One shared QML primitive, `GlassSurface`, draws every glass background. It picks a GPU file (`GlassEffectGpu.qml`, `QtQuick.Effects`/`MultiEffect`, loaded at runtime so Qt 6.4 still builds) or a flat file (`GlassEffectFlat.qml`, today's translucent look) from one switch, `Theme.glassBlur`. `GlassGroup` shares one blur pass between several surfaces. A C++ singleton, `UiCapabilities`, tells QML whether the GPU path is allowed.

**Tech Stack:** Qt 6.4+ (image: 6.9.1), QML / Qt Quick, `QtQuick.Effects` (optional, 6.5+), C++20, CMake presets, QtTest / QtQuickTest, clang-format-18.

**Spec:** [docs/superpowers/specs/2026-10-17-liquid-glass-design.md](../specs/2026-10-17-liquid-glass-design.md) — read it before starting any task.

## Global Constraints

- Minimum Qt stays **6.4**; nothing may `import QtQuick.Effects` except `GlassEffectGpu.qml` and `GlassGroupBlur.qml` (both only ever loaded through a `Loader`), and no CMake change may require Qt 6.5.
- Software renderer (`QT_QUICK_BACKEND=software`, every test) and llvmpipe/softpipe always get the flat path; the flat path must look like today's glass theme.
- No blur while an emulator session is starting, running or stopping (`AppController.state` is `"starting"`, `"running"` or `"stopping"`).
- High contrast replaces glass entirely (`Theme.glassActive` is false); reduced motion makes every new animation instant.
- At most three GPU-blurred regions per screen; cards, rows and tiles never use the GPU path.
- New setting key: `ui.reduceTransparency`, boolean, default `false`, description `"Reduce transparency"`.
- Escape hatch env var: `NEXORA_NO_GLASS_BLUR` (any non-empty value disables the GPU path).
- Build and test **inside WSL** from the repo at `/mnt/d/nexora OS/Nexora-OS`:
  `wsl.exe -e bash -lc 'cd "/mnt/d/nexora OS/Nexora-OS" && cmake --preset debug && cmake --build --preset debug && ctest --preset debug'`
  (single test: append `-R <name>`, e.g. `ctest --preset debug -R tst_console_qml`).
- C++ is formatted with `clang-format-18 -i <files>` (repo `.clang-format`). Match the surrounding comment density and style; every new QML file starts with a short header comment like its neighbours.
- Every new QML file is added to `NEXORA_QML_FILES` in `apps/console-ui/CMakeLists.txt`; every new C++ source to the `SOURCES` list and to `nexora_apply_warnings(...)`.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map

| File | Status | Responsibility |
|---|---|---|
| `shared/include/nexora/shared/Settings.h`, `shared/src/Settings.cpp` | modify | `ui.reduceTransparency` key |
| `apps/console-ui/src/SettingsController.{h,cpp}` | modify | `reduceTransparency` property |
| `apps/console-ui/src/UiCapabilities.{h,cpp}` | create | `gpuEffects` decision (QML singleton) |
| `apps/console-ui/src/UiBootstrap.cpp` | modify | probes the renderer after the first frame |
| `apps/console-ui/qml/Theme.qml` | modify | glass tokens and the `glassBlur` switch |
| `apps/console-ui/qml/GlassSurface.qml` | create | the glass primitive (≈ `glassEffect`) |
| `apps/console-ui/qml/GlassEffectFlat.qml` | create | blur-free layers |
| `apps/console-ui/qml/GlassEffectGpu.qml` | create | `MultiEffect` blur layer |
| `apps/console-ui/qml/GlassGroup.qml` | create | shared blur source (≈ `GlassEffectContainer`) |
| `apps/console-ui/qml/GlassFocusPill.qml` | create | morphing focus capsule (≈ `glassEffectID`) |
| `apps/console-ui/qml/Main.qml` | modify | Theme bindings, scene layer, floating chrome |
| `Header.qml`, `NavTabBar.qml`, `HintBar.qml`, screens | modify | floating bars, content margins |
| dialogs / panels / cards / `NxButton.qml` | modify | migrate to `GlassSurface`, button styles |
| `GlassSheen.qml` | delete (Task 4) | replaced by `GlassSurface`'s rim |
| `tests/unit/tst_settings.cpp`, `tests/unit/tst_uicapabilities.cpp` | modify / create | C++ tests |
| `tests/qml/tst_theme.qml`, `tests/qml/tst_glass.qml` | modify / create | QML tests |
| `tests/integration/tst_qmlsmoke.cpp` | modify | floating layout geometry |
| `docs/design-system.md`, `docs/settings.md`, `docs/troubleshooting.md` | modify | docs |

---

### Task 1: Settings key, `UiCapabilities`, Theme tokens and the `glassBlur` switch

**Files:**
- Modify: `shared/include/nexora/shared/Settings.h` (next to `UiReducedMotion`, line ~48), `shared/src/Settings.cpp:187`
- Modify: `apps/console-ui/src/SettingsController.h:50,97-98`, `apps/console-ui/src/SettingsController.cpp:143`
- Create: `apps/console-ui/src/UiCapabilities.h`, `apps/console-ui/src/UiCapabilities.cpp`
- Modify: `apps/console-ui/src/UiBootstrap.cpp`, `apps/console-ui/CMakeLists.txt`
- Modify: `apps/console-ui/qml/Theme.qml`, `apps/console-ui/qml/Main.qml:570-580`
- Test: `tests/unit/tst_settings.cpp:243-273`, create `tests/unit/tst_uicapabilities.cpp` (+ `tests/unit/CMakeLists.txt`), `tests/qml/tst_theme.qml`

**Interfaces:**
- Produces: `Settings::UiReduceTransparency` (`"ui.reduceTransparency"`); `SettingsController.reduceTransparency` (bool, read/write, `changed` signal); QML singleton `UiCapabilities` (module `Nexora.Console`) with `bool gpuEffects` (NOTIFY `gpuEffectsChanged`) and C++ `static bool UiCapabilities::decide(QSGRendererInterface::GraphicsApi api, const QString &glRenderer, const QByteArray &disableEnv)` and `static UiCapabilities *instance()`, `void setGpuEffects(bool)`; Theme: `reduceTransparency`, `gpuEffects`, `sessionActive` (writable bools), `glassBlur` (readonly), tokens `glassBlurRadius`, `glassSaturation`, `glassFillRegular`, `glassFillRegularActive`, `glassFillClear`, `glassFillProminent`, `glassRim`, `glassRimStrong`, `glassHighlight`, `glassShadow`, `radiusXL`.

- [ ] **Step 1: Failing settings test.** In `tst_settings.cpp::newUiInputAudioLogKeysHaveDefaultsAndValidate` add after the `UiReducedMotion` default check:

```cpp
        QCOMPARE(s.boolValue(Settings::UiReduceTransparency), false);
```
and after the `UiReducedMotion` rejection:
```cpp
        QVERIFY(!s.setValue(Settings::UiReduceTransparency, QStringLiteral("on"), &error));
        QVERIFY(s.setValue(Settings::UiReduceTransparency, true));
```

- [ ] **Step 2: Failing capability test.** Create `tests/unit/tst_uicapabilities.cpp` and register it in `tests/unit/CMakeLists.txt` the same way `tst_settings` is registered (same helper/macro, linking `nexora_ui`):

```cpp
#include "UiCapabilities.h"

#include <QSGRendererInterface>
#include <QtTest>

using nexora::ui::UiCapabilities;

// The GPU glass path is only allowed on a hardware OpenGL/Vulkan/... renderer.
class TestUiCapabilities : public QObject
{
    Q_OBJECT
private slots:
    void hardwareRendererAllowsGpuEffects()
    {
        QVERIFY(UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("Mesa Intel(R) UHD Graphics 620"), {}));
        QVERIFY(UiCapabilities::decide(QSGRendererInterface::Vulkan, {}, {}));
    }
    void softwareRenderersDisableGpuEffects()
    {
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::Software, {}, {}));
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("llvmpipe (LLVM 17.0.6, 256 bits)"), {}));
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("softpipe"), {}));
    }
    void environmentVariableDisablesGpuEffects()
    {
        QVERIFY(!UiCapabilities::decide(QSGRendererInterface::OpenGL, QStringLiteral("AMD Radeon"), QByteArrayLiteral("1")));
    }
    void defaultsToFalseUntilProbed()
    {
        QVERIFY(!UiCapabilities::instance()->gpuEffects());
        QSignalSpy spy(UiCapabilities::instance(), &UiCapabilities::gpuEffectsChanged);
        UiCapabilities::instance()->setGpuEffects(true);
        QCOMPARE(spy.count(), 1);
        UiCapabilities::instance()->setGpuEffects(true);
        QCOMPARE(spy.count(), 1);
        UiCapabilities::instance()->setGpuEffects(false);
    }
};

QTEST_MAIN(TestUiCapabilities)
#include "tst_uicapabilities.moc"
```

- [ ] **Step 3: Failing Theme test.** In `tests/qml/tst_theme.qml` extend `cleanup()` with `Theme.reduceTransparency = false; Theme.gpuEffects = false; Theme.sessionActive = false` and add:

```qml
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
```

- [ ] **Step 4: Run, expect failures.** Run the WSL build command from Global Constraints. Expected: build error (unknown `Settings::UiReduceTransparency`, missing `UiCapabilities.h`). Then, after each piece below, re-run.

- [ ] **Step 5: Settings key.** `Settings.h`: `static inline const QString UiReduceTransparency = QStringLiteral("ui.reduceTransparency");`. `Settings.cpp` after the `UiReducedMotion` entry:

```cpp
        {UiReduceTransparency, false, QStringLiteral("Reduce transparency"), boolean()},
```
`SettingsController.h`: `Q_PROPERTY(bool reduceTransparency READ reduceTransparency WRITE setReduceTransparency NOTIFY changed)`, `bool reduceTransparency() const;`, `void setReduceTransparency(bool value) { set(QStringLiteral("ui.reduceTransparency"), value); }`. `SettingsController.cpp`:

```cpp
bool SettingsController::reduceTransparency() const
{
    return m_settings->boolValue(Settings::UiReduceTransparency);
}
```
If `Settings` keeps a list of known keys anywhere else (grep `UiReducedMotion` across `shared/` and `apps/`), add the new key there too.

- [ ] **Step 6: `UiCapabilities`.** `UiCapabilities.h`:

```cpp
#pragma once

#include <QByteArray>
#include <QObject>
#include <QSGRendererInterface>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class QQuickWindow;

namespace nexora::ui {

// Whether the console UI may use GPU effects (the Liquid Glass backdrop blur). False until the
// main window's scene graph is up, and always false on the software backend, on llvmpipe /
// softpipe and when NEXORA_NO_GLASS_BLUR is set. QML: UiCapabilities.gpuEffects.
class UiCapabilities : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool gpuEffects READ gpuEffects NOTIFY gpuEffectsChanged)

public:
    static UiCapabilities *instance();
    static UiCapabilities *create(QQmlEngine *, QJSEngine *);

    static bool decide(QSGRendererInterface::GraphicsApi api, const QString &glRenderer, const QByteArray &disableEnv);
    // Decides once the window's scene graph is initialised (renderer known).
    static void probe(QQuickWindow *window);

    bool gpuEffects() const { return m_gpuEffects; }
    void setGpuEffects(bool value);

signals:
    void gpuEffectsChanged();

private:
    explicit UiCapabilities(QObject *parent = nullptr);
    bool m_gpuEffects = false;
};

} // namespace nexora::ui
```
`UiCapabilities.cpp`:

```cpp
#include "UiCapabilities.h"

#include <QJSEngine>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>

namespace nexora::ui {

UiCapabilities::UiCapabilities(QObject *parent)
    : QObject(parent)
{
}

UiCapabilities *UiCapabilities::instance()
{
    static UiCapabilities *self = new UiCapabilities;
    return self;
}

UiCapabilities *UiCapabilities::create(QQmlEngine *, QJSEngine *engine)
{
    // One process-wide instance shared with C++; QML must not delete it.
    QJSEngine::setObjectOwnership(instance(), QJSEngine::CppOwnership);
    Q_UNUSED(engine)
    return instance();
}

bool UiCapabilities::decide(QSGRendererInterface::GraphicsApi api, const QString &glRenderer, const QByteArray &disableEnv)
{
    if (!disableEnv.isEmpty() || api == QSGRendererInterface::Software || api == QSGRendererInterface::Unknown)
        return false;
    const QString renderer = glRenderer.toLower();
    return !renderer.contains(QLatin1String("llvmpipe")) && !renderer.contains(QLatin1String("softpipe"));
}

void UiCapabilities::probe(QQuickWindow *window)
{
    if (!window)
        return;
    // Runs on the render thread: read the GL renderer string there, publish on the GUI thread.
    QObject::connect(
        window, &QQuickWindow::sceneGraphInitialized, window,
        [window] {
            const auto api = window->rendererInterface()->graphicsApi();
            QString glRenderer;
            if (api == QSGRendererInterface::OpenGL) {
                if (auto *context = QOpenGLContext::currentContext())
                    glRenderer = QString::fromLatin1(
                        reinterpret_cast<const char *>(context->functions()->glGetString(GL_RENDERER)));
            }
            const bool allowed = decide(api, glRenderer, qgetenv("NEXORA_NO_GLASS_BLUR"));
            QMetaObject::invokeMethod(instance(), [allowed] { instance()->setGpuEffects(allowed); }, Qt::QueuedConnection);
        },
        Qt::DirectConnection);
}

void UiCapabilities::setGpuEffects(bool value)
{
    if (m_gpuEffects == value)
        return;
    m_gpuEffects = value;
    emit gpuEffectsChanged();
}

} // namespace nexora::ui
```
Add both files to `SOURCES` and `nexora_apply_warnings` in `apps/console-ui/CMakeLists.txt`. If `QOpenGLFunctions` is not found with `Qt6::Gui` alone on 6.4, `find_package(Qt6 COMPONENTS OpenGL)` is **not** allowed (new dependency); instead use `context->getProcAddress("glGetString")` cast to `const unsigned char *(*)(unsigned int)` with `0x1F01` (GL_RENDERER). In `UiBootstrap.cpp::loadMainWindow`, before `engine.load(...)` add `QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, ...)`, or simply after the `window` is obtained: `UiCapabilities::probe(window);` (the scene graph initialises after the window is shown, so connecting right after load is early enough).

- [ ] **Step 7: Theme.** In `Theme.qml`, after `property bool reducedMotion: false` add (and document each in the header comment block, same style as the others):

```qml
    property bool reduceTransparency: false
    // UiCapabilities.gpuEffects (hardware renderer) and an emulator session being active; both
    // bound by Main.qml.
    property bool gpuEffects: false
    property bool sessionActive: false
    // Real backdrop blur (GlassEffectGpu) in effect; otherwise glass surfaces use the flat layers.
    readonly property bool glassBlur: glassActive && gpuEffects && !reduceTransparency && !sessionActive
```
After `glassSheen` add:

```qml
    // --- Liquid Glass ---------------------------------------------------------------------------
    readonly property real glassBlurRadius: Math.round(32 * scale)
    readonly property real glassSaturation: 0.35
    readonly property color glassFillRegular: reduceTransparency ? surfaceFloating : Qt.rgba(1, 1, 1, 0.10)
    readonly property color glassFillRegularActive: reduceTransparency ? surfaceHover : Qt.rgba(1, 1, 1, 0.15)
    readonly property color glassFillClear: reduceTransparency ? surfaceFloating : Qt.rgba(1, 1, 1, 0.05)
    readonly property color glassFillProminent: Qt.rgba(accent.r, accent.g, accent.b, reduceTransparency ? 0.85 : 0.30)
    readonly property color glassRim: Qt.rgba(1, 1, 1, 0.20)
    readonly property color glassRimStrong: Qt.rgba(1, 1, 1, 0.30)
    readonly property color glassHighlight: Qt.rgba(1, 1, 1, reduceTransparency ? 0.06 : 0.18)
    readonly property color glassShadow: Qt.rgba(0, 0, 0, 0.35)
```
And in Radii: `readonly property real radiusXL: Math.round(24 * scale)`.

- [ ] **Step 8: Main.qml bindings.** In `Component.onCompleted` after the `reducedMotion` binding:

```qml
        Theme.reduceTransparency = Qt.binding(() => window.settings.reduceTransparency)
        Theme.gpuEffects = Qt.binding(() => UiCapabilities.gpuEffects)
        Theme.sessionActive = Qt.binding(() => ["starting", "running", "stopping"].indexOf(window.app.state) >= 0)
```
Update the comment above them to mention the new inputs.

- [ ] **Step 9: Settings row.** In `SettingsScreen.qml`, right after the `settingReducedMotion` row, add a row with `objectName: "settingReduceTransparency"`, `label: "Reduce transparency"`, `description: "Makes glass panels solid and turns off background blur"`, using exactly the reducedMotion row's toggle pattern on `screen.settings.reduceTransparency`. If `tst_settings_ui.qml` or `tst_qmlsmoke.cpp` count the rows in that section, update the expected count.

- [ ] **Step 10: Run all tests, expect PASS.** Run the WSL command. Then `clang-format-18 -i` the changed C++ files.

- [ ] **Step 11: Commit.**

```bash
git add shared apps/console-ui tests
git commit -m "Liquid Glass groundwork: reduce-transparency setting, GPU capability probe, glass tokens"
```

---

### Task 2: `GlassSurface`, its two paths, and `GlassGroup`

**Files:**
- Create: `apps/console-ui/qml/GlassSurface.qml`, `GlassEffectFlat.qml`, `GlassEffectGpu.qml`, `GlassGroup.qml`
- Modify: `apps/console-ui/CMakeLists.txt` (add the four files to `NEXORA_QML_FILES`)
- Test: create `tests/qml/tst_glass.qml`

**Interfaces:**
- Consumes (Task 1): `Theme.glassBlur`, `Theme.glassActive`, the `glass*` tokens, `Theme.radiusXL`.
- Produces:
  - `GlassSurface` (Item): `property string variant: "regular"` (`regular` | `clear` | `prominent`), `property color tint: Theme.accent`, `property real radius: Theme.radiusLarge`, `property bool interactive: false`, `property bool active: false`, `property Item backdrop: null`, `property bool allowBlur: true`, `readonly property bool blurred` (true when the GPU file is loaded and ready), `readonly property color fillColor`. Typical use: `GlassSurface { anchors.fill: parent; z: -1; backdrop: … }` inside the owner.
  - `GlassGroup` (Item): `property Item backdrop`, `readonly property Item blurredSource` (null on the flat path). A `GlassSurface` whose ancestor is a `GlassGroup` uses the group's `backdrop` and `blurredSource` when its own `backdrop` is null.

- [ ] **Step 1: Failing test.** Create `tests/qml/tst_glass.qml`:

```qml
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

        // Forcing the switch on: with QtQuick.Effects (Qt >= 6.5) the GPU file loads, without it
        // the surface quietly stays flat. Under the software backend MultiEffect cannot render,
        // so only the loader outcome is checked.
        function test_gpuPathLoadsOrFallsBackSilently() {
            Theme.gpuEffects = true
            compare(Theme.glassBlur, true)
            tryVerify(() => plain.state === "gpu" || plain.state === "flat", 2000)
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
            Theme.gpuEffects = true
            compare(cyclic.effectiveBackdrop, null)
            compare(cyclic.blurred, false)
        }

        function test_outsideGlassSurfaceIsHidden() {
            Theme.glass = false
            compare(plain.visible, false)
        }
    }
}
```
Run `ctest --preset debug -R tst_console_qml` (WSL). Expected: FAIL, `GlassSurface is not a type`.

- [ ] **Step 2: `GlassEffectFlat.qml`.**

```qml
import QtQuick
import Nexora.Console

// Blur-free glass layers: fill, specular highlight and rim. Used by GlassSurface whenever the
// GPU path is off (software renderer, llvmpipe, reduce transparency, a running emulator).
Item {
    id: flat
    required property color fillColor
    required property real radius
    required property color rimColor

    Rectangle {
        anchors.fill: parent
        radius: flat.radius
        color: flat.fillColor
    }
    // Specular highlight: light from above, fading out by 45 % of the height.
    Rectangle {
        anchors.fill: parent
        radius: flat.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.glassHighlight }
            GradientStop { position: 0.45; color: "transparent" }
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: flat.radius
        color: "transparent"
        border.width: 1
        border.color: flat.rimColor
    }
}
```

- [ ] **Step 3: `GlassEffectGpu.qml`.** The only file that imports `QtQuick.Effects`.

```qml
import QtQuick
import QtQuick.Effects
import Nexora.Console

// GPU glass: the part of |backdrop| under this surface, blurred and saturated (MultiEffect),
// clipped to the rounded shape, with the flat layers on top. Loaded by GlassSurface only when
// Theme.glassBlur; on Qt < 6.5 this file fails to load and GlassSurface stays flat.
Item {
    id: gpu
    required property color fillColor
    required property real radius
    required property color rimColor
    required property Item backdrop
    // A GlassGroup's already blurred backdrop, if any: then this surface only crops it.
    property Item blurredSource: null

    // The surface's rectangle in the backdrop's coordinates.
    readonly property rect area: {
        void gpu.x; void gpu.y; void gpu.width; void gpu.height
        return backdrop ? gpu.mapToItem(backdrop, 0, 0, gpu.width, gpu.height) : Qt.rect(0, 0, 0, 0)
    }

    ShaderEffectSource {
        id: crop
        anchors.fill: parent
        sourceItem: gpu.blurredSource ? gpu.blurredSource : gpu.backdrop
        sourceRect: gpu.area
        live: true
        visible: false
    }
    Rectangle {
        id: shape
        anchors.fill: parent
        radius: gpu.radius
        visible: false
        layer.enabled: true
    }
    MultiEffect {
        anchors.fill: parent
        source: crop
        blurEnabled: !gpu.blurredSource
        blurMax: Math.max(1, Theme.glassBlurRadius)
        blur: 1.0
        saturation: gpu.blurredSource ? 0 : Theme.glassSaturation
        maskEnabled: true
        maskSource: shape
        shadowEnabled: true
        shadowColor: Theme.glassShadow
        shadowBlur: 0.6
        shadowVerticalOffset: Math.round(8 * Theme.scale)
    }
    GlassEffectFlat {
        anchors.fill: parent
        fillColor: gpu.fillColor
        radius: gpu.radius
        rimColor: gpu.rimColor
    }
}
```

- [ ] **Step 4: `GlassGroup.qml`.**

```qml
import QtQuick
import Nexora.Console

// One shared blur pass for several GlassSurfaces (≈ SwiftUI GlassEffectContainer): surfaces
// inside the group crop |blurredSource| instead of blurring the backdrop each. On the flat
// path it is a plain Item. |backdrop| must not contain the group.
Item {
    id: group
    property Item backdrop: null
    readonly property Item blurredSource: loader.item ? loader.item.output : null

    Loader {
        id: loader
        active: Theme.glassBlur && group.backdrop !== null
        source: "GlassGroupBlur.qml"
        onLoaded: item.backdrop = Qt.binding(() => group.backdrop)
        onStatusChanged: if (status === Loader.Error) console.debug("nexora.ui: glass blur unavailable, using flat glass")
    }
}
```
Also create `GlassGroupBlur.qml` (add it to `NEXORA_QML_FILES` too; second and last file importing `QtQuick.Effects`):

```qml
import QtQuick
import QtQuick.Effects
import Nexora.Console

// GlassGroup's shared pass: the whole backdrop blurred once, kept off-screen as |output|.
Item {
    id: pass
    property Item backdrop: null
    readonly property Item output: blurred

    ShaderEffectSource {
        id: source
        sourceItem: pass.backdrop
        width: pass.backdrop ? pass.backdrop.width : 0
        height: pass.backdrop ? pass.backdrop.height : 0
        live: true
        visible: false
    }
    MultiEffect {
        id: blurred
        width: source.width
        height: source.height
        source: source
        blurEnabled: true
        blurMax: Math.max(1, Theme.glassBlurRadius)
        blur: 1.0
        saturation: Theme.glassSaturation
        visible: false
        layer.enabled: true
    }
}
```
`GlassEffectGpu.qml` and `GlassGroupBlur.qml` are the only two files importing `QtQuick.Effects` (see Global Constraints).

- [ ] **Step 5: `GlassSurface.qml`.**

```qml
import QtQuick
import Nexora.Console

// The Liquid Glass background (≈ SwiftUI glassEffect). Put it inside a panel, filling it, below
// the content. Variants: "regular", "clear" (lighter, for cards and rows) and "prominent"
// (accent tinted, primary actions). |interactive| + |active| (focus/press, set by the owner)
// brighten the fill and the rim and lift the surface slightly.
//
// Theme.glassBlur && allowBlur: GlassEffectGpu blurs |backdrop| (or the enclosing GlassGroup's
// backdrop) under the surface; otherwise, or when that file cannot load, GlassEffectFlat. The
// backdrop must not be an ancestor of the surface; such a backdrop is ignored. Invisible
// outside the glass theme.
Item {
    id: surface
    property string variant: "regular"
    property color tint: Theme.accent
    property real radius: Theme.radiusLarge
    property bool interactive: false
    property bool active: false
    property Item backdrop: null
    property bool allowBlur: true

    readonly property Item group: {
        for (let p = surface.parent; p; p = p.parent) {
            if (p.blurredSource !== undefined && p.backdrop !== undefined)
                return p
        }
        return null
    }
    readonly property Item effectiveBackdrop: {
        const b = backdrop ? backdrop : (group ? group.backdrop : null)
        for (let p = surface.parent; p; p = p.parent) {
            if (p === b)
                return null
        }
        return b
    }
    readonly property bool lit: interactive && active
    readonly property color fillColor: variant === "prominent"
        ? (tint === Theme.accent ? Theme.glassFillProminent : Qt.rgba(tint.r, tint.g, tint.b, Theme.glassFillProminent.a))
        : variant === "clear" ? Theme.glassFillClear
        : lit ? Theme.glassFillRegularActive : Theme.glassFillRegular
    readonly property color rimColor: lit ? Theme.glassRimStrong : Theme.glassRim
    readonly property bool wantsGpu: Theme.glassBlur && allowBlur && effectiveBackdrop !== null
    readonly property bool blurred: state === "gpu"

    visible: Theme.glassActive
    state: wantsGpu && gpuLoader.status === Loader.Ready ? "gpu" : "flat"
    states: [State { name: "flat" }, State { name: "gpu" }]

    scale: lit && !Theme.reducedMotion ? 1.02 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

    Loader {
        id: gpuLoader
        anchors.fill: parent
        active: surface.wantsGpu
        source: "GlassEffectGpu.qml"
        onStatusChanged: if (status === Loader.Error) console.debug("nexora.ui: glass blur unavailable, using flat glass")
        onLoaded: {
            item.fillColor = Qt.binding(() => surface.fillColor)
            item.radius = Qt.binding(() => surface.radius)
            item.rimColor = Qt.binding(() => surface.rimColor)
            item.backdrop = Qt.binding(() => surface.effectiveBackdrop)
            item.blurredSource = Qt.binding(() => surface.group && !surface.backdrop ? surface.group.blurredSource : null)
        }
    }
    GlassEffectFlat {
        anchors.fill: parent
        visible: surface.state === "flat"
        fillColor: surface.fillColor
        radius: surface.radius
        rimColor: surface.rimColor
    }
}
```
Note: `required property` in a `Loader`-created item must be set at creation. If the engine complains ("Required property … was not initialized"), use `setSource()` with an initial-properties object in `onWantsGpuChanged`/`Component.onCompleted` instead of `onLoaded`, or drop `required` in `GlassEffectGpu.qml` and give defaults. Pick whichever passes the test without warnings; Qt prints QML warnings to the test log, so check it.

- [ ] **Step 6: Run, expect PASS.** `ctest --preset debug -R tst_console_qml`. Fix until there are no QML warnings from `Glass*.qml` in the output (`grep -i glass` the log).

- [ ] **Step 7: Commit.**

```bash
git add apps/console-ui/qml/Glass*.qml apps/console-ui/CMakeLists.txt tests/qml/tst_glass.qml
git commit -m "Add GlassSurface and GlassGroup: GPU backdrop blur with a flat fallback"
```

---

### Task 3: Floating glass chrome (header, tab bar, hint bar)

**Files:**
- Modify: `apps/console-ui/qml/Main.qml:195-260` (header/footer, `contentRoot`, `pageArea`), `Header.qml`, `NavTabBar.qml:40-52`, `HintBar.qml:4-20`
- Modify: the eight page screens and `GameDetailsScreen.qml` (content margins)
- Test: `tests/integration/tst_qmlsmoke.cpp`, `tests/qml/tst_navigation.qml` (adjust only if a layout assertion breaks)

**Interfaces:**
- Consumes: `GlassSurface`, `GlassGroup`, `Theme.radiusXL`, `Theme.glassActive`.
- Produces: in `Main.qml` an `Item { id: sceneLayer; objectName: "sceneLayer" }` holding the `Backdrop` and `pageArea`; a `GlassGroup { id: chrome; objectName: "glassChrome"; backdrop: sceneLayer }` holding `Header` (`objectName: "header"`), `NavTabBar` (`tabBar`) and `HintBar` (`hintBar`); `readonly property real chromeTop` and `chromeBottom` on `window` (the heights the bars cover, 0 in the docked themes). Every page screen gets `property real chromeTop: 0` and `property real chromeBottom: 0`, bound from `window`.

- [ ] **Step 1: Failing smoke test.** In `tst_qmlsmoke.cpp`, add a test slot next to the existing glass smoke tests (find them with `grep -n "glass" tests/integration/tst_qmlsmoke.cpp`) that, in the glass theme at 1280×720 and 1920×1080, after the Games tab is shown with the sandbox library:

```cpp
    void glassChromeFloatsWithoutCoveringContent()
    {
        // ... same setup as the existing glass smoke tests (theme "glass", window size) ...
        auto *chrome = item("glassChrome");
        auto *header = item("header");
        auto *hintBar = item("hintBar");
        auto *scene = item("sceneLayer");
        QVERIFY(chrome && header && hintBar && scene);
        // The scene (backdrop + pages) fills the window: content scrolls under the bars.
        QCOMPARE(scene->height(), ui.window->contentItem()->height());
        // Bars float: inset from the window edges.
        QVERIFY(header->mapToScene({0, 0}).y() > 0);
        QVERIFY(header->mapToScene({0, 0}).x() > 0);
        // The first game tile is below the tab bar, not under it.
        auto *tabBar = item("tabBar");
        const auto tiles = visibleItems(ui.window->contentItem(), QStringLiteral("gameTile_"));
        QVERIFY(!tiles.isEmpty());
        const qreal tabBottom = tabBar->mapToScene({0, tabBar->height()}).y();
        QVERIFY(tiles.first()->mapToScene({0, 0}).y() >= tabBottom);
    }
```
Use the file's real helper names (`item(...)`, the prefix helper at line ~269, the tile objectName prefix used by `GameTile`); adapt the snippet to them rather than adding new helpers. Add the same geometry check for the last Settings row: scroll the settings list to its end and verify that row's bottom is above `hintBar`'s top. Run: expect FAIL (`glassChrome` not found).

- [ ] **Step 2: Restructure `Main.qml`.** Remove `header:` / `footer:` from the `ApplicationWindow` and set `background: Rectangle { color: Theme.background }`. Inside `contentRoot`:

```qml
        // Everything the glass bars float over (and blur): the backdrop and the pages.
        Item {
            id: sceneLayer
            objectName: "sceneLayer"
            anchors.fill: parent
            Backdrop { objectName: "backdrop"; anchors.fill: parent }
            Item {
                id: pageArea
                anchors.fill: parent
                anchors.topMargin: Theme.glassActive ? 0 : window.dockedTop
                anchors.bottomMargin: Theme.glassActive ? 0 : window.dockedBottom
                // ... existing pages unchanged, each also gets:
                //     chromeTop: window.chromeTop; chromeBottom: window.chromeBottom
            }
        }
        GlassGroup {
            id: chrome
            objectName: "glassChrome"
            anchors.fill: parent
            backdrop: sceneLayer
            Header { id: headerBar; objectName: "header"; ... }
            NavTabBar { id: tabBar; objectName: "tabBar"; ... }
            HintBar { id: hintBar; objectName: "hintBar"; ... }
        }
```
Layout of the three bars:
- Glass: `Header` at `x = Theme.space4`, `y = Theme.space4`, `width = parent.width - 2 * Theme.space4`; `NavTabBar` directly below with `Theme.space2` gap, same width; `HintBar` at the bottom with `Theme.space4` inset. `window.chromeTop = tabBar.y + tabBar.height + Theme.space2`, `window.chromeBottom = hintBar.visible ? parent.height - hintBar.y + Theme.space2 : Theme.space4`.
- Docked (dark / high contrast): bars at full width, `y = 0` / stacked, `HintBar` at the very bottom, `chromeTop` = `chromeBottom` = 0, `dockedTop = header.height + tabBar.height`, `dockedBottom = hintBar.visible ? hintBar.height : 0` — this must reproduce today's layout exactly.
Move the `footer` `hints:` binding and `visible: window.settings.showHints` onto `hintBar` unchanged. Keep `tabBar` inside `contentRoot` (it already is) so focus handling does not change. Search `Main.qml` and the tests for `window.header`, `window.footer`, `header.` and `footer.` uses and update them.

- [ ] **Step 3: Bars.** `Header.qml`, `NavTabBar.qml`, `HintBar.qml`: in the glass theme draw `GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL }` and make their own background `Rectangle`s and 1 px border lines `visible: !Theme.glassActive` (Header and HintBar are `Rectangle`s: set `color: Theme.glassActive ? "transparent" : Theme.surface`). The surfaces get their backdrop from the `GlassGroup`.

- [ ] **Step 4: Page margins.** For each page (Home, Games, Systems, Favorites, Recent — `GameCollectionScreen`, Emulators, Settings, System — `DiagnosticsScreen`, and `GameDetailsScreen`), add `property real chromeTop: 0` / `chromeBottom: 0` and apply them to the screen's scrolling view (`Flickable` / `ListView` / `GridView`: `topMargin` / `bottomMargin`, so content scrolls under the bars) or, for non-scrolling content, to the top/bottom anchor margins. Make sure key navigation still scrolls the focused item fully into view (`positionViewAtIndex`, `ensureVisible`-style helpers must account for the margins — check each screen's existing helper).

- [ ] **Step 5: Run all tests.** Expected: PASS, including every existing smoke and navigation test. Docked themes must be pixel-for-pixel the same layout as before (existing geometry assertions catch this).

- [ ] **Step 6: Commit.**

```bash
git add apps/console-ui tests
git commit -m "Glass theme: floating glass header, tab bar and hint bar over the pages"
```

---

### Task 4: Glass panels, cards and rows; remove `GlassSheen`

**Files:**
- Modify: `ConfirmDialog.qml`, `GameOptionsDialog.qml`, `MetadataEditor.qml`, `OnScreenKeyboard.qml`, `UpdatePanel.qml`, `TutorialOverlay.qml`, `Toast.qml`, the launch-profile picker in `EmulatorsScreen.qml`, `EmulatorCard.qml`, `SettingRow.qml`, `GameTile.qml`, `GameBar.qml`, `QuickMenu.qml`
- Delete: `apps/console-ui/qml/GlassSheen.qml` (and its `NEXORA_QML_FILES` entry)
- Test: `tests/qml/tst_theme.qml`, `tests/qml/tst_glass.qml`

**Interfaces:**
- Consumes: `GlassSurface` (`variant`, `radius`, `backdrop`, `allowBlur`), `sceneLayer` (Task 3).

- [ ] **Step 1: Failing tests.** In `tst_glass.qml` add a `ConfirmDialog` and an `EmulatorCard`/`SettingRow` instance (use the same required properties `tst_theme.qml`/other tests already pass; grep for an existing instantiation) and:

```qml
        // Floating panels and cards draw GlassSurface; cards never take the GPU path.
        function test_panelsAndCardsUseGlassSurface() {
            Theme.glass = true
            Theme.gpuEffects = true
            const panelGlass = findType(dialog, "GlassSurface")
            verify(panelGlass !== null)
            const rowGlass = findType(row, "GlassSurface")
            verify(rowGlass !== null)
            compare(rowGlass.variant, "clear")
            compare(rowGlass.allowBlur, false)
        }
```
with a `findType(item, name)` helper walking `children` and matching `String(c).startsWith(name)`. Also remove the `GlassSheen` assertions from `tst_theme.qml` (they move here as the rim check: `compare(plainSurface.rimColor, Theme.glassRim)`). Run: FAIL.

- [ ] **Step 2: Floating panels.** In each dialog/panel listed above, replace the `color: Theme.surfaceFloating` background + `GlassSheen {}` with: keep `color: Theme.glassActive ? "transparent" : Theme.surfaceFloating` (the solid look stays for dark/high contrast) and add `GlassSurface { anchors.fill: parent; z: -1; radius: Theme.radiusXL; backdrop: <the window's sceneLayer> }`. Panels that are created inside `Main.qml` receive the backdrop through a new `property Item glassBackdrop: null` set from `Main.qml` (`glassBackdrop: sceneLayer`); panels used by tests without it simply stay flat (`effectiveBackdrop` null). Scrims stay as they are, but use `Qt.rgba(0, 0, 0, 0.40)` in glass. On the flat path these panels must keep today's dense fill (whatever lies behind must never compete with their text): add `property bool dense: false` to `GlassSurface` — when `dense && !blurred`, `fillColor` is `Theme.surfaceFloating` — and set `dense: true` on every floating panel. Add a test to `tst_glass.qml`: a `GlassSurface { dense: true }` on the flat path has `fillColor === Theme.surfaceFloating`.
- [ ] **Step 3: Cards and rows.** `EmulatorCard`, `SettingRow`, `GameTile` info plates: `GlassSurface { anchors.fill: parent; z: -1; variant: "clear"; allowBlur: false; interactive: true; active: <owner>.activeFocus }` replacing `GlassSheen`. Keep their focus ring.
- [ ] **Step 4: Game bar / quick menu.** Keep the `overlay*` colours; add `GlassSurface { …; allowBlur: false; variant: "clear" }` behind their main panels only for the highlight and rim (`visible` follows `Theme.glassActive` already).
- [ ] **Step 5: Delete `GlassSheen.qml`.** `grep -rn GlassSheen apps tests docs` must return only the docs lines you will change in Task 6.
- [ ] **Step 6: Run all tests, expect PASS. Commit.**

```bash
git add -A apps/console-ui tests
git commit -m "Glass panels, cards and rows draw GlassSurface; GlassSheen removed"
```

---

### Task 5: `GlassFocusPill` and `NxButton` glass styles

**Files:**
- Create: `apps/console-ui/qml/GlassFocusPill.qml` (+ `NEXORA_QML_FILES`)
- Modify: `NxButton.qml`, `NavTabBar.qml`, `QuickMenu.qml`, `SettingsScreen.qml` (section list)
- Test: `tests/qml/tst_glass.qml`, `tests/qml/tst_theme.qml`

**Interfaces:**
- Consumes: `GlassSurface`.
- Produces: `GlassFocusPill` (Item): `property Item target` (the item to cover, in any coordinate space), `readonly property bool moving`. `NxButton.style` (string): `"auto"` (default), `"glass"`, `"glassProminent"`, `"solid"`. `"auto"` resolves to `glassProminent` for `kind: "primary"`, `glass` for `normal`, `solid` for `danger`, and to `solid` whenever `!Theme.glassActive`.

- [ ] **Step 1: Failing tests** (append to `tst_glass.qml`):

```qml
    Row {
        id: pillRow
        y: 260
        spacing: 10
        Rectangle { id: a; width: 80; height: 40; color: "transparent" }
        Rectangle { id: b; width: 140; height: 40; color: "transparent" }
    }
    GlassFocusPill { id: pill; parent: pillRow; target: a }
    NxButton { id: playButton; kind: "primary"; text: "Play"; y: 310 }
    NxButton { id: otherButton; text: "Other"; x: 200; y: 310 }

        function test_pillMorphsToTarget() {
            Theme.reducedMotion = false
            pill.target = b
            tryCompare(pill, "x", b.x, 1000)
            tryCompare(pill, "width", b.width, 1000)
        }
        function test_pillJumpsUnderReducedMotion() {
            Theme.reducedMotion = true
            pill.target = a
            compare(pill.x, a.x)
            compare(pill.width, a.width)
        }
        function test_buttonStylesResolve() {
            Theme.glass = true
            compare(playButton.effectiveStyle, "glassProminent")
            compare(otherButton.effectiveStyle, "glass")
            Theme.glass = false
            compare(playButton.effectiveStyle, "solid")
        }
        // Prominent glass label on the flat fill over the darkest backdrop: >= 4.5:1.
        function test_prominentLabelContrast() {
            Theme.glass = true
            const fill = Theme.glassFillProminent
            const bg = Theme.background
            const mix = Qt.rgba(fill.r * fill.a + bg.r * (1 - fill.a), fill.g * fill.a + bg.g * (1 - fill.a),
                                fill.b * fill.a + bg.b * (1 - fill.a), 1)
            verify(contrast(Theme.text, mix) >= 4.5, "contrast " + contrast(Theme.text, mix))
        }
```
with a WCAG `contrast(c1, c2)` helper (relative luminance, sRGB linearisation) inside the TestCase. Run: FAIL.

- [ ] **Step 2: `GlassFocusPill.qml`.**

```qml
import QtQuick
import Nexora.Console

// One glass capsule per container that slides and resizes onto the focused item (≈ SwiftUI
// glassEffectID morphing). Put it in the container below its items and point |target| at
// the focused one. Jumps under reduced motion; the focus ring stays drawn by the items.
Item {
    id: pill
    property Item target: null
    readonly property bool moving: xAnim.running || wAnim.running
    readonly property rect area: {
        void target; if (target) { void target.x; void target.y; void target.width; void target.height }
        return target && parent ? target.mapToItem(parent, 0, 0, target.width, target.height) : Qt.rect(0, 0, 0, 0)
    }
    x: area.x
    y: area.y
    width: area.width
    height: area.height
    visible: target !== null && Theme.glassActive
    z: -1

    Behavior on x { NumberAnimation { id: xAnim; duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on width { NumberAnimation { id: wAnim; duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
    Behavior on height { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }

    GlassSurface {
        anchors.fill: parent
        radius: height / 2
        interactive: true
        active: true
        allowBlur: false
    }
}
```
(With `Theme.animNormal` 0 under reduced motion the behaviours finish immediately; if `compare` right after the change still sees the old value, add `enabled: !Theme.reducedMotion` to each `Behavior`.)

- [ ] **Step 3: Use the pill.** `NavTabBar`: one `GlassFocusPill { target: <current tab delegate> }` replacing the glass-theme selected-tab background (keep the docked-theme highlight as is, `visible: !Theme.glassActive`). `QuickMenu` and the Settings section list: same pattern on their focused/selected entry. Keep `Theme.focusRing` borders untouched.
- [ ] **Step 4: `NxButton` styles.** Add `property string style: "auto"` and `readonly property string effectiveStyle` resolving as in Interfaces. When `effectiveStyle` is `glass` / `glassProminent`: `color: "transparent"`, a `GlassSurface { anchors.fill: parent; z: -1; radius: height / 2; variant: effectiveStyle === "glassProminent" ? "prominent" : "regular"; interactive: true; active: button.highlighted; allowBlur: false }` and `radius: height / 2`; label colours stay `Theme.text`. Armed (confirm) and `danger` keep today's solid look. `solid` is today's code path, unchanged.
- [ ] **Step 5: Run all tests, expect PASS** (including `tst_theme`'s high-contrast button tests). **Commit.**

```bash
git add apps/console-ui tests
git commit -m "Glass focus pill that morphs between items; glass and glassProminent button styles"
```

---

### Task 6: Documentation

**Files:**
- Modify: `docs/design-system.md` (Colour section incl. line 67 "Glass without blur", component table line ~104, tests line ~138, limitations line ~153), `docs/settings.md`, `docs/troubleshooting.md`

- [ ] **Step 1:** `design-system.md`: replace "Glass without blur" with a "Liquid Glass" subsection covering: `Theme.glassBlur` and its four conditions, the GPU vs flat paths and the Qt 6.5 runtime fallback, the new tokens (table rows like the existing ones, with values), `GlassSurface` / `GlassGroup` / `GlassFocusPill` / `NxButton.style` (replace the `GlassSheen` row), the floating chrome and `chromeTop`/`chromeBottom`, the performance budget, and the SwiftUI → QML mapping table from the spec. Update the tests paragraph (`tst_glass.qml`, `tst_uicapabilities`, the smoke geometry check). In limitations: "The GPU blur path has not been verified on real Intel/AMD hardware yet; CI runs only the flat path."
- [ ] **Step 2:** `settings.md`: `ui.reduceTransparency` row (boolean, default `false`, Settings › Accessibility "Reduce transparency": solid glass panels, no background blur).
- [ ] **Step 3:** `troubleshooting.md`: "Glitches or slow UI with the glass theme → set `NEXORA_NO_GLASS_BLUR=1` or turn on Reduce transparency."
- [ ] **Step 4: Commit.**

```bash
git add docs
git commit -m "docs: Liquid Glass design system, reduce-transparency setting, blur escape hatch"
```
