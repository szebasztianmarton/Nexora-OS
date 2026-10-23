# Game library

The game library finds game files in directories you configure, remembers them in a local SQLite database (schema version 2), assigns them to gaming systems, keeps your corrections (title, notes, system, emulator, removed entries) across rescans, and starts them with a matching emulator. It never downloads anything, never executes or opens game files, never deletes them, and never scans outside the configured directories.

Code: `apps/game-library/` (`LibraryScanner`, `GameDatabase`, `LibraryQuery`, `GameLibrary`), UI glue in `apps/console-ui/src/LibraryController.cpp`, launching in `apps/emulator-manager/` and `apps/console-ui/src/AppController.cpp`.

## Contents

- [Configuring directories and file types](#configuring-directories-and-file-types)
- [Gaming systems](#gaming-systems)
- [Scanning](#scanning)
- [When a scan runs](#when-a-scan-runs)
- [Database](#database)
- [Missing files and unmounted drives](#missing-files-and-unmounted-drives)
- [Search, filters and sorting](#search-filters-and-sorting)
- [Emulator assignment](#emulator-assignment)
- [Game details](#game-details)
- [Removing and restoring entries](#removing-and-restoring-entries)
- [Launch options](#launch-options)
- [Launching a game](#launching-a-game)
- [Limitations](#limitations)

## Configuring directories and file types

**Directories** come from the `library.directories` setting ([settings.md](settings.md#keys)): absolute paths, at most 64, `/` refused. They can be added and removed on the Settings tab under "Library folders" (typing the path requires a keyboard; `LibraryController.addDirectory()` additionally requires the directory to exist) or written to the settings file (existence is then only checked by the scan).

**File types** come from two places:

1. Each enabled emulator definition declares the extensions it handles:

   ```json
   { "id": "snes9x", "name": "Snes9x", "executable": "/usr/bin/snes9x",
     "arguments": ["{file}"], "extensions": [".sfc", ".smc"] }
   ```

   Entries must match `^\.[a-z0-9]{1,16}$` after trimming and lower-casing (the leading dot is required here). Disabled emulators contribute no extensions. A full example: [`config/examples/emulators/emulator-with-profiles.json`](../config/examples/emulators/emulator-with-profiles.json).

2. `library.extensions` adds extensions without an emulator (e.g. `[".zip"]`). Such files are listed but show "No emulator handles this file type" and cannot be launched until an emulator declares the extension.

Extension matching is case-insensitive and uses the text after the **last** dot (`game.tar.gz` → `.gz`). Files without an extension are ignored.

## Gaming systems

The built-in system catalog (`config/systems/systems.json`, compiled into the application; 20 systems from `nes` to `dos`, see [emulator-integration.md](emulator-integration.md#gaming-systems)) maps extensions to systems. During a scan every file gets its **system candidates** (all systems its extension maps to). If exactly one system matches, it becomes the game's system; extensions such as `.iso`, `.chd` or `.zip` map to several systems, so those games keep an empty system until you choose one in the [game details](#game-details) (stored as `system_override`, kept across rescans). A system is a grouping and filter, not a compatibility claim: launching still requires an emulator that declares the extension.

**Xbox 360 discs.** `.iso` is declared by several systems, so the scanner checks each `.iso` whose candidates include `xbox360`. It reads 20 bytes at `0x10000` past each of three partition bases (`0x0`, `0x2080000`, `0xFD90000`) and looks for the XDVDFS signature `MICROSOFT*XBOX*MEDIA`. A match makes the system `xbox360`; a miss removes `xbox360` from the candidates and the other candidates are kept as before; if exactly one of them remains, it becomes the game's system under the general rule above. No read goes past the end of the file, at most three reads are made per file, and read errors count as a miss. Rebuilt (xiso) original Xbox images also carry the signature and are reported as Xbox 360; full original Xbox disc dumps are not probed at their partition offset. Xenia does not run original Xbox games. GOD/STFS containers are not detected yet. A `.xex` file maps to `xbox360` without a probe.

The **Systems** tab shows one card per system that has games or an emulator (`SystemListModel`: `systemId`, `name`, `shortName`, `gameCount`, `emulatorCount`); games without a system are counted in an "Other" bucket only when there are any. Accept on a card opens the Games tab filtered to that system; the "Other" card filters to the games without a recognised system (`LibraryController.otherSystemFilter`, value `@other`). When only counts change, the model updates its rows in place instead of resetting, so focus is not lost.

## Scanning

`LibraryScanner::scan()` walks the configured directories. It lists directories and `stat()`s files. The one exception is an `.iso` file whose candidates include `xbox360`: it is opened read-only for the [Xbox 360 disc probe](#gaming-systems), with at most three 20-byte reads and never past the end of the file. No game file is ever executed, and no file contents are logged.

| Rule | Behaviour |
| --- | --- |
| Roots | Only the configured directories. Relative paths, missing or unreadable directories are reported as issues and skipped. A root that resolves to `/` (also through a symlink) is refused. |
| Hidden entries | Files and directories whose name starts with `.` are skipped. |
| System directories | Directories named `proc`, `sys`, `dev`, `run`, `lost+found`, `.Trash-*` or `.git` are never entered, nor any directory that resolves to `/proc`, `/sys`, `/dev`, `/run`, `/boot`, `/etc`, `/usr`, `/bin`, `/sbin`, `/lib`, `/lib64` or `/var`. |
| File kinds | Only regular files; sockets, devices and FIFOs are ignored. |
| Unreadable entries | Unreadable files and inaccessible sub-directories are skipped and reported as issues. |
| Depth | Sub-directories are followed up to 16 levels below the root; deeper ones are reported and not entered. |
| File limit | At most 100 000 game files per scan. When reached, the scan stops and is marked truncated ("scan limit reached"). |
| Duplicates | The same file (same device and inode) is listed once, even when reached through overlapping roots (e.g. `/games` and `/games/snes`), hard links or followed symlinks; the first configured root wins. Duplicates are counted. |
| Issue list | Capped at 200 entries per scan. |
| Paths stored | Canonical absolute paths (symlinks resolved). |
| Titles | Derived from the file name: extension removed, `_` → space, repeated whitespace collapsed (`Super_Mario  World (USA).sfc` → `Super Mario World (USA)`). |

### Symbolic links

- Default (`library.followSymlinks: false`): every symbolic link inside a library directory is skipped and counted as "skipped link". (A configured root that is itself a symlink is resolved and scanned.)
- `library.followSymlinks: true`: a link is followed only if its canonical target lies inside the **same** root. Links that point elsewhere are skipped with the issue "symbolic link points outside the library directory; skipped"; broken links are skipped with "broken symbolic link; skipped". Both count as skipped links. Directory loops are detected and not followed twice.

### Asynchronous scanning

- Scans run on a low-priority worker thread (`nexora-library-scan`). The same thread then merges the result into the database through its own SQLite connection and reads the new entry list; the UI thread only swaps the cached list in. The merge is one `BEGIN IMMEDIATE` transaction, so a favorite or play-count write from the UI during the merge waits (SQLite busy timeout, 5 s) instead of failing; if such a write happened while the worker ran, the UI reloads the entries itself instead of using the worker's snapshot, so the write is never lost. The worker's connection never moves a database aside; corruption recovery only happens when the database is opened at start-up. (An in-memory database, used by some tests, cannot be shared between connections and is merged on the UI thread.)
- At most one scan runs at a time. A refresh requested meanwhile is queued; exactly one more scan runs afterwards, however many requests arrived.
- On shutdown or destruction a running scan is cancelled and joined; a cancelled scan changes nothing in the database.
- **Progress and cancellation:** the scanner reports progress at each root and at most every 100 files; the Games tab shows "N files" and the current root (`LibraryController.scanFilesSeen`, `scanCurrentRoot`) with a **Cancel** button (also Settings › Library › "Cancel scan"; `LibraryController.cancelScan()`). A cancelled scan changes nothing: existing entries keep their `present` state (tested with a 5 000-file tree in `tst_gamelibrary`). Navigation stays responsive during a scan because scanning and merging run on the worker thread.
- After each scan the UI shows "Library updated: N new, M missing, K issue(s)", and `LibraryController.lastScanSummary` / `issues` describe the last scan.

## When a scan runs

| Trigger | Scan? |
| --- | --- |
| Start-up | Only if the **scan signature** changed since the last completed scan, or nothing was scanned yet. The signature is a SHA-256 over the sorted directories, the extension → emulator map and the symlink option, stored in the database (`meta` key `scan-signature`). |
| Emulator definitions reloaded | Only if the signature changed (e.g. an emulator gained an extension). |
| `library.directories`, `library.extensions` or `library.followSymlinks` changed | Only if the signature changed. |
| Manual refresh ("Rescan library" on the Settings tab, "Rescan library" on the Home tab and in the Games tab's empty state; `LibraryController.refresh()`) | Always, exactly one scan (the scan options are updated first without starting a second scan). |

New, removed or renamed files inside an already configured directory do **not** change the signature. There is no file system watching: use the manual refresh after copying games.

## Database

SQLite file, default `$XDG_DATA_HOME/nexora/library.sqlite`, overridable with `--library-db` (location and corruption handling: [settings.md](settings.md#game-library-database); backups: [user-data.md](user-data.md#backup-and-restore)). Schema version 2 (`PRAGMA user_version`); version 1 is the table below without the last six columns:

```sql
CREATE TABLE games (
  id INTEGER PRIMARY KEY,
  path TEXT NOT NULL UNIQUE,          -- canonical absolute path
  title TEXT NOT NULL,
  extension TEXT NOT NULL,
  root TEXT NOT NULL,                 -- canonical library directory it was found under
  size INTEGER NOT NULL DEFAULT 0,
  modified INTEGER,                   -- seconds since epoch
  emulator_candidates TEXT NOT NULL DEFAULT '',  -- comma-separated emulator ids
  emulator_override TEXT NOT NULL DEFAULT '',
  present INTEGER NOT NULL DEFAULT 1,
  favorite INTEGER NOT NULL DEFAULT 0,
  play_count INTEGER NOT NULL DEFAULT 0,
  last_played INTEGER,                -- seconds since epoch, NULL = never
  added_at INTEGER NOT NULL,
  -- added in version 2:
  system_id TEXT NOT NULL DEFAULT '',          -- detected system (unique catalog match)
  system_override TEXT NOT NULL DEFAULT '',    -- user's choice; wins over system_id
  system_candidates TEXT NOT NULL DEFAULT '',  -- comma-separated system ids
  title_override TEXT NOT NULL DEFAULT '',     -- user-edited title (≤ 200 chars)
  notes TEXT NOT NULL DEFAULT '',              -- user notes (≤ 2000 chars)
  hidden INTEGER NOT NULL DEFAULT 0);          -- removed from the library by the user
CREATE INDEX games_present ON games(present);
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);  -- scan-signature, last-scan
```

A game is identified by its canonical path. On each completed scan:

| Column | On rescan |
| --- | --- |
| `title`, `extension`, `root`, `size`, `modified`, `emulator_candidates`, `system_id`, `system_candidates` | Updated from the file system / current emulator definitions / system catalog |
| `present` | Set to 1 for every file found; see [missing files](#missing-files-and-unmounted-drives) |
| `id`, `added_at`, `favorite`, `play_count`, `last_played`, `emulator_override`, `system_override`, `title_override`, `notes`, `hidden` | **Kept** (never touched by a scan) |

A renamed or moved file is a new path, so it becomes a new entry; the old one becomes missing and its favorite/history are not transferred.

**Migration from version 1:** on first open with the MVP 4 code, the database is copied to `library.sqlite.v1-backup-<yyyyMMdd-hhmmss>` (after a WAL checkpoint), then the six columns and the version bump are applied in one transaction. A failure leaves the original usable; a database with a newer version than 2 is refused and left untouched. Details: [user-data.md](user-data.md#versioned-formats-and-migrations); tested by `tst_librarymigration`.

## Missing files and unmounted drives

Entries are never deleted by a scan; they are marked not present (`present = 0`), hidden from every list, and come back with their favorite and history when the file is found again.

| Situation | Result |
| --- | --- |
| File deleted from a directory that was scanned completely | Marked missing. |
| Directory removed from `library.directories` | Its entries are marked missing. |
| Directory still configured but does not exist / is unreadable (e.g. USB drive unplugged, mount point gone) | Issue reported; its entries are **left unchanged** (still listed). Launching such a game fails with "Game file is missing". |
| Mount point still exists but the drive is not mounted (empty directory) | The empty directory is scanned, so its entries are marked missing; they return after the drive is mounted and the library is refreshed. |
| Scan truncated (file limit) or cancelled | Only entries whose directory is no longer configured are marked missing. |
| Game deleted after the last scan | Still listed until the next scan; launching is refused with "Game file is missing" and the UI stays usable. |

**Purge:** when missing entries exist, the Settings tab shows **Remove missing games** (with the count; press Accept twice to confirm). It calls `LibraryController.purgeMissing()`, which permanently deletes all entries with `present = 0`, including their favorites and history. It never runs automatically.

## Search, filters and sorting

In the dashboard:

- **Home tab:** "Continue playing" (recently played), "Favorites", "Recently added" (newest 20 by date added) and "Systems" rows with horizontal navigation, plus status cards (controllers, health summary, update availability); a getting-started panel while the library is empty.
- **Games tab:** search field (`/`, `Ctrl+F` or the Search button/key; needs a keyboard), grid/list toggle, sort selector, system filter chips, scan progress with Cancel, missing-file badges and the game grid. When the cards do not fit the available height (large interface scale at 720p) the grid switches to list rows. Accept opens the [game details](#game-details), the Favorite button (north / `F`) toggles the favorite, the Options button (Select / `O`) opens the [launch options](#launch-options). Empty states explain what to do (no folders → open Settings › Library; no games → rescan; no results → clear search).
- **Favorites** and **Recent** tabs: the same game grid for favorites and recently played games.

Implemented in `LibraryQuery` (pure, unit-tested):

- Missing and removed (hidden) entries are not shown in the normal lists.
- **Search:** the text is split at whitespace (spaces, tabs); every term must occur (case-insensitive substring) in the displayed title (the edited title if set), the file name or the system's name. Example: `mario usa` matches `Super Mario World (USA).sfc`. The library list is rebuilt 150 ms after the last keystroke.
- **System filter:** `LibraryController.systemFilter` (a system id; empty = all; `@other` = games without a recognised system, `library::UnknownSystemFilter`, never a catalog id), set from the chips on the Games tab or a Systems card. The chips list All systems, every system with games, **Other** when there are games without a system, and the active filter even if it has no games (e.g. opened from an empty system's card).
- **Filters:** `all`, `favorites`, `recent` (only games launched at least once), `missing` (query API). The `recent` filter always orders by most recently played and keeps the newest 20 entries by default; the `recentGames` list that `LibraryController` exposes to the UI keeps 12. An emulator filter (games whose candidates include an emulator id) exists in the query API.
- **Sort orders:**

| Order | Rule |
| --- | --- |
| `title` | Title, case-insensitive; ties by path |
| `recent` | Played games first, newest first; never-played games after them, by title |
| `emulator` | Effective emulator id, then title |
| `added` | Newest `added_at` first, then title |

The order comes from `library.sortOrder`. Choosing a sort order on the Games tab saves it as that setting, and changing the setting on the Settings tab re-sorts the list.

## Emulator assignment

- **Candidates:** every enabled emulator that declares the file's extension, in the order the definitions were loaded (sorted by definition file name). Stored per game and refreshed on every scan. For an `.iso` that was probed, candidates are narrowed to emulators that declare one of the file's remaining systems or declare no systems: Xenia is not offered for a PS2 disc, and a PS2 emulator is not offered for an Xbox 360 disc.
- **Effective emulator:** the per-game override if it is still a candidate; otherwise the default emulator for the game's system (`emulators.defaultForSystem`, Settings › Emulators; rows only for catalog systems with games or a compatible emulator, and only enabled emulators that declare the system can be chosen — `LibraryController.setDefaultEmulator()` validates this again) if it is a candidate; otherwise the first candidate. The game tiles, the "emulator" sort order, the game details and the launch all use this same emulator (`LibraryQuery::effectiveEmulatorId()`), so a tile names and greys out the emulator that would actually start. With no candidate the game is shown as "No emulator handles this file type".
- **Availability:** a candidate whose executable is missing or not executable is still assigned, but the game is shown as unavailable with the reason.
- **Override:** chosen in the [game details](#game-details) or the [launch options](#launch-options), stored in `emulator_override` and kept across rescans. Only candidates are accepted (`LibraryController.setGameEmulator()` refuses other emulators: a file is never assumed to work with an emulator that does not declare its extension); an empty choice returns to automatic. An override whose emulator stops declaring the extension is ignored until it does again.

## Game details

Accept on a game tile (Games, Favorites, Recent, Home rows) opens `GameDetailsScreen`; Back closes it and focus returns to the same tile. It shows the artwork placeholder, title, system, emulator, last played, play count and notes, and offers:

| Action | Effect |
| --- | --- |
| Play | launches like before (effective emulator, default profile) |
| Favorite | toggles the favorite |
| Emulator | chooses among the candidates (`LibraryController.setGameEmulator()`; only candidates are accepted) or back to automatic |
| Launch options | opens the [launch options](#launch-options) |
| Edit details | `MetadataEditor` with the on-screen keyboard: title (≤ 200 characters) and notes (≤ 2000, newlines allowed); control characters are rejected (`setGameMetadata()`); an empty title returns to the file-name title |
| Change system | chooses among the system candidates or any catalog system (`setGameSystem()`), or back to automatic |
| Remove from library | confirmation dialog stating "The game file is not deleted", default button Cancel |

The data comes from `LibraryController.gameDetails(id)` (`id, title, originalTitle, path` (redacted), `systemId, systemName, systemCandidates, emulatorId, emulatorName, emulatorCandidates, favorite, playCount, lastPlayed, addedAt, present, notes`). An unknown id shows an error state. Title, notes and every other value are rendered as plain text (markup in a file name or note is shown literally). When saving details, changing the system or removing the game fails, `LibraryController` emits an error notice with the reason (e.g. "Could not save the game details: the game is no longer in the library"); a failed save keeps the editor open with the edits. Tested: `tst_gamedetails.qml`, `tst_uicontext` (`failedEditsEmitNotices`).

## Removing and restoring entries

- **Remove from library** sets `hidden = 1`. The entry disappears from every list and rescans never unhide it; the file on disk is untouched (`GameLibrary::removeFromLibrary()`, tested in `tst_gamelibrary::removeFromLibraryHidesButNeverDeletes`). Favorites and history are kept.
- **Settings › Library › Restore removed games** unhides every hidden entry (`restoreRemovedGames()`).
- **Settings › Library › Remove missing games** is different: it permanently deletes the *database rows* of entries whose file is missing (see [above](#missing-files-and-unmounted-drives)).

## Launch options

The Options button (default Select; keyboard `O`; remappable as `options`) on a game in the Games grid or a Home row, or "Launch options" in the details, opens a modal list. Back (or Options again) closes it and focus returns to the same game.

| Row | Shown when | Effect |
| --- | --- | --- |
| Play | the file is present | Launches like Accept used to (default profile of the effective emulator, if set) |
| Play with “<profile>” | the effective emulator has enabled launch profiles | Launches with that profile |
| Play with emulator defaults | same | Launches with the emulator's own arguments, ignoring the default profile |
| Emulator: automatic / Emulator: <name> | more than one emulator declares the file's extension | Sets or clears the per-game override; unavailable emulators show their reason |
| Add to / Remove from favorites | always | Toggles the favorite |
| Close | always | Closes the dialog |

## Launching a game

`LibraryController.launch(id)`:

1. Refuses entries that are unknown or not present ("This game is no longer in the library").
2. Starts the [effective emulator](#emulator-assignment) through `AppController::launchGame()` with the profile from `emulators.defaultProfiles` for that emulator, if that profile exists and is enabled; otherwise the emulator's own arguments. (`launchWithProfile(id, profile)` from the game options uses the given profile, or none for "emulator defaults".) The running overlay shows "<title> (<emulator name>)".
3. `EmulatorManager::resolve()` builds the command line:
   - The game path must be absolute, an existing regular file and readable; otherwise "Game file is missing".
   - Arguments come from the profile if it defines `arguments`, else from the emulator.
   - Every argument containing `{file}` has it replaced by the absolute game path (also inside a larger argument, e.g. `--rom={file}`). If no argument contains `{file}`, the path is appended as the last argument.
   - The game path is only ever passed as an argument. The emulator executable is started directly with an argument list; no shell is involved, so spaces and quotes in file names need no escaping.
   - Working directory: profile `workingDirectory`, else emulator `workingDirectory`, else the executable's directory.
   - Stop timeout before SIGKILL: the emulator's `stopTimeoutMs` (500–60000) or 5000 ms.
4. Once the emulator process is actually **running**, the play count is incremented and `last_played` set. A launch that fails to start (validation error or `FailedToStart`) does not count; one that starts and later crashes does.
5. When the process ends, a launch-history record is written and the lists are refreshed ([emulator-integration.md](emulator-integration.md#launch-lifecycle)).

Starting an emulator **without** a game (from the emulator list) drops every argument that contains `{file}`.

## Limitations

- No metadata scraping, descriptions or artwork download; titles come from file names unless you edit them; covers are generated placeholders.
- No online services of any kind.
- No ROMs, BIOS files or emulators are provided; you must supply legally obtained files and install emulators yourself.
- No automatic detection of new files (manual refresh), no archive inspection (a `.zip` is one file), no multi-disc grouping.
- Renamed/moved files lose their favorite and history.
- Adding a folder and searching require a keyboard (the on-screen keyboard is used only for editing a game's title and notes); there is no folder browser.
- Not tested with real collections. A synthetic measurement (Release build, 4-core container, 10 000 files, `NEXORA_LIBRARY_BENCH=1 tst_gamelibrary benchmarkEventLoopStall`): scan + merge 0.16–0.25 s, all on the worker thread; the longest stall of the UI event loop during a scan is 5–6 ms (the timer resolution of the measurement), down from 118–139 ms (first scan) and 72–95 ms (unchanged rescan) when the merge ran on the UI thread. Rebuilding the visible lists afterwards (search + sort) takes about 8 ms on the UI thread. The 100 000-file and depth limits are enforced.

## Future plan: RetroArch and PPSSPP

A plan (in Hungarian, not yet implemented) for packaging RetroArch with libretro cores, including PPSSPP: [emulator-integracio.md](emulator-integracio.md).
