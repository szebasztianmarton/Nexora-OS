# Xenia Integration (sub-project A) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an `xbox360` system, detect Xbox discs among `.iso` files, and ship a Xenia emulator definition.

**Architecture:** A small `XboxDiscProbe` reads the XDVDFS signature. The library scanner calls it only for `.iso` files that could be `xbox360`, then narrows both system and emulator candidates. Xenia is one more JSON definition launched through a wrapper (delivered later by sub-project C).

**Tech Stack:** C++17/20, Qt 6 (Core, Test), CMake + Ninja, built and tested in WSL Ubuntu 24.04.

**Spec:** `docs/superpowers/specs/2026-10-23-xenia-integration-design.md`

## Global Constraints

- System id `xbox360`, name `Microsoft Xbox 360`, shortName `Xbox 360`, extensions `[".iso", ".xex"]`.
- Signature `MICROSOFT*XBOX*MEDIA` (20 bytes) at `base + 0x10000`, bases `0x0`, `0x2080000`, `0xFD90000`.
- The probe never reads past the file size; any error → `false`; it never logs file contents.
- Only `.iso` files whose system candidates include `xbox360` are probed.
- Emulator id `xenia`, executable `/usr/lib/nexora/emulators/xenia/xenia-launch`, arguments `["{file}"]`, `pausable: false`, `stopTimeoutMs: 10000`, no `capabilities`.
- `-Werror` CI build must stay warning-free; match surrounding code style (clang-format file in repo).
- All commands run from WSL: `wsl -d Ubuntu-24.04 -- bash -lc "cd '/mnt/d/nexora OS/Nexora-OS' && <cmd>"`.
- Build: `cmake --build --preset debug`. Single test: `ctest --preset debug -R <name> --output-on-failure`.

---

### Task 1: `XboxDiscProbe`

**Files:**
- Create: `apps/game-library/include/nexora/library/XboxDiscProbe.h`
- Create: `apps/game-library/src/XboxDiscProbe.cpp`
- Modify: `apps/game-library/CMakeLists.txt` (add both files to `nexora_library`)
- Create: `tests/unit/tst_xboxdiscprobe.cpp`
- Modify: `tests/unit/CMakeLists.txt` (register test)

**Interfaces:**
- Produces: `bool nexora::library::isXboxDisc(const QString &path);`

- [ ] **Step 1: Write the failing test** — `tests/unit/tst_xboxdiscprobe.cpp`:

```cpp
// XboxDiscProbe against synthetic sparse disc images.
#include "nexora/library/XboxDiscProbe.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using nexora::library::isXboxDisc;

class TestXboxDiscProbe : public QObject
{
    Q_OBJECT

private:
    static constexpr const char *Magic = "MICROSOFT*XBOX*MEDIA";

    // Writes `bytes` at `offset` and makes the file `size` bytes long (sparse).
    static QString image(const QTemporaryDir &dir, const QString &name, qint64 offset, const QByteArray &bytes,
                         qint64 size)
    {
        const QString path = dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || !f.resize(size) || !f.seek(offset) || f.write(bytes) != bytes.size())
            qFatal("cannot write test image");
        return path;
    }

private slots:
    void detectsSignatureAtEveryBase_data()
    {
        QTest::addColumn<qint64>("base");
        QTest::newRow("xiso") << qint64(0x0);
        QTest::newRow("xgd3") << qint64(0x2080000);
        QTest::newRow("xgd2") << qint64(0xFD90000);
    }

    void detectsSignatureAtEveryBase()
    {
        QFETCH(qint64, base);
        QTemporaryDir dir;
        const QString path = image(dir, QStringLiteral("game.iso"), base + 0x10000, Magic, base + 0x20000);
        QVERIFY(isXboxDisc(path));
    }

    void rejectsShiftedSignature()
    {
        QTemporaryDir dir;
        QVERIFY(!isXboxDisc(image(dir, QStringLiteral("a.iso"), 0x10001, Magic, 0x20000)));
    }

    void rejectsTruncatedSignature()
    {
        QTemporaryDir dir;
        // Only the first 10 bytes of the magic fit before the end of the file.
        const QString path = dir.filePath(QStringLiteral("t.iso"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.resize(0x10000));
        QVERIFY(f.seek(0x10000));
        f.write(QByteArray(Magic).left(10));
        f.close();
        QVERIFY(!isXboxDisc(path));
    }

    void rejectsIso9660()
    {
        QTemporaryDir dir;
        QVERIFY(!isXboxDisc(image(dir, QStringLiteral("ps2.iso"), 0x8001, "CD001", 0x100000)));
    }

    void rejectsEmptyAndMissingFiles()
    {
        QTemporaryDir dir;
        QVERIFY(!isXboxDisc(image(dir, QStringLiteral("empty.iso"), 0, QByteArray(), 0)));
        QVERIFY(!isXboxDisc(dir.filePath(QStringLiteral("missing.iso"))));
    }
};

QTEST_GUILESS_MAIN(TestXboxDiscProbe)
#include "tst_xboxdiscprobe.moc"
```

