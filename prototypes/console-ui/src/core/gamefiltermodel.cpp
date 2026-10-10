#include "gamefiltermodel.h"

#include <QDateTime>

#include "gamemodel.h"

namespace nexora {

namespace {

QStringView stripLeadingZeros(QStringView digits)
{
    qsizetype k = 0;
    while (k + 1 < digits.size() && digits.at(k) == QLatin1Char('0'))
        ++k;
    return digits.mid(k);
}

// Case-insensitive, locale-aware comparison where digit runs compare by value,
// so "Game 2" sorts before "Game 10" (QCollator's numeric mode needs ICU).
int compareTitles(const QString &a, const QString &b)
{
    qsizetype i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const bool digitA = a.at(i).isDigit();
        const bool digitB = b.at(j).isDigit();
        qsizetype ei = i, ej = j;
        while (ei < a.size() && a.at(ei).isDigit() == digitA)
            ++ei;
        while (ej < b.size() && b.at(ej).isDigit() == digitB)
            ++ej;
        const QStringView chunkA = QStringView(a).mid(i, ei - i);
        const QStringView chunkB = QStringView(b).mid(j, ej - j);
        int c;
        if (digitA && digitB) {
            const QStringView na = stripLeadingZeros(chunkA);
            const QStringView nb = stripLeadingZeros(chunkB);
            c = na.size() != nb.size() ? (na.size() < nb.size() ? -1 : 1) : na.compare(nb);
        } else {
            c = QString::localeAwareCompare(chunkA.toString().toCaseFolded(), chunkB.toString().toCaseFolded());
        }
        if (c != 0)
            return c;
        i = ei;
        j = ej;
    }
    return (a.size() - i > 0) ? 1 : (b.size() - j > 0) ? -1 : 0;
}

const QStringList &sortModes()
{
    static const QStringList modes{QStringLiteral("title-asc"), QStringLiteral("title-desc"),
                                   QStringLiteral("recent"), QStringLiteral("added")};
    return modes;
}

} // namespace

GameFilterModel::GameFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::rowsInserted, this, &GameFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &GameFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &GameFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &GameFilterModel::countChanged);
    sort(0);
}

int GameFilterModel::count() const { return rowCount(); }
QString GameFilterModel::searchText() const { return m_search; }
QString GameFilterModel::sortMode() const { return m_sortMode; }
bool GameFilterModel::favoritesOnly() const { return m_favoritesOnly; }
bool GameFilterModel::artworkOnly() const { return m_artworkOnly; }
bool GameFilterModel::playedOnly() const { return m_playedOnly; }
QString GameFilterModel::emulatorFilter() const { return m_emulatorFilter; }
QString GameFilterModel::excludePath() const { return m_excludePath; }

bool GameFilterModel::filtersActive() const
{
    return !m_search.trimmed().isEmpty() || m_favoritesOnly || m_artworkOnly || m_playedOnly
           || !m_emulatorFilter.isEmpty();
}

void GameFilterModel::setSearchText(const QString &text)
{
    if (m_search == text)
        return;
    m_search = text;
    refilter();
}

void GameFilterModel::setSortMode(const QString &mode)
{
    if (m_sortMode == mode)
        return;
    if (!sortModes().contains(mode)) {
        qWarning("GameFilterModel: unknown sort mode \"%s\"", qPrintable(mode));
        return;
    }
    m_sortMode = mode;
    invalidate();
    sort(0);
    emit filterChanged();
}

void GameFilterModel::setFavoritesOnly(bool value)
{
    if (m_favoritesOnly == value)
        return;
    m_favoritesOnly = value;
    refilter();
}

void GameFilterModel::setArtworkOnly(bool value)
{
    if (m_artworkOnly == value)
        return;
    m_artworkOnly = value;
    refilter();
}

void GameFilterModel::setPlayedOnly(bool value)
{
    if (m_playedOnly == value)
        return;
    m_playedOnly = value;
    refilter();
}

void GameFilterModel::setEmulatorFilter(const QString &id)
{
    if (m_emulatorFilter == id)
        return;
    m_emulatorFilter = id;
    refilter();
}

void GameFilterModel::setExcludePath(const QString &path)
{
    if (m_excludePath == path)
        return;
    m_excludePath = path;
    refilter();
}

void GameFilterModel::clearFilters()
{
    if (!filtersActive())
        return;
    m_search.clear();
    m_favoritesOnly = false;
    m_artworkOnly = false;
    m_playedOnly = false;
    m_emulatorFilter.clear();
    refilter();
}

void GameFilterModel::refilter()
{
    invalidateFilter();
    emit filterChanged();
}

QVariantMap GameFilterModel::get(int row) const
{
    if (row < 0 || row >= rowCount() || !sourceModel())
        return {};
    const QModelIndex source = mapToSource(index(row, 0));
    QVariantMap map;
    const QHash<int, QByteArray> roles = sourceModel()->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        if (it.key() >= Qt::UserRole)
            map.insert(QString::fromUtf8(it.value()), source.data(it.key()));
    }
    return map;
}

int GameFilterModel::indexOfPath(const QString &path) const
{
    for (int row = 0; row < rowCount(); ++row) {
        if (index(row, 0).data(GameModel::PathRole).toString() == path)
            return row;
    }
    return -1;
}

bool GameFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_excludePath.isEmpty() && idx.data(GameModel::PathRole).toString() == m_excludePath)
        return false;
    if (m_favoritesOnly && !idx.data(GameModel::FavoriteRole).toBool())
        return false;
    if (m_artworkOnly && !idx.data(GameModel::HasArtworkRole).toBool())
        return false;
    if (m_playedOnly && !idx.data(GameModel::LastPlayedRole).toDateTime().isValid())
        return false;
    if (!m_emulatorFilter.isEmpty() && idx.data(GameModel::EmulatorIdRole).toString() != m_emulatorFilter)
        return false;
    const QString search = m_search.trimmed();
    if (!search.isEmpty()) {
        return idx.data(GameModel::TitleRole).toString().contains(search, Qt::CaseInsensitive)
               || idx.data(GameModel::FileNameRole).toString().contains(search, Qt::CaseInsensitive);
    }
    return true;
}

bool GameFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    auto titleLess = [](const QModelIndex &a, const QModelIndex &b) {
        const int c = compareTitles(a.data(GameModel::TitleRole).toString(), b.data(GameModel::TitleRole).toString());
        if (c != 0)
            return c < 0;
        return a.data(GameModel::PathRole).toString() < b.data(GameModel::PathRole).toString();
    };
    // Newest first; invalid dates sort last; ties fall back to title order.
    auto dateOrder = [&](int role) {
        const QDateTime l = left.data(role).toDateTime();
        const QDateTime r = right.data(role).toDateTime();
        if (l.isValid() != r.isValid())
            return l.isValid();
        if (l.isValid() && l != r)
            return l > r;
        return titleLess(left, right);
    };

    if (m_sortMode == QLatin1String("title-desc"))
        return titleLess(right, left);
    if (m_sortMode == QLatin1String("recent"))
        return dateOrder(GameModel::LastPlayedRole);
    if (m_sortMode == QLatin1String("added"))
        return dateOrder(GameModel::FirstSeenRole);
    return titleLess(left, right);
}

} // namespace nexora
