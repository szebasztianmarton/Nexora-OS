#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace nexora::library {

// One game file known to the library database.
struct GameEntry
{
    qint64 id = 0;
    QString path; // canonical absolute path
    QString title; // derived from the file name
    QString extension;
    QString root; // library directory it was found under
    qint64 size = 0;
    QStringList emulatorCandidates; // emulators that handle the extension
    QString emulatorOverride; // user choice, may be empty
    bool present = true; // false: file vanished or its directory was removed
    bool favorite = false;
    int playCount = 0;
    QDateTime lastPlayed; // invalid = never launched
    QDateTime addedAt;

    // The emulator to launch with: the override if it is still a candidate, otherwise the
    // first candidate.
    QString emulatorId() const
    {
        if (!emulatorOverride.isEmpty() && emulatorCandidates.contains(emulatorOverride))
            return emulatorOverride;
        return emulatorCandidates.value(0);
    }
};

} // namespace nexora::library
