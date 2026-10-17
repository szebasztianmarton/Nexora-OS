# Security — sessions-foundation

- No shell anywhere: adapter arguments are explicit lists with placeholder substitution.
- Control socket: owner-only (`QLocalServer::UserAccessOption`), one client, lines ≤ 4096 bytes, one request at a time, timeout. A same-user process that connects first can answer, but can only point Nexora at a non-empty regular file inside the session directory (canonical path check, symlinks rejected).
- RetroArch UDP commands are unauthenticated on 127.0.0.1 (documented in security.md known limitations; adapter unverified).
- meta.json is parsed strictly (version, id = directory name, file-name regex, sha length, 1–2 states, 64 KiB cap); unreadable files are moved aside, never deleted; the state checksum is verified before every resume.
- removeSession() only accepts ids of loaded records (directory names under statesDir), so QML cannot delete arbitrary paths.
- Data safety: no state is deleted because of a failure or a limit; failed saves keep the game running; power requests are dropped if the save fails.
