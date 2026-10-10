#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>

namespace nexora::library {

struct ScanOptions
{
    QStringList roots;                        // absolute directories; never "/" by default
    QHash<QString, QStringList> extensionMap; // ".ext" -> emulator ids handling it
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
    int duplicates = 0;       // same file reached twice (overlapping roots, hard/sym links)
    int skippedSymlinks = 0;
    bool truncated = false;   // maxFiles reached
    bool cancelled = false;
};

// Walks the configured library directories. Pure function of the filesystem and options:
// safe to run on a worker thread. Never executes or opens game files (only stat()).
class LibraryScanner
{
public:
    static ScanResult scan(const ScanOptions &options, const std::atomic_bool *cancel = nullptr);

    // "Super_Mario  World (USA).sfc" -> "Super Mario World (USA)"
    static QString titleFromFileName(const QString &fileName);
};

} // namespace nexora::library
