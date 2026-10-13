#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace nexora::shared {

// Writes via QSaveFile (temporary file + rename), so readers never see a partial file.
// The result is readable and writable by the owner only (0600). Does not create parent
// directories. Returns false with a message in |error| on failure.
bool writeFileAtomically(const QString &path, const QByteArray &data, QString *error);

// Reads at most |maxBytes|; fails (with |error| set) if the file is larger, unreadable or
// missing.
std::optional<QByteArray> readFileLimited(const QString &path, qint64 maxBytes, QString *error);

// Renames |path| to "<path>.<tag>-<yyyyMMdd-hhmmss>" so a broken file can be inspected
// while a fresh one is started. Returns the new name, or an empty string if there was
// nothing to move or the rename failed.
QString moveAside(const QString &path, const QString &tag = QStringLiteral("corrupt"));

} // namespace nexora::shared
