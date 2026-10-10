#pragma once

#include <QSortFilterProxyModel>
#include <QVariantMap>

namespace nexora {

// Search/sort/filter view over GameModel. Instantiable from QML:
//   GameFilterModel { sourceModel: Library.model; favoritesOnly: true; sortMode: "title-asc" }
class GameFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    // "title-asc" | "title-desc" | "recent" (last played desc, never-played last) | "added" (firstSeen desc)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY filterChanged)
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY filterChanged)
    Q_PROPERTY(bool artworkOnly READ artworkOnly WRITE setArtworkOnly NOTIFY filterChanged)
    Q_PROPERTY(bool playedOnly READ playedOnly WRITE setPlayedOnly NOTIFY filterChanged)   // lastPlayed valid
    Q_PROPERTY(QString emulatorFilter READ emulatorFilter WRITE setEmulatorFilter NOTIFY filterChanged) // "" = all
    Q_PROPERTY(QString excludePath READ excludePath WRITE setExcludePath NOTIFY filterChanged) // hide one entry (e.g. the featured game)
    Q_PROPERTY(bool filtersActive READ filtersActive NOTIFY filterChanged) // any filter or non-empty search

public:
    explicit GameFilterModel(QObject *parent = nullptr);

    int count() const;
    QString searchText() const;
    void setSearchText(const QString &text);
    QString sortMode() const;
    void setSortMode(const QString &mode);
    bool favoritesOnly() const;
    void setFavoritesOnly(bool value);
    bool artworkOnly() const;
    void setArtworkOnly(bool value);
    bool playedOnly() const;
    void setPlayedOnly(bool value);
    QString emulatorFilter() const;
    void setEmulatorFilter(const QString &id);
    QString excludePath() const;
    void setExcludePath(const QString &path);
    bool filtersActive() const;

    Q_INVOKABLE QVariantMap get(int row) const;     // keys = GameModel role names; {} if out of range
    Q_INVOKABLE int indexOfPath(const QString &path) const; // proxy row or -1
    Q_INVOKABLE void clearFilters();                // resets search + all filters (keeps sortMode)

signals:
    void countChanged();
    void filterChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    void refilter();

    QString m_search;
    QString m_sortMode = QStringLiteral("title-asc");
    bool m_favoritesOnly = false;
    bool m_artworkOnly = false;
    bool m_playedOnly = false;
    QString m_emulatorFilter;
    QString m_excludePath;
};

} // namespace nexora
