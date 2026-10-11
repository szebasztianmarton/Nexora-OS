# Nexora OS — MVP 4: Production-Ready Gaming Operating System

> This is the MVP 4 brief as given at the start of the milestone (kept verbatim for reference). The implementation status against it is tracked in [mvp-4.md](mvp-4.md).

## 1. Project Overview

You are working on Nexora OS, an open-source, Linux-based gaming operating system designed primarily for x86-64 desktop computers and mini PCs.

Nexora OS transforms a conventional PC into a console-like gaming environment. Its primary purpose is to provide a polished, controller-first interface for discovering, organizing, configuring, and launching locally installed games through open-source emulators.

The system must be designed to operate locally, without requiring a cloud account or a mandatory online service.

MVP 4 builds upon the previous development milestones:

- MVP 1: Qt6/QML desktop prototype, controller input, emulator process management, logging, mock emulator integration, automated tests, and CMake/Ninja build infrastructure.
- MVP 2: Real gaming interface, controller-driven navigation, hardware integration, and Linux gaming environment.
- MVP 3: Bootable standalone Linux gaming operating system integrating the interface, operating-system services, and emulator launching.
- MVP 4: Reliable gaming platform with persistent user data, a complete game library, robust emulator integration, update and recovery mechanisms, diagnostics, security hardening, and release engineering.

Do not discard working implementations from earlier MVPs. Inspect the existing repository before making architectural decisions.

The objective is to turn the existing project into a coherent, testable, maintainable gaming OS rather than creating a disconnected demonstration.

## 2. Core Principles

Follow these principles throughout development:

1. Controller-first: Every essential operation must work without a keyboard or mouse.
2. Local-first: Game metadata, preferences, configuration, and user data remain under the user's control.
3. Open-source: Prefer compatible open-source components and document all external dependencies and their licenses.
4. Reliable: Failures must be recoverable and must never silently corrupt persistent user data.
5. Modular: The interface, emulator integrations, operating-system services, and update mechanisms must have clear boundaries.
6. Accessible: Provide readable typography, configurable interface scale, clear focus indicators, and reduced-motion support.
7. Hardware-aware: Detect available capabilities and expose appropriate functionality without assuming that every PC has identical hardware.
8. Reproducible: Builds, tests, and release artifacts must be reproducible to the extent supported by the chosen toolchain.
9. Safe by default: Validate external inputs, minimize privileges, and never execute arbitrary files simply because they appear in the game library.
10. Honest compatibility: Clearly distinguish tested hardware and software combinations from unverified ones.

## 3. Technology Stack

Preserve the existing stack where practical.

### Core application
- C++20
- Qt 6
- Qt Quick / QML
- Qt Quick Controls
- CMake
- Ninja
- Qt Test
- CTest

### Operating system
- Linux
- x86-64 as the initial target
- Buildroot as the intended long-term system-image build environment
- Linux kernel and drivers selected according to supported hardware
- A minimal init and service architecture appropriate to the chosen Buildroot configuration

### Integration
- QProcess or a dedicated process-management service for emulator execution
- Linux input facilities and appropriate Qt input integration for controller discovery
- PipeWire or the selected supported audio subsystem
- Mesa and appropriate GPU drivers where compatible with the target hardware
- A versioned local configuration and metadata storage format

Do not introduce large frameworks without demonstrating a clear benefit. Keep dependencies documented, pinned where practical, and compatible with the target root filesystem.

Do not assume that every desktop Linux distribution component is automatically available in a minimal Buildroot image.

## 4. Repository Audit and Migration

Before implementation:

1. Inspect the repository tree, build configuration, existing source files, QML components, scripts, tests, and documentation.
2. Identify the actual state of MVP 1, MVP 2, and MVP 3.
3. Build the existing project and run all available tests.
4. Document working functionality, missing functionality, technical debt, and integration risks.
5. Identify whether a bootable image and emulator integrations already exist.
6. Preserve working features and interfaces unless a justified migration is necessary.
7. Produce a short implementation plan with dependencies and acceptance criteria.

Never report an existing feature as complete without verifying it in the repository.

## 5. Application Architecture

Use clearly separated modules.

### 5.1 Gaming Shell

Responsibilities:
- Main navigation
- Home dashboard
- Game library
- Game details
- Emulator management
- Settings
- Notifications
- Quick menu
- Controller navigation
- Loading, empty, and error states

QML should primarily manage presentation and interaction. System operations must be implemented through well-defined C++ services.

### 5.2 Game Library Service

