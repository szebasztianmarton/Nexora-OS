# Installation

Nexora OS 0.4.0 can be used in two ways:

1. **As a desktop application** on an existing Linux system (development and testing). This is the only way that has been exercised: the application, its tests and its headless commands run on Ubuntu 24.04.
2. **As a bootable disk image** for an x86-64 PC (the intended product). The image definition exists, but **the A/B image has not been built or booted yet**, so the steps below are the designed procedure, not a tested one. <!-- TASK12-RESULT: replace with "built and booted in QEMU on <date>" and any corrections found during the boot test -->

There are no published release artifacts yet ([release-process.md](release-process.md)).

## Contents

- [Desktop application](#desktop-application)
- [Disk image: requirements](#disk-image-requirements)
- [Disk image: writing it](#disk-image-writing-it)
- [First boot](#first-boot)
- [Adding emulators and games on the image](#adding-emulators-and-games-on-the-image)
- [Boot and services](#boot-and-services)
- [Updating and uninstalling](#updating-and-uninstalling)
- [Known installation limitations](#known-installation-limitations)

## Desktop application

From source (tested): follow [building.md](building.md), then run `build/<preset>/bin/nexora-console-ui`.

From a release tarball (`nexora-os-<version>-linux-x86_64.tar.gz`, produced by `scripts/release/make-release.sh`; not yet published and not tested as an installation workflow):

```sh
sudo apt install qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
    qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtquick-shapes
mkdir -p ~/nexora && tar -xzf nexora-os-0.4.0-linux-x86_64.tar.gz -C ~/nexora --strip-components=1
~/nexora/bin/nexora-console-ui --version
~/nexora/bin/nexora-console-ui --windowed
```

The tarball is an install tree for prefix `/usr` (`bin/`, `share/nexora/…`); the application finds its emulator definitions relative to its own directory, so it can be run from any directory. Run it as a regular user; add yourself to the `input` group for controllers. On the desktop, power control, system volume and updates are intentionally disabled ([building.md](building.md#development-build-vs-release-image)).

## Disk image: requirements

| Requirement | Why |
| --- | --- |
| x86-64 PC or mini PC | the only target (`nexora-x86_64`) |
| UEFI boot | GRUB is installed as `EFI/BOOT/bootx64.efi`; legacy BIOS/CSM is not supported |
| Secure Boot **disabled** | GRUB and the kernel are unsigned |
| Target disk or USB stick of at least 8 GB | the image is ≈ 5.2 GiB: 1 MiB gap + 64 MiB ESP + 2 × 2048 MiB root slots + 1024 MiB data |
| Intel (Gen4 and newer) or AMD (GCN and newer) graphics with Mesa, or a VM with virtio-gpu/bochs | the drivers in the image; see [hardware-compatibility.md](hardware-compatibility.md) — none verified on hardware |
| A keyboard (for setup and recovery) | adding library folders, the GRUB menu, the recovery menu and shell need one |

**Writing the image erases the whole target disk**, including any other operating system.

## Disk image: writing it

Build it yourself ([buildroot.md](buildroot.md#building), several hours) or use a release `nexora-os-<version>-x86_64-disk.img.xz` once one is published (verify it first: [release-process.md](release-process.md#verifying-a-release-as-a-user)).

```sh
# replace /dev/sdX with the target disk (check with lsblk!)
xz -dc nexora-os-0.4.0-x86_64-disk.img.xz | sudo dd of=/dev/sdX bs=4M conv=fsync status=progress
# or, for a self-built image:
sudo dd if=~/nexora-br/out/images/disk.img of=/dev/sdX bs=4M conv=fsync status=progress
```

The root filesystem is found by partition UUID (fixed per slot), so the disk boots from USB, SATA, NVMe or virtio. Do not write the same image to two disks attached to one machine: the partition UUIDs would collide.

To try it in a VM instead: `scripts/image/qemu-boot-test.sh` (automated) or the QEMU command in [buildroot.md](buildroot.md#running-in-qemu).

## First boot

Designed sequence (not yet observed on the A/B image):

1. The firmware starts GRUB from the ESP; after 3 s it boots "Nexora OS" (slot A).
2. `S01nexora-data` mounts the empty data partition on `/home` and creates `/home/nexora`.
3. udev loads the GPU and input drivers; `S90nexora` starts the session supervisor, which runs the console UI as user `nexora` on tty1 (EGLFS/KMS).
4. The UI creates its data directories under `/home/nexora` and shows the first-start tutorial.
5. Before handing the device to anyone, **set a root password**: log in as root on tty2 (`Ctrl+Alt+F2`, empty password on the development image) or on the serial console and run `passwd`. Or build the image with a password ([buildroot.md](buildroot.md#start-up-design-and-recovery-access)).

## Adding emulators and games on the image

The image ships **no emulator** (only `example.json`, which runs `/bin/true`) and no games. Today this needs a root shell:

- **Emulators:** a native Linux emulator built for the image (e.g. added as a Buildroot package) plus a definition in `/usr/share/nexora/emulators/` ([emulator-integration.md](emulator-integration.md#emulator-definitions)). Both live on the root slot, so the clean way is to include them in the image build; files added by hand to a slot are **lost when an update rewrites the other slot and it becomes active**.
- **Games:** copy them onto the data partition, for example `mount /dev/sdb1 /mnt && cp -r /mnt/roms /home/nexora/ && chown -R nexora:nexora /home/nexora/roms`, then add `/home/nexora/roms` in Settings › Library (typing the path needs a keyboard) or with a system defaults file. There is no automounter and no file manager.

## Boot and services

The boot chain and init-script order (with the bounded waits for optional hardware) is shown in [recovery.md](recovery.md#boot-and-service-dependencies). In short: GRUB → kernel of the selected slot → BusyBox init → `S01nexora-data` → syslog → `S10udevd` → `S80nexora-powerd` → `S85nexora-updated` → `S90nexora` (UI, recovery menu, or nothing) → `S99nexora-bootok`; logins on ttyS0 and tty2 are always available.

## Updating and uninstalling

- Updates: signed bundles installed into the inactive slot ([update-system.md](update-system.md)); user data stays on the data partition.
- Desktop: delete the extracted directory; user data is in `~/.config/nexora`, `~/.local/share/nexora`, `~/.local/state/nexora`, `~/.cache/nexora` ([user-data.md](user-data.md)).
- Image: reformat the disk. Back up `/home/nexora` first.

## Known installation limitations

- The A/B image has not been built or booted; this procedure is unverified.
- No graphical installer; no install-to-internal-disk option from a live USB (write the image directly to the target disk).
- No automounter, no network configuration, no package manager on the image.
- The data partition is fixed at 1 GiB and is not grown to fill the disk.
- Root has no password on the development image.
- UEFI only, Secure Boot off.
