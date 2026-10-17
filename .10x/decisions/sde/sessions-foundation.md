# SDE — sessions-foundation (implementation notes)

Implemented in three commits on `claude/vibrant-mayer-f5agvc`:

1. Notification service — `apps/console-ui/src/NotificationService.{h,cpp}`; Toast.qml is its view; controller/game events posted from `UiContext::wireNotifications()`.
2. Capabilities + adapters — `ConfigLoader` parses `capabilities`; `apps/emulator-manager/src/SaveStateAdapter.cpp` (control-socket, retroarch); `LaunchRequest::extraArguments`; Qt Network added.
3. Quick Resume — `apps/console-ui/src/GameSessionManager.{h,cpp}`; LibraryController and SystemController delegate to it; `EmulatorManager::shutdown()` SIGTERM before SIGKILL; `DataLayout::statesDir`.

Deviations from the spec (all recorded in the spec):
- Critical notifications auto-hide after 3x the duration (a never-hiding critical would block every later toast); history keeps them.
- `launchArguments` added to `saveState` so `{control-socket}` is only passed when a session uses save states.
- RetroArch adapter copies each state out of its slot directory (RetroArch overwrites slot files in place).
- No `<state>/sessions.json`: each session's `meta.json` is the only record (no index/file disagreement; damage stays local).
- No `Suspending` state (SIGSTOP is synchronous); RecoveryRequired is computed by the resume checks instead of stored.
- Control socket in the temp dir (Unix socket path length), owner-only; SHA-256 on the UI thread (states are small; documented as a limitation).
