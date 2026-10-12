# PPSSPP és RetroArch beépítése a Nexora-OS-be

> Állapot: TERV. Még semmi nincs megvalósítva.
> Projektek:
> - RetroArch: https://github.com/libretro/RetroArch (GPLv3)
> - PPSSPP: https://github.com/hrydgard/ppsspp (GPLv2+)

## 1. Háttér

- **RetroArch**: a libretro API egységes kezelőfelülete. Ő kezeli a képet, a hangot, az irányítást, a menüt és a mentéseket. Az egyes konzolokat külön betölthető modulok, ún. **magok** (core-ok) emulálják.
- **PPSSPP**: PSP-emulátor. Két formában használható:
  1. **Önálló program** (`PPSSPPSDL` / `PPSSPPWindows64.exe`). Ez a legjobb PSP-élmény, saját menüvel.
  2. **libretro mag** (`ppsspp_libretro.so` / `.dll`), ami a RetroArchon belül fut.

**Javaslat:** a RetroArch legyen az egységes emulátorfelület, a PPSSPP pedig magként fusson benne. Így minden konzol ugyanazzal az irányítással, menüvel és mentési rendszerrel működik. Az önálló PPSSPP maradhat tartalék.

## 2. Beépítési szintek (a legkönnyebbtől)

| Szint | Mit jelent | Munka | Licenc-kockázat |
|---|---|---|---|
| A. Indítóprogram | A Nexora-OS külső folyamatként indítja: `retroarch -L <mag> <rom>` | Kicsi | Alacsony (külön program) |
| B. Mellékelt binárisok | A Nexora-OS csomagja tartalmazza a lefordított RetroArch-ot és a magokat | Közepes | GPL: a forráskódot is elérhetővé kell tenni |
| C. Saját libretro felület | A Nexora-OS maga tölti be a magokat a `retro_*` API-n keresztül | Nagy | A Nexora-OS kódja is GPL-köteles lehet |

**Kezdésnek: A + B.** A C szintre csak akkor van szükség, ha a játéknak a Nexora-OS saját felületén belül kell futnia.

## 3. Javasolt mappaszerkezet

```
Nexora-OS/
├─ third_party/
│  ├─ RetroArch/            # git submodule
│  └─ ppsspp/               # git submodule (rekurzív!)
├─ emulators/               # build kimenet / csomagolt binárisok (gitignore)
│  ├─ retroarch/
│  │  ├─ retroarch(.exe)
│  │  ├─ cores/             # ppsspp_libretro, snes9x_libretro, mgba_libretro...
│  │  ├─ system/PPSSPP/     # PPSSPP assets (kötelező a maghoz!)
│  │  └─ retroarch.cfg      # Nexora-OS alapbeállítás
│  └─ ppsspp/               # opcionális önálló PPSSPP
├─ scripts/
│  ├─ build-retroarch.(sh|ps1)
│  └─ build-ppsspp.(sh|ps1)
└─ docs/emulator-integracio.md
```

A Nexora-OS jelenlegi felépítése (a repó alapján):

- **Forráskód:** az `apps/` alatt (`apps/console-ui/`, `apps/emulator-manager/`, `apps/game-library/`, `apps/system/`), közös kód a `shared/` alatt.
- **Build:** CMake, a gyökérben lévő `CMakeLists.txt` és `CMakePresets.json`; a meglévő scriptek: `scripts/build.sh`, `scripts/run.sh` (lásd [development.md](development.md)). A javasolt `build-retroarch` / `build-ppsspp` scriptek ide kerülnének.
- **Emulátor-definíciók:** JSON-fájlok a `config/emulators/` mappában (telepítve: `/usr/share/nexora/emulators/`), példa: `config/examples/emulators/emulator-with-profiles.json`.
- **Csomagolás:** Buildroot `BR2_EXTERNAL` fa a `buildroot-external/` mappában (csomag: `buildroot-external/package/nexora-os/`, board: `buildroot-external/board/nexora-x86_64/`, defconfig: `buildroot-external/configs/nexora_x86_64_defconfig`), lásd [buildroot.md](buildroot.md). A RetroArch és a PPSSPP Buildroot-csomagjai a `buildroot-external/package/` alá kerülhetnek.
- **Még nem létezik:** `third_party/`, `emulators/`, `.gitmodules`. Jelenleg egyetlen emulátor sincs csomagolva; a mellékelt `example.json` a `/bin/true`-t futtatja.

