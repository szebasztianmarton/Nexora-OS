#include "nexora/library/LibraryScanner.h"

#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <sys/stat.h>

namespace nexora::library {

namespace {

struct FileKey
{
    quint64 device;
    quint64 inode;
    bool operator==(const FileKey &) const = default;
};

size_t qHash(const FileKey &key, size_t seed = 0)
{
    return ::qHash(key.device, seed) ^ ::qHash(key.inode, seed);
}

bool isInside(const QString &canonicalPath, const QString &canonicalRoot)
{
    return canonicalPath == canonicalRoot || canonicalPath.startsWith(canonicalRoot + u'/');
}

class Walker
{
public:
    Walker(const ScanOptions &options, const std::atomic_bool *cancel, ScanResult &result)
        : m_options(options)
        , m_cancel(cancel)
        , m_result(result)
    { }

    void scanRoot(const QString &configuredRoot)
    {
        const QFileInfo rootInfo(configuredRoot);
        if (configuredRoot.isEmpty() || !rootInfo.isAbsolute()) {
            issue(configuredRoot, QStringLiteral("library directory must be an absolute path"));
            return;
        }
        if (!rootInfo.exists()) {
            issue(configuredRoot, QStringLiteral("library directory does not exist"));
            return;
        }
        if (!rootInfo.isDir() || !rootInfo.isReadable() || !rootInfo.isExecutable()) {
            issue(configuredRoot, QStringLiteral("library directory is not a readable directory"));
            return;
        }
        const QString canonical = rootInfo.canonicalFilePath();
        if (canonical == QStringLiteral("/")) {
            issue(configuredRoot, QStringLiteral("scanning the whole filesystem is not allowed"));
            return;
        }
        if (!m_result.scannedRoots.contains(canonical))
            m_result.scannedRoots.append(canonical);
        m_visitedDirs.clear();
        walk(canonical, canonical, 0);
    }

private:
    bool stopped() const { return m_result.truncated || (m_cancel && m_cancel->load(std::memory_order_relaxed)); }

    void issue(const QString &path, const QString &message)
    {
        // Bounded: a broken disk must not produce an unbounded diagnostics list.
        if (m_result.issues.size() < 200)
            m_result.issues.append({path, message});
    }

    void walk(const QString &dirPath, const QString &root, int depth)
    {
        if (stopped())
            return;
        if (depth > m_options.maxDepth) {
            issue(dirPath,
                  QStringLiteral("maximum directory depth (%1) reached; not descending").arg(m_options.maxDepth));
            return;
        }
        if (m_visitedDirs.contains(dirPath))
            return; // symlink loop
        m_visitedDirs.insert(dirPath);

        QDir dir(dirPath);
        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System;
        const QFileInfoList entries = dir.entryInfoList(filters, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (stopped())
                return;
            if (entry.fileName().startsWith(u'.'))
                continue; // hidden files and directories are not part of the library

            QString path = entry.absoluteFilePath();
            if (entry.isSymLink()) {
                if (!m_options.followSymlinks) {
                    ++m_result.skippedSymlinks;
                    continue;
                }
                const QString target = entry.canonicalFilePath();
                if (target.isEmpty()) {
                    ++m_result.skippedSymlinks;
                    issue(path, QStringLiteral("broken symbolic link; skipped"));
                    continue;
                }
                if (!isInside(target, root)) {
                    ++m_result.skippedSymlinks;
                    issue(path, QStringLiteral("symbolic link points outside the library directory; skipped"));
                    continue;
                }
                path = target;
            }

            const QFileInfo info(path);
            if (info.isDir()) {
                if (!info.isReadable() || !info.isExecutable()) {
                    issue(path, QStringLiteral("directory is not accessible; skipped"));
                    continue;
                }
                walk(info.canonicalFilePath(), root, depth + 1);
                continue;
            }
            if (!info.isFile())
                continue; // sockets, devices, fifos

            const QString suffix = info.suffix().toLower();
            if (suffix.isEmpty())
                continue;
            const QString extension = u'.' + suffix;
            const auto handlers = m_options.extensionMap.constFind(extension);
            if (handlers == m_options.extensionMap.cend())
                continue;
            if (!info.isReadable()) {
                issue(path, QStringLiteral("file is not readable; skipped"));
                continue;
            }

            struct ::stat st
            { };
            const QString canonical = info.canonicalFilePath();
            if (::stat(QFile::encodeName(canonical).constData(), &st) != 0) {
                issue(path, QStringLiteral("cannot stat file; skipped"));
                continue;
            }
            const FileKey key{static_cast<quint64>(st.st_dev), static_cast<quint64>(st.st_ino)};
            if (m_seenFiles.contains(key)) {
                ++m_result.duplicates;
                continue;
            }
            m_seenFiles.insert(key);

            if (m_result.files.size() >= m_options.maxFiles) {
                m_result.truncated = true;
                issue(root, QStringLiteral("more than %1 game files; scan stopped").arg(m_options.maxFiles));
                return;
            }
            ScannedFile file;
            file.path = canonical;
            file.title = LibraryScanner::titleFromFileName(info.fileName());
            file.extension = extension;
            file.root = root;
            file.size = info.size();
            file.modified = info.lastModified();
            file.emulatorIds = *handlers;
            m_result.files.append(file);
        }
    }

    const ScanOptions &m_options;
    const std::atomic_bool *m_cancel;
    ScanResult &m_result;
    QSet<QString> m_visitedDirs;
    QSet<FileKey> m_seenFiles; // across roots, so overlapping roots do not duplicate
};

} // namespace

QString LibraryScanner::titleFromFileName(const QString &fileName)
{
    QString base = fileName;
    const qsizetype dot = base.lastIndexOf(u'.');
    if (dot > 0)
        base.truncate(dot);
    base.replace(u'_', u' ');
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    base = base.replace(spaces, QStringLiteral(" ")).trimmed();
    return base.isEmpty() ? fileName : base;
}

ScanResult LibraryScanner::scan(const ScanOptions &options, const std::atomic_bool *cancel)
{
    ScanResult result;
    Walker walker(options, cancel, result);
    for (const QString &root : options.roots) {
        walker.scanRoot(QDir::cleanPath(root));
        if (cancel && cancel->load())
            break;
    }
    result.cancelled = cancel && cancel->load();
    qCInfo(lcLibrary) << "Library scan:" << result.files.size() << "file(s) in" << result.scannedRoots.size()
                      << "root(s)," << result.duplicates << "duplicate(s)," << result.skippedSymlinks
                      << "skipped link(s)," << result.issues.size() << "issue(s)"
                      << (result.truncated ? "(truncated)" : "") << (result.cancelled ? "(cancelled)" : "");
    return result;
}

} // namespace nexora::library