Responsibilities:
- Add and remove library locations
- Discover supported game entries
- Store game metadata
- Search and filter
- Maintain favorites and recently played entries
- Track library scan status
- Detect missing files
- Avoid duplicate entries
- Preserve user corrections to imported metadata

Use a versioned persistent schema. SQLite is an option if supported and justified by the target system; a simpler versioned data store is acceptable for a deliberately small implementation.

Library scanning must run asynchronously and must never freeze the UI.

### 5.3 Emulator Service

Define a common emulator-adapter interface.

Each adapter should describe:
- Unique emulator identifier
- Display name and version
- Supported systems and file formats
- Executable or approved launch method
- Required and optional arguments
- Supported configuration capabilities
- Availability and health status
- Error reporting
- Graceful shutdown behavior

Keep emulator-specific launch logic out of QML.

Support a configurable mock adapter for automated testing without requiring real game files or emulators.

### 5.4 Controller Service

Responsibilities:
- Discover and disconnect controllers
- Normalize controller input
- Handle button mappings
- Handle analog sticks and dead zones
- Support configurable input mappings
- Detect device removal during navigation or gameplay
- Prevent duplicate actions from key-repeat events
- Expose connection status to the UI

Use stable logical actions such as NavigateUp, NavigateDown, Confirm, Back, OpenQuickMenu, and Pause.

Do not hardcode one controller brand or assume every controller has the same button layout.

### 5.5 Operating System Service

Responsibilities:
- Expose system information
- Report CPU, memory, storage, and graphics information where available
- Expose audio-device information
- Report OS version and build identifier
- Provide safe shutdown and reboot requests
- Report available system updates
- Report diagnostic status

Privileged operations must use a minimal, explicit interface. Never expose an unrestricted shell command executor to QML.

### 5.6 Update and Recovery Service

Responsibilities:
- Identify the current system version
- Verify update metadata and integrity
- Verify release authenticity using trusted signing keys
- Download or import a system image
- Validate storage capacity before writing
- Report progress and errors
- Preserve user data during supported upgrades
- Recover from failed updates where the underlying boot and storage architecture supports it

Do not claim atomic updates or automatic rollback unless the selected partition, bootloader, and update design actually implement them.

### 5.7 Logging and Diagnostics

Provide structured logs for:
- Application startup
- Hardware discovery
- Controller events at appropriate diagnostic levels
- Library scans
- Emulator launch and exit
- Update operations
- Recovery events
- Unexpected errors

Redact secrets and sensitive paths where appropriate. Never log the contents of game files or private user data.

Include log rotation, bounded disk usage, exportable diagnostic reports, and configurable verbosity.

## 6. User Interface and Design System

Create a polished, modern, console-oriented interface.

### 6.1 Visual direction

The interface should feel like a dedicated gaming console rather than a traditional desktop environment.

Use:
- A dark, high-contrast foundation
- Restrained accent colors
- Large game artwork
- Clear information hierarchy
- Consistent spacing and rounded surfaces
- Subtle focus transitions
- Lightweight animation
- Clear selected, focused, disabled, loading, and error states

Avoid excessive glass effects, unnecessary gradients, tiny text, and decorative elements that reduce readability.

All visual decisions must be implemented as reusable design tokens.

### 6.2 Design tokens

Define reusable tokens for:
- Background and surface colors
- Primary and secondary text
- Accent and semantic status colors
- Spacing
- Corner radii
- Typography scale
- Icon sizes
- Focus-ring dimensions
- Animation durations
- Grid and card dimensions

Include:
- Default dark theme
- High-contrast mode
- Configurable interface scale
- Reduced-motion mode

Use a consistent icon set and a clear typography hierarchy.

### 6.3 Main navigation

Provide controller-friendly access to:

1. Home
2. Games
3. Systems
4. Favorites
5. Recently Played
6. Downloads or Imports, if implemented
7. Settings
8. System Information

Only expose navigation items for implemented capabilities. Unavailable functionality must not appear as a fake working feature.

### 6.4 Home Dashboard

Display:
- Recently played games
- Favorite games
- Recently added games
- Installed gaming systems
- Controller connection status
- System health summary
- Update availability, when applicable

Allow users to navigate horizontally through featured game cards using a controller.

Keep the interface responsive at common TV resolutions, including 1280×720 and 1920×1080.

### 6.5 Game Library

Provide:
- Grid and list views
- Search
- Sorting by title, recently played, and date added
- Filtering by gaming system
- Favorites
- Recently played
- Game availability status
- Cover artwork
- Empty-library onboarding
- Library scanning progress
- Missing-file warnings