Register in `tests/unit/CMakeLists.txt` after `tst_librarymigration`:

```cmake
nexora_add_test(tst_xboxdiscprobe SOURCES tst_xboxdiscprobe.cpp LIBS nexora_library LABELS unit)
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build --preset debug`
Expected: FAIL — `nexora/library/XboxDiscProbe.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

`apps/game-library/include/nexora/library/XboxDiscProbe.h`:

```cpp
#pragma once

#include <QString>

namespace nexora::library {

// True when the file holds an Xbox XDVDFS game partition: "MICROSOFT*XBOX*MEDIA" at
// 0x10000 past one of the known partition starts (rebuilt xiso, XGD3, XGD2). Reads at most
// three 20-byte blocks, never past the end of the file; any error means "not an Xbox disc".
// Original Xbox (XGD1) discs carry the same signature and are reported as well.
bool isXboxDisc(const QString &path);

} // namespace nexora::library
```

`apps/game-library/src/XboxDiscProbe.cpp`:

```cpp
#include "nexora/library/XboxDiscProbe.h"

#include <QFile>

namespace nexora::library {

namespace {

constexpr char Magic[] = "MICROSOFT*XBOX*MEDIA";
constexpr qint64 MagicSize = sizeof(Magic) - 1;
constexpr qint64 SignatureOffset = 0x10000;
constexpr qint64 PartitionBases[] = {0x0, 0x2080000, 0xFD90000};

} // namespace

bool isXboxDisc(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const qint64 size = file.size();
    for (const qint64 base : PartitionBases) {
        const qint64 at = base + SignatureOffset;
        if (at + MagicSize > size)
            continue;
        if (!file.seek(at))
            return false;
        const QByteArray bytes = file.read(MagicSize);
        if (bytes.size() != MagicSize)
            return false;
        if (bytes == QByteArrayView(Magic, MagicSize))
            return true;
    }
    return false;
}

} // namespace nexora::library
```

Add to `apps/game-library/CMakeLists.txt` in `add_library(nexora_library ...)`:
`include/nexora/library/XboxDiscProbe.h` after `GameLibrary.h`, and `src/XboxDiscProbe.cpp` after `src/GameLibrary.cpp`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build --preset debug && ctest --preset debug -R tst_xboxdiscprobe --output-on-failure`
Expected: PASS (all 7 cases including the 3 data rows).

- [ ] **Step 5: Commit**

```bash
git add apps/game-library tests/unit/tst_xboxdiscprobe.cpp tests/unit/CMakeLists.txt
git commit -m "Library: XboxDiscProbe detects Xbox XDVDFS disc images"
```

---

### Task 2: `xbox360` system in the catalog

**Files:**
- Modify: `config/systems/systems.json` (add entry after `wii`)
- Modify: `tests/unit/tst_systemcatalog.cpp` (`builtInLoads`, `extensionLookup`)

**Interfaces:**
- Produces: system id `xbox360` with extensions `.iso`, `.xex`.

- [ ] **Step 1: Write the failing test** — in `builtInLoads`, change `>= 19` to `>= 20` and add `"xbox360"` to the id list. In `extensionLookup` add:

```cpp
        QCOMPARE(catalog.systemsForExtension(QStringLiteral(".xex")), QStringList{QStringLiteral("xbox360")});
        QVERIFY(catalog.systemsForExtension(QStringLiteral(".iso")).contains(QStringLiteral("xbox360")));
        QCOMPARE(catalog.find(QStringLiteral("xbox360"))->shortName, QStringLiteral("Xbox 360"));
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build --preset debug && ctest --preset debug -R tst_systemcatalog --output-on-failure`
Expected: FAIL on `xbox360` lookup.

