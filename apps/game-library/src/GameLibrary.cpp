#include "nexora/library/GameLibrary.h"

#include "nexora/shared/Logging.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QThread>

namespace nexora::library {

GameLibrary::GameLibrary(QObject *parent)
    : QObject(parent)
{ }

GameLibrary::~GameLibrary()
{
    if (m_thread) {
        if (m_cancel)
            m_cancel->store(true);
        m_thread->wait();
        delete m_thread; // finished() handler never ran; avoid touching *this from it
    }
}

QString GameLibrary::defaultDatabasePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    return base + QStringLiteral("/nexora/library.sqlite");
}

bool GameLibrary::open(const QString &databasePath)
{
    if (!m_db.open(databasePath))
        return false;
    reloadEntries();
    return true;
}

void GameLibrary::close()
{
    m_db.close();
}

void GameLibrary::setCatalog(const shared::SystemCatalog &catalog)
{
    m_catalog = catalog;
}

ScanOptions GameLibrary::effectiveOptions() const
{
    ScanOptions options = m_options;
    options.extensionSystems.clear();
    for (auto it = options.extensionMap.cbegin(); it != options.extensionMap.cend(); ++it) {
        const QStringList systems = m_catalog.systemsForExtension(it.key());
        if (!systems.isEmpty())
            options.extensionSystems.insert(it.key(), systems);
    }
    return options;
}

void GameLibrary::setScanOptions(const ScanOptions &options)
{
    m_options = options;
}

