#pragma once

#include "nexora/library/GameEntry.h"

#include <QHash>
#include <QList>
#include <QString>

namespace nexora::library {

enum class SortOrder { Title, Recent, Emulator, Added };
enum class LibraryFilter { All, Favorites, Recent, Missing };

// QueryOptions::systemId value matching only entries without a recognised system (the
// "Other" bucket). Never a catalog id: "@" does not occur in one.
inline const QString UnknownSystemFilter = QStringLiteral("@other");

struct QueryOptions
{
    QString search; // whitespace-separated terms; all must match (case-insensitive)
    LibraryFilter filter = LibraryFilter::All;
    SortOrder sort = SortOrder::Title;
    QString emulatorId; // empty = any
    QString systemId; // empty = any; UnknownSystemFilter = no recognised system
    QHash<QString, QString> systemNames; // system id -> short name, searched as well as the title
    QHash<QString, QString> systemDefaultEmulators; // system id -> default emulator id (the Emulator sort)
    int recentLimit = 20; // LibraryFilter::Recent keeps this many most recent entries
};

// Pure filtering/sorting over library entries (no I/O), shared by the UI model and tests.
class LibraryQuery
{
public:
    static QList<GameEntry> apply(QList<GameEntry> entries, const QueryOptions &options);
    // The emulator that launches |entry|: its override if that still handles the file, else
    // the default of its system (|systemDefaultEmulators|) if that does, else the first
    // candidate. The one resolution shared by the launch path, the list models and the sort.
    static QString effectiveEmulatorId(const GameEntry &entry, const QHash<QString, QString> &systemDefaultEmulators);
    static bool matchesSearch(const GameEntry &entry, const QString &search,
                              const QHash<QString, QString> &systemNames = {});

    static SortOrder sortOrderFromString(const QString &name, SortOrder fallback = SortOrder::Title);
    static QString toString(SortOrder order);
    static LibraryFilter filterFromString(const QString &name, LibraryFilter fallback = LibraryFilter::All);
    static QString toString(LibraryFilter filter);
};

} // namespace nexora::library
