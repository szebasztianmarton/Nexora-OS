# Settings

Nexora keeps user preferences (display, audio, controller mapping, game library directories, default launch profiles) in a small, validated JSON file. Emulator definitions are **not** settings: they live in their own directory (`--config-dir`, see [development.md](development.md#adding-an-emulator-configuration)). Favorites, play history and the list of found games live in the [game library database](#game-library-database).

Implementation: `shared/include/nexora/shared/Settings.h`, `shared/src/Settings.cpp`; UI side effects in `apps/console-ui/src/SettingsController.cpp`.

## Contents

- [File locations](#file-locations)
- [Layers and precedence](#layers-and-precedence)
- [File format](#file-format)
- [Keys](#keys)
- [Validation and recovery](#validation-and-recovery)
- [Saving](#saving)
- [Checking a configuration: `--check-config`](#checking-a-configuration---check-config)
- [What is not stored](#what-is-not-stored)
- [Game library database](#game-library-database)
- [Backups](#backups)
- [Planned](#planned)

## File locations

| Layer | Path | Written by Nexora |
| --- | --- | --- |
| User file | `$XDG_CONFIG_HOME/nexora/settings.json` (Qt `QStandardPaths::GenericConfigLocation`; normally `~/.config/nexora/settings.json`; falls back to `~/.config` if Qt reports no location) | yes |
| User file, overridden | `--settings-file <file>` (relative paths are made absolute) | yes |
| System defaults 1 | `<appDir>/../share/nexora/settings-defaults.json` (`<appDir>` = directory of `nexora-console-ui`; e.g. `/usr/share/nexora/settings-defaults.json` for `/usr/bin`) | never |
| System defaults 2 | `/etc/nexora/settings-defaults.json` | never |

`nexora-console-ui --help` prints the effective default user file path.

Missing files are normal: without any file every key has its built-in default. Neither the build nor the Buildroot image currently installs a `settings-defaults.json`; the system layer is there for integrators (e.g. to preset `library.directories` on an image).

On the Buildroot image the console UI runs as user `nexora` with `HOME=/home/nexora` and no `XDG_CONFIG_HOME`, so the user file is expected at `/home/nexora/.config/nexora/settings.json` (derived from the code and the session script; not checked on a booted image). See [buildroot.md](buildroot.md#users-and-permissions).

## Layers and precedence

The effective value of a key is the first of:

1. the user file (only keys the user changed),
2. `/etc/nexora/settings-defaults.json`,
3. `<appDir>/../share/nexora/settings-defaults.json`,
4. the built-in default (table below).

System files are applied in the order above with later files overriding earlier ones, which is why `/etc` wins over `<appDir>/../share`.

Only user overrides are ever written. Setting a key to its current layered default (system default if present, otherwise built-in) removes the override instead of storing a copy, so later changes to the system defaults take effect again.

## File format

User file and system default files use the same format:

```json
{
  "version": 1,
  "settings": {
    "audio.volume": 35,
    "library.directories": ["/srv/games"]
  }
}
```

- `version` must be `1`. A missing or different version makes the whole file unusable (see [recovery](#validation-and-recovery)).
- `settings` must be an object; keys are flat, dotted names.
- Maximum file size: 256 KiB.
- A complete example with every key: [`config/examples/settings.json`](../config/examples/settings.json).

## Keys

All keys, their built-in defaults and validation rules (`Settings::specs()`):

| Key | Type | Default | Allowed values | Effect | Applied |
| --- | --- | --- | --- | --- | --- |
| `ui.language` | string | `"en"` | `"en"` | Interface language. Only English exists; there are no translations. | Stored only. |
| `ui.showHints` | bool | `true` | `true`, `false` | Show the control hint bar. | Immediately: the hint bar at the bottom of the dashboard is shown only while this is `true`. |
| `ui.tutorialCompleted` | bool | `false` | `true`, `false` | The first-start tutorial has been finished or skipped. While `false`, the dashboard shows the tutorial overlay; finishing or skipping it sets `true`. Settings → "Show tutorial" sets it back to `false`, and so does "Reset preferences". A system defaults file can set it to `true` to never show the tutorial. | Immediately. |
| `display.mode` | string | `"fullscreen"` | `"fullscreen"`, `"windowed"` | Window mode. | At start-up, and immediately when changed while running. `--fullscreen` / `--windowed` override it for the session; with either flag, changes are not applied until the next start. |
| `display.resolution` | string | `"auto"` | `"auto"` or `WIDTHxHEIGHT` (each 3–5 digits, no leading zero), e.g. `"1920x1080"`. The settings UI cycles `auto`, `1280x720`, `1920x1080`, `2560x1440`, `3840x2160`. | Initial **window size** only. It does **not** change the display mode/monitor resolution; in full-screen mode the window covers the screen regardless. | At start-up only, and only when neither `--fullscreen` nor `--windowed` is given. |
| `audio.volume` | int | `80` | integer 0–100 (JSON `40.0` is accepted as 40) | Master volume. | **Embedded platform only**, and only if `/usr/bin/amixer` is executable: runs `amixer -q sset Master <n>%`, falling back to the `PCM` control if `Master` fails; asynchronous; applied at start-up and on every change. On the **desktop** platform the value is stored as a preference only and the desktop mixer is left alone. |
| `input.backend` | string | `"auto"` | `"auto"`, `"evdev"`, `"sdl3"`, `"none"` | Controller backend ([controller-support.md](controller-support.md#backends)). | At the **next start**. `--input-backend` overrides it. |
| `input.mapping` | object | see below | action → button; actions `accept`, `back`, `menu`, `favorite`, `search`, `prevTab`, `nextTab`, `gameBar`, `options`; buttons `south`, `east`, `west`, `north`, `start`, `select`, `guide`, `l1`, `r1` | Controller button for each action. | Immediately. A second, stricter check happens when it is applied (see below). |
| `library.directories` | string array | `[]` | absolute paths, max 64 entries; `"/"` refused; paths are cleaned (`/srv/games/` → `/srv/games`) and de-duplicated | Directories scanned for games. | Immediately; triggers a rescan. |
| `library.extensions` | string array | `[]` | `".ext"` with 1–16 lower-case letters/digits, max 128 entries; normalised to lower case and a missing leading dot is added (`"ZIP"` → `".zip"`) | Extra extensions treated as games in addition to those declared by emulator definitions. Files found only through this list have no emulator. | Immediately; triggers a rescan. |
| `library.followSymlinks` | bool | `false` | `true`, `false` | Follow symbolic links that stay inside the same library directory ([game-library.md](game-library.md#symbolic-links)). | Immediately; triggers a rescan. |
| `library.sortOrder` | string | `"title"` | `"title"`, `"recent"`, `"emulator"`, `"added"` | Default sort order of the library list. | Immediately. The setting and the sort selector on the Games tab are kept in sync: changing the setting (Settings tab, "Default game sort") re-sorts the list, and choosing a sort order on the Games tab saves it as the new default. |
| `emulators.defaultProfiles` | object | `{}` | emulator id → profile id; both must match `^[a-z0-9][a-z0-9_-]{0,63}$` | Launch profile used when an emulator or a game is started without an explicit profile. Set from the Emulators tab's profile picker (Y / F on a profile). | Immediately. A profile that does not exist or is disabled is ignored at launch time (the emulator's own arguments are used). |

Default `input.mapping`:

```json
{"accept": "south", "back": "east", "menu": "start", "favorite": "north",
 "search": "west", "prevTab": "l1", "nextTab": "r1", "gameBar": "guide",
 "options": "select"}
```

The value is replaced as a whole, not merged with the defaults: actions left out are unbound, with one exception for compatibility: `options` was added after the format was introduced, so a stored mapping that does not mention it gets `select` for it if no other action uses `select` (otherwise it stays unbound and Select keeps working as before). When the mapping is applied, `ButtonMapping::fromVariantMap()` additionally rejects a button bound to two actions and a mapping without `accept` or `back`. Such a mapping is accepted by the settings store (and reported by [`--check-config`](#checking-a-configuration---check-config)); the controller then uses the **default** mapping and the settings screen reports "Controller mapping ignored (...); using defaults". Remapping from the UI repairs it. Details: [controller-support.md](controller-support.md#configurable-mapping).

## Validation and recovery

Every value is validated on load and on every change (`Settings::setValue()`).

| Situation | Behaviour |
| --- | --- |
| Unknown key in a file | Ignored; issue `unknown setting "<key>" ignored`. |
| Invalid value in a file | That entry is dropped and the layered default is used; issue `"<key>" <reason>; using the default`. The other entries are kept. |
| Invalid value set from the UI | Rejected; the previous value stays; the error is shown (`SettingsController.lastError`). |
| User file not parsable (invalid JSON, not an object, missing/wrong `version`, `settings` not an object, larger than 256 KiB, unreadable) | When the application starts (not with `--check-config`), the file is renamed to `settings.json.corrupt-<yyyyMMdd-HHmmss>` (kept for inspection, never deleted), all user overrides are dropped and defaults apply. Nothing is written until the next change. |
| System defaults file not parsable | That whole file is ignored (an issue is recorded); it is never renamed or modified. |
| Reset to defaults (`SettingsController.resetToDefaults()`) | All user overrides are dropped; the current user file is renamed to `settings.json.bak` (replacing an older `.bak`). Restore by renaming it back while Nexora is not running. |

Issues are logged under the `nexora.config` category and listed on the settings screen (`SettingsController.issues`).

Note: a user file written by a future Nexora with `"version": 2` is treated as unusable and moved aside like a corrupted file.

## Saving

- Every change made from the UI is validated and saved immediately (`SettingsController::set()`); adding or removing a library directory also saves immediately.
- Saves are atomic: `QSaveFile` writes a temporary file in the same directory and renames it over `settings.json`, so a crash or power loss leaves either the old or the new file, never a truncated one. The parent directory is created if needed.
- The saved file contains only the user overrides, formatted as indented JSON.
- If saving fails, the UI shows "Could not save <file>"; the value stays active for the current session.

## Checking a configuration: `--check-config`

```sh
nexora-console-ui --check-config [--config-dir <dir>] [--settings-file <file>]
```

Validates the emulator definitions in the configuration directory and the settings layers (system defaults + user file), prints a report and exits without starting the UI. It works without a display (it switches to the offscreen platform when no `DISPLAY`/`WAYLAND_DISPLAY`/`QT_QPA_PLATFORM` is set).

```
Emulator definitions in /path/config/emulators: 1 valid
Settings (user file /home/me/.config/nexora/settings.json): 0 issue(s)
OK
```

- Exit status 0 when there are no errors, 1 otherwise.
- Emulator definition warnings (e.g. unknown keys) are printed but do not fail the check; every settings issue (including an unknown key or a problem in a system defaults file) counts as an error.
- `input.mapping` is additionally checked with the controller's rules (no button bound twice, `accept` and `back` bound); each violation is printed as `error: input.mapping: ...` and fails the check.
- The executables named in emulator definitions are not checked for existence here; the UI reports them as unavailable at run time.
- It is read-only: settings are loaded in `Settings::LoadMode::Inspect`, so an unparsable user file is reported but **not** renamed, and nothing is written.

CI runs this for `config/emulators` and `config/examples/emulators` with `config/examples/settings.json` ([testing.md](testing.md#continuous-integration)).

## What is not stored

- No secrets: no passwords, tokens, accounts or network credentials. Nexora has no online features.
- Not the emulator definitions (separate directory), the log file location (`--log-file`), or the window position/size chosen at run time.
- Not favorites, play counts, last-played times, per-game emulator choices or the list of found games: those are in the library database.

## Game library database

| | |
| --- | --- |
| Default path | `$XDG_DATA_HOME/nexora/library.sqlite` (Qt `QStandardPaths::GenericDataLocation`; normally `~/.local/share/nexora/library.sqlite`; on the image expected at `/home/nexora/.local/share/nexora/library.sqlite`, not checked on a booted image) |
| Override | `--library-db <file>` (relative paths are made absolute) |
| Format | SQLite (Qt `QSQLITE` driver), WAL journal mode, so `library.sqlite-wal` and `library.sqlite-shm` may exist next to it |
| Unusable file | A file that is not a usable SQLite database is renamed to `library.sqlite.corrupt-<yyyyMMdd-HHmmss>` (its `-wal` file is moved with it, `-shm` is removed) and a new, empty database is created; favorites and play history then start empty. A database written by a **newer** schema version, or one that is busy/locked, is left untouched: the library is disabled for that run and the error is shown in Diagnostics. |
| Database cannot be opened at all | The library is disabled for the session (logged as critical); emulators can still be started. |

Schema and rescan behaviour: [game-library.md](game-library.md#database).

## Emulator output logs

| | |
| --- | --- |
| Location | `$XDG_STATE_HOME/nexora/emulator-logs/` (normally `~/.local/state/nexora/emulator-logs/`; a relative `XDG_STATE_HOME` is ignored as the XDG specification requires). On the image: `/home/nexora/.local/state/nexora/emulator-logs/` (not checked on a booted image). |
| Files | `<emulator id>.log` for the latest launch of that emulator, `<emulator id>.log.1` for the one before. Each holds a header (time, program, arguments, working directory), the emulator's output with `out| ` / `err| ` prefixes and a footer with the outcome (e.g. "crashed"). |
| Size | At most 1 MiB per file; further output is dropped with a note. Two files per emulator. |
| Shown | The Diagnostics tab ("Emulator output log") shows the current file; the last 4000 characters of output are also shown there. |

The logs contain game paths and whatever the emulator prints; Nexora writes nothing else into them. They can be deleted at any time.

## Backups

What to back up:

| File | Contains | Loss means |
| --- | --- | --- |
| `settings.json` | Your preference overrides | Defaults return; directories and mapping must be set again |
| `library.sqlite` (+ `-wal`) | Favorites, play counts, last played, per-game emulator choice, list of found games | Favorites and history are lost; games are found again by the next scan |
| Emulator definition directory | Emulators and launch profiles | Emulators must be configured again |

Recommendations:

- Copy files while Nexora is **not running**. For the database, either copy `library.sqlite` together with `library.sqlite-wal` (if present), or use `sqlite3 library.sqlite ".backup library-backup.sqlite"`, which produces a consistent single file.
- Keep `*.corrupt-*` and `settings.json.bak` files until you have checked them; Nexora never deletes them, so remove old ones by hand.
- Game files, BIOS files and emulator save data are not managed by Nexora; back them up separately.

## Planned

Not implemented in MVP 2:

- Further languages (`ui.language` accepts only `en`).
- Applying `display.resolution` as a real display mode (KMS mode setting).
- Switching the controller backend without a restart.
- Migration of future settings format versions (currently a different `version` is treated as unusable).
