# Xenia (Xbox 360) integration — sub-projects B and C: image and package

Date: 2026-10-23
Status: draft for review (decisions already taken: Buildroot 2026.08.x now; console UI runs under cage, EGLFS kept as fallback)
Depends on: [sub-project A](2026-10-23-xenia-integration-design.md) (catalog, disc probe, `xenia.json`)

## Why the image has to change

Research facts (xenia-canary `canary_experimental`, Buildroot tags 2026.02.3 / 2026.08.1, checked 2026-10-23):

- Xenia's Linux build is **X11-only**: GTK3 X11 backend, Vulkan surface `VK_KHR_xcb_surface` only, no Wayland code. It needs Xwayland or Xorg.
- It renders **only through Vulkan** (`libvulkan.so.1` is `dlopen`ed, so `ldd` does not show it).
- Mesa builds its Vulkan **X11 platform only when GLX is enabled** (`BR2_PACKAGE_MESA3D_OPENGL_GLX` → `MESA3D_NEEDS_X11`).
- **RADV** (`BR2_PACKAGE_MESA3D_VULKAN_DRIVER_AMD`) exists from Buildroot 2026.05; 2026.02.3 has only anv (Intel).
- The prebuilt binary is `xenia_canary_linux.AppImage` (~19 MB, rolling release per commit, tag = 7-char commit hash, sha256 from the GitHub API asset `digest`). It bundles GTK3, SDL2, cairo/pango and the X extension libraries; the system must provide glibc ≥ 2.38, libstdc++ (GLIBCXX_3.4.32), libX11/libX11-xcb/libxcb, fontconfig, freetype, harfbuzz, fribidi, zlib, alsa-lib, libgpg-error, libdrm, libgbm, libwayland-client and the Vulkan loader + ICDs. Built with `-mavx`: **the CPU must support AVX**.
- Today the UI runs on Qt EGLFS/KMS and owns the display; Qt cannot hand DRM master to a child process.

## Sub-project B — image and display

### B1. Buildroot 2026.08.1

- `scripts/image/build-image.sh`: `BUILDROOT_VERSION` default `2026.08.1`; `docs/buildroot.md`: new pin (tag + commit), the symbol check re-run against 2026.08.1, and the trade-off stated plainly: **2026.08 is not an LTS branch** (2026.02.x is); we move to the next `.02` LTS (2027.02) when it ships.
- Re-verify every existing defconfig symbol and `BR2_GLOBAL_PATCH_DIR` patch against 2026.08.1 (Mesa 26.1.x, Qt 6 version, eudev, GRUB2, toolchain). Any renamed symbol is fixed in the same commit.

### B2. Graphics stack additions (`nexora_x86_64_defconfig`)