## 4. Forráskód behúzása

```bash
git checkout -b feature/emulators
git submodule add https://github.com/libretro/RetroArch third_party/RetroArch
git submodule add https://github.com/hrydgard/ppsspp third_party/ppsspp
git submodule update --init --recursive   # a PPSSPP-nek sok almodulja van
```

Érdemes egy stabil tag-et kiválasztani (pl. `git -C third_party/ppsspp checkout v1.x.x`), hogy a build reprodukálható legyen.

> Alternatíva fordítás nélkül: lefordított RetroArch és magok letölthetők innen:
> https://buildbot.libretro.com/stable/ és https://buildbot.libretro.com/nightly/

## 5. Fordítás

### RetroArch
**Linux:**
```bash
cd third_party/RetroArch
./configure --prefix=/usr     # függőségek: SDL2, Mesa/OpenGL vagy Vulkan, ALSA/PulseAudio, udev, zlib
make -j$(nproc)
```
**Windows (MSYS2 MinGW64 shellben):**
```bash
pacman -S --needed git make mingw-w64-x86_64-toolchain mingw-w64-x86_64-SDL2 mingw-w64-x86_64-zlib
cd third_party/RetroArch && ./configure && make -j$(nproc)
```

### PPSSPP libretro mag
```bash
cd third_party/ppsspp
cmake -B build-libretro -DLIBRETRO=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-libretro -j
# Kimenet: ppsspp_libretro.so / .dll -> emulators/retroarch/cores/
```
**Fontos:** másold a `third_party/ppsspp/assets/` tartalmát az `emulators/retroarch/system/PPSSPP/` mappába, különben a mag nem indul el vagy nem lesznek betűtípusok.

