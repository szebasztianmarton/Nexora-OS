#include "jsonstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace nexora {

JsonStore::JsonStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

QJsonObject JsonStore::load(QString *errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();
    QFile file(m_filePath);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot read %1: %2").arg(m_filePath, file.errorString());
        return {};
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject())
        return doc.object();

    const QString reason = parseError.error != QJsonParseError::NoError
                               ? parseError.errorString()
                               : QStringLiteral("top-level value is not an object");
    const QString backup = m_filePath + QStringLiteral(".corrupt");
    QFile::remove(backup);
    const bool kept = QFile::rename(m_filePath, backup);
    if (errorMessage) {
        *errorMessage = kept ? QStringLiteral("%1 is corrupt (%2); it was kept as %3")
                                   .arg(QFileInfo(m_filePath).fileName(), reason, backup)
                             : QStringLiteral("%1 is corrupt (%2)").arg(QFileInfo(m_filePath).fileName(), reason);
    }
    qWarning("JsonStore: %s is corrupt: %s", qPrintable(m_filePath), qPrintable(reason));
    return {};
}

bool JsonStore::save(const QJsonObject &object, QString *errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(m_filePath, file.errorString());
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(m_filePath, file.errorString());
        return false;
    }
    return true;
}

} // namespace nexora
