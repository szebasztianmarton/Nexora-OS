#include "GameListModel.h"

#include "nexora/library/LibraryQuery.h"

#include <QLocale>

namespace nexora::ui {

GameListModel::GameListModel(QObject *parent)
    : QAbstractListModel(parent)
{ }

int GameListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant GameListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const library::GameEntry &e = m_entries.at(index.row());
    // The emulator that launch() would use, not merely the entry's own override/first candidate.
    const QString emulatorId = library::LibraryQuery::effectiveEmulatorId(e, m_systemDefaultEmulators);
    const auto info = [&] { return m_info ? m_info(emulatorId) : std::pair<QString, QString>{}; };
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole: return e.displayTitle();
    case GameIdRole: return e.id;
    case PathRole: return e.path;
    case EmulatorIdRole: return emulatorId;
    case EmulatorNameRole: {
        const QString name = info().first;
        return name.isEmpty() ? emulatorId : name;
    }
    case FavoriteRole: return e.favorite;
    case PlayCountRole: return e.playCount;
    case LastPlayedRole:
        return e.lastPlayed.isValid() ? QLocale().toString(e.lastPlayed.toLocalTime(), QLocale::ShortFormat)
                                      : QString();
    case AvailableRole: return !emulatorId.isEmpty() && info().second.isEmpty();
    case UnavailableReasonRole:
        return emulatorId.isEmpty() ? QStringLiteral("No emulator handles %1 files").arg(e.extension) : info().second;
    case SystemIdRole: return e.systemId;
    case SystemShortNameRole: return m_systemInfo ? m_systemInfo(e.systemId) : QString();
    case DisplayTitleRole: return e.displayTitle();
    case PresentRole: return e.present;
    default: return {};
    }
}

QHash<int, QByteArray> GameListModel::roleNames() const
{
    return {{GameIdRole, "gameId"},
            {TitleRole, "title"},
            {PathRole, "path"},
            {EmulatorIdRole, "emulatorId"},
            {EmulatorNameRole, "emulatorName"},
            {FavoriteRole, "favorite"},
            {PlayCountRole, "playCount"},
            {LastPlayedRole, "lastPlayed"},
            {AvailableRole, "available"},
            {UnavailableReasonRole, "unavailableReason"},
            {SystemIdRole, "systemId"},
            {SystemShortNameRole, "systemShortName"},
            {DisplayTitleRole, "displayTitle"},
            {PresentRole, "present"}};
}

void GameListModel::setSystemDefaultEmulators(const QHash<QString, QString> &defaults)
{
    if (defaults == m_systemDefaultEmulators)
        return;
    m_systemDefaultEmulators = defaults;
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(rowCount() - 1),
                         {EmulatorIdRole, EmulatorNameRole, AvailableRole, UnavailableReasonRole});
}

void GameListModel::setEntries(const QList<library::GameEntry> &entries)
{
    const bool sameRows = entries.size() == m_entries.size()
        && std::equal(entries.cbegin(), entries.cend(), m_entries.cbegin(),
                      [](const auto &a, const auto &b) { return a.id == b.id; });
    if (sameRows) {
        m_entries = entries;
        if (!m_entries.isEmpty())
            emit dataChanged(index(0), index(rowCount() - 1));
        return;
    }
    const int oldCount = rowCount();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (oldCount != rowCount())
        emit countChanged();
}

int GameListModel::indexOfGame(qint64 gameId) const
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == gameId)
            return int(i);
    }
    return -1;
}

qint64 GameListModel::gameIdAt(int row) const
{
    return row >= 0 && row < rowCount() ? m_entries.at(row).id : 0;
}

} // namespace nexora::ui
