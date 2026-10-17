# ADR-003: Central notification service in console-ui

**Status:** Accepted
**Date:** 2026-10-17
**Feature:** sessions-foundation
**Author:** 10x-Team (Architect + Staff Engineer)

## Context
One Toast instance is fed ad hoc by several sources and overwrites itself; there are no categories, dedup, history or quiet mode.

## Decision
A C++ `NotificationService` owned by UiContext with categories, severities, a dedup window, a queue with critical priority, bounded in-memory history and persisted warnings/criticals; settings for quiet mode, hidden categories and duration. QML shows `notifications.current`; producers call `post()`.

## Alternatives Considered
| Alternative | Pros | Cons | Why Not |
|---|---|---|---|
| Keep Toast, add a QML queue | small | logic in QML, untestable from C++, no persistence | fails dedup/history requirements |
| Notifications in shared/ for all apps | reusable by tools | only the UI shows them | YAGNI |

## Consequences
### Positive
- Testable policy; every producer gets dedup and quiet mode for free.
### Negative
- QML call sites change from Toast.show() to the service.
### Risks
- Notifications are invisible while an EGLFS emulator owns the display; they wait in the queue/history.

## Dependencies
Producers: InputController, GameSessionManager, LibraryController, SettingsController.
