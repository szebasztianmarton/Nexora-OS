#include "gamemodel.h"

#include <QUrl>

namespace nexora {

GameModel::GameModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GameModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant GameModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const GameEntry &e = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole: return e.title;
    case PathRole: return e.path;
    case FileNameRole: return e.fileName;
    case ExtensionRole: return e.extension;
    case DirectoryRole: return e.directory;
    case FileSizeRole: return e.fileSize;
    case ArtworkUrlRole: return e.artworkPath.isEmpty() ? QString() : QUrl::fromLocalFile(e.artworkPath).toString();
    case HasArtworkRole: return !e.artworkPath.isEmpty();
    case EmulatorIdRole: return e.emulatorId;
    case EmulatorNameRole: return m_emulatorInfo.value(e.emulatorId).first;
    case EmulatorOverriddenRole: return e.emulatorOverridden;
    case PlatformRole: return m_emulatorInfo.value(e.emulatorId).second;
    case FavoriteRole: return e.favorite;
    case LastPlayedRole: return e.lastPlayed;
    case FirstSeenRole: return e.firstSeen;
    default: return {};
    }
}

QHash<int, QByteArray> GameModel::roleNames() const
{
    return {{PathRole, "path"},
            {TitleRole, "title"},
            {FileNameRole, "fileName"},
            {ExtensionRole, "extension"},
            {DirectoryRole, "directory"},
            {FileSizeRole, "fileSize"},
            {ArtworkUrlRole, "artworkUrl"},
            {HasArtworkRole, "hasArtwork"},
            {EmulatorIdRole, "emulatorId"},
            {EmulatorNameRole, "emulatorName"},
            {EmulatorOverriddenRole, "emulatorOverridden"},
            {PlatformRole, "platform"},
            {FavoriteRole, "favorite"},
            {LastPlayedRole, "lastPlayed"},
            {FirstSeenRole, "firstSeen"}};
}

int GameModel::indexOfPath(const QString &path) const
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).path == path)
            return row;
    }
    return -1;
}

const GameEntry *GameModel::entry(const QString &path) const
{
    const int row = indexOfPath(path);
    return row < 0 ? nullptr : &m_entries.at(row);
}

void GameModel::resetEntries(const QList<GameEntry> &entries)
{
    const qsizetype oldCount = m_entries.size();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (oldCount != m_entries.size())
        emit countChanged();
}

void GameModel::updateEntry(const GameEntry &entry)
{
    const int row = indexOfPath(entry.path);
    if (row < 0)
        return;
    m_entries[row] = entry;
    emit dataChanged(index(row), index(row));
}

void GameModel::removeEntry(const QString &path)
{
    const int row = indexOfPath(path);
    if (row < 0)
        return;
    beginRemoveRows({}, row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void GameModel::setEmulatorInfo(const QHash<QString, QPair<QString, QString>> &info)
{
    if (m_emulatorInfo == info)
        return;
    m_emulatorInfo = info;
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(int(m_entries.size()) - 1), {EmulatorNameRole, PlatformRole});
}

QVariantMap GameModel::toVariantMap(const GameEntry &entry) const
{
    const QString artworkUrl = entry.artworkPath.isEmpty() ? QString() : QUrl::fromLocalFile(entry.artworkPath).toString();
    const QPair<QString, QString> info = m_emulatorInfo.value(entry.emulatorId);
    return {{QStringLiteral("path"), entry.path},
            {QStringLiteral("title"), entry.title},
            {QStringLiteral("fileName"), entry.fileName},
            {QStringLiteral("extension"), entry.extension},
            {QStringLiteral("directory"), entry.directory},
            {QStringLiteral("fileSize"), entry.fileSize},
            {QStringLiteral("artworkUrl"), artworkUrl},
            {QStringLiteral("hasArtwork"), !entry.artworkPath.isEmpty()},
            {QStringLiteral("emulatorId"), entry.emulatorId},
            {QStringLiteral("emulatorName"), info.first},
            {QStringLiteral("emulatorOverridden"), entry.emulatorOverridden},
            {QStringLiteral("platform"), info.second},
            {QStringLiteral("favorite"), entry.favorite},
            {QStringLiteral("lastPlayed"), entry.lastPlayed},
            {QStringLiteral("firstSeen"), entry.firstSeen}};
}

} // namespace nexora