| Purpose | Symbols |
| --- | --- |
| Vulkan drivers | `BR2_PACKAGE_MESA3D_VULKAN_DRIVER_INTEL`, `BR2_PACKAGE_MESA3D_VULKAN_DRIVER_AMD`, `BR2_PACKAGE_MESA3D_VULKAN_DRIVER_SWRAST` (lavapipe, for QEMU smoke tests only) |
| Vulkan X11 platform | `BR2_PACKAGE_XORG7`, `BR2_PACKAGE_MESA3D_OPENGL_GLX` |
| Loader | `BR2_PACKAGE_VULKAN_LOADER` (pulls `VULKAN_HEADERS`; builds XCB + Wayland WSI when those libs are present) |
| Compositor | `BR2_PACKAGE_CAGE`, `BR2_PACKAGE_WLROOTS_XWAYLAND`, `BR2_PACKAGE_XWAYLAND`, `BR2_PACKAGE_LIBEPOXY` (glamor), `BR2_PACKAGE_SEATD`, `BR2_PACKAGE_SEATD_DAEMON` |
| X keymaps (Xwayland runtime) | `BR2_PACKAGE_XKEYBOARD_CONFIG`, `BR2_PACKAGE_XAPP_XKBCOMP` |
| UI as Wayland client | `BR2_PACKAGE_QT6WAYLAND` |
| Xenia runtime libs not already present | `BR2_PACKAGE_FRIBIDI`, `BR2_PACKAGE_LIBGPG_ERROR` (others come with Qt/Xorg/ALSA; C's package `select`s all of them so the list is enforced in one place) |

Size estimate: ~100–130 MB extra in a 2048 MB root slot; measured with `du -sh output/target` in the build and written to `docs/buildroot.md`.

### B3. Session: UI under cage, EGLFS fallback

`/etc/default/nexora` gets `NEXORA_DISPLAY=cage` (default) or `eglfs`.

- New init script `S85seatd`: starts `seatd -g video` as root (BusyBox init, no logind). `nexora` is already in `video`, `input` and `render`.
- `nexora-session`, cage mode:
  - creates `XDG_RUNTIME_DIR=/run/user/<uid>` (owner `nexora`, mode 0700) before each start;
  - exports `QT_QPA_PLATFORM=wayland`, `LIBSEAT_BACKEND=seatd`, and unsets the `eglfs` variables;
  - runs `cage -d -s -- /usr/bin/nexora-console-ui --log-file …` instead of the UI directly (same `start-stop-daemon --chuid nexora` path, same restart/backoff, same `ui-starts` counter). `-d` hides client decorations, `-s` lets cage switch VTs.
- **Automatic fallback:** if cage mode reaches `NEXORA_MAX_RAPID_FAILURES`, the supervisor logs it to `/dev/kmsg`, switches to `eglfs` mode for the rest of the boot and resets the counter; only when EGLFS also keeps failing does it give up to the recovery login on tty2 (today's behaviour). In EGLFS mode Xenia cannot run (no X server); the UI shows the normal launch failure.
- Window behaviour (cage v0.2.x): a newly mapped toplevel (Xenia via Xwayland) is placed on top, maximised and focused; when it unmaps, focus returns to the UI. No hand-off code in the UI.

### B4. Emulator child environment

`EmulatorManager::launch()` currently passes the UI's whole environment to the emulator. Add a small, tested scrub: remove `QT_QPA_PLATFORM`, `QT_QPA_EGLFS_*`, `QT_QPA_PLATFORMTHEME` and `QT_QUICK_BACKEND` from the child's `QProcessEnvironment`; keep `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR` and everything else. Unit test in `tst_emulatormanager` with the fake emulator printing its environment.

### B5. Liquid Glass on Wayland

`UiCapabilities` reads the GL renderer the same way under the Wayland QPA. The existing renderer deny-list (llvmpipe, softpipe, virgl) still applies. Re-run `tst_uicapabilities` and the QML smoke tests with `QT_QPA_PLATFORM=wayland` under a headless compositor in CI if available; otherwise document as hardware-test item.

### B6. Verification

- `scripts/image/qemu-boot-test.sh`: the boot test asserts that cage is running and the UI connected (`pidof cage`, UI log line), plus a forced-failure run (`NEXORA_DISPLAY=cage` with cage removed) that must end in EGLFS mode, not the recovery VT.
- QEMU Vulkan smoke test: `vulkaninfo --summary` (from `BR2_PACKAGE_VULKAN_TOOLS`, test image only) lists lavapipe.
- Real hardware (Intel + AMD): manual checklist in `docs/testing.md` — UI under cage, controller input, glass blur, Xenia launch to its window and back.

## Sub-project C — xenia-canary package

### C1. Buildroot package `buildroot-external/package/xenia-canary/`

- `Config.in`: `BR2_PACKAGE_XENIA_CANARY`, `depends on BR2_x86_64`, `depends on BR2_TOOLCHAIN_USES_GLIBC`; `select`s the runtime libraries from B2 (Vulkan loader, Mesa Vulkan drivers via a comment/depends, libX11, libxcb, fontconfig, freetype, harfbuzz, fribidi, alsa-lib, libgpg-error, libdrm, mesa3d gbm, wayland). Registered in `buildroot-external/Config.in` / `external.mk`; enabled in the defconfig.
- `xenia-canary.mk` (generic-package):
  - `XENIA_CANARY_VERSION` = one pinned tag (chosen when C is built; the newest tag that passes the QEMU smoke test), `XENIA_CANARY_SITE = https://github.com/xenia-canary/xenia-canary/releases/download/$(XENIA_CANARY_VERSION)`, `XENIA_CANARY_SOURCE = xenia_canary_linux.AppImage`, extract step overridden (it is not an archive).
  - `xenia-canary.hash`: sha256 of the AppImage (from the API `digest`, re-verified by download) + sha256 of `LICENSE`. `BR2_DOWNLOAD_FORCE_CHECK_HASHES=y` already enforces it. The AppImage is mirrored to the project's download mirror (`BR2_BACKUP_SITE`) because old rolling tags may disappear.
  - Build step: compute the squashfs offset from the ELF header (`e_shoff + e_shnum * e_shentsize`, small helper script `xenia-appimage-offset`, unit-tested on a fixture) and unpack with `host-squashfs` `unsquashfs -o <offset>`. The AppImage is never executed at build time; no FUSE on the target.
  - Install: `usr/bin/xenia_canary` and `usr/lib/*` to `/usr/lib/nexora/emulators/xenia/` (keeps the `$ORIGIN/../lib` RUNPATH). Bundled pulse/systemd/codec libraries are kept for now; trimming is a later size optimisation.
  - `XENIA_CANARY_LICENSE = BSD-3-Clause` plus the bundled libraries' licenses (LGPL-2.1+ for GTK3/SDL2/glib etc.), `XENIA_CANARY_LICENSE_FILES` from the unpacked tree, so `make legal-info` is complete.

### C2. `xenia-launch` wrapper

`/usr/lib/nexora/emulators/xenia/xenia-launch` (POSIX sh, shellcheck-clean, installed by the package):

1. Exactly one argument (the game path) or exit 64 with a message.
2. `grep -qw avx /proc/cpuinfo` or exit 69 with "Xenia needs a CPU with AVX".
3. No `DISPLAY` → exit 69 with "Xenia needs the cage display mode" (EGLFS fallback case).
4. `storage=$XDG_DATA_HOME/nexora/emulators/xenia` (fallback `$HOME/.local/share/…`), `mkdir -p` with mode 0700.
5. `exec xenia_canary --fullscreen=true --gpu=vulkan --apu=alsa --hid=sdl --discord=false --storage_root="$storage" "$1"`.

Messages go to stderr, which `EmulatorManager` already captures into `<state>/emulator-logs/xenia.log` and shows on startup failure. Tested by a shell test (bats-style script under `tests/image/`, run by ctest like `shellcheck_image_scripts`) with a stub `xenia_canary` and fake `/proc/cpuinfo` via an env override used only by the test.

### C3. Storage

Xenia's config (`xenia-canary.config.toml`), content/saves and shader cache live under `/home/nexora/.local/share/nexora/emulators/xenia` on the 1 GiB data partition. Games themselves stay on external storage (library directories). Shader caches can grow; documented in `docs/user-data.md` with the cleanup path. Not included in backups by default (like Quick Resume states).

## Out of scope

- gamescope (not packaged in Buildroot; revisit for scaling/frame limiting).
- NVIDIA GPUs.
- GOD/STFS detection, Xenia Quick Resume, per-game Xenia config UI.
- Trimming the bundled libraries.

## Risks

- Moving off the LTS branch: more frequent Buildroot updates until 2027.02.
- Moving the UI from EGLFS to Wayland can change input latency, glass blur and the boot path; the automatic EGLFS fallback limits the blast radius.
- Xenia on Linux is experimental upstream; games may fail even with a correct stack.
- Rolling upstream tags may vanish; the mirror and the pinned hash protect reproducible builds.