Game cards must have visible focus states and must remain usable when artwork is missing.

### 6.6 Game Details

Display:
- Game title
- Cover artwork
- Gaming system
- Selected emulator
- Last-played information
- User-defined metadata
- Launch action
- Emulator configuration action
- Favorite toggle
- Remove-from-library action

Do not delete original game files when removing an entry from the library.

### 6.7 Quick Menu

Provide an overlay accessible through a configurable controller shortcut.

Include:
- Return to game
- Resume or pause, when supported by the emulator
- Controller status
- Audio controls
- Display settings shortcut
- Exit game
- System information shortcut

Use an explicit confirmation when an operation could terminate a running game or discard unsaved emulator state.

### 6.8 Settings

Organize settings into the following sections:

General:
- Language where translations exist
- UI scale
- Theme
- Reduced motion
- Startup behavior

Controllers:
- Connected devices
- Input mapping
- Dead-zone configuration
- Test-input screen

Display:
- Available resolutions
- Refresh-rate choices where supported
- Display selection where supported
- UI scaling

Audio:
- Available output devices
- Output selection
- Volume controls
- Mute

Library:
- Library locations
- Rescan
- Artwork preferences
- Metadata overrides

Emulators:
- Installed and detected emulators
- Default emulator per supported system
- Per-emulator configuration
- Diagnostic status

System:
- OS version
- Build identifier
- Storage usage
- Update status
- Diagnostics
- Restart and shutdown

Settings must persist across reboots and be validated before application.

## 7. Game Discovery and Import

Implement a predictable library-import workflow.

Users must be able to select or configure approved local library directories.

The scanning process must:
- Run in a background worker
- Support cancellation
- Report progress
- Handle inaccessible directories
- Avoid traversing unwanted system directories
- Avoid following symbolic links in a way that creates recursive loops
- Detect duplicate entries
- Preserve existing metadata
- Record import errors
- Resume normal UI interaction during scanning

Use explicit file-format and emulator-compatibility rules.

Do not download, distribute, or bundle copyrighted commercial games, BIOS files, or firmware without appropriate authorization.

The OS must not imply that a file is compatible simply because its extension looks familiar.

## 8. Emulator Execution and Lifecycle

Implement a complete emulator lifecycle:

1. Validate the game entry.
2. Verify that the selected emulator is available.
3. Validate the launch configuration.
4. Display a loading state.
5. Start the emulator through the dedicated process manager.
6. Record the start time and process identifier.
7. Detect successful startup or failure.
8. Track process termination.
9. Capture a safe, bounded diagnostic record.
10. Restore the gaming shell.
11. Update play history.
12. Refresh relevant library state.

Handle:
- Missing executable
- Invalid configuration
- Unsupported game format
- Failed process creation
- Non-zero exit code
- Unexpected termination
- Application shutdown while a child process is running
- Controller disconnection
- Duplicate launch attempts

Do not construct shell command strings from untrusted metadata. Pass executable arguments as distinct arguments and use a strict allowlist for any supported launch configuration.

Support graceful termination first. Use forced termination only as a bounded fallback.

## 9. Persistent User Data

Define and document a stable user-data layout.

Separate:
- System files
- User configuration
- Game-library metadata
- Emulator configuration
- Save files
- Artwork cache
- Logs
- Update metadata
- Diagnostic reports

Requirements:
- User data must survive normal application restarts.
- Supported OS upgrades must preserve user data.
- Resetting application preferences must not silently delete game files.
- Cache cleanup must not remove user-created saves.
- Schema migrations must be versioned and testable.
- Interrupted writes must not leave the primary database unusable.

Use atomic file replacement where appropriate and create backups before destructive schema migrations.

Document backup and restore procedures.

## 10. System Image and Boot Experience

Integrate the gaming shell with the existing Linux boot process.

Requirements:
- Boot into the intended gaming session.
- Avoid exposing an unnecessary desktop environment.
- Display a clear startup or recovery state when startup fails.
- Start services in dependency order.
- Avoid indefinite boot waits caused by optional hardware.
- Preserve useful diagnostic logs after failed startup.
- Support a documented recovery or maintenance mode.
- Provide a safe shutdown path.

Keep the system image minimal without removing drivers or services required for supported hardware.

### Buildroot integration

Maintain a reproducible Buildroot configuration where feasible.

Document:
- Buildroot version
- Kernel configuration
- Root filesystem configuration
- Toolchain requirements
- Qt configuration
- Required packages
- Firmware and driver requirements
- Bootloader assumptions
- Image-generation process
- Image verification procedure

