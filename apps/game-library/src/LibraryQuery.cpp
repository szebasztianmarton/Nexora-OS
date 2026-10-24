#include "nexora/library/LibraryQuery.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace nexora::library {

bool LibraryQuery::matchesSearch(const GameEntry &entry, const QString &search,
                                 const QHash<QString, QString> &systemNames)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    const QStringList terms = search.split(whitespace, Qt::SkipEmptyParts);
    if (terms.isEmpty())
        return true;
    const QString fileName = QFileInfo(entry.path).fileName();
    const QString title = entry.displayTitle();
    const QString systemName = systemNames.value(entry.systemId);
    return std::all_of(terms.cbegin(), terms.cend(), [&](const QString &term) {
        return title.contains(term, Qt::CaseInsensitive) || fileName.contains(term, Qt::CaseInsensitive)
            || systemName.contains(term, Qt::CaseInsensitive);
    });
}

QList<GameEntry> LibraryQuery::apply(QList<GameEntry> entries, const QueryOptions &options)
{
    entries.removeIf([&](const GameEntry &e) {
        if (e.hidden)
            return true;
        if ((options.filter == LibraryFilter::Missing) == e.present)
            return true; // Missing shows only absent entries, every other view only present ones
        if (options.filter == LibraryFilter::Favorites && !e.favorite)
            return true;
        if (options.filter == LibraryFilter::Recent && !e.lastPlayed.isValid())
            return true;
        if (!options.emulatorId.isEmpty() && !e.emulatorCandidates.contains(options.emulatorId))
            return true;
        if (options.systemId == UnknownSystemFilter) {
            if (!e.systemId.isEmpty())
                return true;
        } else if (!options.systemId.isEmpty() && e.systemId != options.systemId) {
            return true;
        }
        return !matchesSearch(e, options.search, options.systemNames);
    });

    auto byTitle = [](const GameEntry &a, const GameEntry &b) {
        const int c = a.displayTitle().compare(b.displayTitle(), Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a.path < b.path;
    };
    auto byRecent = [&](const GameEntry &a, const GameEntry &b) {
        if (a.lastPlayed.isValid() != b.lastPlayed.isValid())
            return a.lastPlayed.isValid(); // played before never-played
        if (a.lastPlayed != b.lastPlayed)
            return a.lastPlayed > b.lastPlayed;
        return byTitle(a, b);
    };

    // The Recent filter always orders by recency so the limit keeps the newest entries.
    const SortOrder order = options.filter == LibraryFilter::Recent ? SortOrder::Recent : options.sort;
    switch (order) {
    case SortOrder::Title: std::sort(entries.begin(), entries.end(), byTitle); break;
    case SortOrder::Recent: std::sort(entries.begin(), entries.end(), byRecent); break;
    case SortOrder::Emulator:
        std::sort(entries.begin(), entries.end(), [&](const GameEntry &a, const GameEntry &b) {
            const int c = effectiveEmulatorId(a, options.systemDefaultEmulators)
                              .compare(effectiveEmulatorId(b, options.systemDefaultEmulators));
            return c != 0 ? c < 0 : byTitle(a, b);
        });
        break;
    case SortOrder::Added:
        std::sort(entries.begin(), entries.end(), [&](const GameEntry &a, const GameEntry &b) {
            return a.addedAt != b.addedAt ? a.addedAt > b.addedAt : byTitle(a, b);
        });
        break;
    }
    if (options.filter == LibraryFilter::Recent && options.recentLimit >= 0 && entries.size() > options.recentLimit) {
        entries.resize(options.recentLimit);
    }
    return entries;
}

QString LibraryQuery::effectiveEmulatorId(const GameEntry &entry, const QHash<QString, QString> &systemDefaultEmulators)
{
    if (!entry.emulatorOverride.isEmpty() && entry.emulatorCandidates.contains(entry.emulatorOverride))
        return entry.emulatorOverride;
    if (!entry.systemId.isEmpty()) {
        const QString systemDefault = systemDefaultEmulators.value(entry.systemId);
        if (!systemDefault.isEmpty() && entry.emulatorCandidates.contains(systemDefault))
            return systemDefault;
    }
    return entry.emulatorCandidates.value(0);
}

SortOrder LibraryQuery::sortOrderFromString(const QString &name, SortOrder fallback)
{
    if (name == QLatin1String("title"))
        return SortOrder::Title;
    if (name == QLatin1String("recent"))
        return SortOrder::Recent;
    if (name == QLatin1String("emulator"))
        return SortOrder::Emulator;
    if (name == QLatin1String("added"))
        return SortOrder::Added;
    return fallback;
}

QString LibraryQuery::toString(SortOrder order)
{
    switch (order) {
    case SortOrder::Title: return QStringLiteral("title");
    case SortOrder::Recent: return QStringLiteral("recent");
    case SortOrder::Emulator: return QStringLiteral("emulator");
    case SortOrder::Added: return QStringLiteral("added");
    }
    return QStringLiteral("title");
}

LibraryFilter LibraryQuery::filterFromString(const QString &name, LibraryFilter fallback)
{
    if (name == QLatin1String("all"))
        return LibraryFilter::All;
    if (name == QLatin1String("favorites"))
        return LibraryFilter::Favorites;
    if (name == QLatin1String("recent"))
        return LibraryFilter::Recent;
    if (name == QLatin1String("missing"))
        return LibraryFilter::Missing;
    return fallback;
}

QString LibraryQuery::toString(LibraryFilter filter)
{
    switch (filter) {
    case LibraryFilter::All: return QStringLiteral("all");
    case LibraryFilter::Favorites: return QStringLiteral("favorites");
    case LibraryFilter::Recent: return QStringLiteral("recent");
    case LibraryFilter::Missing: return QStringLiteral("missing");
    }
    return QStringLiteral("all");
}

} // namespace nexora::library
