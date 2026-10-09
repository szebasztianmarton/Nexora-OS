Nexora OS

A Linux-based gaming operating system built for controller-first gaming.

Nexora OS is an open-source gaming operating system designed to provide a console-like experience on x86-64 PCs and mini PCs. Its goal is to bring compatible native Linux emulators together in a unified, controller-driven interface.

Vision

Create a dedicated gaming environment that boots directly into a simple, polished interface, allowing users to browse, launch, and manage their emulators without needing a keyboard or mouse for everyday use.

MVP — Minimum Viable Product

The first version of Nexora OS will focus on a small, reliable, testable foundation rather than supporting every emulator or gaming platform.

MVP goals

* Bootable Linux environment: Create a minimal Linux-based operating system using Buildroot.
* Controller-first UI: Build a Qt 6 / QML interface navigable with a game controller.
* Emulator Manager: Launch and monitor compatible native Linux emulator processes.
* Process lifecycle management: Handle successful exits, crashes, missing executables, invalid configurations, and launch failures.
* Emulator configuration: Define emulator executables, launch arguments, and settings through configuration files.
* Logging and diagnostics: Record application events and errors to simplify troubleshooting.
* Automated testing: Test core functionality with a mock emulator and automated unit and integration tests.
* Reproducible builds: Use CMake, Ninja, and a documented build process.

MVP scope

The initial release should demonstrate the complete basic workflow:

1. Boot into Nexora OS.
2. Navigate the interface using a controller.
3. Select an available emulator.
4. Launch a test application or compatible emulator.
5. Return to the Nexora OS interface after the process exits.
6. Recover gracefully from failures and provide useful logs.

MVP acceptance criteria: The system can boot on the selected test hardware, the UI is usable with a controller, and the emulator launch-and-return workflow passes automated tests.

Technology Stack

* Operating system: Buildroot-based Linux
* Languages: C++20
* UI: Qt 6 and QML
* Build system: CMake and Ninja
* Process management: Qt QProcess
* Input: SDL3 and/or Linux input interfaces
* Configuration and persistence: JSON initially, SQLite where structured persistence is needed
* Testing: Qt Test and CTest
* CI: GitHub Actions

Development Roadmap

Phase 1 — Foundation

* Set up the repository and project structure.
* Establish coding standards and the build system.
* Implement shared types, configuration handling, and logging.
* Configure automated builds and tests.

Phase 2 — Desktop Prototype

* Create the main QML interface.
* Implement controller navigation.
* Build the emulator library and selection screen.
* Add a mock emulator for repeatable testing.

Phase 3 — Emulator Management

* Implement process launching and monitoring.
* Handle crashes, timeouts, and invalid configurations.
* Add emulator profiles and error reporting.
* Test the complete launch-and-return workflow.

Phase 4 — Bootable Nexora OS

* Create the Buildroot external tree.
* Package the application and its dependencies.
* Configure startup, graphics, input, and system services.
* Produce a bootable image for selected x86-64 hardware.

Phase 5 — MVP Release

* Validate the image on target hardware.
* Run integration and regression tests.
* Document installation, configuration, troubleshooting, and known limitations.
* Publish an initial experimental release.

Future Versions

v0.2 — Improved Experience

* Better controller support and hot-plug handling.
* Persistent settings and configurable controls.
* Improved error messages and diagnostics.
* A more polished library interface.

v0.5 — Expanded Emulator Support

* Integration of additional compatible native Linux emulators.
* Per-emulator settings and launch profiles.
* Game library scanning and metadata.
* Save-state and artwork integration where supported by the selected emulator.

v1.0 — Stable Release

* Reliable installation and system updates.
* Hardware compatibility testing.
* Recovery and configuration-reset options.
* Documented release and regression-testing process.

Design Principles

* Controller-first interaction
* Minimal distractions
* Modular architecture
* Open-source components
* Reliable process management
* Reproducible builds
* Clear licensing and attribution

Scope and Limitations

Nexora OS will integrate existing emulator projects rather than reimplement every console emulator from scratch. Platform support will depend on emulator compatibility, hardware requirements, and licensing.

Game files, BIOS files, firmware, and other copyrighted materials will not be bundled unless redistribution is explicitly permitted. Users must supply files they are legally entitled to use.

Project Status

Current stage: Initial development and MVP planning.

Nexora OS is experimental software. Hardware compatibility, supported emulators, and release readiness will be documented as development progresses.