# Sub-project 2 — Collections, pinned games, play time, customisable Home

Plan steps 4–5 of docs/console-experience.md. Decisions by the user (2026-10-17): collections are managed from Game options, filtered on the Games tab and shown as Home rows (no new tab); Home is customised from Settings › Home (show/hide, move, reset); play time counts emulator run time without Nexora's pauses.

## 1. Library schema v3 (GameDatabase)

- `games` gains `play_seconds INTEGER NOT NULL DEFAULT 0` and `pinned INTEGER NOT NULL DEFAULT 0` (0 = not pinned, otherwise pin order, ascending).
- `collections(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE COLLATE NOCASE, created_at INTEGER NOT NULL)`.
- `collection_games(collection_id INTEGER NOT NULL, game_id INTEGER NOT NULL, position INTEGER NOT NULL, added_at INTEGER NOT NULL, PRIMARY KEY(collection_id, game_id))`; memberships are removed explicitly when a collection is deleted or a game row is purged (no reliance on `PRAGMA foreign_keys`).
- Migration v2 → v3 (and v1 → v2 → v3) in one transaction after the existing `library.sqlite.v<N>-backup-<time>` copy; a newer schema is still refused untouched.
- Limits: collection names 1–60 characters, no control characters, unique ignoring case; at most 100 collections; at most 5000 games per collection; at most 24 pinned games.
- Deleting a collection never touches games; removing a game from a collection never touches the game; "Remove missing games" also drops their memberships.

## 2. Play time

- `ProcessOutcome` gains `pausedMs` (time spent SIGSTOPped by Nexora). Play time of a run = (endedAt − startedAt) − pausedMs, added to the game when the run lasted at least 1 s.
- Recorded by LibraryController for the game whose launch it counted (library launches and Quick Resume resumes); emulator-only launches have no game.
- Shown in game details ("Played 3 h 12 min") and on the System tab's launch history is unchanged.

## 3. Collections and pinning (LibraryController)

- `collections` property: `[{id, name, count}]` sorted by name.
- Invokables: `createCollection(name) -> id`, `renameCollection(id, name)`, `deleteCollection(id)`, `setInCollection(gameId, collectionId, bool)`, `collectionsOf(gameId)`, `setPinned(gameId, bool)`, `collectionGames(id) -> GameListModel*` (cached per id), `pinnedGames` model.
- Games tab: the system chips row gains collection chips (filter `collection:<id>`), cleared when the collection is deleted.
- Errors (duplicate name, limit, database failure) are reported through the existing `notice` signal → notifications.

## 4. Home layout (Settings › Home)

- Setting `home.sections`: ordered string array of visible section ids; allowed ids `statusCards`, `quickResume`, `pinned`, `recent`, `favorites`, `recentlyAdded`, `systems`, `collection:<id>`; duplicates removed; unknown ids dropped at load with an issue; default = `statusCards, quickResume, pinned, recent, favorites, recentlyAdded, systems`.
- A `collection:<id>` whose collection no longer exists is skipped when rendering (and removed when the collection is deleted).
- HomeScreen renders the quick actions first (always), then the sections in that order; sections without content stay hidden as today; focus order follows the visual order.
- Settings › Home: one row per known section (including every collection): shown/hidden toggle, Move up / Move down, and "Reset Home layout".

## 5. Game options

- "Pin to Home" / "Unpin from Home".
- "Collections…": a page listing all collections with a check mark for membership (Accept toggles) and "New collection…" (on-screen keyboard; the new collection gets the game).
- Settings › Library › Collections: rename (on-screen keyboard) and delete (confirmation; "games are not removed").

## 6. Tests and docs

Unit/integration: schema creation, v2 → v3 and v1 → v3 migration with backup and data preserved, collection CRUD and limits, membership cleanup on purge, pin order, play time accounting incl. pauses (fake emulator), `home.sections` validation. QML smoke: Home order follows the setting, Settings › Home reorder/hide, Game options pin + collection, Games collection filter. Docs: game-library.md (collections, pinning, play time, schema v3), settings.md (`home.sections`), user-data.md (schema v3 migration), design-system.md, testing.md, console-experience.md status.
