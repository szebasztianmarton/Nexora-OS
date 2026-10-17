# ADR-001: Quick Resume through persistent emulator save states, one live process

**Status:** Accepted
**Date:** 2026-10-17
**Feature:** sessions-foundation
**Author:** 10x-Team (Architect + Staff Engineer)

## Context
The brief asks for Xbox-style Quick Resume. Nexora runs one emulator process (EmulatorManager). On the image the emulator owns the display through EGLFS/KMS (DRM master); a SIGSTOPped background process would most likely keep it, blocking the next game. Nothing about display hand-over is verified yet.

## Decision
Quick Resume uses persistent save states written by the emulator through a capability adapter; at most one emulator process is alive. Suspending = save state, verify, quit. Resuming = launch with the state. The existing SIGSTOP pause stays as "suspended in memory" while the Quick Menu is open. Games without a save-state capability are shown as not resumable.

## Alternatives Considered
| Alternative | Pros | Cons | Why Not |
|---|---|---|---|
| Several SIGSTOPped processes | instant switch, no emulator support needed | lost on reboot; DRM master conflict on the image; memory pressure | unproven on target, not persistent |
| Both, memory only on desktop | best desktop UX | two code paths, desktop-only feature | user chose the single path |
| CRIU process checkpoint | emulator-agnostic | root, kernel config, GPU state not checkpointable | unsafe and unverified |

## Consequences
### Positive
- Survives reboots and power loss once the state is written; works with one display owner.
### Negative
- Depends on per-emulator adapters; resume is slower than a memory resume.
### Risks
- Emulator state formats differ by version: record and check the emulator version; never delete state on mismatch.

## Dependencies
ADR-002 (adapters). Constrains later work on multi-process sessions (would need a new ADR).
