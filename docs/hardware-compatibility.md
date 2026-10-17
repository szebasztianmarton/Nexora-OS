# Hardware compatibility

This page separates what has actually been run from what is only expected to work because the drivers are in the image. **No physical PC, GPU, controller or audio device has been tested with Nexora OS 0.4.0**, and the A/B disk image has not been built or booted yet. Please report results (machine, GPU, controller model, connection, what worked) so this table can grow.

## Contents

- [Tested](#tested)
- [Expected (unverified)](#expected-unverified)
- [Unsupported](#unsupported)
- [Earlier image results](#earlier-image-results)
- [How to report a result](#how-to-report-a-result)

## Tested

Only these environments have run Nexora code for 0.4.0:

| Environment | What ran | Result | Date |
| --- | --- | --- | --- |
| Linux container: Ubuntu 24.04, 4 vCPU, Qt 6.4.2, GCC 13.3, Clang 18.1.3 | full build with `-Werror` (GCC and Clang), all 41 CTest tests on the offscreen Qt platform with the software renderer (no GPU, no display, no input devices), `qmllint`, `shellcheck`, `--check-config`, `--version` | 41/41 pass with both compilers | 2026-10-16 |
| WSL 2, Ubuntu 24.04 (Windows host), Qt 6.4.2, GCC 13 | the development environment named in the MVP 4 plan, used for the per-task build and test runs (same offscreen test suite) | the MVP 4 baseline audit there recorded 19/19 passing; later per-task results are not recorded in the repository, and this environment was not re-run for this report | 2026-10-11 … |
| Build host, scratch files | image shell helpers (`nexora-updated`, `nexora-bootok`) in test mode against 16 MiB scratch "slots" and a scratch `grubenv`; `grub-script-check` on `grub.cfg` | pass (`test_update_helper`, `grub_cfg_syntax`) | 2026-10-16 |
| QEMU (x86-64, UEFI/OVMF) | **A/B image not built or booted yet** <!-- TASK12-RESULT: add the QEMU boot result (accel, display device, time to "Starting nexora: OK", what was checked) --> | — | — |

The container runs as root, so two permission tests skip themselves there ([testing.md](testing.md#tests-skipped-as-root)). A virtual machine result is not a hardware result: nothing in this table says anything about real GPUs, controllers, audio devices or disks.

## Expected (unverified)

The image contains drivers for the following. They are expected to work because the kernel module, Mesa driver or firmware is included, but **none has been tested**.

| Area | Expected to work | Driver / component |
| --- | --- | --- |
| CPU / platform | x86-64 PCs and mini PCs with UEFI | generic `x86-64` toolchain, Linux 6.12.91 |
| Intel graphics | Gen8 and newer (Broadwell+) | `i915`/`xe` + Mesa `iris`, firmware `i915`/`xe` |
| Intel graphics (older) | Gen4–Gen7.5 | `i915` + Mesa `crocus` |
| AMD graphics | GCN and newer | `amdgpu` (+ DC) + Mesa `radeonsi`, firmware `amdgpu` |
| Virtual machines | QEMU virtio-gpu (2D and virgl 3D), bochs/std VGA | `virtio-gpu`, `bochs`, Mesa `virgl` / `llvmpipe` |
| Any KMS device without a 3D driver | software rendering | Mesa `llvmpipe` (slow) |
| Storage | SATA (AHCI), NVMe, virtio-blk/scsi, USB mass storage (incl. UAS) | built-in drivers |
| USB sticks (data) | FAT32/vfat | `vfat` (no exFAT or NTFS in the kernel) |
| Controllers | Xbox 360/One/Series (USB), PlayStation 3/4/5 (USB), Nintendo Switch Pro / Joy-Con, generic USB HID pads that report `BTN_GAMEPAD` | `xpad`, `hid-sony`, `hid-playstation`, `hid-nintendo`, `hid-microsoft`, `hid-generic` via evdev |
| Keyboard | USB HID keyboards | `usbhid`, Qt evdev keyboard |
| Audio | Intel HDA (Realtek and generic codecs, HDMI audio), USB audio | `snd-hda-intel`, `snd-usb-audio`, ALSA `amixer` for volume |
| Display | the first connected output of the first DRM device, its preferred mode | Qt EGLFS/KMS |

Button positions per controller family are listed in [controller-support.md](controller-support.md#button-names-and-controller-layouts); the risk of swapped west/north buttons on some evdev drivers is documented there.

## Unsupported

| Not supported | Reason |
| --- | --- |
| NVIDIA GPUs | neither the proprietary driver nor `nouveau` is in the image |
| ARM, RISC-V and 32-bit x86 | the image and toolchain are x86-64 only |
| Legacy BIOS / CSM boot | GRUB is built for `x86_64-efi` only |
| Secure Boot | GRUB and the kernel are unsigned |
| Vulkan | no Vulkan drivers or loader in the image |
| Wi-Fi, Ethernet configuration, Bluetooth | no network configuration and no Bluetooth stack (wireless controllers only through their USB mode or a USB dongle that presents a HID gamepad) |
| Multiple displays, display selection, mode switching | EGLFS uses one output; `display.resolution` only sizes the window on a desktop |
| Audio output selection | the output device list is read-only ("Selection not supported yet") |
| SDL3 gamepad backend on the image | Buildroot 2026.02.x has no SDL3 package |
| Real emulators | none packaged; none tested |

## Earlier image results

Before MVP 4, commit `73a4df0` (on `feature/mvp2`, 2026-10-10) recorded a complete build of the **single-root MVP 2/3 image** (application 0.2.0) and a boot in QEMU (UEFI/OVMF, TCG, `-cpu qemu64`, virtio-gpu 2D with llvmpipe) into the console UI, with keyboard navigation, an emulator launch of `/bin/true` and a power-off through `nexora-powerd`. That text was later superseded in `docs/buildroot.md` by a merge. It predates the A/B partition layout, the data partition, the update helpers and the MVP 4 application, so it is **not evidence for 0.4.0**; it only suggests that the Qt 6.9 EGLFS stack worked on virtio-gpu in a VM. It also noted that under TCG with `-cpu max` llvmpipe dropped UI elements while `-cpu qemu64` rendered correctly.

## How to report a result

Open an issue with: machine model, CPU, GPU (`lspci -nn | grep -i vga`), how Nexora was run (desktop build or image, QEMU or hardware), controller model and connection (`cat /proc/bus/input/devices`), and the result of the [controller checklist](controller-support.md#manual-test-checklist). Attach a diagnostics report (System tab › Export diagnostics, or `nexora-console-ui --export-diagnostics`); it is redacted, but read it before sharing.
