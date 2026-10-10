# Nexora OS — Buildroot image (x86-64)

This document describes the `BR2_EXTERNAL` tree in [`buildroot-external/`](../buildroot-external) that turns the Nexora source tree into a bootable x86-64 disk image: Linux + BusyBox init + Mesa + Qt 6 (EGLFS/KMS) + `nexora-console-ui`.

> **Status:** the configuration is validated (Kconfig symbol survival, kernel fragment, `check-package`) and a **complete image was built** from `nexora_x86_64_defconfig` (2026-10-10, `disk.img` 2.2 GB) and **booted in QEMU** (UEFI/OVMF, TCG, virtio-gpu 2D + llvmpipe) into the console UI; see [What was validated](#what-was-validated). It has **not** been booted on real hardware, and GPU drivers other than virtio-gpu, gamepads and audio remain untested.

## Contents

- [Pinned Buildroot version](#pinned-buildroot-version)
- [Layout](#layout)
- [Building](#building)
- [Running in QEMU](#running-in-qemu)
- [Writing to a USB stick / real PCs](#writing-to-a-usb-stick--real-pcs)
- [Image contents](#image-contents)
- [Start-up design and recovery access](#start-up-design-and-recovery-access)
- [Power requests (poweroff/reboot)](#power-requests-poweroffreboot)
- [Users and permissions](#users-and-permissions)
- [What was validated](#what-was-validated)
- [Known limitations](#known-limitations)
- [Requirements for the application](#requirements-for-the-application)

## Pinned Buildroot version

**Buildroot 2026.02.3** (tag `2026.02.3`, commit `679b9ead7620bbf193620d1ebf56f53c1764d37a`, released 2026-06-16).

- 2026.02.x is the current long-term-support branch (Buildroot's `.02` releases are maintained for a year). It is the newest 2026.02 point release at the time of writing; later releases (2026.05.x, 2026.08) are not LTS.
- Versions it brings: Qt **6.9.1**, Mesa **26.0.1**, eudev 3.2.14, Bootlin external toolchain "x86-64 glibc stable 2025.08-1" (GCC 14).
- It has **no `sdl3` package** (SDL3 first appears in 2026.08), so the image uses the evdev gamepad backend; see [Known limitations](#known-limitations).

The version is pinned in two places: this document and `BUILDROOT_VERSION` in [`.github/workflows/buildroot.yml`](../.github/workflows/buildroot.yml).

## Layout

```
buildroot-external/
├── external.desc                 name: NEXORA  (→ $(BR2_EXTERNAL_NEXORA_PATH))
├── external.mk, Config.in        include/source the packages below
├── package/nexora-os/            cmake-package building this repository from local source
├── configs/nexora_x86_64_defconfig
├── board/nexora-x86_64/
│   ├── linux.fragment            kernel options on top of x86_64_defconfig
│   ├── patches/linux/linux.hash  hash of the custom kernel tarball
│   ├── users.table               the "nexora" user
│   ├── grub.cfg                  GRUB2 (x86_64-efi) menu
│   ├── genimage.cfg              GPT disk image layout
│   ├── post-build.sh             installs grub.cfg, adds the tty2 recovery getty
│   ├── post-image.sh             fills in the root UUID, runs genimage
│   └── rootfs-overlay/
│       ├── etc/init.d/S80nexora-powerd
│       ├── etc/init.d/S90nexora
│       ├── etc/default/nexora    optional settings for S90nexora
│       ├── etc/nexora-release
│       ├── etc/udev/rules.d/60-nexora.rules
│       └── usr/libexec/nexora/{nexora-session,nexora-powerd}
└── scripts/
    ├── check-defconfig.sh        defconfig symbol-survival check
    └── check-kernel-fragment.sh  kernel fragment survival check
```

### The `nexora-os` package

- `NEXORA_OS_SITE = $(BR2_EXTERNAL_NEXORA_PATH)/..`, `NEXORA_OS_SITE_METHOD = local`: the package is rsynced from the checkout containing the external tree (excluding `.git`, `build/`, `build-*/` and `buildroot-external/`). Keep the Buildroot output directory **outside** the repository.
- CMake options: `-DBUILD_TESTING=OFF -DNEXORA_BUILD_FAKE_EMULATOR=OFF -DNEXORA_WITH_SDL3=OFF -DNEXORA_WARNINGS_AS_ERRORS=OFF -DQT_HOST_PATH=$(HOST_DIR)`. With testing and the fake emulator off, the top-level `CMakeLists.txt` skips `tests/` and the generated development configurations, and Qt Test is not required.
- `NEXORA_OS_SUPPORTS_IN_SOURCE_BUILD = NO` because the project rejects in-source builds.
- Selects Qt6 `qt6base` GUI + SQL + SQLite plugin (the game library uses `Qt6::Sql`/`QSQLITE`) and `qt6declarative` Quick (which also provides QuickControls2). Depends on `BR2_PACKAGE_QT6` and GCC ≥ 10 (C++20).
- Installs what the CMake install rules install: `/usr/bin/nexora-console-ui` and `/usr/share/nexora/emulators/example.json` (the default config directory resolved from `<bindir>/../share/nexora/emulators`). No extra install hooks.
- License: `NEXORA_OS_LICENSE = Unknown (licensing decision pending)`; there is no license file yet, so `make legal-info` will warn for this package.

## Building

Host: a Linux machine with the [Buildroot host requirements](https://buildroot.org/downloads/manual/manual.html#requirement-mandatory). On Debian/Ubuntu:

```sh
sudo apt-get install build-essential file cpio unzip rsync bc wget python3 perl libncurses-dev git
```

Resources for a cold build of this configuration: **~25–30 GB of disk** (LLVM and Clang are built twice, for the host and the target, because Mesa's iris/radeonsi/llvmpipe drivers need them; `linux-firmware` alone extracts to ~1.8 GB) and several hours on 4 cores.

```sh
# 1. Buildroot, outside the Nexora checkout
git clone --depth 1 --branch 2026.02.3 https://gitlab.com/buildroot.org/buildroot.git ~/br/buildroot

# 2. Configure (BR2_EXTERNAL is remembered in the output directory)
make -C ~/br/buildroot BR2_EXTERNAL=$PWD/buildroot-external O=$HOME/br/out nexora_x86_64_defconfig

# 3. Build
make -C ~/br/buildroot O=$HOME/br/out
```

Result: `~/br/out/images/disk.img` (GPT: 64 MiB EFI system partition with GRUB2 and the kernel, ext4 root of 2 GiB), plus `bzImage`, `rootfs.ext2` and `efi-part/`.

After changing the application sources, rebuild only the package and the image:

```sh
make -C ~/br/buildroot O=$HOME/br/out nexora-os-rebuild all
```

Configuration checks (fast, no build needed for the first one):

```sh
buildroot-external/scripts/check-defconfig.sh ~/br/buildroot /tmp/nexora-defconfig-check
make -C ~/br/buildroot O=$HOME/br/out linux-configure
buildroot-external/scripts/check-kernel-fragment.sh ~/br/out
(cd buildroot-external && find . -type f | sort | xargs ~/br/buildroot/utils/check-package --br2-external)
```

`check-package` needs `shellcheck`, `python3-magic` and `flake8` on the host.

### CI

[`.github/workflows/buildroot.yml`](../.github/workflows/buildroot.yml):

- **validate** (push/PR touching `buildroot-external/**`, and manual runs): shallow-clones the pinned Buildroot, runs `check-package --br2-external` on the whole tree and `scripts/check-defconfig.sh`.
- **image** (only `workflow_dispatch` with `full_build: true`): full build on `/mnt`, caches `dl/`, uploads `disk.img.xz` and the build log. A cold build **may exceed the 6-hour limit of GitHub-hosted runners**; it has not been run on GitHub yet.

## Running in QEMU

The image boots through UEFI only, so QEMU needs OVMF firmware (Ubuntu/Debian package `ovmf`; file names differ between distributions).

With virgl (3D acceleration through the host GPU; needs a host OpenGL display such as GTK or SDL):

```sh
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/nexora-vars.fd
qemu-system-x86_64 \
    -machine q35,accel=kvm -cpu host -smp 2 -m 2048 \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=/tmp/nexora-vars.fd \
    -drive file=$HOME/br/out/images/disk.img,format=raw,if=virtio \
    -device virtio-vga-gl -display gtk,gl=on \
    -device qemu-xhci -device usb-kbd -device usb-tablet \
    -audiodev pa,id=snd0 -device intel-hda -device hda-duplex,audiodev=snd0 \
    -serial mon:stdio
```

Without host GL (Mesa then renders with llvmpipe on top of the KMS device): replace `-device virtio-vga-gl -display gtk,gl=on` with `-device virtio-vga -display gtk` (virtio-gpu 2D) or `-vga std` (bochs-drm). Without KVM, drop `accel=kvm` and replace `-cpu host` with `-cpu qemu64` (very slow).

> **Do not use `-cpu max` without KVM.** Under TCG emulation with `-cpu max`, llvmpipe renders incorrectly: antialiased rounded-rectangle borders and whole items (the tab bar, the hint bar, the logo frame) are missing, with no warning in the log. The same image renders correctly with `-cpu qemu64`. This was observed only under TCG; it is an emulation artefact, not an image or application defect, but it was not investigated further. Use `-audiodev alsa,...` or `-audiodev none,...` if PulseAudio/PipeWire-Pulse is not available.

A USB game controller can be passed through with `-device usb-host,vendorid=0x045e,productid=0x028e` (example IDs: Xbox 360 pad; needs permissions on the host's `/dev/bus/usb` node).

The serial console (`-serial mon:stdio`) shows the kernel log and offers a root login (see [recovery access](#start-up-design-and-recovery-access)).

## Writing to a USB stick / real PCs

```sh
sudo dd if=$HOME/br/out/images/disk.img of=/dev/sdX bs=4M conv=fsync status=progress
```

Requirements: an x86-64 PC booting in **UEFI** mode (legacy BIOS/CSM boot is not supported, see [limitations](#known-limitations)); Secure Boot must be disabled (GRUB and the kernel are unsigned). The root filesystem is found by partition UUID, so the image boots from USB, SATA, NVMe or virtio disks.

## Image contents

| Area | Choice |
| --- | --- |
| Architecture / toolchain | x86-64 (generic `-march=x86-64`), Bootlin external toolchain, glibc, GCC 14 |
| Kernel | Linux **6.12.91** (6.12 LTS), `x86_64_defconfig` + [`linux.fragment`](../buildroot-external/board/nexora-x86_64/linux.fragment), `bzImage` with EFI stub |
| Bootloader | GRUB2 `x86_64-efi` as `EFI/BOOT/bootx64.efi` (removable-media path, needs no NVRAM entry); menu entries "Nexora OS" and "Nexora OS (recovery: no console UI)" |
| Disk image | genimage: GPT, ESP (FAT, 64 MiB, GRUB + `bzImage` + `grub.cfg`) and ext4 root (2 GiB, label `nexora-root`); `root=PARTUUID=<rootfs UUID>` |
| Init | BusyBox init, `/etc/init.d/S??*` scripts; eudev for `/dev` with module autoloading and hwdb |
| Graphics | Mesa 26.0.1: EGL, GBM, OpenGL ES; Gallium drivers **iris** (Intel Gen8+), **crocus** (Intel Gen4–7.5), **radeonsi** (AMD GCN and newer), **virgl** (QEMU virtio-gpu 3D), **llvmpipe** (software fallback, also used via kms_swrast on virtio-gpu 2D/bochs). No X11, no Wayland. |
| Qt | Qt 6.9.1: `qt6base` (GUI, EGLFS, OpenGL ES 2, fontconfig, HarfBuzz, PNG, SQL + SQLite), `qt6declarative` (Quick, QuickControls2), `qt6shadertools`; default QPA `eglfs`, used with the `eglfs_kms` integration |
| Fonts | DejaVu (all variants) via fontconfig |
| Firmware | `linux-firmware`: i915, xe, amdgpu |
| Kernel drivers | DRM: simpledrm (built in, boot framebuffer), i915, xe, amdgpu (+DC), virtio-gpu, bochs (modules). Input: evdev, USB HID, hid-generic, xpad, hid-sony, hid-playstation, hid-nintendo, hid-microsoft. USB: xHCI/EHCI/OHCI/UHCI, usb-storage, UAS. Storage: AHCI, NVMe, virtio-blk/scsi. Sound: snd-hda-intel (+ Realtek/HDMI/generic codecs), snd-usb-audio (modules). Serial 8250 console. |
| Audio user space | alsa-lib, alsa-utils (`amixer`, `aplay`) |

GPU drivers are modules on purpose: built-in amdgpu/i915 would request their firmware before the root filesystem is mounted. eudev loads them during `S10udevd`, and `S90nexora` waits up to 10 s for `/dev/dri/card*`.

## Start-up design and recovery access

Boot sequence (BusyBox init → `/etc/init.d/rcS`):

1. `S01syslogd`, `S02klogd`, `S02sysctl`, `S10udevd` (coldplug, module loading, `udevadm settle`), …
2. **`S80nexora-powerd`** — starts the root power-request daemon (below).
3. **`S90nexora`** — unless the kernel command line contains `nexora.noui` or `/etc/default/nexora` sets `NEXORA_AUTOSTART=no`:
   - creates `/var/log/nexora` (owner `nexora`, mode 0750; refuses if it is a symlink) and `XDG_RUNTIME_DIR=/run/user/1000` (owner `nexora`, mode 0700);
   - waits up to 10 s for a DRM device;
   - starts the supervisor `/usr/libexec/nexora/nexora-session` (root) with `start-stop-daemon` (pidfile `/var/run/nexora.pid`).
4. The supervisor runs, in a loop:

   ```
   /usr/bin/nexora-console-ui --log-file /var/log/nexora/console-ui.log [$NEXORA_CONSOLE_UI_ARGS]   (window mode comes from the display.mode setting, default fullscreen)
   ```

   as user **`nexora`** (via `start-stop-daemon --chuid nexora`, which also sets the supplementary groups), in a new session whose controlling terminal is **tty1** (owned by `nexora` while the UI runs, so Qt can switch it to graphics mode and mute the console keyboard), with the environment `QT_QPA_PLATFORM=eglfs`, `QT_QPA_EGLFS_INTEGRATION=eglfs_kms`, `XDG_RUNTIME_DIR=/run/user/1000`, `NEXORA_PLATFORM=embedded`, `HOME=/home/nexora`.
   - Whenever the UI exits (crash, `Ctrl+Q`, …) it is restarted after a delay of 1, 2, 4, … up to 30 s.
   - A run of at least 15 s resets the backoff. After **5 consecutive runs shorter than 15 s** the supervisor gives up, logs it and switches to tty2 (`chvt 2`), so a broken UI cannot lock up the machine. Limits are configurable in `/etc/default/nexora`.

Logs:

| File | Written by | Content |
| --- | --- | --- |
| `/var/log/nexora/console-ui.log` | the UI (`--log-file`) | application log |
| `/run/nexora-session/session.log` | supervisor (root-owned dir) | start/exit/backoff messages and the UI's stdout/stderr — **early Qt errors** (e.g. EGLFS/KMS initialisation, missing QML modules) appear only here |
| `/var/log/messages` | syslogd | `nexora-session` and `nexora-powerd` messages |

`/var/log` is a symlink to `/tmp` (tmpfs) in Buildroot's default skeleton, so these logs do not survive a reboot. The UI rotates its own log file; the supervisor rotates `session.log` once (`.1`) when it exceeds 1 MiB at a (re)start.

### Recovery access

Login prompts are never disabled:

- **Serial console `ttyS0`** (115200 baud; `BR2_TARGET_GENERIC_GETTY_PORT`) — QEMU `-serial mon:stdio`, or a real serial port/adapter.
- **`tty2`** — added to `/etc/inittab` by `post-build.sh`. Reachable with `Ctrl+Alt+F2` when the UI is not running (and automatically after the supervisor gives up). Whether `Ctrl+Alt+F2` works *while* the Qt EGLFS UI runs depends on Qt's evdev keyboard handler and has not been tested.
- **GRUB entry "Nexora OS (recovery: no console UI)"** — boots with `nexora.noui`, so tty1 shows the normal console and the logins above are available. Press any arrow key during the 3 s GRUB timeout to select it.
- From a root shell: `/etc/init.d/S90nexora stop|start|restart`; set `NEXORA_AUTOSTART=no` in `/etc/default/nexora` to disable autostart persistently.

**Root password:** this is a development image — `BR2_TARGET_GENERIC_ROOT_PASSWD` is empty, so **root logs in without a password** on ttyS0/tty2. There are no network services (no SSH, no telnet), so access requires physical/serial access. Before distributing an image, set a password with `make menuconfig` → *System configuration* → *Root password* (or add `BR2_TARGET_GENERIC_ROOT_PASSWD="..."` to a local copy of the defconfig), or run `passwd` on the device (the root filesystem is writable ext4). The `nexora` user cannot log in (password field `*`).

## Power requests (poweroff/reboot)

The UI runs unprivileged and cannot power off the machine itself. A narrowly scoped root helper provides exactly two operations.

**Server:** `/usr/libexec/nexora/nexora-powerd` (BusyBox shell, root), started by `/etc/init.d/S80nexora-powerd` (pidfile `/var/run/nexora-powerd.pid`).

**Endpoint:** FIFO **`/run/nexora/power-request`**, owner `root:nexora`, mode **0620** (only root and members of group `nexora` can write; nobody but root can read). Its directory `/run/nexora` is `root:nexora` 0750. The FIFO is recreated on every daemon start.

**Protocol:**

- The client writes one line, terminated by `\n`, in a single `write()`:
  - `poweroff\n` — sync and power off (`poweroff`, i.e. BusyBox init runs the shutdown scripts),
  - `reboot\n` — sync and reboot.
- The line must match exactly (case-sensitive, no surrounding spaces). Everything else — including an empty line, `poweroff now`, or `Poweroff` — is ignored and logged (`daemon.warning`, tag `nexora-powerd`, with a sanitised excerpt of at most 32 characters from `[A-Za-z0-9_.-]`). After an invalid request the daemon sleeps 1 s.
- Lines are read in chunks of at most 64 bytes; longer input is treated as several invalid lines.
- There is no reply. Success is observable only as the system shutting down.

**Client** (as implemented in `EmbeddedSystemIntegration::requestPower()`, `apps/system/src/SystemIntegration.cpp`):

```c++
int fd = ::open("/run/nexora/power-request", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
// fd < 0: ENOENT (daemon never started), ENXIO (no reader: daemon not running),
//         EACCES (process not in group nexora) -> report "power control unavailable"
static const char req[] = "poweroff\n";          // or "reboot\n"
ssize_t n = ::write(fd, req, sizeof req - 1);     // < PIPE_BUF: atomic, never interleaved
// n != sizeof req - 1 (e.g. EAGAIN if the pipe is full) -> report failure
::close(fd);
```

**Security reasoning:**

- *Minimal capability:* the only thing a compromised UI (or an emulator it launched, which runs as the same user) can do through this channel is power off or reboot — which a person at the console can do with the power button anyway. No arguments, paths, or commands cross the boundary.
- *No interpretation:* the daemon compares the line against two fixed words with a `case` statement; the text is never evaluated, expanded, used as a file name or passed to another program. The only use of the text is the sanitised log excerpt.
- *Access control by the kernel:* write permission on the FIFO (group `nexora`, mode 0620) and search permission on `/run/nexora` (0750) restrict who can send requests; `/run` is root-owned, so the path cannot be replaced by an unprivileged user.
- *No blocking of the client:* `O_NONBLOCK` makes `open()` fail immediately with `ENXIO` if the daemon is not running, instead of hanging the UI. The daemon keeps its own write end open (`exec 3<>`), so it never sees end-of-file and does not busy-loop.
- *Bounded resources:* fixed-size reads, a 1 s pause after each invalid request (limits log flooding), sanitised log output.
- Alternatives not chosen: running the UI as root (it refuses by design), `sudo`/setuid helpers (larger attack surface, extra packages), D-Bus + logind/polkit (not available with BusyBox init).

## Users and permissions

[`users.table`](../buildroot-external/board/nexora-x86_64/users.table):

```
nexora 1000 nexora 1000 * /home/nexora /bin/sh input,video,render,audio Nexora OS console user
```

| Group | Why | Device nodes (from `60-nexora.rules`) |
| --- | --- | --- |
| `input` | gamepads and keyboards via evdev (app + Qt's evdev input) | `/dev/input/event*`, `/dev/input/js*` → `root:input 0660` |
| `video` | KMS/DRM display | `/dev/dri/card*` → `root:video 0660` |
| `render` | GPU render nodes (Mesa) | `/dev/dri/renderD*` → `root:render 0660` |
| `audio` | ALSA for emulators | `/dev/snd/*` → `root:audio 0660` |
| `nexora` (primary) | power-request FIFO | `/run/nexora/power-request` → `root:nexora 0620` |

UID/GID 1000 is fixed so that `XDG_RUNTIME_DIR=/run/user/1000` is stable. `/home/nexora` (on the writable ext4 root) holds the user's settings. The groups `input` and `render` do not exist in Buildroot's default `/etc/group` and are created by the users table.

## What was validated

Done in a container (Ubuntu 24.04 host, 4 cores, 15 GB RAM) against Buildroot 2026.02.3:

| Check | Command | Result |
| --- | --- | --- |
| Kconfig symbols | `scripts/check-defconfig.sh <buildroot> <out>` | all options of `nexora_x86_64_defconfig` present unchanged in the generated `.config` (0 problems). A negative test with two non-existent/unsatisfiable symbols reported both. |
| Every symbol exists in 2026.02.3 | grep of `Config.in` files | yes (Qt6, Mesa, eudev, GRUB2, Bootlin toolchain, linux-firmware, dejavu, alsa-utils, system options) |
| `savedefconfig` round trip | `make savedefconfig` | the committed defconfig *is* the `savedefconfig` output |
| Kernel fragment | `make linux-configure` + `scripts/check-kernel-fragment.sh` | all 68 options of `linux.fragment` present in the kernel `.config` of Linux 6.12.91 |
| Lint | `utils/check-package --br2-external` on every file of the tree | 0 warnings (covers `Config.in`, `.mk`, defconfig, `.hash`, init scripts incl. shellcheck, all shell scripts) |
| Supervisor logic | `nexora-session` run under BusyBox `sh` with a fake application (paths substituted) | correct arguments/environment; the app is a session leader with the VT as stdin; restart with 1/2/4 s backoff; gives up after the configured number of rapid failures; SIGTERM stops supervisor and app |
| Power daemon logic | `nexora-powerd` under BusyBox `sh` with stubbed `poweroff`/`reboot`/`logger` | rejected `hello world`, `$(touch /tmp/pwn)` (not executed), a 200-byte line, `rebootx`; accepted `poweroff` → `sync` + `poweroff` |

Full build and boot (same container, 2026-10-10):

| Check | How | Result |
| --- | --- | --- |
| Complete image build | `make` with `nexora_x86_64_defconfig` (cold, about 6 h on 4 cores including host/target LLVM, Qt 6.9.1, Mesa) | succeeded: `images/disk.img` (2.2 GB GPT: 64 MiB EFI vfat + 2 GiB ext4), `bzImage` 12 MB; `nexora-os` compiles against cross-compiled Qt 6.9.1 |
| Image contents | `debugfs` on `rootfs.ext2` | user `nexora` (1000) in `input`, `video`, `render`, `audio`; inittab getty on ttyS0 and tty2; `/etc/nexora-release` says `NEXORA_PLATFORM=embedded` |
| Boot in QEMU | `qemu-system-x86_64 -machine q35 -cpu qemu64 -m 2048`, OVMF, virtio-blk, `virtio-vga`, TCG (no KVM) | GRUB → Linux 6.12.91 → BusyBox init → `nexora-session`; the UI starts under EGLFS/KMS on virtio-gpu with llvmpipe and shows the dashboard (screenshots taken with the QEMU monitor) |
| Privilege split | `ps` on the running system | `nexora-console-ui` runs as `nexora`; only `nexora-session` and `nexora-powerd` run as root; `/run/nexora/power-request` is `prw--w---- root:nexora` |
| Keyboard under EGLFS | QEMU `sendkey` (PS/2 keyboard) | PageDown/PageUp switch tabs, arrows move focus, Enter activates: the window has focus without further action |
| Emulator launch | Enter on the shipped "Example" emulator (`/bin/true`) | log: `Starting` → `Running` → `exited normally` |
| Power round trip | as user `nexora`: `echo rm-rf` and then `echo poweroff` into the FIFO | the invalid request is ignored; `poweroff` remounts the root read-only and powers the VM down (ACPI S5) |
| Volume | automatic at UI start | fails as expected in a VM without a sound card (`no usable mixer control`), and the UI continues |

**Not validated (yet):**

- Real hardware: booting from USB/SATA/NVMe on a PC; the i915, xe and amdgpu drivers; Mesa iris/radeonsi.
- Gamepads under the image (no controller was attached to the VM); audio output; the UI's own Restart/Power off buttons (the FIFO protocol was exercised from a shell, and the client is covered by unit tests); VT switching while the UI runs; KVM/virgl acceleration.
- The image job of the GitHub Actions workflow has not run on GitHub.

## Known limitations

- **UEFI only.** No legacy BIOS (CSM) boot. A hybrid BIOS+UEFI image would need GRUB `i386-pc`, a BIOS boot partition and a second `grub.cfg`.
- **No SDL3.** Buildroot 2026.02.x has no `sdl3` package; the app is built with `-DNEXORA_WITH_SDL3=OFF` (evdev backend only). When moving to a release that has it (2026.08+, or the 2027.02 LTS), add a `BR2_PACKAGE_NEXORA_OS_SDL3` sub-option that selects `BR2_PACKAGE_SDL3` and passes `-DNEXORA_WITH_SDL3=ON`.
- **Build cost.** iris, radeonsi and llvmpipe require LLVM (and iris additionally Clang/libclc for Mesa's precompiled shaders), so LLVM/Clang are built for host and target. A lighter QEMU-only variant could use virgl + softpipe without LLVM, but would not drive modern Intel/AMD GPUs.
- **No NVIDIA** support (nouveau is not enabled), no Vulkan, no Wi-Fi/network configuration, no automatic updates.
- **Development root access:** empty root password (see above).
- **Volatile logs:** `/var/log` and `/run` are tmpfs.
- **Single display:** EGLFS shows one full-screen window on the first connected output of the first DRM device; multi-GPU/multi-monitor selection is not configured (`QT_QPA_EGLFS_KMS_CONFIG` can be used if needed).
- Emulators: none are packaged; the shipped `example.json` runs `/bin/true`.
- The package version (`NEXORA_OS_VERSION = 0.2.0`, `/etc/nexora-release`) is set independently of `project(... VERSION ...)` in the top-level `CMakeLists.txt`.

## Requirements for the application

Checked against the application sources at the time of writing; items marked *open* are not (yet) handled on the C++/QML side:

1. **Platform:** runs with `QT_QPA_PLATFORM=eglfs` + `eglfs_kms`, OpenGL ES 2 (no desktop GL), no window manager; every window is full-screen and only one top-level window should be shown. `InputController` posts key events to `QGuiApplication::focusWindow()`; the window calls `requestActivate()` and was verified to receive keyboard input under EGLFS in QEMU.
2. **Keyboard:** Qt's built-in evdev keyboard/mouse handlers read `/dev/input/event*` directly (no libinput in the image); this needs the `input` group (provided).
3. **Fonts:** DejaVu via fontconfig; do not depend on other font families.
4. **Embedded detection:** `NEXORA_PLATFORM=embedded` is set in the environment and in `/etc/nexora-release` (also `NEXORA_VERSION`), matching what `SystemIntegration` reads.
5. **Power actions:** the client in `SystemIntegration.cpp` matches the protocol above (`O_WRONLY|O_NONBLOCK|O_CLOEXEC`, single write of `poweroff\n`/`reboot\n`, `ENXIO` reported as "not listening").
6. **Volume:** `SystemIntegration` runs `/usr/bin/amixer -q sset Master N%`; `alsa-utils` with `amixer` is in the image. Not every card has a `Master` control (USB/HDMI-only audio may not) — *open*: fall back gracefully.
7. **Exit = restart:** the supervisor restarts the UI on any exit; quitting is not a way to reach a shell on the device.
8. **Writable state:** `$HOME=/home/nexora` (persistent ext4) — default settings file and the SQLite game library (`QStandardPaths::GenericDataLocation` → `~/.local/share`) live there. `XDG_RUNTIME_DIR` is provided.
9. **Version:** `project(NexoraOS VERSION ...)` in `CMakeLists.txt` is independent of `NEXORA_OS_VERSION = 0.2.0` / `/etc/nexora-release`; keep them in sync when releasing.
