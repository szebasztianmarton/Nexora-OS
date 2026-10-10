#pragma once

#include <QJsonObject>
#include <QString>

namespace nexora {

// Small helper for reading/writing a JSON object file atomically (QSaveFile).
// A missing file yields an empty object and no error; a corrupt file yields an
// empty object and an error message, and the corrupt file is preserved as
// <file>.corrupt so data is never silently destroyed.
class JsonStore
{
public:
    explicit JsonStore(QString filePath);

    QString filePath() const { return m_filePath; }

    QJsonObject load(QString *errorMessage = nullptr) const;
    bool save(const QJsonObject &object, QString *errorMessage = nullptr) const;

private:
    QString m_filePath;
};

} // namespace nexora
