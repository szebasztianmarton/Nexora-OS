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
    QString systemId; // user override if set, else the unique detected system, else empty
    QStringList systemCandidates; // all systems the extension maps to
    QString titleOverride; // user-edited title; empty = derived title
    QString notes; // user text, at most 2000 characters
    bool hidden = false; // removed from the library by the user; rescans never unhide
    bool present = true; // false: file vanished or its directory was removed
    bool favorite = false;
    int playCount = 0;
    QDateTime lastPlayed; // invalid = never launched
    QDateTime addedAt;

    QString displayTitle() const { return titleOverride.isEmpty() ? title : titleOverride; }

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
