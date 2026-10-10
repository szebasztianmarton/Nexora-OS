#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>

namespace nexora {

struct GameEntry
{
    QString path;             // absolute, unique key
    QString title;            // derived from the file name (extension stripped, '_' -> ' ')
    QString fileName;
    QString extension;        // lower-case, no dot
    QString directory;        // the configured library directory it was found under
    qint64 fileSize = 0;
    QString artworkPath;      // "" if no sidecar artwork was found
    QString emulatorId;       // effective: user override, else auto-resolved by extension, else ""
    bool emulatorOverridden = false;
    bool favorite = false;
    QDateTime lastPlayed;     // invalid if never launched from Nexora
    QDateTime firstSeen;      // when Nexora first indexed it
};

// Flat list of indexed games. Mutated only by GameLibrary.
class GameModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        FileNameRole,
        ExtensionRole,
        DirectoryRole,
        FileSizeRole,         // qint64 bytes
        ArtworkUrlRole,       // "file:///..." or ""
        HasArtworkRole,
        EmulatorIdRole,
        EmulatorNameRole,     // resolved via a name lookup set by GameLibrary, "" if none
        EmulatorOverriddenRole,
        PlatformRole,         // first configured platform of the emulator, or ""
        FavoriteRole,
        LastPlayedRole,       // QDateTime (invalid if never)
        FirstSeenRole         // QDateTime
    };
    Q_ENUM(Roles)

    explicit GameModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_entries.size(); }
    int indexOfPath(const QString &path) const;
    const GameEntry *entry(const QString &path) const;
    const QList<GameEntry> &entries() const { return m_entries; }

    void resetEntries(const QList<GameEntry> &entries);
    void updateEntry(const GameEntry &entry);        // matched by path, emits dataChanged
    void removeEntry(const QString &path);
    // id -> {name, platform}; refreshes the emulator name/platform roles.
    void setEmulatorInfo(const QHash<QString, QPair<QString, QString>> &info);

    QVariantMap toVariantMap(const GameEntry &entry) const; // keys = role names

signals:
    void countChanged();

private:
    void rebuildIndex();

    QList<GameEntry> m_entries;
    QHash<QString, int> m_rowByPath; // path -> row, kept in sync with m_entries
    QHash<QString, QPair<QString, QString>> m_emulatorInfo;
};

} // namespace nexora
