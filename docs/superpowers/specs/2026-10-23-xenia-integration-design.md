# Xenia (Xbox 360) integration — sub-project A: software integration

Date: 2026-10-23
Status: approved design (sub-project A of three)

## Context

Nexora launches every emulator through a JSON definition (`EmulatorConfig`, loaded by
`ConfigLoader`) that names a native Linux executable; `EmulatorManager` starts it with
`QProcess` and supervises it. Systems come from `config/systems/systems.json`, and the
library scanner assigns systems and emulator candidates **by file extension only**.

Xenia (xenia-canary) is the Xbox 360 emulator. Its titles are mostly disc images (`.iso`)
and bare executables (`.xex`). `.iso` is already declared by `ps2`, `psp` and `gamecube`, and
emulator candidates are not filtered by system, so without extra work Xenia would be offered
for PS2 discs and Xbox discs would need a manual system pick.

The whole effort is split into three sub-projects, each with its own spec and plan:

- **A — software integration** (this document): catalog entry, disc detection, emulator
  definition, tests, docs. Works on a developer Linux machine with Xenia installed at the
  configured path.
- **B — image**: Vulkan (Mesa anv/radv + loader), a Wayland kiosk compositor, and the display
  hand-off away from the Qt EGLFS UI.
- **C — packaging**: a Buildroot package for the prebuilt xenia-canary Linux binary under
  `/usr/lib/nexora/emulators/xenia`.

## Goals

1. An `xbox360` system exists in the catalog.
2. An Xbox 360 `.iso` is identified as `xbox360` automatically, without a manual system pick.
3. A non-Xbox `.iso` never gets `xbox360` as a system candidate and never gets Xenia as an
   emulator candidate.
4. A Xenia definition ships with Nexora; until sub-project C installs the binary, the
   existing health check reports it as not installed.

## Non-goals

- GOD/STFS (extension-less LIVE/CON/PIRS containers) — later.
- Quick Resume for Xenia (Xenia has no external save-state API).
- Pausing Xenia (SIGSTOP on a Vulkan process is untested and risky).
- Anything in the image (sub-projects B and C).

## Design

### 1. System catalog

Add to `config/systems/systems.json`:

```json
{ "id": "xbox360", "name": "Microsoft Xbox 360", "shortName": "Xbox 360", "extensions": [".iso", ".xex"] }
```

### 2. `XboxDiscProbe`

New unit in `apps/game-library` (`include/nexora/library/XboxDiscProbe.h`,
`src/XboxDiscProbe.cpp`), one free function:

```cpp
namespace nexora::library {
// True when the file holds an Xbox XDVDFS game partition.
bool isXboxDisc(const QString &path);
}
```

- Reads 20 bytes at `base + 0x10000` and compares them with `MICROSOFT*XBOX*MEDIA`.
- Partition bases checked, in order: `0x0` (xiso / rebuilt image), `0x2080000` (XGD3),
  `0xFD90000` (XGD2).
- A base whose signature would lie beyond the end of the file is skipped; no read ever
  goes past the file size.
- Any open or read error returns `false`. The probe never throws and never logs file
  contents.
- At most three 20-byte reads per file; only `.iso` files whose candidates include
  `xbox360` are probed.

### 3. Scanner integration

`ScanOptions` gains:

```cpp
QHash<QString, QStringList> emulatorSystems; // emulator id -> system ids it declares (empty = any)
```

In the walker, after `emulatorIds` and `systemIds` are set for a file:

- If the extension is `.iso` and `systemIds` contains `xbox360`, run `isXboxDisc`:
  - signature found → `systemIds = {"xbox360"}`;
  - not found → remove `xbox360` from `systemIds`.
- Then, only when the probe ran, filter `emulatorIds` to emulators whose declared systems
  intersect `systemIds`; an emulator with no declared systems, or missing from
  `emulatorSystems`, is kept (current behaviour for generic emulators).
- `.xex` needs no probe: only `xbox360` declares it.

The probe logic lives in one small helper (`classifyXboxDisc(ScannedFile&, const ScanOptions&)`)
so it is testable without a full scan. `LibraryController::updateScanOptions` fills
`emulatorSystems` from `m_app->enabledEmulators()`.

Database behaviour needs no change: `GameDatabase` already sets `system_id` when exactly one
system remains, and stores the candidates.

### 4. Xenia emulator definition

New `config/emulators/xenia.json`, installed next to `example.json` (both the build-tree copy
and the `install()` rule in `CMakeLists.txt`):

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

`xenia-launch` is a wrapper delivered by sub-project C. It checks for AVX (xenia-canary is
built with `-mavx`) and runs
`xenia_canary --fullscreen=true --gpu=vulkan --discord=false --storage_root=<data>/emulators/xenia <file>`,
so data-partition paths and Xenia flags stay out of the static definition. All keys are in
`ConfigLoader`'s allowed set. There is no `capabilities` entry, so Quick
Resume is not offered.

### 5. Tests (test-first)

- `tst_xboxdiscprobe` (unit, new): sparse temp files with the magic at each base → `true`;
  magic one byte off → `false`; file truncated before the signature → `false`; ISO9660-style
  file (`CD001` at `0x8001`) → `false`; empty file and missing file → `false`.
- `tst_libraryscanner` (extend): Xbox `.iso` → systems `{xbox360}`, emulators `{xenia}` only;
  PS2-style `.iso` → no `xbox360`, no `xenia`; generic emulator without systems stays a
  candidate in both cases; `.xex` → `xbox360` without probing.
- `tst_configloader` (extend): the shipped `config/emulators/xenia.json` loads without issues.
- `tst_systemcatalog` (extend): `xbox360` exists; `.xex` maps only to it.

### 6. Docs

- `docs/emulator-integration.md`: Xenia section (definition, not pausable, no Quick Resume,
  depends on sub-projects B and C on the image).
- `docs/game-library.md`: the `.iso` header probe and the emulator-candidate filter.
- `docs/roadmap.md`: Xenia sub-projects B and C.

## Error handling

- Unreadable or odd files fall back to "not an Xbox disc", which keeps today's behaviour for
  every other system.
- A missing Xenia binary surfaces through the existing `EmulatorHealth` "not installed" state;
  no special case.

## Risks

- Xenia on Linux is experimental upstream; real titles may fail even after B and C.
- XGD1 (original Xbox) discs also carry the XDVDFS signature and would be classified as
  `xbox360`. Xenia does not run original Xbox games; acceptable for now, documented.
