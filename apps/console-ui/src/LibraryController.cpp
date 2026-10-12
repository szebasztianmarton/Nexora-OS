#include "LibraryController.h"

#include "AppController.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/Settings.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>

#include <utility>

namespace nexora::ui {

using shared::Settings;

LibraryController::LibraryController(library::GameLibrary *library, shared::Settings *settings, AppController *app,
                                     QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_settings(settings)
    , m_app(app)
{
    m_query.sort = library::LibraryQuery::sortOrderFromString(settings->stringValue(Settings::LibrarySortOrder));
    const auto info = [this](const QString &emulatorId) -> std::pair<QString, QString> {
        const auto it = m_emulatorInfo.constFind(emulatorId);
        if (it != m_emulatorInfo.cend())
            return *it;
        return {QString(), m_app->unavailableReason(emulatorId)};
    };
    rebuildEmulatorInfo();
    for (GameListModel *model : {&m_games, &m_recent, &m_favorites})
        model->setEmulatorInfo(info);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(150);
    connect(&m_searchDebounce, &QTimer::timeout, this, &LibraryController::rebuildViews);

    connect(m_library, &library::GameLibrary::entriesChanged, this, &LibraryController::rebuildViews);
    connect(m_library, &library::GameLibrary::scanningChanged, this, &LibraryController::scanningChanged);
    connect(m_library, &library::GameLibrary::scanFinished, this,
            [this](const library::ApplyStats &stats, int issueCount) {
                emit notice(QStringLiteral("Library updated: %1 new, %2 missing, %3 issue(s)")
                                .arg(stats.added)
                                .arg(stats.missing)
                                .arg(issueCount),
                            false);
            });
    // Emulator definitions decide which extensions are games and how they launch.
    connect(m_library, &library::GameLibrary::scanFailed, this, [this](const QString &error) {
        emit notice(QStringLiteral("Library scan could not be saved: %1").arg(error), true);
    });
    // A launch counts as "played" only once the emulator process is actually running.
    connect(m_app->manager(), &emulator::EmulatorManager::stateChanged, this, [this](emulator::ProcessState state) {
        if (m_pendingLaunch == 0)
            return;
        if (state == emulator::ProcessState::Running)
            m_library->recordLaunch(std::exchange(m_pendingLaunch, 0));
        else if (state == emulator::ProcessState::Failed || state == emulator::ProcessState::Exited)
            m_pendingLaunch = 0;
    });
    connect(m_app, &AppController::configChanged, this, [this] {
        rebuildEmulatorInfo();
        updateScanOptions();
        rebuildViews();
    });
    connect(m_settings, &Settings::valueChanged, this, [this](const QString &key) {
        if (key == Settings::LibraryDirectories || key == Settings::LibraryExtensions
            || key == Settings::LibraryFollowSymlinks) {
            if (key == Settings::LibraryDirectories)
                emit directoriesChanged();
            updateScanOptions();
        } else if (key == Settings::LibrarySortOrder) {
            setSortOrder(m_settings->stringValue(Settings::LibrarySortOrder));
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
    // The library screen's choice becomes the remembered default.
    m_settings->setValue(Settings::LibrarySortOrder, library::LibraryQuery::toString(s));
    m_settings->save();
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

void LibraryController::updateScanOptions(bool force, bool autoScan)
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
    if (autoScan && (force || m_library->needsRescan()))
        m_library->refresh();
}

bool LibraryController::refresh()
{
    if (!m_library->isOpen()) {
        emit notice(QStringLiteral("The game library is unavailable: %1").arg(m_library->lastError()), true);
        return false;
    }
    updateScanOptions(/*force=*/false, /*autoScan=*/false);
    const bool started = m_library->refresh();
    emit notice(started ? QStringLiteral("Scanning library…")
                        : QStringLiteral("A scan is already running; queued another"),
                false);
    return started;
}

bool LibraryController::launch(qint64 gameId)
{
    return launchEntry(gameId, {}, true);
}

bool LibraryController::launchWithProfile(qint64 gameId, const QString &profileId)
{
    return launchEntry(gameId, profileId, false);
}

bool LibraryController::launchEntry(qint64 gameId, const QString &profileId, bool applyDefaultProfile)
{
    const auto entry = m_library->entry(gameId);
    if (!entry || !entry->present) {
        emit notice(QStringLiteral("This game is no longer in the library"), true);
        return false;
    }
    m_pendingLaunch = gameId;
    if (!m_app->launchGame(entry->emulatorId(), entry->path, entry->title, profileId, applyDefaultProfile)) {
        m_pendingLaunch = 0;
        return false;
    }
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
    if (!m_library->isOpen())
        return 0;
    const int removed = m_library->purgeMissing();
    emit notice(removed > 0 ? QStringLiteral("Removed %1 missing game(s) and their history").arg(removed)
                            : QStringLiteral("No missing games to remove"),
                false);
    return removed;
}

QVariantMap LibraryController::gameDetails(qint64 gameId) const
{
    const auto entry = m_library->entry(gameId);
    if (!entry)
        return {};
    const QString effective = entry->emulatorId();
    QVariantList emulators;
    for (const QString &id : entry->emulatorCandidates) {
        const auto it = m_emulatorInfo.constFind(id);
        const QString reason = it != m_emulatorInfo.cend() ? it->second : m_app->unavailableReason(id);
        emulators.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), it != m_emulatorInfo.cend() && !it->first.isEmpty() ? it->first : id},
            {QStringLiteral("available"), reason.isEmpty()},
            {QStringLiteral("reason"), reason},
            {QStringLiteral("selected"), id == effective},
        });
    }
    QVariantList profiles;
    const QString defaultProfile = m_settings->mapValue(Settings::EmulatorDefaultProfiles).value(effective).toString();
    for (const auto &config : m_app->enabledEmulators()) {
        if (config.id != effective)
            continue;
        for (const auto &p : config.profiles) {
            if (p.enabled)
                profiles.append(QVariantMap{{QStringLiteral("id"), p.id},
                                            {QStringLiteral("name"), p.name},
                                            {QStringLiteral("isDefault"), p.id == defaultProfile}});
        }
    }
    const QString overrideId = entry->emulatorOverride;
    return {
        {QStringLiteral("gameId"), entry->id},
        {QStringLiteral("title"), entry->title},
        {QStringLiteral("path"), entry->path},
        {QStringLiteral("size"), QLocale::c().formattedDataSize(entry->size)},
        {QStringLiteral("present"), entry->present},
        {QStringLiteral("favorite"), entry->favorite},
        {QStringLiteral("playCount"), entry->playCount},
        {QStringLiteral("lastPlayed"),
         entry->lastPlayed.isValid() ? entry->lastPlayed.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                     : QString()},
        {QStringLiteral("emulatorId"), effective},
        {QStringLiteral("automatic"), overrideId.isEmpty() || !entry->emulatorCandidates.contains(overrideId)},
        {QStringLiteral("emulators"), emulators},
        {QStringLiteral("profiles"), profiles},
    };
}

bool LibraryController::setGameEmulator(qint64 gameId, const QString &emulatorId)
{
    const auto entry = m_library->entry(gameId);
    if (!entry)
        return false;
    if (!emulatorId.isEmpty() && !entry->emulatorCandidates.contains(emulatorId)) {
        emit notice(QStringLiteral("\"%1\" does not handle %2 files").arg(emulatorId, entry->extension), true);
        return false;
    }
    if (!m_library->setEmulatorOverride(gameId, emulatorId)) {
        emit notice(QStringLiteral("Could not save the emulator choice for %1").arg(entry->title), true);
        return false;
    }
    return true;
}

void LibraryController::rebuildEmulatorInfo()
{
    m_emulatorInfo.clear();
    for (const auto &c : m_app->enabledEmulators())
        m_emulatorInfo.insert(c.id, {c.name, m_app->unavailableReason(c.id)});
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
