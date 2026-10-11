#pragma once

#include "nexora/shared/DataLayout.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace nexora::system {

struct BackupManifest
{
    int version = 1;
    QString createdAt;
    QString appVersion;
    QString buildId;
    QList<QPair<QString, QString>> files; // relative path, sha256 hex
};

// Backups of the user's own data: settings, the library database, launch history and (on
// request) emulator saves. Layout of a backup directory:
//
//   manifest.json                     {"version":1,"createdAt","appVersion","buildId","files":[{"path","sha256"}]}
//   config/settings.json              <- layout.configDir
//   data/library.sqlite               <- layout.dataDir (written through the DbSnapshot callback)
//   state/launch-history.json         <- layout.stateDir
//   saves/...                         <- layout.savesDir (only with includeSaves)
//
// Nothing is ever deleted: restoring moves the current files aside first.
class BackupService
{
public:
    // Writes a consistent copy of the library database to |destFile| (e.g. SQLite
    // "VACUUM INTO"). Keeps QtSql out of nexora_system.
    using DbSnapshot = std::function<bool(const QString &destFile, QString *error)>;

    BackupService(shared::DataLayout layout, DbSnapshot snapshot);

    // Creates <backupDir>/nexora-backup-<yyyyMMdd-hhmmss>/. Saves are bounded to 4 GiB; a
    // larger tree aborts with an error. On failure the partial backup directory is removed.
    std::optional<QString> createBackup(bool includeSaves, QString *error);

    // Verifies the manifest and every hash first (all or nothing): refuses a path that is
    // absolute, contains "..", is a symlink, is outside the known locations, or a file in
    // the backup that the manifest does not list. Then moves each current file aside to
    // "<file>.pre-restore-<ts>" and copies the backup in, one file at a time. The whole
    // file set is atomic: if any file fails to apply, every file already swapped earlier
    // in this call is rolled back to its pre-restore content before returning false, so
    // the live data never ends up a mix of old and new files. The caller must have closed
    // the library database.
    bool restoreBackup(const QString &backupDir, QString *error);

    QStringList listBackups() const; // absolute paths, newest first

private:
    shared::DataLayout m_layout;
    DbSnapshot m_snapshot;
};

} // namespace nexora::system
