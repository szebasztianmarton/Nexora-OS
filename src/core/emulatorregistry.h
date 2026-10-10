#pragma once

#include <QAbstractListModel>
#include <QList>

#include "emulatordefinition.h"

namespace nexora {

// Read-only model over <configDir>/emulators.json (format: {"version":1,"emulators":[...]}).
// Configuration editing from the UI is intentionally not supported in this
// release; users edit the JSON file and press "Reload".
class EmulatorRegistry : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int availableCount READ availableCount NOTIFY statusChanged)
    Q_PROPERTY(QString configPath READ configPath CONSTANT)
    Q_PROPERTY(bool configExists READ configExists NOTIFY reloaded)
    Q_PROPERTY(QString loadError READ loadError NOTIFY reloaded)        // whole-file problem (unreadable/corrupt JSON)
    Q_PROPERTY(QStringList skippedEntries READ skippedEntries NOTIFY reloaded) // "entry #3: missing id" etc.
    Q_PROPERTY(QStringList supportedExtensions READ supportedExtensions NOTIFY reloaded) // union over enabled emulators

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ExecutableRole,
        ArgumentsRole,          // QStringList
        WorkingDirectoryRole,
        EnabledRole,
        PlatformsRole,          // QStringList
        ExtensionsRole,         // QStringList
        ProfilesRole,           // QVariantList of {id, name, arguments}
        StatusRole,             // statusKey string
        StatusTextRole,         // human readable
        ValidationMessagesRole, // QStringList
        CanLaunchRole,          // bool
        GameCountRole           // int, supplied by GameLibrary via setGameCounts()
    };
    Q_ENUM(Roles)

    explicit EmulatorRegistry(const QString &configDirectory, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int availableCount() const;
    QString configPath() const;
    bool configExists() const;
    QString loadError() const;
    QStringList skippedEntries() const;
    QStringList supportedExtensions() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void validateAll();
    Q_INVOKABLE void validate(const QString &id);
    Q_INVOKABLE QVariantMap get(int row) const;        // same keys as role names; empty map if out of range
    Q_INVOKABLE QVariantMap byId(const QString &id) const;
    Q_INVOKABLE int indexOf(const QString &id) const;   // -1 if unknown
    // First enabled+available emulator handling the extension, else first enabled one, else "".
    Q_INVOKABLE QString emulatorForExtension(const QString &extension) const;

    const EmulatorDefinition *find(const QString &id) const;
    QList<EmulatorDefinition> definitions() const { return m_definitions; }
    void setGameCounts(const QHash<QString, int> &countsByEmulatorId);

signals:
    void countChanged();
    void statusChanged();
    void reloaded();

private:
    QString m_configPath;
    QString m_loadError;
    QStringList m_skipped;
    QList<EmulatorDefinition> m_definitions;
    QHash<QString, int> m_gameCounts;
};

} // namespace nexora
