#include "nexora/system/BackupService.h"

#include "nexora/shared/AtomicFile.h"
#include "nexora/shared/BuildInfo.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace nexora::system {

namespace {

constexpr qint64 MaxSavesBytes = qint64(4) * 1024 * 1024 * 1024;
constexpr qint64 MaxManifestBytes = 8 * 1024 * 1024;
constexpr QFileDevice::Permissions OwnerReadWrite = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

const QString ManifestName = QStringLiteral("manifest.json");
const QString SettingsRel = QStringLiteral("config/settings.json");
const QString LibraryRel = QStringLiteral("data/library.sqlite");
const QString HistoryRel = QStringLiteral("state/launch-history.json");
const QString SavesPrefix = QStringLiteral("saves/");

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

std::optional<QString> sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return std::nullopt;
    return QString::fromLatin1(hash.result().toHex());
}

bool copyOwnerOnly(const QString &from, const QString &to, QString *error)
{
    if (!QDir().mkpath(QFileInfo(to).absolutePath()))
        return fail(error, QStringLiteral("cannot create directory for %1").arg(to));
    QFile::remove(to);
    if (!QFile::copy(from, to))
        return fail(error, QStringLiteral("cannot copy %1 to %2").arg(from, to));
    QFile::setPermissions(to, OwnerReadWrite);
    return true;
}

// Maps a relative backup path to the live file it restores to; empty if it is not a known
// location. The caller has already validated |rel| as a clean relative path.
QString destinationFor(const shared::DataLayout &layout, const QString &rel)
{
    if (rel == SettingsRel)
        return layout.configDir + QStringLiteral("/settings.json");
    if (rel == LibraryRel)
        return layout.dataDir + QStringLiteral("/library.sqlite");
    if (rel == HistoryRel)
        return layout.stateDir + QStringLiteral("/launch-history.json");
    if (rel.startsWith(SavesPrefix) && rel.size() > SavesPrefix.size())
        return layout.savesDir + u'/' + rel.mid(SavesPrefix.size());
    return {};
}

bool isCleanRelativePath(const QString &rel)
{
    if (rel.isEmpty() || rel.startsWith(u'/') || rel.contains(u'\\') || rel.contains(QChar(0)))
        return false;
    const QStringList parts = rel.split(u'/');
    return std::none_of(parts.cbegin(), parts.cend(), [](const QString &p) {
        return p.isEmpty() || p == QLatin1String(".") || p == QLatin1String("..");
    });
}

QByteArray manifestToJson(const BackupManifest &m)
{
    QJsonArray files;
    for (const auto &entry : m.files)
        files.append(QJsonObject{{QStringLiteral("path"), entry.first}, {QStringLiteral("sha256"), entry.second}});
    const QJsonObject root{{QStringLiteral("version"), m.version},
                           {QStringLiteral("createdAt"), m.createdAt},
                           {QStringLiteral("appVersion"), m.appVersion},
                           {QStringLiteral("buildId"), m.buildId},
                           {QStringLiteral("files"), files}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<BackupManifest> parseManifest(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        fail(error, QStringLiteral("manifest is not valid JSON"));
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    BackupManifest m;
    m.version = root.value(QStringLiteral("version")).toInt(-1);
    if (m.version != 1) {
        fail(error, QStringLiteral("unsupported backup version %1").arg(m.version));
        return std::nullopt;
    }
    m.createdAt = root.value(QStringLiteral("createdAt")).toString();
    m.appVersion = root.value(QStringLiteral("appVersion")).toString();
    m.buildId = root.value(QStringLiteral("buildId")).toString();
    const QJsonValue files = root.value(QStringLiteral("files"));
    if (!files.isArray()) {
        fail(error, QStringLiteral("manifest has no file list"));
        return std::nullopt;
    }
    static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]{64}$"));
    for (const QJsonValue &v : files.toArray()) {
        const QJsonObject e = v.toObject();
        const QString path = e.value(QStringLiteral("path")).toString();
        const QString sha = e.value(QStringLiteral("sha256")).toString();
        if (path.isEmpty() || !hex.match(sha).hasMatch()) {
            fail(error, QStringLiteral("manifest has a malformed file entry"));
            return std::nullopt;
        }
        m.files.append({path, sha.toLower()});
    }
    return m;
}

} // namespace

