#pragma once

#include <QObject>
#include <QString>

namespace nexora::emulator {
Q_NAMESPACE

// Lifecycle of the (single) managed emulator process.
//
//   Idle ──launch──► Starting ──started──► Running ──exit 0──► Exited
//     │                 │  │                 │  │
//     │                 │  └─stop─► Stopping ◄┘  └─exit≠0 / crash─► Failed
//     │                 └─failed to start──────────────────────────► Failed
//     └─validation error──────────────────────────────────────────► Failed
//   Stopping ──any finish──► Exited
//   Exited / Failed ──launch──► Starting   (or ──validation error──► Failed)
enum class ProcessState { Idle, Starting, Running, Stopping, Exited, Failed };
Q_ENUM_NS(ProcessState)

enum class EmulatorError {
    None,
    InvalidConfig,
    Disabled,
    ExecutableMissing,
    NotAFile,
    NotExecutable,
    AlreadyActive,
    FailedToStart,
    NonZeroExit,
    Crashed,
    ProfileNotFound,
    ProfileDisabled,
    InvalidWorkingDirectory,
    GameFileMissing,
};
Q_ENUM_NS(EmulatorError)

bool isValidTransition(ProcessState from, ProcessState to);

// Starting, Running and Stopping own a live process; no new launch is allowed.
bool isActive(ProcessState state);

QString nameOf(ProcessState state);
QString nameOf(EmulatorError error);

// User-facing message for |error|; |detail| (a path, exit code, …) is appended when relevant.
QString describe(EmulatorError error, const QString &detail = {});

} // namespace nexora::emulator
