#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

namespace nexora::library {

struct ScanOptions
{
    QStringList roots; // absolute directories; never "/" by default
    QHash<QString, QStringList> extensionMap; // ".ext" -> emulator ids handling it
    QHash<QString, QStringList> extensionSystems; // ".ext" -> system ids (filled by GameLibrary)
    QHash<QString, QStringList> emulatorSystems; // emulator id -> system ids it declares (empty = any)
    // Symbolic links are skipped unless this is set; even then a link is only followed when
    // its target stays inside the same library root (checked on canonical paths).
    bool followSymlinks = false;
    int maxDepth = 16;
    int maxFiles = 100000;
};

struct ScannedFile
{
    QString path; // canonical
    QString title;
    QString extension;
    QString root;
    qint64 size = 0;
    QDateTime modified;
    QStringList emulatorIds;
    QStringList systemIds; // systems the extension maps to; empty if unknown
};

struct ScanIssue
{
    QString path;
    QString message;
};

struct ScanResult
{
    QList<ScannedFile> files;
    QList<ScanIssue> issues;
    QStringList scannedRoots; // canonical roots that existed and were readable
    int duplicates = 0; // same file reached twice (overlapping roots, hard/sym links)
    int skippedSymlinks = 0;
    bool truncated = false; // maxFiles reached
    bool cancelled = false;
};

// Walks the configured library directories. Pure function of the filesystem and options:
// safe to run on a worker thread. Never executes or opens game files (only stat()).
// Called at the start of each root and then at most every 100 game files found, from the
// scanning thread.
using ScanProgress = std::function<void(int filesSeen, const QString &currentRoot)>;

class LibraryScanner
{
public:
    // Directories named proc, sys, dev, run, lost+found, .Trash-* or .git, and directories
    // that resolve to a system location (/proc, /sys, /dev, /run, /boot, /etc, /usr, /bin,
    // /sbin, /lib, /lib64, /var), are never entered.
    static ScanResult scan(const ScanOptions &options, const std::atomic_bool *cancel = nullptr,
                           const ScanProgress &progress = {});

    // "Super_Mario  World (USA).sfc" -> "Super Mario World (USA)"
    static QString titleFromFileName(const QString &fileName);

    // Narrows an .iso that could be an Xbox 360 disc: the disc header decides whether the
    // system is xbox360 alone or not xbox360 at all, and emulator candidates are reduced to
    // those that declare one of the remaining systems (or no systems). Other files are untouched.
    static void classifyXboxDisc(ScannedFile &file, const ScanOptions &options);
};

} // namespace nexora::library