QString GameLibrary::optionsSignature() const
{
    const ScanOptions options = effectiveOptions();
    QStringList parts = options.roots;
    parts.sort();
    QStringList exts;
    for (auto it = options.extensionMap.cbegin(); it != options.extensionMap.cend(); ++it)
        exts.append(it.key() + u'=' + it.value().join(u'|'));
    for (auto it = options.extensionSystems.cbegin(); it != options.extensionSystems.cend(); ++it)
        exts.append(QStringLiteral("system:") + it.key() + u'=' + it.value().join(u'|'));
    exts.sort();
    parts += exts;
    parts.append(m_options.followSymlinks ? QStringLiteral("follow") : QStringLiteral("nofollow"));
    return QString::fromLatin1(
        QCryptographicHash::hash(parts.join(u'\n').toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool GameLibrary::needsRescan() const
{
    return m_db.isOpen() && m_db.meta(QStringLiteral("scan-signature")) != optionsSignature();
}

bool GameLibrary::refresh()
{
    if (m_thread) {
        m_rerunQueued = true;
        return false;
    }
    auto result = std::make_shared<ScanResult>();
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const ScanOptions options = effectiveOptions();
    const auto cancel = m_cancel;
    QThread *thread = QThread::create([this, options, result, cancel] {
        // Queued to the owner's thread; dropped if the library is destroyed meanwhile.
        const ScanProgress progress = [this](int filesSeen, const QString &currentRoot) {
            QMetaObject::invokeMethod(
                this, [this, filesSeen, currentRoot] { emit scanProgress(filesSeen, currentRoot); },
                Qt::QueuedConnection);
        };
        *result = LibraryScanner::scan(options, cancel.get(), progress);
    });
    thread->setObjectName(QStringLiteral("nexora-library-scan"));
    connect(thread, &QThread::finished, this, [this, thread, result] {
        if (m_thread == thread)
            m_thread = nullptr;
        thread->deleteLater();
        onScanDone(*result);
    });
    m_thread = thread;
    emit scanningChanged(true);
    thread->start(QThread::LowPriority);
    return true;
}

void GameLibrary::waitForScan()
{
    while (m_thread) {
        m_thread->wait();
        QCoreApplication::sendPostedEvents(); // deliver finished() to onScanDone
        QCoreApplication::processEvents();
    }
}

void GameLibrary::cancelScan()
{
    m_rerunQueued = false;
    if (m_cancel)
        m_cancel->store(true);
}

void GameLibrary::onScanDone(const ScanResult &result)
{
    m_lastScan = result;
    bool stored = false;
    if (!result.cancelled) {
        if (const auto stats = m_db.applyScan(result, m_options.roots)) {
            stored = true;
            m_lastStats = *stats;
            m_db.setMeta(QStringLiteral("scan-signature"), optionsSignature());
            m_db.setMeta(QStringLiteral("last-scan"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        }
        reloadEntries();
    }
    emit scanningChanged(false);
    if (stored)
        emit scanFinished(m_lastStats, int(result.issues.size()));
    else if (!result.cancelled)
        emit scanFailed(m_db.lastError());
    if (m_rerunQueued) {
        m_rerunQueued = false;
        refresh();
    }
}

void GameLibrary::reloadEntries()
{
    m_entries = m_db.entries();
    emit entriesChanged();
}

std::optional<GameEntry> GameLibrary::entry(qint64 id) const
{
    return m_db.entry(id);
}

QList<GameEntry> GameLibrary::missingEntries() const
{
    QList<GameEntry> missing;
    for (const GameEntry &e : m_db.entries(true, false)) {
        if (!e.present)
            missing.append(e);
    }
    return missing;
}

template<typename F> void GameLibrary::updateCached(qint64 id, F update)
{
    for (GameEntry &e : m_entries) {
        if (e.id == id) {
            update(e);
            break;
        }
    }
    emit entriesChanged();
}

bool GameLibrary::setFavorite(qint64 id, bool favorite)
{
    if (!m_db.setFavorite(id, favorite))
        return false;
    updateCached(id, [favorite](GameEntry &e) { e.favorite = favorite; });
    return true;
}

bool GameLibrary::recordLaunch(qint64 id)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (!m_db.recordLaunch(id, now))
        return false;
    // Same second resolution as the database, so cached and reloaded entries agree.
    const QDateTime stored = QDateTime::fromSecsSinceEpoch(now.toSecsSinceEpoch(), Qt::UTC);
    updateCached(id, [stored](GameEntry &e) {
        ++e.playCount;
        e.lastPlayed = stored;
    });
    return true;
}

bool GameLibrary::setEmulatorOverride(qint64 id, const QString &emulatorId)
{
    if (!m_db.setEmulatorOverride(id, emulatorId))
        return false;
    updateCached(id, [&emulatorId](GameEntry &e) { e.emulatorOverride = emulatorId; });
    return true;
}

void GameLibrary::refreshCached(qint64 id)
{
    const auto fresh = m_db.entry(id);
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id != id)
            continue;
        if (fresh && fresh->present && !fresh->hidden)
            m_entries[i] = *fresh;
        else
            m_entries.removeAt(i);
        break;
    }
    emit entriesChanged();
}

bool GameLibrary::setMetadata(qint64 id, const QString &title, const QString &notes)
{
    if (!m_db.setMetadata(id, title, notes))
        return false;
    refreshCached(id);
    return true;
}

bool GameLibrary::setSystemOverride(qint64 id, const QString &systemId)
{
    if (!systemId.isEmpty()) {
        const auto current = m_db.entry(id);
        if (!current)
            return false;
        if (!current->systemCandidates.contains(systemId) && !m_catalog.find(systemId))
            return false;
    }
    if (!m_db.setSystemOverride(id, systemId))
        return false;
    refreshCached(id);
    return true;
}

bool GameLibrary::removeFromLibrary(qint64 id)
{
    if (!m_db.setHidden(id, true))
        return false;
    refreshCached(id);
    return true;
}

bool GameLibrary::restoreHidden()
{
    if (m_db.restoreHidden() == 0)
        return false;
    reloadEntries();
    return true;
}

QList<GameEntry> GameLibrary::hiddenEntries() const
{
    QList<GameEntry> hidden;
    for (const GameEntry &e : m_db.entries(true, true)) {
        if (e.hidden)
            hidden.append(e);
    }
    return hidden;
}

int GameLibrary::purgeMissing()
{
    const int removed = m_db.purgeMissing();
    reloadEntries();
    return removed;
}

} // namespace nexora::library
