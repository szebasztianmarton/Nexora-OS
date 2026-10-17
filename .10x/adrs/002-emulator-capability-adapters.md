# ADR-002: Emulator capability block and save-state adapters

**Status:** Accepted
**Date:** 2026-10-17
**Feature:** sessions-foundation
**Author:** 10x-Team (Architect + Staff Engineer)

## Context
Only `pausable` exists. Save states need a way to ask a running emulator to save and to start it with a state, without shell commands in configuration.

## Decision
Optional `capabilities` object in emulator definitions (`pause`, `saveState{adapter, timeoutMs, resumeArguments, port}`) and a C++ `SaveStateAdapter` interface with two implementations: `control-socket` (Nexora line protocol over a per-session Unix socket, implemented by the fake emulator) and `retroarch` (network commands + appended config; marked unverified). Placeholders `{state-file}` and `{control-socket}` are substituted into explicit argument lists only.

## Alternatives Considered
| Alternative | Pros | Cons | Why Not |
|---|---|---|---|
| Configurable shell commands for save/load | flexible | arbitrary command execution from config | violates the no-shell rule |
| Hard-coded RetroArch only | less abstraction | cannot be tested without RetroArch | untestable now |
| Signals (SIGUSR1 = save) | simple | no path, no result, emulator-specific | no error reporting |

## Consequences
### Positive
- Testable end to end with the fake emulator; real emulators add an adapter.
### Negative
- The control-socket protocol must be implemented by an emulator or wrapper.
### Risks
- RetroArch adapter details unverified: documented as such until integration tests exist.

## Dependencies
Used by ADR-001. Later per-game profiles validate settings against these capabilities.
