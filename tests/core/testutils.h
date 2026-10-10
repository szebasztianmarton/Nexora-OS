#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace testutils {

inline QString mockEmulator()
{
    return QStringLiteral(NEXORA_MOCK_EMULATOR);
}

inline bool writeFile(const QString &path, const QByteArray &content = QByteArray("x"))
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(content) == content.size();
}

inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

inline bool writeEmulators(const QString &configDir, const QJsonArray &emulators)
{
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("emulators"), emulators}};
    return writeFile(QDir(configDir).filePath(QStringLiteral("emulators.json")), QJsonDocument(root).toJson());
}

// A valid entry pointing at the mock emulator.
inline QJsonObject mockEntry(const QString &id, const QStringList &extensions = {QStringLiteral("mock")},
                             const QStringList &arguments = {QStringLiteral("{game}")})
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("name"), QStringLiteral("Emulator %1").arg(id)},
                       {QStringLiteral("executable"), mockEmulator()},
                       {QStringLiteral("arguments"), QJsonArray::fromStringList(arguments)},
                       {QStringLiteral("platforms"), QJsonArray{QStringLiteral("Test Platform")}},
                       {QStringLiteral("extensions"), QJsonArray::fromStringList(extensions)}};
}

} // namespace testutils
