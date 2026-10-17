# Nexora OS — Buildroot image (x86-64)

This document describes the `BR2_EXTERNAL` tree in [`buildroot-external/`](../buildroot-external) that turns the Nexora source tree into a bootable x86-64 disk image: Linux + BusyBox init + Mesa + Qt 6 (EGLFS/KMS) + `nexora-console-ui`.

> **Status (MVP 4, 2026-10-16):** the tree now builds an **A/B image**: an EFI system partition with GRUB and a boot-counting `grub.cfg`, two root slots that each carry their own kernel, and a persistent data partition for `/home`, plus the root update helper, boot confirmation and a recovery menu ([update-system.md](update-system.md), [recovery.md](recovery.md)). **This A/B image has not been built or booted yet.** <!-- TASK12-RESULT: replace with the build result (date, duration, disk.img size) and the QEMU boot result from scripts/image/qemu-boot-test.sh --> What has been checked: the shell helpers against scratch files (`test_update_helper`), `grub.cfg` with `grub-script-check`, all image scripts with `shellcheck -s sh`, and (before MVP 4) the configuration checks listed under [What was validated](#what-was-validated). An earlier single-root image (MVP 2/3) was reported built and booted in QEMU in commit `73a4df0`; that does not cover the A/B layout.

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
│   ├── grub.cfg                  GRUB2 (x86_64-efi): A/B slot selection with boot counting
│   ├── genimage.cfg              GPT layout: ESP, rootA, rootB, data
│   ├── post-build.sh             installs grub.cfg, fills /etc/nexora-release, adds the tty2 getty
│   ├── post-image.sh             initial grubenv and grubenv.bak, empty data.ext4, runs genimage
│   ├── update-keys/              README + .gitignore only; release public keys (*.pem) go here
│   └── rootfs-overlay/
│       ├── etc/fstab                         documents the ESP and data mounts (noauto)
│       ├── etc/init.d/S01nexora-data         mounts ESP (/boot/efi) and data (/home), bounded
│       ├── etc/init.d/S80nexora-powerd       power-request daemon
│       ├── etc/init.d/S85nexora-updated      update install daemon
│       ├── etc/init.d/S90nexora              console UI supervisor or recovery menu
│       ├── etc/init.d/S99nexora-bootok       trial-boot confirmation
│       ├── etc/default/nexora                optional settings for the scripts
│       ├── etc/nexora-release                version, build id, board (filled by post-build.sh)
│       ├── etc/udev/rules.d/60-nexora.rules
│       └── usr/libexec/nexora/{nexora-session,nexora-powerd,nexora-updated,nexora-bootok,
│                               nexora-recovery,nexora-functions}
├── scripts/
│   ├── check-defconfig.sh        defconfig symbol-survival check
│   └── check-kernel-fragment.sh  kernel fragment survival check
└── tests/test-update-helper.sh   host test of nexora-updated / nexora-bootok (CTest test_update_helper)
```

### The `nexora-os` package

- `NEXORA_OS_SITE = $(BR2_EXTERNAL_NEXORA_PATH)/..`, `NEXORA_OS_SITE_METHOD = local`: the package is rsynced from the checkout containing the external tree (excluding `.git`, `build/`, `build-*/` and `buildroot-external/`). Keep the Buildroot output directory **outside** the repository.
- CMake options: `-DBUILD_TESTING=OFF -DNEXORA_BUILD_FAKE_EMULATOR=OFF -DNEXORA_WITH_SDL3=OFF -DNEXORA_WARNINGS_AS_ERRORS=OFF -DQT_HOST_PATH=$(HOST_DIR)`. With testing and the fake emulator off, the top-level `CMakeLists.txt` skips `tests/` and the generated development configurations, and Qt Test is not required.
- `NEXORA_OS_SUPPORTS_IN_SOURCE_BUILD = NO` because the project rejects in-source builds.
- Selects Qt6 `qt6base` GUI + SQL + SQLite plugin (the game library uses `Qt6::Sql`/`QSQLITE`) and `qt6declarative` Quick (which also provides QuickControls2). Depends on `BR2_PACKAGE_QT6` and GCC ≥ 10 (C++20).
- Depends on OpenSSL (Ed25519/SHA-256 for updates). The build id is taken from the source tree with `git describe --always --dirty` and passed as `-DNEXORA_BUILD_ID` (the rsynced copy has no `.git`).
- Installs what the CMake install rules install: `/usr/bin/nexora-console-ui`, `/usr/bin/nexora-update-tool`, `/usr/share/nexora/emulators/example.json` (the default config directory resolved from `<bindir>/../share/nexora/emulators`), `/usr/share/nexora/systems.json` and `/usr/share/nexora/build-info` (version, build id, board, used by `post-build.sh` for `/etc/nexora-release`). A post-install hook copies every `board/nexora-x86_64/update-keys/*.pem` to `/usr/share/nexora/update-keys/`; with no key the image refuses all updates.
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

Or, equivalently, `scripts/image/build-image.sh` (clones the pinned tag into `~/nexora-br/buildroot`, output in `~/nexora-br/out`; extra arguments are passed to `make`, e.g. `nexora-os-rebuild all`).

Result: `images/disk.img`, GPT, about 5.2 GiB:

| # | GPT name | Size | Content |
| --- | --- | --- | --- |
| 1 | `nexora-esp` | 64 MiB vfat | `EFI/BOOT/bootx64.efi` (GRUB), `grub.cfg`, `grubenv` (initially `nexora_slot=A`, no trial); **no kernel** |
| 2 | `nexora-rootA` | 2048 MiB ext4 | root filesystem, kernel at `/boot/bzImage` |
| 3 | `nexora-rootB` | 2048 MiB ext4 | the same root filesystem image (genimage writes `rootfs.ext4` twice) |
| 4 | `nexora-data` | 1024 MiB ext4 | empty, label `nexora-data`, fixed filesystem UUID; mounted on `/home` |

Partition UUIDs are fixed constants in `genimage.cfg`; `grub.cfg` passes `root=PARTUUID=<slot UUID>`. The data filesystem is not resized to fill a larger disk. Also produced: `bzImage`, `rootfs.ext4` (the update-bundle image), `efi-part/`, `data.ext4`.

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

Automated check: `scripts/image/qemu-boot-test.sh [disk.img]` boots a temporary copy headless (`virtio-vga`, no network, KVM when available), captures the serial console to `qemu-serial.log` and requires `Starting nexora: OK` within `NEXORA_QEMU_TIMEOUT` seconds (default 300), then keeps watching the log for `NEXORA_QEMU_SETTLE` seconds (default 60) and fails if `nexora-session` gives up or restarts the UI `NEXORA_QEMU_MAX_RESTARTS` times or more (default 2; both are written to the kernel log and so reach the serial console), and finally powers the VM off through the QEMU monitor (`socat` needed for that step). It prints PASS with the settle time, or FAIL with the reason. It checks that the UI session started and stayed up, not that the UI rendered. Result for the A/B image: **not yet run** <!-- TASK12-RESULT -->.

Interactive runs:

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

Without host GL (Mesa then renders with llvmpipe on top of the KMS device): replace `-device virtio-vga-gl -display gtk,gl=on` with `-device virtio-vga -display gtk` (virtio-gpu 2D) or `-vga std` (bochs-drm). Without KVM, drop `accel=kvm` and replace `-cpu host` with `-cpu qemu64` (very slow; an earlier image rendered incomplete UI elements with llvmpipe under TCG with `-cpu max`, see commit `73a4df0`). Use `-audiodev alsa,...` or `-audiodev none,...` if PulseAudio/PipeWire-Pulse is not available.

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
| Bootloader | GRUB2 `x86_64-efi` as `EFI/BOOT/bootx64.efi` (removable-media path, needs no NVRAM entry), built-in modules `boot linux ext2 fat part_gpt part_msdos normal efi_gop search search_label search_fs_uuid echo test loadenv configfile regexp`; GRUB tools (`grub-editenv`) installed in the root filesystem; menu entries "Nexora OS", "Nexora OS (previous slot)", "Nexora OS (recovery menu)", "Nexora OS (maintenance shell: no console UI)" |
| Disk image | genimage: GPT with ESP, two root slots and a data partition (table above); kernel installed into each root filesystem (`BR2_LINUX_KERNEL_INSTALL_TARGET`) |
| Filesystem tools | e2fsprogs (`e2fsck` for the data partition) |
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

1. **`S01nexora-data`** — before udev, finds the partitions by GPT name on the boot disk through sysfs, mounts the ESP on `/boot/efi` (root only) and, after `e2fsck -p`, the data partition on `/home`; never blocks for more than 10 s and never fails the boot (details in [user-data.md](user-data.md#on-the-image)).
2. `S01syslogd`, `S02klogd`, `S02sysctl`, `S10udevd` (coldplug, module loading, `udevadm settle`), …
3. **`S80nexora-powerd`** — starts the root power-request daemon (below).
4. **`S85nexora-updated`** — starts the root update install daemon (FIFO `/run/nexora/update-request`, [update-system.md](update-system.md#the-root-helper-nexora-updated)).
5. **`S90nexora`** — with `nexora.recovery` on the kernel command line it starts the text recovery menu on tty1 instead ([recovery.md](recovery.md#recovery-menu)); otherwise, unless the kernel command line contains `nexora.noui` or `/etc/default/nexora` sets `NEXORA_AUTOSTART=no`:
   - creates `/var/log/nexora` (owner `nexora`, mode 0750; refuses if it is a symlink) and `XDG_RUNTIME_DIR=/run/user/1000` (owner `nexora`, mode 0700);
   - waits up to 10 s for a DRM device;
   - starts the supervisor `/usr/libexec/nexora/nexora-session` (root) with `start-stop-daemon` (pidfile `/var/run/nexora.pid`).
6. The supervisor runs, in a loop:

   ```
   /usr/bin/nexora-console-ui --log-file /var/log/nexora/console-ui.log [$NEXORA_CONSOLE_UI_ARGS]   (window mode comes from the display.mode setting, default fullscreen)
   ```

   as user **`nexora`** (via `start-stop-daemon --chuid nexora`, which also sets the supplementary groups), in a new session whose controlling terminal is **tty1** (owned by `nexora` while the UI runs, so Qt can switch it to graphics mode and mute the console keyboard), with the environment `QT_QPA_PLATFORM=eglfs`, `QT_QPA_EGLFS_INTEGRATION=eglfs_kms`, `XDG_RUNTIME_DIR=/run/user/1000`, `NEXORA_PLATFORM=embedded`, `HOME=/home/nexora`.
   - Before every start the supervisor increments `/run/nexora-session/ui-starts` (read by `nexora-bootok`).
   - Whenever the UI exits (crash, `Ctrl+Q`, …) it is restarted after a delay of 1, 2, 4, … up to 30 s.
   - A run of at least 15 s resets the backoff. After **5 consecutive runs shorter than 15 s** the supervisor gives up, logs it and switches to tty2 (`chvt 2`), so a broken UI cannot lock up the machine. Limits are configurable in `/etc/default/nexora`.
   - Each restart and giving up are also written to the kernel log (`/dev/kmsg`, e.g. `nexora-session: nexora-console-ui exited with status 1 after 2s; restarting in 1s`), so they reach the serial console.
7. **`S99nexora-bootok`** — in the background: if the running slot is on trial after an update, confirms it in `grubenv` once the UI has stayed up for the whole 45 s (supervisor and UI running at every 5 s check, UI start counter and supervisor PID unchanged), or records a rollback ([update-system.md](update-system.md#boot-confirmation-and-rollback-record)).

Logs:

| File | Written by | Content |
| --- | --- | --- |
| `/var/log/nexora/console-ui.log` | the UI (`--log-file`) | application log |
| `/run/nexora-session/session.log` | supervisor (root-owned dir) | start/exit/backoff messages and the UI's stdout/stderr — **early Qt errors** (e.g. EGLFS/KMS initialisation, missing QML modules) appear only here |
| `/var/log/messages` | syslogd | `nexora-session`, `nexora-powerd`, `nexora-updated`, `nexora-bootok` and `nexora-data` messages |

The dependency graph is drawn in [recovery.md](recovery.md#boot-and-service-dependencies).

`/var/log` is a symlink to `/tmp` (tmpfs) in Buildroot's default skeleton, so these logs do not survive a reboot. The UI rotates its own log file; the supervisor rotates `session.log` once (`.1`) when it exceeds 1 MiB at a (re)start.

### Recovery access

Login prompts are never disabled:

- **Serial console `ttyS0`** (115200 baud; `BR2_TARGET_GENERIC_GETTY_PORT`) — QEMU `-serial mon:stdio`, or a real serial port/adapter.
- **`tty2`** — added to `/etc/inittab` by `post-build.sh`. Reachable with `Ctrl+Alt+F2` when the UI is not running (and automatically after the supervisor gives up). Whether `Ctrl+Alt+F2` works *while* the Qt EGLFS UI runs depends on Qt's evdev keyboard handler and has not been tested.
- **GRUB entry "Nexora OS (maintenance shell: no console UI)"** — boots with `nexora.noui`, so tty1 shows the normal console and the logins above are available. Press any arrow key during the 3 s GRUB timeout to select it.
- **GRUB entry "Nexora OS (recovery menu)"** — boots with `nexora.recovery`: a keyboard-driven text menu (slots and versions, boot the previous slot, reset preferences, export diagnostics, restore the latest backup, repair the boot environment, reboot, shell). See [recovery.md](recovery.md).
- **GRUB entry "Nexora OS (previous slot)"** — boots the other root slot once.
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
| `nexora` (primary) | power-request and update-request FIFOs | `/run/nexora/power-request`, `/run/nexora/update-request` → `root:nexora 0620`; `/run/nexora/update-status` → `root:nexora 0640` |

UID/GID 1000 is fixed so that `XDG_RUNTIME_DIR=/run/user/1000` is stable. `/home/nexora` is on the data partition (created by `S01nexora-data` on first boot, mode 0700) and holds all user data ([user-data.md](user-data.md)). The groups `input` and `render` do not exist in Buildroot's default `/etc/group` and are created by the users table.

## What was validated

MVP 4 checks of the A/B tree (2026-10-16, build host only; no image built):

| Check | How | Result |
| --- | --- | --- |
| Update helper and boot confirmation | `test_update_helper` (CTest, label `system`): `nexora-updated --once` and `nexora-bootok` in test mode on 16 MiB scratch slot files and a scratch grubenv | pass: installs into B and A, refuses tampered/not-newer/too-large bundles, bad request lines, symlinked bundle files and installs during a trial boot, writes the verified image even if it is swapped after verification, replaces grubenv via a validated copy with `grubenv.bak`; confirmation, unhealthy UI, UI restarted during the window, rollback and manual-slot cases |
| GRUB script | `grub-script-check grub.cfg` (CTest `grub_cfg_syntax`) | syntax OK; the logic was **not executed** by GRUB |
| Shell scripts | `shellcheck -s sh` on all overlay and board scripts (CTest `shellcheck_image_scripts`) | clean |
| Defconfig / kernel fragment for the new symbols (`BR2_LINUX_KERNEL_INSTALL_TARGET`, `BR2_PACKAGE_E2FSPROGS`, GRUB modules and tools, NLS options) | `check-defconfig.sh`, `check-kernel-fragment.sh` against a Buildroot checkout | <!-- TASK12-RESULT: result of running both checks in Task 12 --> not yet run for the MVP 4 changes |
| Image build, QEMU boot | `scripts/image/build-image.sh`, `scripts/image/qemu-boot-test.sh` | **not yet built/booted** <!-- TASK12-RESULT --> |

Checks done before MVP 4 (single-root tree), in a container (Ubuntu 24.04 host, 4 cores, 15 GB RAM) against Buildroot 2026.02.3:

| Check | Command | Result |
| --- | --- | --- |
| Kconfig symbols | `scripts/check-defconfig.sh <buildroot> <out>` | all options of `nexora_x86_64_defconfig` present unchanged in the generated `.config` (0 problems). A negative test with two non-existent/unsatisfiable symbols reported both. |
| Every symbol exists in 2026.02.3 | grep of `Config.in` files | yes (Qt6, Mesa, eudev, GRUB2, Bootlin toolchain, linux-firmware, dejavu, alsa-utils, system options) |
| `savedefconfig` round trip | `make savedefconfig` | the committed defconfig *is* the `savedefconfig` output |
| Kernel fragment | `make linux-configure` + `scripts/check-kernel-fragment.sh` | all 68 options of `linux.fragment` present in the kernel `.config` of Linux 6.12.91 |
| Lint | `utils/check-package --br2-external` on every file of the tree | 0 warnings (covers `Config.in`, `.mk`, defconfig, `.hash`, init scripts incl. shellcheck, all shell scripts) |
| Supervisor logic | `nexora-session` run under BusyBox `sh` with a fake application (paths substituted) | correct arguments/environment; the app is a session leader with the VT as stdin; restart with 1/2/4 s backoff; gives up after the configured number of rapid failures; SIGTERM stops supervisor and app |
| Source downloads | `make source` (with `BR2_DOWNLOAD_FORCE_CHECK_HASHES=y`) | exit 0, no errors: every package source was fetched and matched its hash, including Linux 6.12.91 against the tree's own `patches/linux/linux.hash`, Qt 6.9.1, Mesa 26.0.1 and GRUB 2.12 (2026-10-11) |
| Partial image build | `make` from a clean output directory, `nexora_x86_64_defconfig`, source tree at commit d4c35b4 | 75 packages built and installed without an error (2026-10-11): the Bootlin x86-64 glibc toolchain 2025.08-1, the host tools (Python 3.14, Meson, Ninja, genimage, e2fsprogs, dosfstools, fakeroot, …), host LLVM 21.1.8 and target LLVM 21.1.8, BusyBox, eudev, util-linux, kmod, elfutils, ALSA lib/utils, fontconfig, freetype, HarfBuzz, DejaVu, libdrm, libpng, PCRE2, double-conversion, linux-firmware. Stopped by the session's 2-hour limit after ≈ 2 h while compiling target Clang; not resumed. |
| Power daemon logic | `nexora-powerd` under BusyBox `sh` with stubbed `poweroff`/`reboot`/`logger` | rejected `hello world`, `$(touch /tmp/pwn)` (not executed), a 200-byte line, `rebootx`; accepted `poweroff` → `sync` + `poweroff` |

A separate session recorded in commit `73a4df0` (2026-10-10, on `feature/mvp2`) a complete build of the single-root image and a QEMU boot into the UI (TCG, `-cpu qemu64`, virtio-gpu 2D + llvmpipe) with keyboard navigation, an emulator launch and a power-off via `nexora-powerd`; that text was superseded in this document by a later merge. It predates MVP 4 and is not evidence for the A/B image.

**Not validated (yet):**

- Building the MVP 4 A/B image (`nexora-os` 0.4.0 against Qt 6.9.1, OpenSSL, GRUB with the new modules and tools, genimage with four partitions).
- Booting it in QEMU or on any PC; first-boot data partition creation; the GRUB boot counting and rollback; the update helper on real block devices; the recovery menu; EGLFS/KMS start-up of the 0.4.0 UI; GPU drivers; gamepads under EGLFS; audio; VT switching while the UI runs.
- The opt-in full-image job of `buildroot.yml` has never been run on GitHub (the `validate` job has passed).

## Known limitations

- **UEFI only.** No legacy BIOS (CSM) boot. A hybrid BIOS+UEFI image would need GRUB `i386-pc`, a BIOS boot partition and a second `grub.cfg`.
- **No SDL3.** Buildroot 2026.02.x has no `sdl3` package; the app is built with `-DNEXORA_WITH_SDL3=OFF` (evdev backend only). When moving to a release that has it (2026.08+, or the 2027.02 LTS), add a `BR2_PACKAGE_NEXORA_OS_SDL3` sub-option that selects `BR2_PACKAGE_SDL3` and passes `-DNEXORA_WITH_SDL3=ON`.
- **Build cost.** iris, radeonsi and llvmpipe require LLVM (and iris additionally Clang/libclc for Mesa's precompiled shaders), so LLVM/Clang are built for host and target. A lighter QEMU-only variant could use virgl + softpipe without LLVM, but would not drive modern Intel/AMD GPUs.
- **No NVIDIA** support (nouveau is not enabled), no Vulkan, no Wi-Fi/network configuration. Updates are local signed bundles only ([update-system.md](update-system.md)); GRUB and the ESP are never updated by a bundle.
- **No automounter:** removable media must be mounted by hand from a root shell.
- **Fixed partition sizes:** 2 GiB per root slot (the update helper refuses larger images), 1 GiB data partition, not grown to fill the disk.
- **Development root access:** empty root password (see above).
- **Volatile logs:** `/var/log` and `/run` are tmpfs.
- **Single display:** EGLFS shows one full-screen window on the first connected output of the first DRM device; multi-GPU/multi-monitor selection is not configured (`QT_QPA_EGLFS_KMS_CONFIG` can be used if needed).
- Emulators: none are packaged; the shipped `example.json` runs `/bin/true`. Emulator definitions live on the root slot.
- `NEXORA_OS_VERSION` in `nexora-os.mk` (0.4.0) and the login banner `/etc/issue` (`BR2_TARGET_GENERIC_ISSUE="Nexora OS 0.4.0 (development image)"` in the defconfig) must be kept equal to `project(... VERSION ...)` by hand; `make-release.sh` refuses to package a `rootfs.ext4` whose `/etc/nexora-release` names another version ([release-process.md](release-process.md)); `/etc/nexora-release` is filled from the version compiled into the application, so it is always consistent with the binary.

## Requirements for the application

Checked against the application sources at the time of writing; items marked *open* are not (yet) handled on the C++/QML side:

1. **Platform:** runs with `QT_QPA_PLATFORM=eglfs` + `eglfs_kms`, OpenGL ES 2 (no desktop GL), no window manager; every window is full-screen and only one top-level window should be shown. *Open:* `InputController` posts key events to `QGuiApplication::focusWindow()`; verify on EGLFS that the window has focus after `showFullScreen()` (call `requestActivate()` if not).
2. **Keyboard:** Qt's built-in evdev keyboard/mouse handlers read `/dev/input/event*` directly (no libinput in the image); this needs the `input` group (provided).
3. **Fonts:** DejaVu via fontconfig; do not depend on other font families.
4. **Embedded detection:** `NEXORA_PLATFORM=embedded` is set in the environment and in `/etc/nexora-release` (also `NEXORA_VERSION`), matching what `SystemIntegration` reads.
5. **Power actions:** the client in `SystemIntegration.cpp` matches the protocol above (`O_WRONLY|O_NONBLOCK|O_CLOEXEC`, single write of `poweroff\n`/`reboot\n`, `ENXIO` reported as "not listening").
6. **Volume:** `SystemIntegration` runs `/usr/bin/amixer -q sset Master N%`; `alsa-utils` with `amixer` is in the image. Not every card has a `Master` control (USB/HDMI-only audio may not) — *open*: fall back gracefully.
7. **Exit = restart:** the supervisor restarts the UI on any exit; quitting is not a way to reach a shell on the device.
8. **Writable state:** `$HOME=/home/nexora` on the data partition — settings, library, history, backups and diagnostics ([user-data.md](user-data.md)). `XDG_RUNTIME_DIR` is provided.
9. **Version:** `/etc/nexora-release` carries the version, build id and board compiled into the application (`share/nexora/build-info`); the update helper reads `NEXORA_VERSION` from it.