Do not mix development-host paths into the target filesystem.

Provide a clearly documented development build separate from the release image.

## 11. Updates and Recovery

Design the update system around explicit, verifiable state transitions.

Suggested states:

- Idle
- Checking
- Available
- Downloading
- Verifying
- Ready to Install
- Installing
- Restart Required
- Completed
- Failed
- Recovery Required

Requirements:
- Verify signatures against trusted keys.
- Verify image checksums.
- Reject malformed metadata.
- Reject incompatible target images.
- Detect insufficient storage.
- Avoid replacing the active bootable system prematurely.
- Preserve user data.
- Report meaningful errors.
- Record the previous and new build identifiers.
- Support recovery through a documented procedure.

For a robust rollback design, prefer an architecture with separate system slots or another explicitly validated recovery mechanism.

If rollback is not implemented for the current hardware configuration, state that limitation clearly.

Updates must never silently execute arbitrary downloaded scripts with unrestricted privileges.

## 12. Security and Privacy

Implement the following safeguards:

- Run the gaming shell with the least privileges required.
- Keep privileged system actions in a restricted service.
- Validate paths and configuration values.
- Prevent path traversal in imported metadata.
- Avoid shell injection.
- Restrict emulator launch configuration to approved executables and arguments.
- Verify update authenticity and integrity.
- Protect update signing keys from inclusion in public builds.
- Avoid collecting telemetry by default.
- Redact sensitive information from diagnostic exports.
- Set safe file permissions for user data.
- Handle removable media and disappearing mounts gracefully.

Document the threat model and known security limitations.

## 13. Performance and Reliability

Set measurable targets and verify them on named test hardware.

Initial engineering targets:
- Main-menu navigation should remain responsive during library scanning.
- Normal navigation should not block on disk I/O or emulator discovery.
- Library scanning should be cancellable.
- The UI should recover from emulator termination.
- Memory usage should be measured during extended operation.
- Background tasks must have explicit cancellation and shutdown behavior.
- Artwork loading must not block controller input.
- Logs and caches must have bounded storage usage.

Measure startup time, UI responsiveness, memory consumption, and crash frequency. Report actual results instead of claiming performance improvements without evidence.

## 14. Testing Strategy

### Unit tests
Test:
- Configuration parsing and validation
- Library indexing
- Duplicate detection
- Metadata persistence
- Emulator adapter behavior
- Process-state transitions
- Controller mapping
- Update-state transitions
- Error handling
- Schema migration

### Integration tests
Test:
- Game import and removal
- Emulator launch using a mock executable
- Child-process termination
- UI restoration after emulator exit
- Controller disconnection
- Settings persistence
- Interrupted library scans
- Update verification failures
- Backup and restore
- Recovery from malformed configuration

### UI tests
Test:
- Main navigation
- Focus movement
- Confirmation dialogs
- Empty and loading states
- Controller-only operation
- High-contrast mode
- Reduced-motion mode
- Common TV resolutions

### System tests
Where the environment permits:
- Bootable image generation
- Boot in a virtual machine
- Shell startup
- Shutdown
- Image integrity verification
- Storage failure scenarios
- Update and recovery workflows
- Real controller detection on supported hardware

Do not make physical-hardware test claims based solely on a virtual-machine test.

## 15. CI/CD and Release Engineering

Extend the existing CMake/Ninja and GitHub Actions setup.

The CI pipeline should include:
1. Formatting and static checks.
2. C++ compilation.
3. QML validation where tooling supports it.
4. Unit tests.
5. Integration tests using mock components.
6. Security-oriented input-validation tests.
7. Packaging checks.
8. Documentation checks.
9. Buildroot configuration validation.
10. Release artifact integrity verification.

Provide:
- Versioned release artifacts
- Build identifiers
- Checksums
- Release notes
- Known limitations
- Hardware compatibility notes
- Installation instructions
- Recovery instructions

Signing should be implemented with a defined trust model rather than a placeholder verification function.

## 16. Documentation Deliverables

Create or update these documents:

README.md
docs/architecture.md
docs/roadmap.md
docs/mvp-4.md
docs/building.md
docs/installation.md
docs/hardware-compatibility.md
docs/controller-support.md
docs/game-library.md
docs/emulator-integration.md
docs/user-data.md
docs/update-system.md
docs/recovery.md
docs/security.md
docs/testing.md
docs/release-process.md
docs/troubleshooting.md
docs/design-system.md

Each document must describe the implementation that actually exists.

