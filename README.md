# Nexora OS

> Kontrollerrel vezérelhető, Linux-alapú játék-operációs rendszer x86-64 PC-khez és mini PC-khez. / A Linux-based gaming operating system built for controller-first gaming on x86-64 PCs and mini PCs.

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6.4%2B-41CD52?logo=qt)
![CMake](https://img.shields.io/badge/CMake-3.21%2B-064F8C?logo=cmake)
![SDL2](https://img.shields.io/badge/SDL-2-1E3A5F)
![Status](https://img.shields.io/badge/status-experimental-F4C96B)

---

## 🇭🇺 Magyar

### Leírás

A Nexora OS egy nyílt forráskódú játék-operációs rendszer, amely konzolszerű élményt ad x86-64 PC-ken és mini PC-ken. Célja, hogy a kompatibilis, natív Linuxos emulátorokat egyetlen kontrollerrel vezérelhető felületen fogja össze.

**Vízió:** dedikált játékkörnyezet, amely közvetlenül egy egyszerű, igényes felületre indul. Itt a felhasználó billentyűzet és egér nélkül böngészheti, indíthatja és kezelheti az emulátorait.

### Funkciók (megvalósítva)

- **Kontroller-első konzolfelület** (Qt 6 / QML), TV-re tervezve: 1920×1080 elsődleges, 1280×720 támogatott. Reszponzív méretezés, kompakt oldalsáv.
- **Képernyők:** Kezdőlap (folytatás, nemrég játszott, kedvencek, gyors elérés), Játékkönyvtár (keresés, rendezés, szűrés, reszponzív rács), Játék részletei, Kedvencek, Emulátorok és emulátor részletei, Beállítások (8 kategória), első indítási varázsló.
- **Emulator Manager:** egyszerre egy külső folyamat (QProcess, soha nem shellen keresztül). Kezeli a normál kilépést, a hibakóddal való kilépést, az összeomlást, a hiányzó futtatható fájlt és a sikertelen indítást. Újrapróbálás, leállítás, visszatérés a felületre.
- **Játékkönyvtár-indexelés** háttérszálon, kizárólag a felhasználó által megadott mappákban. Borítókép-támogatás, kedvencek, legutóbb játszott játékok, emulátor-felülbírálás, eltávolítás a könyvtárból (a fájl nem törlődik).
- **Kontroller-támogatás** SDL2-vel: hot-plug, logikai műveletek, a billentyűzettel közös navigáció, eszközfüggő gombfeliratok.
- **Diagnosztika:** valós rendszerinformáció, naplófájl-megjelenítő, nemrégi hibák, másolható riport (a saját mappa `~`-ként jelenik meg, környezeti változók nincsenek benne).
- **Állapotkezelés:** minden adatvezérelt képernyőn van betöltési, üres, nem elérhető, hiba- és részleges állapot.

A tervezési részletek (design tokenek, fókusz- és Vissza-modell, képernyő- és komponensleltár, ismert korlátok): **[docs/UI_DESIGN.md](docs/UI_DESIGN.md)**.

### Technológiák

| Terület | Technológia |
|---|---|
| Nyelv | C++20 |
| UI | Qt 6 (tesztelve: 6.4.2), Qt Quick / QML, QtQuick.Shapes |
| Build | CMake ≥ 3.21, Ninja |
| Folyamatkezelés | QProcess |
| Bemenet | SDL2 (opcionális), billentyűzet |
| Perzisztencia | JSON (`settings.json`, `emulators.json`, `library-state.json`) |
| Tesztek | Qt Test, Qt Quick Test, CTest |
| Operációs rendszer (tervezett) | Buildroot-alapú Linux |

### Telepítés

Előfeltételek (Ubuntu 24.04):

```sh
sudo apt-get install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtqml-workerscript qml6-module-qtqml-models qml6-module-qttest qml6-module-qtcore \
  libsdl2-dev
```

Fordítás:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

CMake-opciók: `-DNEXORA_WITH_SDL=OFF` (kontroller-támogatás nélkül), `-DNEXORA_BUILD_TESTS=OFF`, `-DNEXORA_BUILD_ID=<azonosító>`.

### Használat

Fejlesztői sandbox (mock emulátor, mintafájlok, generált borítóképek egy eldobható konfigurációs mappában):

```sh
scripts/run-dev.sh "$PWD/build"                          # ablakos mód, 1920x1080
scripts/run-dev.sh "$PWD/build" --size 1280x720          # 720p elrendezés
NEXORA_SANDBOX_RESET=1 scripts/run-dev.sh "$PWD/build"   # újrakezdés (első indítási varázsló)
```

Közvetlen indítás:

```sh
build/apps/console-ui/nexora-console-ui [--config-dir DIR] [--log-dir DIR] [--windowed] [--size WxH]
```

Az emulátorokat a `<config>/emulators.json` fájl írja le; a formátum leírása a [docs/UI_DESIGN.md](docs/UI_DESIGN.md) fájlban található. A felületen ez a konfiguráció ebben a kiadásban csak olvasható: módosítás után válaszd az *Újratöltés* (*Reload configuration*) műveletet.

#### Vezérlés

| Művelet | Billentyűzet | Kontroller |
|---|---|---|
| Navigálás | Nyilak | D-pad / bal kar |
| Megerősítés | Enter / Szóköz | Déli gomb (A / Cross) |
| Vissza | Esc / Backspace | Keleti gomb (B / Circle) |
| Kedvenc (másodlagos) | F | Nyugati gomb (X / Square) |
| Keresés | `/` | Északi gomb (Y / Triangle) |
| További műveletek | M / Menü | Start |
| Előző / következő fül | Page Up / Page Down | LB / RB |

A Vissza gomb soha nem lép ki az alkalmazásból.

### Környezeti változók

| Változó | Leírás | Alapérték |
|---|---|---|
| `NEXORA_CONFIG_DIR` | Konfigurációs mappa | `~/.config/nexora` |
| `NEXORA_LOG_DIR` | Napló mappa | `~/.local/share/nexora/logs` |
| `NEXORA_SANDBOX` | Sandbox helye (`run-dev.sh`) | `<build>/dev-sandbox` |
| `NEXORA_SANDBOX_RESET` | `1` esetén a sandbox újralétrehozása | `0` |

### Tesztek

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

- **Backend:** 8 Qt Test program (beállítások, emulátor-regiszter, folyamatkezelés a mock emulátorral, könyvtár-indexelés, szűrőmodell, kontroller, diagnosztika, mappaböngésző).
- **Felület:** 1 Qt Quick Test program a valódi backenddel és QML modullal (komponensek, modális fókusz, oldalsáv-navigáció, Vissza-előzmények, részletoldal fókusz-visszaállítással, teljes indítás → futás → visszatérés, 720p elrendezés).

### Projekt struktúra

```
apps/console-ui/        QML felület (modul: Nexora) és belépési pont
  qml/styles/           design tokenek (Theme, Dimensions, Typography, Motion)
  qml/singletons/       Router, Input, GameActions
  qml/components/       újrahasználható komponensek
  qml/pages/            képernyők
  src/                  Backend-bekötés, main.cpp
src/core/               C++ szolgáltatások (beállítások, emulátorok, folyamatok, könyvtár, kontroller, diagnosztika)
tools/mock-emulator/    determinisztikus teszt-emulátor
tests/core/             backend unit tesztek
tests/qml/              UI tesztek
scripts/run-dev.sh      fejlesztői sandbox
docs/UI_DESIGN.md       UI tervezési és implementációs dokumentáció
```

### MVP – minimálisan működő termék

Az első verzió egy kicsi, megbízható, tesztelhető alapra koncentrál.

- **Bootolható Linux** környezet Buildroottal.
- **Kontroller-első UI** Qt 6 / QML alapon. *(megvalósítva)*
- **Emulator Manager:** natív Linuxos emulátorfolyamatok indítása és felügyelete. *(megvalósítva)*
- **Folyamatéletciklus:** kilépés, összeomlás, hiányzó fájl, hibás konfiguráció és indítási hiba kezelése. *(megvalósítva)*
- **Emulátor-konfiguráció** konfigurációs fájlokkal. *(megvalósítva)*
- **Naplózás és diagnosztika.** *(megvalósítva)*
- **Automatizált tesztek** mock emulátorral. *(megvalósítva)*
- **Reprodukálható build** CMake-kel és Ninjával. *(megvalósítva)*

**Elfogadási feltétel:** a rendszer elindul a kiválasztott hardveren, a UI kontrollerrel használható, és az indítás-visszatérés folyamat átmegy az automatizált teszteken.

### Fejlesztési ütemterv

1. **Alapok:** repository, build, közös típusok, konfiguráció, naplózás, automatizált build és tesztek. *(kész)*
2. **Asztali prototípus:** QML felület, kontroller-navigáció, emulátorkönyvtár, mock emulátor. *(kész)*
3. **Emulátorkezelés:** folyamatindítás és -felügyelet, hibakezelés, profilok, hibajelentés. *(nagyrészt kész)*
4. **Bootolható Nexora OS:** Buildroot external tree, csomagolás, indítás, grafika, bemenet, rendszerszolgáltatások.
5. **MVP kiadás:** validálás célhardveren, regressziós tesztek, dokumentáció, első kísérleti kiadás.

**Későbbi verziók:**

- **v0.2:** jobb hot-plug, konfigurálható vezérlés, jobb diagnosztika.
- **v0.5:** több emulátor, emulátoronkénti beállítások, metaadatok, save state.
- **v1.0:** telepítés, frissítések, hardverkompatibilitás, helyreállítás.

### Tervezési elvek

Kontroller-első interakció, minimális zavaró elem, moduláris architektúra, nyílt forráskódú komponensek, megbízható folyamatkezelés, reprodukálható build, egyértelmű licencelés és forrásmegjelölés.

### Hatókör és korlátok

A Nexora OS meglévő emulátorprojekteket integrál, nem írja újra őket. A platformtámogatás az emulátorok kompatibilitásától, a hardverigénytől és a licencektől függ.

Játékfájlokat, BIOS-t, firmware-t és más szerzői jog által védett anyagot csak akkor csomagol, ha a terjesztés kifejezetten engedélyezett. A felhasználónak magának kell biztosítania azokat a fájlokat, amelyek használatára jogosult.

A felület aktuális korlátai (csak olvasható emulátor-konfiguráció, nincs rendszerszintű hangerőszabályzás, nincs felbontásváltás, nincs gombátkiosztás) a [docs/UI_DESIGN.md](docs/UI_DESIGN.md) fájlban vannak felsorolva.

### Projekt állapota

**Kísérleti fejlesztés.** A konzolfelület és a backend asztali környezetben (ablakos vagy teljes képernyős módban) működik, automatizált tesztekkel. A bootolható Buildroot-image, a valódi kontrolleres hardveres validálás és a kiadás még hátravan.

### Licensz

Még nincs meghatározva.

---

## 🇬🇧 English

### Description

Nexora OS is an open-source gaming operating system that provides a console-like experience on x86-64 PCs and mini PCs. Its goal is to bring compatible native Linux emulators together in a unified, controller-driven interface.

**Vision:** a dedicated gaming environment that boots directly into a simple, polished interface. From there, users browse, launch and manage their emulators without needing a keyboard or mouse for everyday use.

### Features (implemented)

- **Controller-first console UI** (Qt 6 / QML) designed for TVs: 1920×1080 primary, 1280×720 supported, with responsive scaling and a compact sidebar.
- **Screens:** Home (continue playing, recently played, favorites, quick access), Game Library (search, sort, filters, responsive grid), Game Details, Favorites, Emulators and emulator details, Settings (8 categories), and a first-run setup flow.
- **Emulator Manager:** one external process at a time (QProcess, never through a shell). It handles normal exits, error exits, crashes, missing executables and failed starts, and supports retry, stop and return to the UI.
- **Game library indexing** on a worker thread, only in folders the user configured. Supports artwork, favorites, recently played, per-game emulator override, and removal from the library (files are never deleted).
- **Controller support** via SDL2: hot-plug, logical actions shared with the keyboard navigation path, and device-specific button labels.
- **Diagnostics:** real system information, a log viewer, recent errors, and a copyable report (the home folder is shown as `~`; no environment variables).
- **State handling:** every data-driven screen has loading, empty, unavailable, error and partial states.

Design details (design tokens, focus and Back model, screen and component inventory, known limitations): **[docs/UI_DESIGN.md](docs/UI_DESIGN.md)**.

### Tech Stack

| Area | Technology |
|---|---|
| Language | C++20 |
| UI | Qt 6 (tested with 6.4.2), Qt Quick / QML, QtQuick.Shapes |
| Build | CMake ≥ 3.21, Ninja |
| Process management | QProcess |
| Input | SDL2 (optional), keyboard |
| Persistence | JSON (`settings.json`, `emulators.json`, `library-state.json`) |
| Tests | Qt Test, Qt Quick Test, CTest |
| Operating system (planned) | Buildroot-based Linux |

### Installation

Prerequisites (Ubuntu 24.04):

```sh
sudo apt-get install cmake ninja-build g++ qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtquick-shapes \
  qml6-module-qtqml-workerscript qml6-module-qtqml-models qml6-module-qttest qml6-module-qtcore \
  libsdl2-dev
```

Build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

CMake options: `-DNEXORA_WITH_SDL=OFF` (no controller support), `-DNEXORA_BUILD_TESTS=OFF`, `-DNEXORA_BUILD_ID=<id>`.

### Usage

Developer sandbox (mock emulator, sample files and generated cover art in a throw-away config directory):

```sh
scripts/run-dev.sh "$PWD/build"                          # windowed, 1920x1080
scripts/run-dev.sh "$PWD/build" --size 1280x720          # 720p layout
NEXORA_SANDBOX_RESET=1 scripts/run-dev.sh "$PWD/build"   # start over (shows first run)
```

Direct start:

```sh
build/apps/console-ui/nexora-console-ui [--config-dir DIR] [--log-dir DIR] [--windowed] [--size WxH]
```

Emulators are defined in `<config>/emulators.json`; the format is described in [docs/UI_DESIGN.md](docs/UI_DESIGN.md). In this release the configuration is read-only in the UI: edit the file, then choose *Reload configuration*.

#### Controls

| Action | Keyboard | Controller |
|---|---|---|
| Navigate | Arrow keys | D-pad / left stick |
| Confirm | Enter / Space | South (A / Cross) |
| Back | Esc / Backspace | East (B / Circle) |
| Favorite (secondary) | F | West (X / Square) |
| Search | `/` | North (Y / Triangle) |
| More actions | M / Menu | Start |
| Previous / next tab | Page Up / Page Down | LB / RB |

Back never exits the application.

### Environment Variables

| Variable | Description | Default |
|---|---|---|
| `NEXORA_CONFIG_DIR` | Configuration directory | `~/.config/nexora` |
| `NEXORA_LOG_DIR` | Log directory | `~/.local/share/nexora/logs` |
| `NEXORA_SANDBOX` | Sandbox location (`run-dev.sh`) | `<build>/dev-sandbox` |
| `NEXORA_SANDBOX_RESET` | `1` recreates the sandbox | `0` |

### Tests

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

- **Backend:** 8 Qt Test executables (settings, emulator registry, process lifecycle with the mock emulator, library indexing, filter model, controller, diagnostics, directory browser).
- **UI:** 1 Qt Quick Test executable running the real backend and QML module (components, modal focus, sidebar navigation, Back history, detail pages with focus restoration, the full launch → running → return cycle, 720p layout).

### Project Structure

```
apps/console-ui/        QML interface (module: Nexora) and entry point
  qml/styles/           design tokens (Theme, Dimensions, Typography, Motion)
  qml/singletons/       Router, Input, GameActions
  qml/components/       reusable components
  qml/pages/            screens
  src/                  backend wiring, main.cpp
src/core/               C++ services (settings, emulators, processes, library, controller, diagnostics)
tools/mock-emulator/    deterministic test emulator
tests/core/             backend unit tests
tests/qml/              UI tests
scripts/run-dev.sh      developer sandbox
docs/UI_DESIGN.md       UI design and implementation document
```

### MVP — Minimum Viable Product

The first version focuses on a small, reliable, testable foundation rather than supporting every emulator or platform.

- **Bootable Linux** environment built with Buildroot.
- **Controller-first UI** with Qt 6 / QML. *(implemented)*
- **Emulator Manager:** launch and monitor native Linux emulator processes. *(implemented)*
- **Process lifecycle:** exits, crashes, missing executables, invalid configurations and launch failures. *(implemented)*
- **Emulator configuration** through configuration files. *(implemented)*
- **Logging and diagnostics.** *(implemented)*
- **Automated testing** with a mock emulator. *(implemented)*
- **Reproducible builds** with CMake and Ninja. *(implemented)*

**Acceptance criteria:** the system boots on the selected test hardware, the UI is usable with a controller, and the emulator launch-and-return workflow passes automated tests.

### Development Roadmap

1. **Foundation:** repository, build system, shared types, configuration, logging, automated builds and tests. *(done)*
2. **Desktop prototype:** QML interface, controller navigation, emulator library, mock emulator. *(done)*
3. **Emulator management:** process launching and monitoring, failure handling, profiles, error reporting. *(mostly done)*
4. **Bootable Nexora OS:** Buildroot external tree, packaging, startup, graphics, input and system services.
5. **MVP release:** validation on target hardware, regression tests, documentation, first experimental release.

**Future versions:**

- **v0.2:** better hot-plug, configurable controls, improved diagnostics.
- **v0.5:** more emulators, per-emulator settings, metadata, save states.
- **v1.0:** installation, updates, hardware compatibility, recovery.

### Design Principles

Controller-first interaction, minimal distractions, modular architecture, open-source components, reliable process management, reproducible builds, clear licensing and attribution.

### Scope and Limitations

Nexora OS integrates existing emulator projects rather than reimplementing them. Platform support depends on emulator compatibility, hardware requirements and licensing.

Game files, BIOS files, firmware and other copyrighted materials will not be bundled unless redistribution is explicitly permitted. Users must supply files they are legally entitled to use.

Current UI limitations (read-only emulator configuration, no system volume control, no resolution switching, no button remapping) are listed in [docs/UI_DESIGN.md](docs/UI_DESIGN.md).

### Project Status

**Experimental development.** The console UI and backend run in a desktop session (windowed or full screen) with automated tests. The bootable Buildroot image, hardware validation with real controllers, and a release are still to come.

### License

Not yet specified.