- [ ] **Step 3: Add the entry** to `config/systems/systems.json` after the `wii` line, matching the file's one-line-per-system format:

```json
    { "id": "xbox360", "name": "Microsoft Xbox 360", "shortName": "Xbox 360", "extensions": [".iso", ".xex"] },
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build --preset debug && ctest --preset debug -R "tst_systemcatalog|tst_systemlistmodel|tst_gamelibrary" --output-on-failure`
Expected: PASS. If another test hard-codes the system count (search `grep -rn "19" tests | grep -i system`), update it to 20.

- [ ] **Step 5: Commit**

```bash
git add config/systems/systems.json tests/unit/tst_systemcatalog.cpp
git commit -m "Catalog: add the Microsoft Xbox 360 system"
```

---

### Task 3: Scanner narrows Xbox `.iso` candidates

**Files:**
- Modify: `apps/game-library/include/nexora/library/LibraryScanner.h` (`ScanOptions::emulatorSystems`, `classifyXboxDisc` declaration)
- Modify: `apps/game-library/src/LibraryScanner.cpp` (call after `file.systemIds = ...`, ~line 206)
- Modify: `apps/console-ui/src/LibraryController.cpp:185-200` (`updateScanOptions` fills `emulatorSystems`)
- Test: `tests/unit/tst_libraryscanner.cpp`

**Interfaces:**
- Consumes: `bool isXboxDisc(const QString &path)` (Task 1).
- Produces:
  - `ScanOptions::emulatorSystems` — `QHash<QString, QStringList>`, emulator id → system ids it declares (empty or absent = any).
  - `void LibraryScanner::classifyXboxDisc(ScannedFile &file, const ScanOptions &options);` (public static).

- [ ] **Step 1: Write the failing tests** — add to `tst_libraryscanner.cpp` (include `<QFile>` already present):

```cpp
    // An .iso declared by ps2 and xbox360, handled by pcsx2, xenia and a generic emulator.
    static ScanOptions isoOptions(const QString &root)
    {
        ScanOptions o;
        o.roots = {root};
        o.extensionMap = {{QStringLiteral(".iso"), {QStringLiteral("pcsx2"), QStringLiteral("xenia"),
                                                     QStringLiteral("generic")}},
                          {QStringLiteral(".xex"), {QStringLiteral("xenia")}}};
        o.extensionSystems = {{QStringLiteral(".iso"), {QStringLiteral("ps2"), QStringLiteral("xbox360")}},
                              {QStringLiteral(".xex"), {QStringLiteral("xbox360")}}};
        o.emulatorSystems = {{QStringLiteral("pcsx2"), {QStringLiteral("ps2")}},
                             {QStringLiteral("xenia"), {QStringLiteral("xbox360")}},
                             {QStringLiteral("generic"), {}}};
        return o;
    }

    static void xboxImage(const QString &path)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.resize(0x20000));
        QVERIFY(f.seek(0x10000));
        f.write("MICROSOFT*XBOX*MEDIA");
    }

    void xboxIsoIsNarrowedToXbox()
    {
        QTemporaryDir dir;
        xboxImage(dir.filePath(QStringLiteral("halo.iso")));
        const auto r = LibraryScanner::scan(isoOptions(dir.path()));
        QCOMPARE(r.files.size(), 1);
        QCOMPARE(r.files.first().systemIds, QStringList{QStringLiteral("xbox360")});
        QCOMPARE(r.files.first().emulatorIds, QStringList({QStringLiteral("xenia"), QStringLiteral("generic")}));
    }

    void otherIsoLosesXbox()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("gt4.iso")), QByteArray(0x9000, '\0'));
        const auto r = LibraryScanner::scan(isoOptions(dir.path()));
        QCOMPARE(r.files.size(), 1);
        QCOMPARE(r.files.first().systemIds, QStringList{QStringLiteral("ps2")});
        QCOMPARE(r.files.first().emulatorIds, QStringList({QStringLiteral("pcsx2"), QStringLiteral("generic")}));
    }

    void xexIsNotProbed()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("default.xex")));
        const auto r = LibraryScanner::scan(isoOptions(dir.path()));
        QCOMPARE(r.files.size(), 1);
        QCOMPARE(r.files.first().systemIds, QStringList{QStringLiteral("xbox360")});
        QCOMPARE(r.files.first().emulatorIds, QStringList{QStringLiteral("xenia")});
    }

    void isoWithoutXboxCandidateIsUntouched()
    {
        QTemporaryDir dir;
        xboxImage(dir.filePath(QStringLiteral("x.iso")));
        ScanOptions o = isoOptions(dir.path());
        o.extensionSystems[QStringLiteral(".iso")] = {QStringLiteral("ps2"), QStringLiteral("psp")};
        const auto r = LibraryScanner::scan(o);
        QCOMPARE(r.files.first().systemIds, QStringList({QStringLiteral("ps2"), QStringLiteral("psp")}));
        QCOMPARE(r.files.first().emulatorIds.size(), 3);
    }
```

