# Handoff

From: Discovery (3 read-only Explore audits: UI/input, sessions/library/save data, system/image).
To: Brainstorming for the first sub-project.

- Gap report and dependency-aware plan: `docs/console-experience.md`.
- Recommended first sub-project: notification service + emulator capability model + game session manager (plan steps 1-3), because Quick Resume, a safe shutdown and the Quick Menu completion depend on them.
- Key constraints: no packaged emulator (adapters can only be proven with the fake emulator), shell cannot draw over EGLFS games, no network/automount on the image, Qt 6.4 minimum, untested hardware.
- Known reliability bug found: a running emulator is SIGKILLed with no grace at shutdown (`EmulatorManager::shutdown`, apps/emulator-manager/src/EmulatorManager.cpp:268-284).
