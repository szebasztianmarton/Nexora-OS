#include "nexora/emulator/EmulatorProcessState.h"

namespace nexora::emulator {

bool isValidTransition(ProcessState from, ProcessState to)
{
    using S = ProcessState;
    switch (from) {
    case S::Idle:
    case S::Exited:
    case S::Failed: return to == S::Starting || to == S::Failed;
    case S::Starting: return to == S::Running || to == S::Stopping || to == S::Failed;
    case S::Running: return to == S::Stopping || to == S::Exited || to == S::Failed;
    case S::Stopping: return to == S::Exited;
    }
    return false;
}

bool isActive(ProcessState state)
{
    return state == ProcessState::Starting || state == ProcessState::Running || state == ProcessState::Stopping;
}

QString nameOf(ProcessState state)
{
    switch (state) {
    case ProcessState::Idle: return QStringLiteral("Idle");
    case ProcessState::Starting: return QStringLiteral("Starting");
    case ProcessState::Running: return QStringLiteral("Running");
    case ProcessState::Stopping: return QStringLiteral("Stopping");
    case ProcessState::Exited: return QStringLiteral("Exited");
    case ProcessState::Failed: return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

QString nameOf(EmulatorError error)
{
    switch (error) {
    case EmulatorError::None: return QStringLiteral("None");
    case EmulatorError::InvalidConfig: return QStringLiteral("InvalidConfig");
    case EmulatorError::Disabled: return QStringLiteral("Disabled");
    case EmulatorError::ExecutableMissing: return QStringLiteral("ExecutableMissing");
    case EmulatorError::NotAFile: return QStringLiteral("NotAFile");
    case EmulatorError::NotExecutable: return QStringLiteral("NotExecutable");
    case EmulatorError::AlreadyActive: return QStringLiteral("AlreadyActive");
    case EmulatorError::FailedToStart: return QStringLiteral("FailedToStart");
    case EmulatorError::NonZeroExit: return QStringLiteral("NonZeroExit");
    case EmulatorError::Crashed: return QStringLiteral("Crashed");
    case EmulatorError::ProfileNotFound: return QStringLiteral("ProfileNotFound");
    case EmulatorError::ProfileDisabled: return QStringLiteral("ProfileDisabled");
    case EmulatorError::InvalidWorkingDirectory: return QStringLiteral("InvalidWorkingDirectory");
    case EmulatorError::GameFileMissing: return QStringLiteral("GameFileMissing");
    }
    return QStringLiteral("Unknown");
}

QString describe(EmulatorError error, const QString &detail)
{
    auto withDetail
        = [&](const QString &text) { return detail.isEmpty() ? text : text + QStringLiteral(": ") + detail; };
    switch (error) {
    case EmulatorError::None: return QStringLiteral("No error");
    case EmulatorError::InvalidConfig: return withDetail(QStringLiteral("Invalid emulator configuration"));
    case EmulatorError::Disabled: return withDetail(QStringLiteral("Emulator is disabled in its configuration"));
    case EmulatorError::ExecutableMissing: return withDetail(QStringLiteral("Emulator executable not found"));
    case EmulatorError::NotAFile: return withDetail(QStringLiteral("Emulator executable is not a regular file"));
    case EmulatorError::NotExecutable: return withDetail(QStringLiteral("Emulator file is not executable"));
    case EmulatorError::AlreadyActive: return withDetail(QStringLiteral("Another emulator is already running"));
    case EmulatorError::FailedToStart: return withDetail(QStringLiteral("Emulator failed to start"));
    case EmulatorError::NonZeroExit: return withDetail(QStringLiteral("Emulator exited with an error code"));
    case EmulatorError::Crashed: return withDetail(QStringLiteral("Emulator crashed"));
    case EmulatorError::ProfileNotFound: return withDetail(QStringLiteral("Launch profile not found"));
    case EmulatorError::ProfileDisabled: return withDetail(QStringLiteral("Launch profile is disabled"));
    case EmulatorError::InvalidWorkingDirectory: return withDetail(QStringLiteral("Working directory does not exist"));
    case EmulatorError::GameFileMissing: return withDetail(QStringLiteral("Game file is missing or unreadable"));
    }
    return withDetail(QStringLiteral("Unknown error"));
}

} // namespace nexora::emulator