- [ ] **Step 2: Run them to verify they fail**

Run: `cmake --build --preset debug`
Expected: FAIL — `'struct nexora::library::ScanOptions' has no member named 'emulatorSystems'`.

- [ ] **Step 3: Implement**

In `LibraryScanner.h`, inside `ScanOptions` after `extensionSystems`:

```cpp
    QHash<QString, QStringList> emulatorSystems; // emulator id -> system ids it declares (empty = any)
```

In the `LibraryScanner` class declaration (next to `titleFromFileName`):

```cpp
    // Narrows an .iso that could be an Xbox 360 disc: the disc header decides whether the
    // system is xbox360 alone or not xbox360 at all, and emulator candidates are reduced to
    // those that declare one of the remaining systems (or no systems). Other files are untouched.
    static void classifyXboxDisc(ScannedFile &file, const ScanOptions &options);
```

In `LibraryScanner.cpp`, add `#include "nexora/library/XboxDiscProbe.h"`, call `LibraryScanner::classifyXboxDisc(file, m_options);` right after `file.systemIds = m_options.extensionSystems.value(extension);`, and define:

```cpp
void LibraryScanner::classifyXboxDisc(ScannedFile &file, const ScanOptions &options)
{
    static const QString xbox360 = QStringLiteral("xbox360");
    if (file.extension != QStringLiteral(".iso") || !file.systemIds.contains(xbox360))
        return;
    if (isXboxDisc(file.path))
        file.systemIds = {xbox360};
    else
        file.systemIds.removeAll(xbox360);

    QStringList kept;
    for (const QString &id : std::as_const(file.emulatorIds)) {
        const QStringList declared = options.emulatorSystems.value(id);
        const bool any = declared.isEmpty();
        const bool matches = std::any_of(declared.cbegin(), declared.cend(),
                                         [&](const QString &s) { return file.systemIds.contains(s); });
        if (any || matches)
            kept.append(id);
    }
    file.emulatorIds = kept;
}
```

(add `#include <algorithm>` if not already present.)

In `LibraryController::updateScanOptions`, inside the existing `for (const auto &config : m_app->enabledEmulators())` loop, add as the first statement:

```cpp
        options.emulatorSystems.insert(config.id, config.systems);
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build --preset debug && ctest --preset debug -R "tst_libraryscanner|tst_gamelibrary|tst_appcontroller" --output-on-failure`
Expected: PASS, existing scanner cases unchanged.

- [ ] **Step 5: Commit**

```bash
git add apps/game-library apps/console-ui/src/LibraryController.cpp tests/unit/tst_libraryscanner.cpp
git commit -m "Library: Xbox disc header decides xbox360 for .iso files"
```

---

### Task 4: Ship the Xenia definition

**Files:**
- Create: `config/emulators/xenia.json`
- Modify: `CMakeLists.txt:102-104` (copy to build tree + install)
- Modify: `tests/unit/CMakeLists.txt` (give `tst_configloader` `NEXORA_SOURCE_DIR`)
- Test: `tests/unit/tst_configloader.cpp`

**Interfaces:**
- Consumes: `ConfigLoader::loadDirectory(dir, catalog)`, `SystemCatalog::builtIn()` (includes `xbox360` from Task 2).
- Produces: emulator id `xenia`.

- [ ] **Step 1: Write the failing test** — in `tests/unit/CMakeLists.txt` after the `tst_configloader` line:

```cmake
target_compile_definitions(tst_configloader PRIVATE NEXORA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
```

and in `tst_configloader.cpp` a new slot:

