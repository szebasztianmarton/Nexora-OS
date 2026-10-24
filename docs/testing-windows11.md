# Nexora OS tesztelése Windows 11-en

Ez a leírás lépésről lépésre bemutatja, hogyan lehet a Nexora OS-t egy Windows 11-es gépen lefordítani, tesztelni és kipróbálni.

> **Fontos:** a Nexora OS Linuxra készül (evdev, Unix-jelek, POSIX folyamatkezelés, Buildroot), ezért **natív Windowsos (MSVC/MinGW) fordítás nem támogatott**. Windows 11-en a **WSL 2** (Windows Subsystem for Linux) és egy Ubuntu 24.04 környezet a hivatalos út: ez ugyanaz a platform, amelyen a CI is fut. A grafikus felület a **WSLg**-n keresztül egy rendes Windows-ablakban jelenik meg.

## Tartalom

1. [Mit tartalmaz a repó?](#1-mit-tartalmaz-a-repó)
2. [Előfeltételek](#2-előfeltételek)
3. [WSL 2 és Ubuntu 24.04 telepítése](#3-wsl-2-és-ubuntu-2404-telepítése)
4. [A repó klónozása](#4-a-repó-klónozása)
5. [Fő alkalmazás (MVP 2): fordítás és automatizált tesztek](#5-fő-alkalmazás-mvp-2-fordítás-és-automatizált-tesztek)
6. [Fő alkalmazás: kézi tesztelés ablakban](#6-fő-alkalmazás-kézi-tesztelés-ablakban)
7. [Console UI prototípus: fordítás és tesztek](#7-console-ui-prototípus-fordítás-és-tesztek)
8. [Kontroller tesztelése](#8-kontroller-tesztelése)
9. [Bootolható image (Buildroot) építése és futtatása](#9-bootolható-image-buildroot-építése-és-futtatása)
10. [Hibaelhárítás](#10-hibaelhárítás)
11. [Tesztelési ellenőrzőlista](#11-tesztelési-ellenőrzőlista)

## 1. Mit tartalmaz a repó?

| Rész | Hely | Leírás |
| --- | --- | --- |
| **Fő alkalmazás (MVP 1–4)** | gyökérkönyvtár (`apps/`, `shared/`, `tests/`, `CMakePresets.json`) | Kontrollerrel vezérelhető dashboard (Home, Games, Emulators, Settings, Diagnostics), Emulator Manager, játékkönyvtár (SQLite), beállítások, evdev/SDL3 bemenet. 49 automatizált CTest-teszt. |
| **Buildroot image** | `buildroot-external/` | Bootolható x86-64 UEFI disk image, amely közvetlenül a fő alkalmazásba indul. |
| **Console UI prototípus** | `prototypes/console-ui/` | A `claude/nexora-console-ui` ágon készült, külön fejlesztett konzolfelület (Kezdőlap, Könyvtár, Kedvencek, Emulátorok, 8 beállítási kategória, első indítási varázsló, SDL2). Önálló CMake-projekt. |

## 2. Előfeltételek

- Windows 11 (22H2 vagy újabb ajánlott), rendszergazdai jogosultság a telepítéshez.
- A BIOS/UEFI-ben engedélyezett **hardveres virtualizáció** (Intel VT-x / AMD-V). Ellenőrzés: *Feladatkezelő → Teljesítmény → CPU → Virtualizáció: Engedélyezve*.
- Szabad lemezterület: kb. **10 GB** az alkalmazáshoz és a tesztekhez, további **30 GB** a Buildroot image-hez.
- RAM: legalább 8 GB (a Buildroot-építéshez 16 GB ajánlott).
- Frissített GPU-illesztőprogram (a WSLg grafikus gyorsításához).

## 3. WSL 2 és Ubuntu 24.04 telepítése

Nyiss egy **rendszergazdai PowerShell** ablakot:

```powershell
wsl --install -d Ubuntu-24.04
```

Indítsd újra a gépet, ha a telepítő kéri. Az első indításkor az Ubuntu kér egy felhasználónevet és jelszót. **Ne root felhasználóként dolgozz**: az alkalmazás rootként nem indul el, és két jogosultsági teszt rootként kimarad (lásd [testing.md](testing.md#tests-skipped-as-root)).

Ellenőrzés PowerShellben:

```powershell
wsl --version          # WSL 2.x, WSLg-vel
wsl -l -v              # Ubuntu-24.04, VERSION 2
```

Ha a `VERSION` értéke 1: `wsl --set-version Ubuntu-24.04 2`. Ha régi a WSL: `wsl --update`.

Ajánlott: a WSL-nek adható memória és processzorszám beállítása a `%UserProfile%\.wslconfig` fájlban (különösen a Buildroot-építéshez):

```ini
[wsl2]
memory=12GB
processors=8
nestedVirtualization=true
```

Módosítás után: `wsl --shutdown`, majd nyisd meg újra az Ubuntut.

A további parancsok **az Ubuntu terminálban** futnak (Start menü → *Ubuntu 24.04*, vagy Windows Terminal).

## 4. A repó klónozása

A repót **a Linux fájlrendszerre** (`~/...`) klónozd, ne a `/mnt/c/...` alá. A Windows-meghajtón a fordítás sokkal lassabb, és a fájljogosultságok (futtatható bitek, `chmod`) nem működnek megbízhatóan, ami teszthibákat okoz.

```sh
sudo apt-get update
sudo apt-get install -y git
cd ~
git clone https://github.com/szebasztianmarton/Nexora-OS.git
cd Nexora-OS
git checkout main
```

**Sorvégek:** ebben a repóban kapcsold ki a sorvég-konverziót: `git config core.autocrlf false` (a fenti `git checkout` előtt, vagy utána a fájlok újra-kicsekkolásával). Ha a rendszer- vagy globális gitconfig `core.autocrlf=true`-t állít (Git for Windows alapértelmezése, ha a repót Windowsból is használod), a munkafa CRLF sorvégű fájlokat kaphat; a `.gitattributes` életbe lépése előtt kicsekkolt fájlok így CRLF-esek maradtak, és ez eltörte a Buildroot-buildet. Ellenőrzés: `scripts/check-line-endings.sh` (CTest-ként: `check_line_endings`), amely hibát jelez, ha a `git ls-files --eol` bármely fájlra `i/crlf`-et vagy `w/crlf`-et mutat.

A fájlok Windowsból is elérhetők a Fájlkezelőben: `\\wsl$\Ubuntu-24.04\home\<felhasználó>\Nexora-OS`. VS Code-dal: a repó mappájában `code .` (a *WSL* bővítménnyel).

## 5. Fő alkalmazás (MVP 2): fordítás és automatizált tesztek

### 5.1 Függőségek

Ugyanazok a csomagok, mint a CI-ban (`.github/workflows/ci.yml`):

```sh
sudo apt-get install -y cmake ninja-build g++ clang \
    qt6-base-dev qt6-declarative-dev \
    qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-window \
    libqt6sql6-sqlite libgl-dev
```

Opcionális, a CI lint lépéséhez: `sudo apt-get install -y clang-format-18 shellcheck`.

### 5.2 Fordítás és az összes teszt egy paranccsal

```sh
scripts/build.sh ci --test
```

Ez a `ci` presettel (Debug, `-Werror`) konfigurál, fordít, majd lefuttatja a `ctest`-et. **Elvárt eredmény:**

```
100% tests passed, 0 tests failed out of 49
```

A 4 `system` tesztből 3 (image-segédszkriptek, GRUB-szintaxis, shellcheck) csak akkor regisztrálódik, ha a `bash`, a `grub-common` és a `shellcheck` telepítve van, a `test_release_scripts` pedig csak `openssl`-lel; nélkülük kevesebb a tesztek száma. A negyedik, a `check_line_endings`, mindig regisztrálódik.

### 5.3 Tesztek külön-külön

```sh
cmake --preset debug
cmake --build --preset debug

ctest --preset debug -L unit            # 31 unit teszt
ctest --preset debug -L integration     # 14 integrációs teszt
ctest --preset debug -R tst_qmlsmoke --output-on-failure -V   # egy teszt részletes kimenettel
```

Egy tesztprogram közvetlenül, egyetlen tesztfüggvénnyel:

```sh
export QT_QPA_PLATFORM=offscreen
build/debug/tests/tst_emulatormanager -functions          # függvények listája
build/debug/tests/tst_emulatormanager stopEscalatesToKill
```

A tesztek nem igényelnek kijelzőt, kontrollert, valódi emulátort vagy hálózatot: a QML-tesztek az `offscreen` Qt-platformon futnak, a folyamatkezelési tesztek egy beépített fake emulátort (`nexora-fake-emulator`) használnak, minden fájl ideiglenes könyvtárba kerül.

### 5.4 Konfigurációk ellenőrzése (mint a CI-ban)

```sh
app=build/debug/bin/nexora-console-ui
"$app" --check-config --config-dir config/emulators          --settings-file config/examples/settings.json
"$app" --check-config --config-dir config/examples/emulators --settings-file config/examples/settings.json
```

Mindkét parancsnak 0-s kilépési kóddal kell befejeződnie (`echo $?`).

### 5.5 Formázás és lint (opcionális, mint a CI `lint` jobja)

```sh
git ls-files '*.cpp' '*.h' ':!prototypes' | xargs clang-format-18 --dry-run --Werror
for f in $(git ls-files '*.json'); do python3 -m json.tool "$f" > /dev/null || echo "HIBÁS: $f"; done
shellcheck scripts/*.sh
```

## 6. Fő alkalmazás: kézi tesztelés ablakban

Windows 11-en a WSLg automatikusan megjeleníti a Linuxos ablakot, külön X-szerver nem kell.

```sh
scripts/run.sh
```

Más indítási módok:

```sh
scripts/run.sh release -- --fullscreen                         # teljes képernyő
scripts/run.sh debug -- --input-backend none --log-file /tmp/nexora.log
QT_LOGGING_RULES="nexora.*.debug=true" scripts/run.sh          # részletes napló
```

**Játékkönyvtár:** a repó gyökerében lévő `.games/` mappa (git-ignorált, sosem kerül commitba) a fejlesztői játékkönyvtár: ha a konfiguráláskor létezik, a build a `build/<preset>/share/nexora/settings-defaults.json` fájlba írja `library.directories`-ként, így a Games fül beállítás nélkül is ezt a mappát olvassa. Másik mappa: `-DNEXORA_DEV_LIBRARY_DIR=<mappa>`. A mappa létrehozása vagy törlése után futtasd újra a konfigurálást.

**Fake emulátorok:** alapból nem jelennek meg. A `-DNEXORA_DEV_FAKE_EMULATOR_CONFIGS=ON` kapcsolóval a build a fejlesztői emulátor-konfigurációkat (`fake-ok`, `fake-fail`, `fake-crash`, `fake-long`) a `build/debug/share/nexora/emulators/` mappába generálja, így valódi emulátor nélkül is kipróbálható az indítás és visszatérés; kikapcsolt opcióval újrakonfigurálva a fájlok eltűnnek.

### Vezérlés billentyűzettel

| Művelet | Billentyű |
| --- | --- |
| Navigálás | Nyilak |
| Megerősítés / indítás | Enter |
| Vissza | Esc / Backspace |
| Fülváltás | Page Up / Page Down, Ctrl+Tab |
| Keresés (Games) | `/` vagy Ctrl+F |
| Kedvenc | F |
| Diagnosztika | F1 |
| Kilépés | Ctrl+Q |

### Mit érdemes kipróbálni

1. **Emulators** fül (a `NEXORA_DEV_FAKE_EMULATOR_CONFIGS=ON` kapcsolóval konfigurált buildben) → `fake-ok` indítása: megjelenik a futás-overlay, a folyamat kilép, a fókusz visszatér ugyanarra az elemre.
2. `fake-fail` és `fake-crash`: a felület hibaüzenetet mutat, nem fagy le, utána újra lehet indítani.
3. `fake-long` indítása, majd Enter az overlayen: a folyamat leáll, a UI visszajön.
4. **Settings** → könyvtármappa hozzáadása (pl. egy `~/nexora-test-games` mappa néhány fájllal a konfigurációban megadott kiterjesztéssel) → **Games** fülön megjelennek, keresés, kedvenc jelölés.
5. **F1 / Diagnostics**: konfigurációs hibák, bemeneti backend állapota, napló.
6. Ablak átméretezése és teljes képernyő (`--fullscreen`) kipróbálása.

A beállítások a WSL-en belül a `~/.config/nexora/`, a játékkönyvtár adatbázisa a `~/.local/share/nexora/` alatt van. Tiszta újrakezdéshez ezeket töröld.

## 7. Console UI prototípus: fordítás és tesztek

A `prototypes/console-ui/` önálló CMake-projekt, a saját mappájából kell fordítani.

### 7.1 Függőségek

```sh
sudo apt-get install -y cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-tools-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtquick-shapes \
    qml6-module-qtqml-workerscript qml6-module-qtqml-models qml6-module-qttest qml6-module-qtcore \
    libsdl2-dev
```

### 7.2 Fordítás és tesztek

```sh
cd ~/Nexora-OS/prototypes/console-ui
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Elvárt eredmény: minden teszt sikeres (`100% tests passed`).

### 7.3 Kipróbálás fejlesztői sandboxszal

A sandbox mock emulátort, mintajátékokat és generált borítóképeket hoz létre egy eldobható mappában:

```sh
scripts/run-dev.sh "$PWD/build"                          # ablakos mód, 1920x1080
scripts/run-dev.sh "$PWD/build" --size 1280x720          # 720p elrendezés
NEXORA_SANDBOX_RESET=1 scripts/run-dev.sh "$PWD/build"   # újrakezdés (első indítási varázsló)
```

Billentyűk: nyilak (navigálás), Enter/Szóköz (megerősítés), Esc/Backspace (vissza), F (kedvenc), `/` (keresés), M (további műveletek), Page Up/Down (fülváltás). Részletek: [prototypes/console-ui/README.md](../prototypes/console-ui/README.md).

## 8. Kontroller tesztelése

A WSL 2 alapértelmezés szerint **nem látja** a Windowshoz csatlakoztatott USB-kontrollereket. Két lehetőség van:

### 8.1 USB-továbbítás a WSL-be (usbipd-win)

Rendszergazdai PowerShellben:

```powershell
winget install --interactive --exact dorssel.usbipd-win
usbipd list                                  # keresd meg a kontroller BUSID-jét, pl. 2-3
usbipd bind --busid 2-3
usbipd attach --wsl --busid 2-3
```

Az Ubuntuban:

```sh
ls -l /dev/input/event*          # meg kell jelennie egy új eszköznek
sudo usermod -aG input "$USER"   # utána: wsl --shutdown és új terminál
```

Ezután az alkalmazás **F1 / Diagnostics** oldalán látszik a bemeneti backend és a felismert eszköz. Visszaadás Windowsnak: `usbipd detach --busid 2-3`.

> **Korlát:** a WSL-kernel nem minden kontroller-illesztőt tartalmaz (pl. `xpad`, `hid-playstation`), ezért ha nem jelenik meg `/dev/input/event*` eszköz vagy nincs `BTN_GAMEPAD` képessége, a kontroller WSL alatt nem tesztelhető. Ilyenkor a 9. pont szerinti image-en vagy valódi gépen teszteld. A Bluetooth-kontrollerek nem továbbíthatók.

### 8.2 Kontroller teszt valódi Linuxon / az image-en

A teljes kontroller-ellenőrzőlista (felismerés, hot-plug, gombkiosztás, átrendezés, Select + Start kombináció, több kontroller) a [controller-support.md](controller-support.md#manual-test-checklist) dokumentumban található.

## 9. Bootolható image (Buildroot) építése és futtatása

> **Állapot:** a Buildroot-konfiguráció validált, de teljes image-et még senki nem bootolt (lásd [buildroot.md](buildroot.md)). Ez a rész ezért egyben az első boot-teszt leírása is.

### 9.1 Buildroot előkészítése a WSL-ben

A Buildroot leáll, ha a `PATH` szóközt tartalmaz, a WSL pedig alapból hozzáfűzi a Windows útvonalait (pl. `/mnt/c/Program Files/...`). Kapcsold ki:

```sh
sudo tee -a /etc/wsl.conf > /dev/null <<'EOF'
[interop]
appendWindowsPath = false
EOF
```

Majd PowerShellben `wsl --shutdown`, és nyisd meg újra az Ubuntut. (Ezután a `code .` nem működik a WSL-ből; ha kell, állítsd vissza az építés után.)

Host-csomagok:

```sh
sudo apt-get install -y build-essential file cpio unzip rsync bc wget python3 perl libncurses-dev git
```

### 9.2 Az image építése

A Buildroot kimeneti könyvtára **a repón kívül, a Linux fájlrendszeren** legyen:

```sh
git clone --depth 1 --branch 2026.02.3 https://gitlab.com/buildroot.org/buildroot.git ~/br/buildroot
cd ~/Nexora-OS
make -C ~/br/buildroot BR2_EXTERNAL=$PWD/buildroot-external O=$HOME/br/out nexora_x86_64_defconfig
make -C ~/br/buildroot O=$HOME/br/out
```

Egy hideg build **több óráig** tart és kb. 25–30 GB helyet igényel. Eredmény: `~/br/out/images/disk.img`.

### 9.3 Futtatás QEMU-ban (a WSL-en belül, ajánlott)

Windows 11-en a WSL 2 támogatja a beágyazott virtualizációt, így a QEMU KVM-gyorsítással fut, az ablak pedig WSLg-n keresztül jelenik meg.

```sh
sudo apt-get install -y qemu-system-x86 ovmf
ls -l /dev/kvm                    # ha nincs: lásd .wslconfig nestedVirtualization=true
sudo usermod -aG kvm "$USER"      # utána wsl --shutdown

cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/nexora-vars.fd
qemu-system-x86_64 \
    -machine q35,accel=kvm -cpu host -smp 2 -m 2048 \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=/tmp/nexora-vars.fd \
    -drive file=$HOME/br/out/images/disk.img,format=raw,if=virtio \
    -device virtio-vga -display gtk \
    -device qemu-xhci -device usb-kbd -device usb-tablet \
    -audiodev none,id=snd0 -device intel-hda -device hda-duplex,audiodev=snd0 \
    -serial mon:stdio
```

- WSLg alatt a `virtio-vga` (2D) a biztonságos választás; a `virtio-vga-gl -display gtk,gl=on` (3D) működhet, de nem garantált.
- Ha nincs `/dev/kvm`: hagyd el az `accel=kvm` és `-cpu host` részt (nagyon lassú, de működik).
- A soros konzol (`-serial mon:stdio`) a terminálban mutatja a kernel naplóját, és root bejelentkezést kínál helyreállításhoz.

### 9.4 Valódi gépen (USB-ről)

1. Másold ki az image-et Windowsra: `cp ~/br/out/images/disk.img /mnt/c/Users/<felhasználó>/Downloads/`.
2. Írd USB-meghajtóra **Rufus**-szal (*DD-image mód*) vagy **balenaEtcher**-rel. **Figyelem:** a meghajtó teljes tartalma törlődik.
3. A célgépen: **UEFI-boot** (a legacy/CSM nem támogatott), **Secure Boot kikapcsolva**, boot USB-ről.

> A Hyper-V és a VirtualBox virtuális GPU-ihoz (hyperv_drm, vmwgfx) nincs illesztő az image kernelében, ezért ott valószínűleg nem lesz kép. Használd a QEMU-t vagy valódi hardvert.

### 9.5 Mit kell ellenőrizni az image-en

- A gép elindul, és a GRUB menü után automatikusan betölt a Nexora felület.
- A *Nexora OS (recovery: no console UI)* menüpont konzolra indít.
- `Ctrl+Alt+F2` (tty2) bejelentkezési promptot ad.
- Kontroller (USB-továbbítással: `-device usb-host,vendorid=0x045e,productid=0x028e`) működik a felületen.
- Kikapcsolás és újraindítás a felületről.
- Naplók: `/var/log/nexora/console-ui.log`, `/run/nexora-session/session.log`.

## 10. Hibaelhárítás

| Tünet | Megoldás |
| --- | --- |
| `wsl --install` hiba: virtualizáció nincs engedélyezve | Kapcsold be a VT-x/AMD-V-t a BIOS/UEFI-ben; Windows-szolgáltatások: *Virtual Machine Platform* és *Windows Subsystem for Linux*. |
| Nem jelenik meg ablak | `wsl --update`, majd `wsl --shutdown`. Ellenőrizd: `echo $WAYLAND_DISPLAY $DISPLAY` nem üres. Szoftveres rendereléshez: `QT_QUICK_BACKEND=software scripts/run.sh`. |
| `module "QtQuick..." is not installed` | Hiányzó `qml6-module-*` csomag; telepítsd az 5.1 (vagy 7.1) pont teljes listáját. |
| A játékkönyvtár le van tiltva | Hiányzik a `libqt6sql6-sqlite` csomag. |
| Az alkalmazás 2-es kóddal kilép | Rootként fut. Használj normál felhasználót (vagy fejlesztéshez `-- --allow-root`). |
| Lassú fordítás, furcsa jogosultsági teszthibák | A repó a `/mnt/c/...` alatt van; klónozd a `~/` alá. |
| `Your PATH contains spaces...` (Buildroot) | Lásd 9.1: `appendWindowsPath = false`. |
| Elfogy a memória a Buildroot alatt | Növeld a `memory` értéket a `.wslconfig`-ban, vagy építs kevesebb szálon: `make ... -j2`. |
| Nincs `/dev/kvm` | `.wslconfig`: `nestedVirtualization=true`, `wsl --shutdown`; a CPU-nak támogatnia kell. |
| Teszthiba | Részletes kimenet: `ctest --preset debug --output-on-failure -V`, napló: `build/<preset>/Testing/Temporary/LastTest.log`. |

## 11. Tesztelési ellenőrzőlista

Másold ki és pipáld ki az eredményeket (dátum, Windows-build, GPU):

- [ ] WSL 2 + Ubuntu 24.04 telepítve, nem root felhasználó
- [ ] `scripts/build.sh ci --test` → 49/49 teszt sikeres
- [ ] `--check-config` mindkét példakonfigurációra 0-s kóddal tér vissza
- [ ] Fő alkalmazás elindul ablakban (WSLg), billentyűzettel navigálható
- [ ] `fake-ok` / `fake-fail` / `fake-crash` / `fake-long` indítása és visszatérés rendben (`NEXORA_DEV_FAKE_EMULATOR_CONFIGS=ON` build)
- [ ] Játékkönyvtár: mappa hozzáadása, keresés, kedvenc, indítás
- [ ] Console UI prototípus: fordítás, minden teszt sikeres
- [ ] Console UI prototípus: `run-dev.sh` sandbox, első indítási varázsló, 1080p és 720p elrendezés
- [ ] Kontroller (usbipd vagy image): felismerés, navigáció, Select + Start
- [ ] Buildroot image: build sikeres
- [ ] Image QEMU-ban: boot, automatikus UI-indítás, recovery menüpont, tty2
- [ ] Image valódi gépen (UEFI, Secure Boot ki)

Az eredményeket (sikeres és sikertelen lépések, hibaüzenetek, naplók) GitHub issue-ként érdemes rögzíteni.
