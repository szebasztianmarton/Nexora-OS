#include "LibraryController.h"

#include "AppController.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/Settings.h"

#include <QDir>
#include <QFileInfo>

namespace nexora::ui {

using shared::Settings;

LibraryController::LibraryController(library::GameLibrary *library, shared::Settings *settings,
                                     AppController *app, QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_settings(settings)
    , m_app(app)
{
    m_query.sort = library::LibraryQuery::sortOrderFromString(settings->stringValue(Settings::LibrarySortOrder));
    const auto info = [this](const QString &emulatorId) -> std::pair<QString, QString> {
        QString name;
        for (const auto &c : m_app->enabledEmulators()) {
            if (c.id == emulatorId)
                name = c.name;
        }
        return {name, m_app->unavailableReason(emulatorId)};
    };
    for (GameListModel *model : {&m_games, &m_recent, &m_favorites})
        model->setEmulatorInfo(info);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(150);
    connect(&m_searchDebounce, &QTimer::timeout, this, &LibraryController::rebuildViews);

    connect(m_library, &library::GameLibrary::entriesChanged, this, &LibraryController::rebuildViews);
    connect(m_library, &library::GameLibrary::scanningChanged, this, &LibraryController::scanningChanged);
    connect(m_library, &library::GameLibrary::scanFinished, this, [this](const library::ApplyStats &stats, int issueCount) {
        emit notice(QStringLiteral("Library updated: %1 new, %2 missing, %3 issue(s)")
                        .arg(stats.added)
                        .arg(stats.missing)
                        .arg(issueCount),
                    false);
    });
    // Emulator definitions decide which extensions are games and how they launch.
    connect(m_app, &AppController::configChanged, this, [this] {
        updateScanOptions();
        rebuildViews();
    });
    connect(m_settings, &Settings::valueChanged, this, [this](const QString &key) {
        if (key == Settings::LibraryDirectories || key == Settings::LibraryExtensions
            || key == Settings::LibraryFollowSymlinks) {
            if (key == Settings::LibraryDirectories)
                emit directoriesChanged();
            updateScanOptions();
        }
    });
    rebuildViews();
}

void LibraryController::setSearchText(const QString &text)
{
    if (text == m_query.search)
        return;
    m_query.search = text;
    emit queryChanged();
    m_searchDebounce.start(); // typing should not rebuild per keystroke
}

QString LibraryController::filter() const
{
    return library::LibraryQuery::toString(m_query.filter);
}

void LibraryController::setFilter(const QString &filter)
{
    const auto f = library::LibraryQuery::filterFromString(filter, m_query.filter);
    if (f == m_query.filter)
        return;
    m_query.filter = f;
    emit queryChanged();
    rebuildViews();
}

QString LibraryController::sortOrder() const
{
    return library::LibraryQuery::toString(m_query.sort);
}

void LibraryController::setSortOrder(const QString &order)
{
    const auto s = library::LibraryQuery::sortOrderFromString(order, m_query.sort);
    if (s == m_query.sort)
        return;
    m_query.sort = s;
    emit queryChanged();
    rebuildViews();
}

QStringList LibraryController::directories() const
{
    return m_settings->stringListValue(Settings::LibraryDirectories);
}

QStringList LibraryController::issues() const
{
    QStringList list = m_library->databaseIssues();
    for (const auto &issue : m_library->lastScan().issues)
        list.append(issue.path + QStringLiteral(": ") + issue.message);
    return list;
}

QString LibraryController::lastScanSummary() const
{
    const auto &scan = m_library->lastScan();
    if (scan.scannedRoots.isEmpty() && scan.files.isEmpty() && scan.issues.isEmpty())
        return directories().isEmpty() ? QStringLiteral("No library directories configured")
                                       : QStringLiteral("Not scanned in this session");
    return QStringLiteral("%1 game file(s) in %2 director%3; %4 duplicate(s), %5 skipped link(s)%6")
        .arg(scan.files.size())
        .arg(scan.scannedRoots.size())
        .arg(scan.scannedRoots.size() == 1 ? QStringLiteral("y") : QStringLiteral("ies"))
        .arg(scan.duplicates)
        .arg(scan.skippedSymlinks)
        .arg(scan.truncated ? QStringLiteral("; scan limit reached") : QString());
}

void LibraryController::updateScanOptions(bool force)
{
    library::ScanOptions options;
    options.roots = directories();
    options.followSymlinks = m_settings->boolValue(Settings::LibraryFollowSymlinks);
    for (const auto &config : m_app->enabledEmulators()) {
        for (const QString &ext : config.extensions) {
            QStringList &ids = options.extensionMap[ext];
            if (!ids.contains(config.id))
                ids.append(config.id);
        }
    }
    // Extra extensions without an emulator still show up (as "no emulator" entries).
    for (const QString &ext : m_settings->stringListValue(Settings::LibraryExtensions))
        options.extensionMap.insert(ext, options.extensionMap.value(ext));
    m_library->setScanOptions(options);
    if (!m_library->isOpen())
        return;
    if (force || m_library->needsRescan())
        m_library->refresh();
}

bool LibraryController::refresh()
{
    if (!m_library->isOpen()) {
        emit notice(QStringLiteral("The game library is unavailable: %1").arg(m_library->lastError()), true);
        return false;
    }
    updateScanOptions();
    const bool started = m_library->refresh();
    emit notice(started ? QStringLiteral("Scanning library…") : QStringLiteral("A scan is already running; queued another"),
                false);
    return started;
}

bool LibraryController::launch(qint64 gameId)
{
    const auto entry = m_library->entry(gameId);
    if (!entry || !entry->present) {
        emit notice(QStringLiteral("This game is no longer in the library"), true);
        return false;
    }
    if (!m_app->launchGame(entry->emulatorId(), entry->path, entry->title))
        return false;
    m_library->recordLaunch(gameId);
    return true;
}

bool LibraryController::toggleFavorite(qint64 gameId)
{
    const auto entry = m_library->entry(gameId);
    return entry && m_library->setFavorite(gameId, !entry->favorite);
}

bool LibraryController::addDirectory(const QString &path)
{
    const QString clean = QDir::cleanPath(path.trimmed());
    const QFileInfo info(clean);
    if (!info.isAbsolute() || !info.isDir()) {
        emit notice(QStringLiteral("\"%1\" is not an existing absolute directory").arg(path), true);
        return false;
    }
    QStringList dirs = directories();
    if (dirs.contains(clean)) {
        emit notice(QStringLiteral("%1 is already in the library").arg(clean), false);
        return false;
    }
    dirs.append(clean);
    QString error;
    if (!m_settings->setValue(Settings::LibraryDirectories, dirs, &error)) {
        emit notice(error, true);
        return false;
    }
    m_settings->save();
    emit notice(QStringLiteral("Added %1").arg(clean), false);
    return true;
}

bool LibraryController::removeDirectory(const QString &path)
{
    QStringList dirs = directories();
    if (!dirs.removeOne(path))
        return false;
    m_settings->setValue(Settings::LibraryDirectories, dirs);
    m_settings->save();
    emit notice(QStringLiteral("Removed %1").arg(path), false);
    return true;
}

int LibraryController::purgeMissing()
{
    return m_library->purgeMissing();
}

void LibraryController::rebuildViews()
{
    const QList<library::GameEntry> all = m_library->entries();
    m_games.setEntries(library::LibraryQuery::apply(all, m_query));

    library::QueryOptions recent;
    recent.filter = library::LibraryFilter::Recent;
    recent.recentLimit = 12;
    m_recent.setEntries(library::LibraryQuery::apply(all, recent));

    library::QueryOptions favorites;
    favorites.filter = library::LibraryFilter::Favorites;
    m_favorites.setEntries(library::LibraryQuery::apply(all, favorites));
    emit contentChanged();
}

} // namespace nexora::ui