### PPSSPP önálló verzió (opcionális)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# Windows: Visual Studio-val a Windows/PPSSPP.sln is használható
```

## 6. RetroArch alapbeállítás (`emulators/retroarch/retroarch.cfg`)

```ini
libretro_directory   = ":/cores"
system_directory     = ":/system"
savefile_directory   = "~/.nexora/saves"
savestate_directory  = "~/.nexora/states"
video_fullscreen     = "true"
menu_driver          = "ozone"
input_autodetect_enable = "true"
config_save_on_exit  = "false"
# Távvezérlés a Nexora-OS felől (UDP, 55355-ös port)
network_cmd_enable   = "true"
network_cmd_port     = "55355"
```
A `:/` a RetroArch futtatható fájljának mappáját jelenti, így a csomag hordozható marad.

## 7. Integráció a Nexora-OS oldalán

### 7.1 Emulátorkezelő modul (`EmulatorManager`)

> Megjegyzés: az `EmulatorManager` már létezik: `apps/emulator-manager/include/nexora/emulator/EmulatorManager.h` és `apps/emulator-manager/src/EmulatorManager.cpp` (mellette `EmulatorHealth`, `EmulatorProcessState`, `LaunchHistory`). A terv ezt bővíti, nem hoz létre új modult. A meglévő indítási és hozzárendelési logika leírása: [game-library.md](game-library.md#emulator-assignment).

Feladatai:
1. **Felismerés**: megtalálja a `retroarch` binárist és a `cores/` mappát, és listázza az elérhető magokat.
2. **Kiterjesztés → mag hozzárendelés**:

| Kiterjesztés | Rendszer | Mag |
|---|---|---|
| `.iso .cso .pbp .chd` | PSP | `ppsspp_libretro` |
| `.nes` | NES | `nestopia_libretro` / `fceumm_libretro` |
| `.sfc .smc` | SNES | `snes9x_libretro` |
| `.gba .gb .gbc` | GBA/GB | `mgba_libretro` |
| `.md .gen` | Mega Drive | `genesis_plus_gx_libretro` |

3. **Indítás**:
   ```
   retroarch --config emulators/retroarch/retroarch.cfg -L cores/ppsspp_libretro.<so|dll> "<rom>"
   ```
   Tartalék önálló PPSSPP-vel: `PPSSPPSDL --fullscreen "<rom>"`
4. **Életciklus**: figyeli a folyamat kilépését, utána visszahozza a Nexora-OS felületét. Hiba esetén elmenti a naplót (`--log-file`).
5. **Vezérlés futás közben** (opcionális): UDP parancsok a 7.2 szakasz szerint.

### 7.2 UDP parancsok (ha `network_cmd_enable = true`)
`QUIT`, `PAUSE_TOGGLE`, `SAVE_STATE`, `LOAD_STATE`, `SCREENSHOT`, `MENU_TOGGLE`, `RESET`
Pl. Linuxon: `echo -n "QUIT" | nc -u -w1 127.0.0.1 55355`

### 7.3 Játékkönyvtár
- A ROM-mappa (`~/.nexora/roms/<rendszer>/`) beolvasása, a kiterjesztés alapján a rendszer meghatározása.
- Opcionálisan borítóképek és metaadatok: libretro-thumbnails (https://github.com/libretro-thumbnails).

### 7.4 Platformonként
- **Linux-alapú OS**: a build recept (TODO: Yocto / Buildroot / Arch PKGBUILD / Debian csomag?) telepítse a binárisokat a `/usr/lib/nexora/emulators` mappába. A kontrollerekhez udev jogok kellenek (`input` csoport). A repó alapján a Nexora-OS Buildroot-alapú (`buildroot-external/`, [buildroot.md](buildroot.md)), és a `nexora` felhasználó már tagja az `input` csoportnak (`buildroot-external/board/nexora-x86_64/users.table`).
- **Windows**: hordozható mappa a programmal együtt csomagolva.
- **Android**: a RetroArch külön alkalmazás, intenttel indítható (`com.retroarch`, extrák: `ROM`, `LIBRETRO`, `CONFIGFILE`).

## 8. Jogi tudnivalók
- Mindkét projekt **GPL**. Ha a Nexora-OS binárisként mellékeli őket, a pontos forráskódot (vagy a submodule commitot) is elérhetővé kell tenni, a licencfájlokkal együtt (`emulators/LICENSES/`).
- **C szintnél** (ha a libretro kód bekerül a Nexora-OS saját binárisába) a Nexora-OS is GPL-köteles lehet. Ilyenkor érdemes jogi szempontból is átnézni.
- **ROM-ot és BIOS-t nem szabad mellékelni.** A PSP-hez nem kell BIOS, de más rendszerekhez (pl. PS1) igen: ezt a felhasználó adja meg a `system/` mappában.

## 9. Lépések sorrendje (ellenőrzőlista)
- [ ] 1. Új branch: `feature/emulators`
- [ ] 2. Submodule-ok hozzáadása (4. szakasz), vagy lefordított buildek letöltése
- [ ] 3. Build scriptek: `scripts/build-retroarch`, `scripts/build-ppsspp`
- [ ] 4. `emulators/` mappa összeállítása + `retroarch.cfg` + PPSSPP assets
- [ ] 5. Kézi teszt: `retroarch -L cores/ppsspp_libretro.* teszt.iso`
- [ ] 6. `EmulatorManager` modul (felismerés, hozzárendelés, indítás, kilépés figyelése)
- [ ] 7. Játékkönyvtár felület a Nexora-OS-ben
- [ ] 8. Kontroller automatikus felismerése + teszt
- [ ] 9. Csomagolás + licencfájlok
- [ ] 10. Dokumentáció frissítése

## 10. Ellenőrzés
- Elindul-e a `retroarch --version` a csomagolt mappából?
- Betölt-e a PPSSPP mag egy saját, legálisan dumpolt PSP ISO-t vagy homebrew-t, hibaüzenet nélkül?
- Ha a Nexora-OS-ből indítod a játékot, kilépés után visszatér-e a felületre?
- Megmaradnak-e a mentések és állapotmentések a `~/.nexora/` mappában?
- Működik-e a kontroller újracsatlakoztatás nélkül?

## 11. Nyitott kérdések (a megvalósítás előtt eldöntendő)
1. Mi a Nexora-OS alapja (Linux disztró / asztali felület / web / Android)? (A repó alapján: saját, Buildroot-tal épített x86-64 Linux-kép, Qt 6 / QML konzolfelülettel EGLFS/KMS-en; natív Windows build nem támogatott, Windows 11-en csak WSL 2-ben fejleszthető, lásd [testing-windows11.md](testing-windows11.md).)
2. A, B vagy C szint?
3. Forrásból fordítunk, vagy a buildbotról töltjük le a binárisokat?
4. Az önálló PPSSPP is kell, vagy elég a mag?