```cpp
    void shippedXeniaDefinitionLoads()
    {
        const auto catalog = nexora::shared::SystemCatalog::builtIn();
        const auto result
            = ConfigLoader::loadDirectory(QStringLiteral(NEXORA_SOURCE_DIR "/config/emulators"), &catalog);
        QVERIFY(!result.hasErrors());
        const auto it = std::find_if(result.emulators.cbegin(), result.emulators.cend(),
                                     [](const auto &e) { return e.id == QStringLiteral("xenia"); });
        QVERIFY(it != result.emulators.cend());
        QCOMPARE(it->executable, QStringLiteral("/usr/lib/nexora/emulators/xenia/xenia-launch"));
        QCOMPARE(it->arguments, QStringList{QStringLiteral("{file}")});
        QCOMPARE(it->systems, QStringList{QStringLiteral("xbox360")});
        QCOMPARE(it->extensions, QStringList({QStringLiteral(".iso"), QStringLiteral(".xex")}));
        QVERIFY(!it->pausable);
        QCOMPARE(it->stopTimeoutMs, std::optional<int>(10000));
        QVERIFY(!it->saveState.has_value());
    }
```

(`EmulatorConfig::stopTimeoutMs` is `std::optional<int>`, `saveState` is `std::optional<SaveStateCapability>`; add `#include <algorithm>` if missing.)

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build --preset debug && ctest --preset debug -R tst_configloader --output-on-failure`
Expected: FAIL at `it != result.emulators.cend()`.

- [ ] **Step 3: Implement** — `config/emulators/xenia.json`:

```json
{
  "id": "xenia",
  "name": "Xenia (Xbox 360)",
  "version": "1.0",
  "executable": "/usr/lib/nexora/emulators/xenia/xenia-launch",
  "arguments": ["{file}"],
  "extensions": [".iso", ".xex"],
  "systems": ["xbox360"],
  "pausable": false,
  "stopTimeoutMs": 10000,
  "enabled": true
}
```

In `CMakeLists.txt`, replace the example-only rules with:

```cmake
foreach(definition example.json xenia.json)
    configure_file(config/emulators/${definition} ${NEXORA_BUILD_CONFIG_DIR}/${definition} COPYONLY)
    install(FILES config/emulators/${definition}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/nexora/emulators)
endforeach()
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug -R "tst_configloader|tst_emulatorhealth|tst_appcontroller" --output-on-failure`
Expected: PASS. `./build/debug/bin/nexora-console-ui --check-config` (offscreen) reports no errors.

- [ ] **Step 5: Commit**

```bash
git add config/emulators/xenia.json CMakeLists.txt tests/unit/CMakeLists.txt tests/unit/tst_configloader.cpp
git commit -m "Emulators: ship the Xenia (Xbox 360) definition"
```

---

### Task 5: Docs and full verification

**Files:**
- Modify: `docs/emulator-integration.md` (new "Xenia (Xbox 360)" section)
- Modify: `docs/game-library.md` (header probe + emulator-candidate narrowing)
- Modify: `docs/roadmap.md` (Xenia sub-projects B and C)

- [ ] **Step 1: Write the docs** — content, in the files' existing style and language:
  - `emulator-integration.md`: the `xenia.json` definition; launched through `xenia-launch` (AVX check, `--fullscreen=true --gpu=vulkan --discord=false --storage_root=<data>/emulators/xenia`); not pausable; no Quick Resume; on the image it needs sub-projects B (Vulkan, cage + Xwayland) and C (package), until then health shows "not installed".
  - `game-library.md`: `.iso` files that could be Xbox 360 are probed for `MICROSOFT*XBOX*MEDIA` at `0x10000` past bases `0x0`, `0x2080000`, `0xFD90000`; a match makes the system `xbox360`, a miss removes it; emulator candidates are narrowed to emulators declaring a remaining system (or none); original Xbox discs also match; GOD/STFS is not detected yet.
  - `roadmap.md`: add "Xenia B: Buildroot 2026.08.x, Mesa anv+radv Vulkan, GLX, cage+Xwayland, UI as Wayland client (EGLFS fallback)" and "Xenia C: xenia-canary AppImage package, xenia-launch wrapper".

- [ ] **Step 2: Full verification**

Run: `cmake --preset ci && cmake --build --preset ci && ctest --preset ci`
Expected: zero warnings, all tests pass (previous baseline 43 + `tst_xboxdiscprobe` = 44).

- [ ] **Step 3: Commit**

```bash
git add docs
git commit -m "Docs: Xenia definition, Xbox disc detection, roadmap for image and package"
```