BackupService::BackupService(shared::DataLayout layout, DbSnapshot snapshot)
    : m_layout(std::move(layout))
    , m_snapshot(std::move(snapshot))
{ }

std::optional<QString> BackupService::createBackup(bool includeSaves, QString *error)
{
    if (!QDir().mkpath(m_layout.backupDir)) {
        fail(error, QStringLiteral("cannot create %1").arg(m_layout.backupDir));
        return std::nullopt;
    }
    const QString stem = m_layout.backupDir + QStringLiteral("/nexora-backup-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString root = stem;
    for (int n = 1; QFileInfo::exists(root); ++n)
        root = stem + u'-' + QString::number(n);
    if (!QDir().mkpath(root)) {
        fail(error, QStringLiteral("cannot create %1").arg(root));
        return std::nullopt;
    }
    QFile::setPermissions(root, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    BackupManifest manifest;
    manifest.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    manifest.appVersion = QString::fromLatin1(shared::build::Version);
    manifest.buildId = QString::fromLatin1(shared::build::BuildId);

    // Runs |step| and removes the partial backup (our own, just-created directory) on failure.
    auto run = [&](bool ok) {
        if (!ok)
            QDir(root).removeRecursively();
        return ok;
    };
    auto record = [&](const QString &rel) {
        const auto sha = sha256File(root + u'/' + rel);
        if (!sha)
            return fail(error, QStringLiteral("cannot hash %1").arg(rel));
        manifest.files.append({rel, *sha});
        return true;
    };
    auto addCopy = [&](const QString &source, const QString &rel) {
        return copyOwnerOnly(source, root + u'/' + rel, error) && record(rel);
    };

    const QString settings = m_layout.configDir + QStringLiteral("/settings.json");
    if (QFileInfo::exists(settings) && !run(addCopy(settings, SettingsRel)))
        return std::nullopt;

    const QString library = m_layout.dataDir + QStringLiteral("/library.sqlite");
    if (QFileInfo::exists(library)) {
        const QString dest = root + u'/' + LibraryRel;
        bool ok = QDir().mkpath(QFileInfo(dest).absolutePath());
        if (!ok)
            fail(error, QStringLiteral("cannot create directory for %1").arg(dest));
        if (ok && m_snapshot) {
            QString snapshotError;
            ok = m_snapshot(dest, &snapshotError);
            if (!ok)
                fail(error, QStringLiteral("library snapshot failed: %1").arg(snapshotError));
        } else if (ok) {
            ok = copyOwnerOnly(library, dest, error);
        }
        if (ok) {
            QFile::setPermissions(dest, OwnerReadWrite);
            ok = record(LibraryRel);
        }
        if (!run(ok))
            return std::nullopt;
    }

    const QString history = m_layout.stateDir + QStringLiteral("/launch-history.json");
    if (QFileInfo::exists(history) && !run(addCopy(history, HistoryRel)))
        return std::nullopt;

    if (includeSaves && QDir(m_layout.savesDir).exists()) {
        QStringList files;
        qint64 total = 0;
        QDirIterator it(m_layout.savesDir, QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            total += it.fileInfo().size();
            if (total > MaxSavesBytes) {
                fail(error, QStringLiteral("saves are larger than 4 GiB; back them up separately"));
                run(false);
                return std::nullopt;
            }
            files.append(it.filePath());
        }
        const QDir savesDir(m_layout.savesDir);
        for (const QString &file : files) {
            if (!run(addCopy(file, SavesPrefix + savesDir.relativeFilePath(file))))
                return std::nullopt;
        }
    }

    QString writeError;
    if (!run(shared::writeFileAtomically(root + u'/' + ManifestName, manifestToJson(manifest), &writeError))) {
        fail(error, writeError);
        return std::nullopt;
    }
    return root;
}

bool BackupService::restoreBackup(const QString &backupDir, QString *error)
{
    const QFileInfo dirInfo(backupDir);
    if (!dirInfo.isDir())
        return fail(error, QStringLiteral("%1 is not a backup directory").arg(backupDir));
    const QString root = dirInfo.canonicalFilePath();

    // ---- Verify everything before touching anything. ----
    const QString manifestPath = root + u'/' + ManifestName;
    if (QFileInfo(manifestPath).isSymLink())
        return fail(error, QStringLiteral("manifest is a symlink"));
    QString readError;
    const auto manifestData = shared::readFileLimited(manifestPath, MaxManifestBytes, &readError);
    if (!manifestData)
        return fail(error, QStringLiteral("cannot read the backup manifest: %1").arg(readError));
    QString parseError;
    const auto manifest = parseManifest(*manifestData, &parseError);
    if (!manifest)
        return fail(error, parseError);

    struct Item
    {
        QString source;
        QString destination;
    };
    QList<Item> items;
    QSet<QString> listed;
    for (const auto &[rel, sha] : manifest->files) {
        if (!isCleanRelativePath(rel))
            return fail(error, QStringLiteral("refusing unsafe path in manifest: %1").arg(rel));
        const QString destination = destinationFor(m_layout, rel);
        if (destination.isEmpty() || rel == ManifestName)
            return fail(error, QStringLiteral("refusing unexpected path in manifest: %1").arg(rel));
        if (listed.contains(rel))
            return fail(error, QStringLiteral("duplicate path in manifest: %1").arg(rel));
        listed.insert(rel);

        const QString source = root + u'/' + rel;
        const QFileInfo info(source);
        if (info.isSymLink())
            return fail(error, QStringLiteral("refusing symlink in backup: %1").arg(rel));
        if (!info.isFile() || info.canonicalFilePath() != QDir::cleanPath(source))
            return fail(error, QStringLiteral("backup file is missing or misplaced: %1").arg(rel));
        const auto actual = sha256File(source);
        if (!actual || *actual != sha)
            return fail(error, QStringLiteral("checksum mismatch for %1; the backup is damaged").arg(rel));
        items.append({source, destination});
    }
    if (items.isEmpty())
        return fail(error, QStringLiteral("the backup contains no files"));

    QDirIterator scan(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                      QDirIterator::Subdirectories);
    while (scan.hasNext()) {
        scan.next();
        const QFileInfo info = scan.fileInfo();
        if (info.isSymLink())
            return fail(error, QStringLiteral("refusing symlink in backup: %1").arg(scan.filePath()));
        if (info.isDir())
            continue;
        const QString rel = QDir(root).relativeFilePath(scan.filePath());
        if (rel != ManifestName && !listed.contains(rel))
            return fail(error, QStringLiteral("backup contains an unlisted file: %1").arg(rel));
    }

    // ---- Apply: move the current files aside, then copy the backup in. ----
    for (const Item &item : items) {
        if (!QDir().mkpath(QFileInfo(item.destination).absolutePath()))
            return fail(error, QStringLiteral("cannot create directory for %1").arg(item.destination));
        QString aside;
        if (QFileInfo::exists(item.destination)) {
            aside = shared::moveAside(item.destination, QStringLiteral("pre-restore"));
            if (aside.isEmpty())
                return fail(error, QStringLiteral("cannot move %1 aside").arg(item.destination));
        }
        // A stale WAL/SHM next to a replaced database would be replayed into the restored one.
        if (item.destination.endsWith(QLatin1String(".sqlite"))) {
            for (const char *suffix : {"-wal", "-shm"})
                shared::moveAside(item.destination + QLatin1String(suffix), QStringLiteral("pre-restore"));
        }
        const QString temp = item.destination + QStringLiteral(".restore-tmp");
        QString copyError;
        if (!copyOwnerOnly(item.source, temp, &copyError) || !QFile::rename(temp, item.destination)) {
            QFile::remove(temp);
            if (!aside.isEmpty() && !QFileInfo::exists(item.destination))
                QFile::rename(aside, item.destination);
            return fail(error, QStringLiteral("restore of %1 failed: %2").arg(item.destination, copyError));
        }
    }
    return true;
}

QStringList BackupService::listBackups() const
{
    const QDir dir(m_layout.backupDir);
    QStringList names = dir.entryList({QStringLiteral("nexora-backup-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(names.begin(), names.end(), std::greater<QString>());
    QStringList paths;
    for (const QString &name : names)
        paths.append(dir.absoluteFilePath(name));
    return paths;
}

} // namespace nexora::system
