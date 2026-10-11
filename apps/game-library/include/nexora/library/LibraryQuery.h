#pragma once

#include "nexora/library/GameEntry.h"

#include <QHash>
#include <QList>
#include <QString>

namespace nexora::library {

enum class SortOrder { Title, Recent, Emulator, Added };
enum class LibraryFilter { All, Favorites, Recent, Missing };

struct QueryOptions
{
    QString search; // whitespace-separated terms; all must match (case-insensitive)
    LibraryFilter filter = LibraryFilter::All;
    SortOrder sort = SortOrder::Title;
    QString emulatorId; // empty = any
    QString systemId; // empty = any
    QHash<QString, QString> systemNames; // system id -> short name, searched as well as the title
    int recentLimit = 20; // LibraryFilter::Recent keeps this many most recent entries
};

// Pure filtering/sorting over library entries (no I/O), shared by the UI model and tests.
class LibraryQuery
{
public:
    static QList<GameEntry> apply(QList<GameEntry> entries, const QueryOptions &options);
    static bool matchesSearch(const GameEntry &entry, const QString &search,
                              const QHash<QString, QString> &systemNames = {});

    static SortOrder sortOrderFromString(const QString &name, SortOrder fallback = SortOrder::Title);
    static QString toString(SortOrder order);
    static LibraryFilter filterFromString(const QString &name, LibraryFilter fallback = LibraryFilter::All);
    static QString toString(LibraryFilter filter);
};

} // namespace nexora::library
