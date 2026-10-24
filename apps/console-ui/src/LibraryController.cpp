#include "LibraryController.h"

#include "AppController.h"
#include "GameSessionManager.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/Redaction.h"
#include "nexora/shared/Settings.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QQmlEngine>
#include <QSet>
#include <QSignalBlocker>

#include <algorithm>
#include <memory>
#include <utility>

namespace nexora::ui {

using shared::Settings;

LibraryController::LibraryController(library::GameLibrary *library, shared::Settings *settings, AppController *app,
                                     const shared::SystemCatalog *catalog, QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_settings(settings)
    , m_app(app)
    , m_catalog(catalog)
{
    m_query.sort = library::LibraryQuery::sortOrderFromString(settings->stringValue(Settings::LibrarySortOrder));
    rebuildEmulatorInfo();
    for (GameListModel *model : {&m_games, &m_recent, &m_favorites, &m_recentlyAdded, &m_pinned})
        setupModel(model);
    applySystemDefaultEmulators();
    m_collectionsSignature = collectionsSignature();
    connect(m_library, &library::GameLibrary::collectionsChanged, this, [this] {
        // Collections can disappear (deleted, or a restored backup without them): drop their
        // models and a filter on them, so nothing keeps showing a collection that is gone.
        QSet<qint64> existing;
        for (const auto &c : m_library->collections())
            existing.insert(c.id);
        for (auto it = m_collectionModels.begin(); it != m_collectionModels.end();) {
            if (existing.contains(it->first))
                ++it;
            else
                it = m_collectionModels.erase(it);
        }
        if (m_collectionFilter > 0 && !existing.contains(m_collectionFilter)) {
            m_collectionFilter = 0;
            emit queryChanged();
        }
        emit collectionsChanged();
        // Home rebuilds its collection rows from the layout: only when a name or the set of
        // collections changed, not for a membership change (that would drop the focused row).
        const QString signature = collectionsSignature();
        if (signature != m_collectionsSignature) {
            m_collectionsSignature = signature;
            emit homeLayoutChanged();
        }
        rebuildViews();
    });

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(150);
    connect(&m_searchDebounce, &QTimer::timeout, this, &LibraryController::rebuildViews);

    connect(m_library, &library::GameLibrary::entriesChanged, this, &LibraryController::rebuildViews);
    connect(m_library, &library::GameLibrary::scanningChanged, this, [this](bool scanning) {
        if (scanning) {
            m_scanFilesSeen = 0;
            m_scanCurrentRoot.clear();
            emit scanProgressChanged();
        }
        emit scanningChanged();
    });
    connect(m_library, &library::GameLibrary::scanProgress, this, [this](int filesSeen, const QString &currentRoot) {
        m_scanFilesSeen = filesSeen;
        m_scanCurrentRoot = currentRoot;
        emit scanProgressChanged();
    });
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
        if (state == emulator::ProcessState::Running) {
            const qint64 id = std::exchange(m_pendingLaunch, 0);
            m_runningGame = id; // play time is added when this run ends
            writeRetryingAfterScan(QStringLiteral("the play count of game %1").arg(id),
                                   [this, id] { return m_library->recordLaunch(id); });
        } else if (state == emulator::ProcessState::Failed || state == emulator::ProcessState::Exited)
            m_pendingLaunch = 0;
    });
    // Play time: run time without Nexora's pauses, for the game whose launch was counted.
    connect(m_app->manager(), &emulator::EmulatorManager::processFinished, this,
            [this](const QString &, const emulator::ProcessOutcome &outcome) {
                const qint64 gameId = std::exchange(m_runningGame, 0);
                const qint64 seconds = outcome.activeMs() / 1000;
                if (gameId > 0 && seconds >= 1)
                    writeRetryingAfterScan(QStringLiteral("the play time of game %1").arg(gameId),
                                           [this, gameId, seconds] { return m_library->addPlayTime(gameId, seconds); });
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
        } else if (key == Settings::EmulatorDefaultForSystem) {
            applySystemDefaultEmulators();
            rebuildViews();
        } else if (key == Settings::HomeSections) {
            emit homeLayoutChanged();
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

void LibraryController::setSystemFilter(const QString &systemId)
{
    if (systemId == m_query.systemId)
        return;
    m_query.systemId = systemId;
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
        options.emulatorSystems.insert(config.id, config.systems);
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

bool LibraryController::cancelScan()
{
    const bool was = m_library->isScanning();
    m_library->cancelScan();
    return was;
}

QString LibraryController::effectiveEmulatorId(const library::GameEntry &entry) const
{
    return library::LibraryQuery::effectiveEmulatorId(entry, systemDefaultEmulators());
}

QHash<QString, QString> LibraryController::systemDefaultEmulators() const
{
    QHash<QString, QString> defaults;
    const QVariantMap stored = m_settings->mapValue(Settings::EmulatorDefaultForSystem);
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
        defaults.insert(it.key(), it.value().toString());
    return defaults;
}

void LibraryController::applySystemDefaultEmulators()
{
    const QHash<QString, QString> defaults = systemDefaultEmulators();
    m_query.systemDefaultEmulators = defaults;
    for (GameListModel *model : {&m_games, &m_recent, &m_favorites, &m_recentlyAdded, &m_pinned})
        model->setSystemDefaultEmulators(defaults);
    for (auto it = m_collectionModels.begin(); it != m_collectionModels.end(); ++it)
        it->second->setSystemDefaultEmulators(defaults);
}

QString LibraryController::systemName(const QString &systemId) const
{
    if (systemId.isEmpty() || !m_catalog)
        return {};
    const auto *system = m_catalog->find(systemId);
    return system ? system->name : QString();
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
    if (m_sessions) {
        // A plain launch resumes a saved Quick Resume state; an explicit profile starts fresh.
        const bool preferResume = profileId.isEmpty() && applyDefaultProfile;
        return m_sessions->startGame(*entry, effectiveEmulatorId(*entry), profileId, applyDefaultProfile, preferResume);
    }
    m_pendingLaunch = gameId;
    if (!m_app->launchGame(effectiveEmulatorId(*entry), entry->path, entry->displayTitle(), profileId,
                           applyDefaultProfile)) {
        m_pendingLaunch = 0;
        return false;
    }
    return true;
}

void LibraryController::setSessions(GameSessionManager *sessions)
{
    if (m_sessions)
        m_sessions->disconnect(this);
    m_sessions = sessions;
    if (m_sessions)
        connect(m_sessions, &GameSessionManager::launchAccepted, this,
                [this](qint64 gameId) { m_pendingLaunch = gameId; });
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
    const QStringList previous = directories();
    QStringList dirs = previous;
    if (dirs.contains(clean)) {
        emit notice(QStringLiteral("%1 is already in the library").arg(clean), false);
        return false;
    }
    dirs.append(clean);
    if (!saveDirectories(previous, dirs))
        return false;
    emit notice(QStringLiteral("Added %1").arg(clean), false);
    return true;
}

bool LibraryController::removeDirectory(const QString &path)
{
    const QStringList previous = directories();
    QStringList dirs = previous;
    if (!dirs.removeOne(path))
        return false;
    if (!saveDirectories(previous, dirs))
        return false;
    emit notice(QStringLiteral("Removed %1").arg(path), false);
    return true;
}

bool LibraryController::saveDirectories(const QStringList &previous, const QStringList &dirs)
{
    // Not on disk means not kept: the running library must not scan what a restart forgets,
    // so nothing hears about the change (no scan starts) until it is saved.
    bool saved = false;
    {
        const QSignalBlocker blocker(m_settings);
        QString error;
        if (!m_settings->setValue(Settings::LibraryDirectories, dirs, &error)) {
            emit notice(error, true);
            return false;
        }
        saved = m_settings->save();
        if (!saved)
            m_settings->setValue(Settings::LibraryDirectories, previous);
    }
    if (!saved) {
        emit notice(QStringLiteral("Could not save %1").arg(m_settings->paths().userFile), true);
        return false;
    }
    emit m_settings->valueChanged(Settings::LibraryDirectories, m_settings->value(Settings::LibraryDirectories));
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
        emit notice(QStringLiteral("Could not save the emulator choice for %1").arg(entry->displayTitle()), true);
        return false;
    }
    return true;
}

QVariantMap LibraryController::gameDetails(qint64 id) const
{
    const auto entry = m_library->entry(id);
    if (!entry)
        return {};
    const QString emulatorId = effectiveEmulatorId(*entry);
    const auto emulatorInfo = m_emulatorInfo.constFind(emulatorId);
    const QString systemName = (!entry->systemId.isEmpty() && m_catalog && m_catalog->find(entry->systemId))
        ? m_catalog->find(entry->systemId)->name
        : QString();

    QVariantList emulators;
    for (const QString &candidate : entry->emulatorCandidates) {
        const auto it = m_emulatorInfo.constFind(candidate);
        const QString reason = it != m_emulatorInfo.cend() ? it->second : m_app->unavailableReason(candidate);
        emulators.append(QVariantMap{
            {QStringLiteral("id"), candidate},
            {QStringLiteral("name"), it != m_emulatorInfo.cend() && !it->first.isEmpty() ? it->first : candidate},
            {QStringLiteral("available"), reason.isEmpty()},
            {QStringLiteral("reason"), reason},
            {QStringLiteral("selected"), candidate == emulatorId},
        });
    }
    QVariantList profiles;
    const QString defaultProfile = m_settings->mapValue(Settings::EmulatorDefaultProfiles).value(emulatorId).toString();
    for (const auto &config : m_app->enabledEmulators()) {
        if (config.id != emulatorId)
            continue;
        for (const auto &p : config.profiles) {
            if (p.enabled)
                profiles.append(QVariantMap{{QStringLiteral("id"), p.id},
                                            {QStringLiteral("name"), p.name},
                                            {QStringLiteral("isDefault"), p.id == defaultProfile}});
        }
    }
    const QString overrideId = entry->emulatorOverride;

    return QVariantMap{
        {QStringLiteral("id"), entry->id},
        {QStringLiteral("gameId"), entry->id},
        {QStringLiteral("title"), entry->displayTitle()},
        {QStringLiteral("originalTitle"), entry->title},
        {QStringLiteral("path"), shared::redact(entry->path, shared::defaultRedactionRules())},
        {QStringLiteral("size"), QLocale::c().formattedDataSize(entry->size)},
        {QStringLiteral("systemId"), entry->systemId},
        {QStringLiteral("systemName"), systemName},
        {QStringLiteral("systemCandidates"), entry->systemCandidates},
        {QStringLiteral("emulatorId"), emulatorId},
        {QStringLiteral("emulatorName"),
         emulatorInfo != m_emulatorInfo.cend() && !emulatorInfo->first.isEmpty() ? emulatorInfo->first : emulatorId},
        {QStringLiteral("emulatorCandidates"), entry->emulatorCandidates},
        {QStringLiteral("automatic"), overrideId.isEmpty() || !entry->emulatorCandidates.contains(overrideId)},
        {QStringLiteral("emulators"), emulators},
        {QStringLiteral("profiles"), profiles},
        {QStringLiteral("favorite"), entry->favorite},
        {QStringLiteral("playCount"), entry->playCount},
        {QStringLiteral("lastPlayed"),
         entry->lastPlayed.isValid() ? entry->lastPlayed.toUTC().toString(Qt::ISODate) : QString()},
        {QStringLiteral("lastPlayedText"),
         entry->lastPlayed.isValid() ? entry->lastPlayed.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                     : QString()},
        {QStringLiteral("addedAt"),
         entry->addedAt.isValid() ? entry->addedAt.toUTC().toString(Qt::ISODate) : QString()},
        {QStringLiteral("present"), entry->present},
        {QStringLiteral("notes"), entry->notes},
        {QStringLiteral("playSeconds"), double(entry->playSeconds)},
        {QStringLiteral("playTimeText"), formatPlayTime(entry->playSeconds)},
        {QStringLiteral("pinned"), entry->pinned > 0},
    };
}

QString LibraryController::failureReason(qint64 id) const
{
    if (!m_library->entry(id))
        return QStringLiteral("the game is no longer in the library");
    const QString error = m_library->lastError();
    return error.isEmpty() ? QStringLiteral("the library database refused the change") : error;
}

QVariantList LibraryController::defaultEmulatorChoices(const QString &systemId) const
{
    if (systemId.isEmpty() || !m_catalog || !m_catalog->find(systemId))
        return {};
    QVariantList choices{
        QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QStringLiteral("Automatic")}}};
    for (const auto &config : m_app->enabledEmulators()) {
        if (config.systems.contains(systemId))
            choices.append(QVariantMap{{QStringLiteral("id"), config.id},
                                       {QStringLiteral("name"), config.name.isEmpty() ? config.id : config.name}});
    }
    return choices;
}

bool LibraryController::setDefaultEmulator(const QString &systemId, const QString &emulatorId)
{
    const QVariantList choices = defaultEmulatorChoices(systemId);
    const bool offered = std::any_of(choices.cbegin(), choices.cend(), [&emulatorId](const QVariant &choice) {
        return choice.toMap().value(QStringLiteral("id")).toString() == emulatorId;
    });
    if (!offered) {
        qCWarning(lcLibrary).noquote() << "Refused default emulator" << emulatorId << "for system" << systemId;
        return false;
    }
    QVariantMap defaults = m_settings->mapValue(Settings::EmulatorDefaultForSystem);
    if (emulatorId.isEmpty())
        defaults.remove(systemId);
    else
        defaults.insert(systemId, emulatorId);
    QString error;
    if (!m_settings->setValue(Settings::EmulatorDefaultForSystem, defaults, &error) || !m_settings->save()) {
        emit notice(error.isEmpty() ? QStringLiteral("Could not save the default emulator") : error, true);
        return false;
    }
    return true;
}

bool LibraryController::setGameMetadata(qint64 id, const QString &title, const QString &notes)
{
    if (m_library->setMetadata(id, title, notes))
        return true;
    emit notice(QStringLiteral("Could not save the game details: %1").arg(failureReason(id)), true);
    return false;
}

bool LibraryController::setGameSystem(qint64 id, const QString &systemId)
{
    if (m_library->setSystemOverride(id, systemId))
        return true;
    emit notice(QStringLiteral("Could not change the system: %1").arg(failureReason(id)), true);
    return false;
}

bool LibraryController::removeGame(qint64 id)
{
    if (m_library->removeFromLibrary(id))
        return true;
    emit notice(QStringLiteral("Could not remove the game: %1").arg(failureReason(id)), true);
    return false;
}

int LibraryController::restoreRemovedGames()
{
    const int count = int(m_library->hiddenEntries().size());
    return m_library->restoreHidden() ? count : 0;
}

void LibraryController::rebuildEmulatorInfo()
{
    m_emulatorInfo.clear();
    for (const auto &c : m_app->enabledEmulators())
        m_emulatorInfo.insert(c.id, {c.name, m_app->unavailableReason(c.id)});
}

void LibraryController::rebuildSystems()
{
    const QList<library::GameEntry> all = m_library->entries();
    QHash<QString, int> gameCounts;
    for (const auto &e : all)
        gameCounts[e.systemId] += 1;

    QHash<QString, int> emulatorCounts;
    for (const auto &c : m_app->enabledEmulators()) {
        for (const QString &systemId : c.systems)
            emulatorCounts[systemId] += 1;
    }

    QList<SystemListModel::Entry> entries;
    if (m_catalog) {
        for (const auto &system : m_catalog->systems()) {
            SystemListModel::Entry entry;
            entry.id = system.id;
            entry.name = system.name;
            entry.shortName = system.shortName;
            entry.gameCount = gameCounts.value(system.id);
            entry.emulatorCount = emulatorCounts.value(system.id);
            entries.append(entry);
        }
    }
    const int other = gameCounts.value(QString());
    if (other > 0) {
        SystemListModel::Entry entry;
        entry.id.clear();
        entry.name = QStringLiteral("Other");
        entry.shortName = QStringLiteral("Other");
        entry.gameCount = other;
        entry.emulatorCount = 0;
        entries.append(entry);
    }
    m_systems.setEntries(entries);
}

void LibraryController::rebuildViews()
{
    const QList<library::GameEntry> all = m_library->entries();
    const QList<library::GameEntry> gamesSource
        = m_query.filter == library::LibraryFilter::Missing ? m_library->missingEntries() : all;
    library::QueryOptions query = m_query;
    if (m_collectionFilter > 0) {
        const QList<qint64> ids = m_library->collectionGameIds(m_collectionFilter);
        query.gameIds = QSet<qint64>(ids.cbegin(), ids.cend());
    }
    m_games.setEntries(library::LibraryQuery::apply(gamesSource, query));

    library::QueryOptions recent;
    recent.filter = library::LibraryFilter::Recent;
    recent.recentLimit = 12;
    m_recent.setEntries(library::LibraryQuery::apply(all, recent));

    library::QueryOptions favorites;
    favorites.filter = library::LibraryFilter::Favorites;
    m_favorites.setEntries(library::LibraryQuery::apply(all, favorites));

    QList<library::GameEntry> recentlyAdded = all;
    std::sort(recentlyAdded.begin(), recentlyAdded.end(), [](const auto &a, const auto &b) {
        return a.addedAt != b.addedAt ? a.addedAt > b.addedAt : a.id > b.id;
    });
    if (recentlyAdded.size() > 20)
        recentlyAdded.resize(20);
    m_recentlyAdded.setEntries(recentlyAdded);

    QList<library::GameEntry> pinned;
    std::copy_if(all.cbegin(), all.cend(), std::back_inserter(pinned), [](const auto &e) { return e.pinned > 0; });
    std::sort(pinned.begin(), pinned.end(), [](const auto &a, const auto &b) { return a.pinned < b.pinned; });
    m_pinned.setEntries(pinned);

    for (auto it = m_collectionModels.begin(); it != m_collectionModels.end(); ++it)
        rebuildCollectionModel(it->first, it->second.get(), all);

    rebuildSystems();
    emit contentChanged();
}

// A play-count or play-time write can fail while the scan thread's merge holds the SQLite write
// lock; it is then tried once more when that scan has finished.
void LibraryController::writeRetryingAfterScan(const QString &what, std::function<bool()> write)
{
    if (write())
        return;
    qCWarning(lcUi).noquote() << "Could not record" << what << "-" << m_library->lastError();
    if (!m_library->isScanning())
        return;
    auto retry = std::make_shared<QMetaObject::Connection>();
    *retry = connect(m_library, &library::GameLibrary::scanningChanged, this,
                     [this, what, write = std::move(write), retry](bool scanning) {
                         if (scanning)
                             return;
                         disconnect(*retry);
                         if (!write())
                             qCWarning(lcUi).noquote() << "Could not record" << what << "-" << m_library->lastError();
                     });
}

void LibraryController::setupModel(GameListModel *model)
{
    model->setEmulatorInfo([this](const QString &emulatorId) -> std::pair<QString, QString> {
        const auto it = m_emulatorInfo.constFind(emulatorId);
        if (it != m_emulatorInfo.cend())
            return *it;
        return {QString(), m_app->unavailableReason(emulatorId)};
    });
    model->setSystemInfo([this](const QString &systemId) -> QString {
        if (systemId.isEmpty() || !m_catalog)
            return {};
        const auto *system = m_catalog->find(systemId);
        return system ? system->shortName : QString();
    });
    model->setSystemDefaultEmulators(m_query.systemDefaultEmulators);
}

void LibraryController::rebuildCollectionModel(qint64 id, GameListModel *model, const QList<library::GameEntry> &all)
{
    QHash<qint64, const library::GameEntry *> byId;
    for (const auto &e : all)
        byId.insert(e.id, &e);
    QList<library::GameEntry> games;
    for (qint64 gameId : m_library->collectionGameIds(id)) {
        if (const auto *e = byId.value(gameId))
            games.append(*e);
    }
    model->setEntries(games);
}

// --- Collections and pinning -------------------------------------------------------------

QVariantList LibraryController::collections() const
{
    QVariantList list;
    for (const auto &c : m_library->collections())
        list.append(QVariantMap{{QStringLiteral("id"), double(c.id)},
                                {QStringLiteral("name"), c.name},
                                {QStringLiteral("count"), c.count}});
    return list;
}

QString LibraryController::collectionsSignature() const
{
    QStringList parts;
    for (const auto &c : m_library->collections())
        parts.append(QString::number(c.id) + u':' + c.name);
    return parts.join(u'\n');
}

QString LibraryController::collectionName(qint64 collectionId) const
{
    for (const auto &c : m_library->collections()) {
        if (c.id == collectionId)
            return c.name;
    }
    return {};
}

void LibraryController::setCollectionFilter(qint64 collectionId)
{
    if (collectionId == m_collectionFilter)
        return;
    m_collectionFilter = collectionId;
    emit queryChanged();
    rebuildViews();
}

qint64 LibraryController::createCollection(const QString &name, qint64 gameId)
{
    const auto id = m_library->createCollection(name);
    if (!id) {
        emit notice(QStringLiteral("Could not create the collection: %1").arg(m_library->lastError()), true);
        return 0;
    }
    if (gameId > 0)
        setInCollection(gameId, *id, true);
    emit notice(QStringLiteral("Collection \"%1\" created").arg(collectionName(*id)), false);
    return *id;
}

bool LibraryController::renameCollection(qint64 id, const QString &name)
{
    if (!m_library->renameCollection(id, name)) {
        emit notice(QStringLiteral("Could not rename the collection: %1").arg(m_library->lastError()), true);
        return false;
    }
    return true;
}

bool LibraryController::deleteCollection(qint64 id)
{
    const QString name = collectionName(id);
    if (!m_library->deleteCollection(id)) {
        emit notice(QStringLiteral("Could not delete the collection: %1").arg(m_library->lastError()), true);
        return false;
    }
    m_collectionModels.erase(id);
    if (m_collectionFilter == id)
        setCollectionFilter(0);
    QStringList sections = m_settings->stringListValue(Settings::HomeSections);
    if (sections.removeAll(QStringLiteral("collection:%1").arg(id)) > 0)
        writeHomeSections(sections);
    emit notice(QStringLiteral("Collection \"%1\" deleted; its games stay in the library").arg(name), false);
    return true;
}

bool LibraryController::setInCollection(qint64 gameId, qint64 collectionId, bool member)
{
    if (!m_library->setInCollection(collectionId, gameId, member)) {
        emit notice(QStringLiteral("Could not change the collection: %1").arg(m_library->lastError()), true);
        return false;
    }
    return true;
}

QVariantList LibraryController::collectionsOf(qint64 gameId) const
{
    QVariantList ids;
    for (qint64 id : m_library->collectionsOf(gameId))
        ids.append(double(id));
    return ids;
}

GameListModel *LibraryController::collectionGames(qint64 collectionId)
{
    if (collectionName(collectionId).isEmpty())
        return nullptr;
    auto &slot = m_collectionModels[collectionId];
    if (!slot) {
        slot = std::make_unique<GameListModel>();
        QQmlEngine::setObjectOwnership(slot.get(), QQmlEngine::CppOwnership);
        setupModel(slot.get());
        rebuildCollectionModel(collectionId, slot.get(), m_library->entries());
    }
    return slot.get();
}

bool LibraryController::setPinned(qint64 gameId, bool pinned)
{
    if (!m_library->setPinned(gameId, pinned)) {
        emit notice(pinned ? QStringLiteral("Could not pin the game: %1").arg(m_library->lastError())
                           : QStringLiteral("Could not unpin the game: %1").arg(m_library->lastError()),
                    true);
        return false;
    }
    return true;
}

// --- Home layout ---------------------------------------------------------------------------

namespace {

const QStringList &builtInSections()
{
    static const QStringList ids{QStringLiteral("statusCards"), QStringLiteral("quickResume"),
                                 QStringLiteral("pinned"),      QStringLiteral("recent"),
                                 QStringLiteral("favorites"),   QStringLiteral("recentlyAdded"),
                                 QStringLiteral("systems")};
    return ids;
}

QString builtInLabel(const QString &id)
{
    static const QHash<QString, QString> labels{
        {QStringLiteral("statusCards"), QStringLiteral("Status cards")},
        {QStringLiteral("quickResume"), QStringLiteral("Quick Resume")},
        {QStringLiteral("pinned"), QStringLiteral("Pinned")},
        {QStringLiteral("recent"), QStringLiteral("Continue playing")},
        {QStringLiteral("favorites"), QStringLiteral("Favorites")},
        {QStringLiteral("recentlyAdded"), QStringLiteral("Recently added")},
        {QStringLiteral("systems"), QStringLiteral("Systems")},
    };
    return labels.value(id);
}

} // namespace

QStringList LibraryController::homeSections() const
{
    QSet<qint64> existing;
    for (const auto &c : m_library->collections())
        existing.insert(c.id);
    QStringList visible;
    for (const QString &id : m_settings->stringListValue(Settings::HomeSections)) {
        if (id.startsWith(QLatin1String("collection:")) && !existing.contains(id.mid(11).toLongLong()))
            continue; // deleted collection
        visible.append(id);
    }
    return visible;
}

QVariantList LibraryController::homeSectionRows() const
{
    const QStringList visible = homeSections();
    QVariantList rows;
    QStringList known = builtInSections();
    const auto all = m_library->collections();
    for (const auto &c : all)
        known.append(QStringLiteral("collection:%1").arg(c.id));
    auto label = [&](const QString &id) {
        if (!id.startsWith(QLatin1String("collection:")))
            return builtInLabel(id);
        const qint64 cid = id.mid(11).toLongLong();
        for (const auto &c : all) {
            if (c.id == cid)
                return QStringLiteral("Collection: %1").arg(c.name);
        }
        return id;
    };
    for (const QString &id : visible)
        rows.append(QVariantMap{
            {QStringLiteral("id"), id}, {QStringLiteral("label"), label(id)}, {QStringLiteral("visible"), true}});
    for (const QString &id : std::as_const(known)) {
        if (!visible.contains(id))
            rows.append(QVariantMap{
                {QStringLiteral("id"), id}, {QStringLiteral("label"), label(id)}, {QStringLiteral("visible"), false}});
    }
    return rows;
}

bool LibraryController::writeHomeSections(const QStringList &sections)
{
    QString error;
    if (!m_settings->setValue(Settings::HomeSections, sections, &error)) {
        emit notice(QStringLiteral("Could not change the Home layout: %1").arg(error), true);
        return false;
    }
    if (!m_settings->save()) {
        // The layout applies now but is not kept over a restart.
        emit notice(QStringLiteral("Could not save the Home layout to %1").arg(m_settings->paths().userFile), true);
        return false;
    }
    return true;
}

bool LibraryController::setHomeSectionVisible(const QString &id, bool visible)
{
    QStringList sections = homeSections();
    if (visible == sections.contains(id))
        return true;
    if (visible)
        sections.append(id);
    else
        sections.removeAll(id);
    return writeHomeSections(sections);
}

bool LibraryController::moveHomeSection(const QString &id, int delta)
{
    QStringList sections = homeSections();
    const qsizetype from = sections.indexOf(id);
    const qsizetype to = from + delta;
    if (from < 0 || to < 0 || to >= sections.size())
        return false;
    sections.move(from, to);
    return writeHomeSections(sections);
}

void LibraryController::resetHomeLayout()
{
    m_settings->setValue(Settings::HomeSections, Settings::spec(Settings::HomeSections)->defaultValue);
    if (!m_settings->save())
        emit notice(QStringLiteral("Could not save the Home layout to %1").arg(m_settings->paths().userFile), true);
}

QString LibraryController::formatPlayTime(qint64 seconds)
{
    if (seconds < 60)
        return seconds <= 0 ? QStringLiteral("Not played yet") : QStringLiteral("Less than a minute");
    const qint64 minutes = seconds / 60;
    if (minutes < 60)
        return QStringLiteral("%1 min").arg(minutes);
    return QStringLiteral("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

} // namespace nexora::ui