Include diagrams for:
- Application architecture
- Game-launch lifecycle
- Controller-input flow
- User-data organization
- Update and recovery flow
- Boot and service dependencies

## 17. Recommended Implementation Phases

### Phase 1: Audit and stabilization
- Audit previous MVPs.
- Build the existing code.
- Run existing tests.
- Fix critical regressions.
- Document the current architecture.

### Phase 2: Library and emulator integration
- Implement persistent game metadata.
- Complete the library scanning workflow.
- Finalize the emulator-adapter interface.
- Implement process lifecycle management.
- Add mock-based integration tests.

### Phase 3: Controller and UI completion
- Finalize navigation and focus handling.
- Complete the game library and details screens.
- Implement the quick menu.
- Finish controller settings.
- Validate keyboard-free operation.

### Phase 4: User data and diagnostics
- Finalize data-directory separation.
- Implement migrations and backups.
- Add structured logging.
- Implement diagnostic exports.
- Test failure recovery.

### Phase 5: OS integration
- Integrate shell startup with the Linux session.
- Verify service ordering.
- Validate hardware detection.
- Test shutdown and recovery mode.
- Validate the Buildroot image.

### Phase 6: Updates and recovery
- Implement signed update metadata.
- Verify image integrity.
- Implement safe installation states.
- Test interrupted updates.
- Implement or document the available rollback mechanism.

### Phase 7: Quality assurance
- Complete automated tests.
- Test in a virtual machine.
- Test on available physical hardware.
- Profile performance.
- Resolve release-blocking defects.

### Phase 8: MVP 4 release candidate
- Generate versioned artifacts.
- Verify checksums and signatures.
- Publish documentation and release notes.
- Record known limitations.
- Produce a final acceptance report.

## 18. MVP 4 Acceptance Criteria

MVP 4 is complete only when the following criteria are demonstrated:

- [ ] The project builds using documented instructions.
- [ ] Existing MVP functionality remains operational.
- [ ] The gaming shell starts in the intended environment.
- [ ] Essential navigation works with a supported controller.
- [ ] Games can be imported from approved local directories.
- [ ] Library metadata persists across restarts.
- [ ] Duplicate and missing game entries are handled safely.
- [ ] A mock emulator can be launched and terminated through the process manager.
- [ ] The shell returns to a valid state after emulator exit.
- [ ] Emulator failures are reported clearly.
- [ ] User settings persist across restarts.
- [ ] Logs and diagnostics are available.
- [ ] Backup and restore procedures are tested.
- [ ] Update artifacts are authenticated and integrity-checked.
- [ ] Unsupported update or rollback scenarios are explicitly documented.
- [ ] CI tests pass in the supported build environment.
- [ ] A bootable image is generated and tested where the build environment permits.
- [ ] Installation, compatibility, recovery, and troubleshooting documentation is complete.
- [ ] No critical release-blocking defects remain.

## 19. Development Rules

Work incrementally.

For every implementation phase:

1. Inspect the existing code.
2. Explain the intended changes.
3. Implement a cohesive, testable unit.
4. Build and run relevant tests.
5. Fix regressions before continuing.
6. Update documentation.
7. Record remaining limitations.

Do not replace working code with empty stubs to satisfy a checklist.

Do not claim that an emulator, hardware feature, installer, updater, or recovery mechanism works unless it has been implemented and tested.

Do not hide failing tests or suppress errors to make CI appear successful.

If a requirement depends on unavailable hardware or infrastructure, implement the testable portion and document what remains unverified.

Prioritize reliability, maintainability, controller usability, and safe operating-system behavior over feature count.

## 20. Final Deliverables

The final MVP 4 implementation must include:

1. Updated Nexora OS source code.
2. A complete Qt6/QML gaming interface.
3. A modular C++20 backend.
4. A persistent game-library implementation.
5. Emulator adapters and process management.
6. Controller discovery and input mapping.
7. Settings and user-data persistence.
8. Structured logging and diagnostics.
9. Update-integrity verification and the implemented recovery mechanism.
10. Automated tests and CI configuration.
11. Buildroot integration and a bootable image where supported.
12. Complete architecture, build, installation, security, and recovery documentation.
13. A release checklist and a truthful MVP 4 acceptance report.

Start by auditing the existing repository and identifying the actual completion state of MVP 3. Then implement MVP 4 in the phases above, preserving working functionality and validating every significant change.

The definition of success is a reliable, controller-first, locally operated gaming OS with a clear architecture and a reproducible path toward future releases—not simply a visually impressive interface.
