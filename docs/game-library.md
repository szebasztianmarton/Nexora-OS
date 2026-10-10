# Game library

The game library finds game files in directories you configure, remembers them in a local SQLite database, and starts them with a matching emulator. It never downloads anything, never executes game files and never scans outside the configured directories.

Code: `apps/game-library/` (`LibraryScanner`, `GameDatabase`, `LibraryQuery`, `GameLibrary`), UI glue in `apps/console-ui/src/LibraryController.cpp`, launching in `apps/emulator-manager/` and `apps/console-ui/src/AppController.cpp`.

## Contents

- [Configuring directories and file types](#configuring-directories-and-file-types)
- [Scanning](#scanning)
- [When a scan runs](#when-a-scan-runs)
- [Database](#database)
- [Missing files and unmounted drives](#missing-files-and-unmounted-drives)
- [Search, filters and sorting](#search-filters-and-sorting)
- [Emulator assignment](#emulator-assignment)
- [Launching a game](#launching-a-game)
- [Limitations](#limitations)

## Configuring directories and file types

**Directories** come from the `library.directories` setting ([settings.md](settings.md#keys)): absolute paths, at most 64, `/` refused. They can be added from the UI (`LibraryController.addDirectory()`, which additionally requires the directory to exist) or written to the settings file (existence is then only checked by the scan).

**File types** come from two places:

1. Each enabled emulator definition declares the extensions it handles:

   ```json
   { "id": "snes9x", "name": "Snes9x", "executable": "/usr/bin/snes9x",
     "arguments": ["{file}"], "extensions": [".sfc", ".smc"] }
   ```

   Entries must match `^\.[a-z0-9]{1,16}$` after trimming and lower-casing (the leading dot is required here). Disabled emulators contribute no extensions. A full example: [`config/examples/emulators/emulator-with-profiles.json`](../config/examples/emulators/emulator-with-profiles.json).

2. `library.extensions` adds extensions without an emulator (e.g. `[".zip"]`). Such files are listed but show "No emulator handles this file type" and cannot be launched until an emulator declares the extension.

Extension matching is case-insensitive and uses the text after the **last** dot (`game.tar.gz` → `.gz`). Files without an extension are ignored.

## Scanning

`LibraryScanner::scan()` walks the configured directories. It only lists directories and `stat()`s files; it never opens, reads or executes a game file.

| Rule | Behaviour |
| --- | --- |
| Roots | Only the configured directories. Relative paths, missing or unreadable directories are reported as issues and skipped. A root that resolves to `/` (also through a symlink) is refused. |
| Hidden entries | Files and directories whose name starts with `.` are skipped. |
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
- `library.followSymlinks: true`: a link is followed only if its canonical target lies inside the **same** root. Links that point elsewhere (and broken links) are skipped with the issue "symbolic link points outside the library directory; skipped". Directory loops are detected and not followed twice.

### Asynchronous scanning

- Scans run on a low-priority worker thread (`nexora-library-scan`); the UI stays responsive. Database writes happen on the UI thread after the scan finished.
- At most one scan runs at a time. A refresh requested meanwhile is queued; exactly one more scan runs afterwards, however many requests arrived.
- On shutdown or destruction a running scan is cancelled and joined; a cancelled scan changes nothing in the database.
- After each scan the UI shows "Library updated: N new, M missing, K issue(s)", and `LibraryController.lastScanSummary` / `issues` describe the last scan.

## When a scan runs

| Trigger | Scan? |
| --- | --- |
| Start-up | Only if the **scan signature** changed since the last completed scan, or nothing was scanned yet. The signature is a SHA-256 over the sorted directories, the extension → emulator map and the symlink option, stored in the database (`meta` key `scan-signature`). |
| Emulator definitions reloaded | Only if the signature changed (e.g. an emulator gained an extension). |
| `library.directories`, `library.extensions` or `library.followSymlinks` changed | Only if the signature changed. |
| Manual refresh (`LibraryController.refresh()`) | Always. |

New, removed or renamed files inside an already configured directory do **not** change the signature. There is no file system watching: use the manual refresh after copying games.

## Database

SQLite file, default `$XDG_DATA_HOME/nexora/library.sqlite`, overridable with `--library-db` (location, corruption handling and backups: [settings.md](settings.md#game-library-database)). Schema version 1 (`PRAGMA user_version`):

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
  added_at INTEGER NOT NULL);
CREATE INDEX games_present ON games(present);
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);  -- scan-signature, last-scan
```

A game is identified by its canonical path. On each completed scan:

| Column | On rescan |
| --- | --- |
| `title`, `extension`, `root`, `size`, `modified`, `emulator_candidates` | Updated from the file system / current emulator definitions |
| `present` | Set to 1 for every file found; see [missing files](#missing-files-and-unmounted-drives) |
| `id`, `added_at`, `favorite`, `play_count`, `last_played`, `emulator_override` | **Kept** |

A renamed or moved file is a new path, so it becomes a new entry; the old one becomes missing and its favorite/history are not transferred.

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

**Purge:** `LibraryController.purgeMissing()` permanently deletes all entries with `present = 0`, including their favorites and history. It only runs as an explicit user action.

## Search, filters and sorting

Implemented in `LibraryQuery` (pure, unit-tested):

- Missing entries are never shown.
- **Search:** the text is split at spaces; every term must occur (case-insensitive substring) in the title **or** the file name. Example: `mario usa` matches `Super Mario World (USA).sfc`. The library list is rebuilt 150 ms after the last keystroke.
- **Filters:** `all`, `favorites`, `recent` (only games launched at least once). The `recent` filter always orders by most recently played and keeps the newest 20 entries by default; the `recentGames` list that `LibraryController` exposes to the UI keeps 12. An emulator filter (games whose candidates include an emulator id) exists in the query API.
- **Sort orders:**

| Order | Rule |
| --- | --- |
| `title` | Title, case-insensitive; ties by path |
| `recent` | Played games first, newest first; never-played games after them, by title |
| `emulator` | Effective emulator id, then title |
| `added` | Newest `added_at` first, then title |

The initial order comes from `library.sortOrder`; the library screen can change it for the session.

## Emulator assignment

- **Candidates:** every enabled emulator that declares the file's extension, in the order the definitions were loaded (sorted by definition file name). Stored per game and refreshed on every scan.
- **Effective emulator:** the per-game override if it is still a candidate, otherwise the first candidate. With no candidate the game is shown as "No emulator handles this file type".
- **Availability:** a candidate whose executable is missing or not executable is still assigned, but the game is shown as unavailable with the reason.
- **Override:** stored in `emulator_override` and kept across rescans (`GameLibrary::setEmulatorOverride()`, covered by `tst_gamelibrary`). It is not yet exposed in the UI.

## Launching a game

`LibraryController.launch(id)`:

1. Refuses entries that are unknown or not present ("This game is no longer in the library").
2. Starts the effective emulator through `AppController::launchGame()` with the profile from `emulators.defaultProfiles` for that emulator, if that profile exists and is enabled; otherwise the emulator's own arguments. The running overlay shows "<title> (<emulator name>)".
3. `EmulatorManager::resolve()` builds the command line:
   - The game path must be absolute, an existing regular file and readable; otherwise "Game file is missing".
   - Arguments come from the profile if it defines `arguments`, else from the emulator.
   - Every argument containing `{file}` has it replaced by the absolute game path (also inside a larger argument, e.g. `--rom={file}`). If no argument contains `{file}`, the path is appended as the last argument.
   - The game path is only ever passed as an argument. The emulator executable is started directly with an argument list; no shell is involved, so spaces and quotes in file names need no escaping.
   - Working directory: profile `workingDirectory`, else emulator `workingDirectory`, else the executable's directory.
   - Stop timeout before SIGKILL: the emulator's `stopTimeoutMs` (500–60000) or 5000 ms.
4. If the launch was accepted, the play count is incremented and `last_played` set. This happens when the process is being started, so a launch that later crashes still counts.

Starting an emulator **without** a game (from the emulator list) drops every argument that contains `{file}`.

## Limitations

- No metadata scraping, descriptions or artwork; titles come from file names.
- No online services of any kind.
- No ROMs, BIOS files or emulators are provided; you must supply legally obtained files and install emulators yourself.
- No automatic detection of new files (manual refresh), no archive inspection (a `.zip` is one file), no multi-disc grouping.
- Renamed/moved files lose their favorite and history.
- Per-game emulator override has no UI yet.
- Not tested with very large real collections; the 100 000-file limit and depth limit are enforced, but performance was only checked with the small test fixtures.
