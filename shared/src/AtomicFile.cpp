#include "nexora/shared/AtomicFile.h"

#include <QDateTime>
#include <QFile>
#include <QSaveFile>

namespace nexora::shared {

namespace {

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

constexpr QFileDevice::Permissions OwnerReadWrite = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

} // namespace

bool writeFileAtomically(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));
        return false;
    }
    file.setPermissions(OwnerReadWrite);
    if (file.write(data) != data.size()) {
        setError(error, QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));
        return false;
    }
    // QSaveFile may carry over the permissions of a file it replaced; enforce 0600 regardless.
    if (!QFile::setPermissions(path, OwnerReadWrite)) {
        setError(error, QStringLiteral("cannot restrict permissions of %1").arg(path));
        return false;
    }
    return true;
}

std::optional<QByteArray> readFileLimited(const QString &path, qint64 maxBytes, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot read %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }
    if (file.size() > maxBytes) {
        setError(error, QStringLiteral("%1 is larger than %2 bytes").arg(path).arg(maxBytes));
        return std::nullopt;
    }
    // Read one byte more than allowed so a file that grew since size() is still caught.
    QByteArray data = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        setError(error, QStringLiteral("cannot read %1: %2").arg(path, file.errorString()));
        return std::nullopt;
    }
    if (data.size() > maxBytes) {
        setError(error, QStringLiteral("%1 is larger than %2 bytes").arg(path).arg(maxBytes));
        return std::nullopt;
    }
    return data;
}

QString moveAside(const QString &path, const QString &tag)
{
    if (!QFile::exists(path))
        return {};
    const QString base
        = path + u'.' + tag + u'-' + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString target = base;
    for (int n = 1; QFile::exists(target); ++n)
        target = base + u'-' + QString::number(n);
    return QFile::rename(path, target) ? target : QString();
}

} // namespace nexora::shared
