#include "nexora/library/GameLibrary.h"

#include "nexora/shared/Logging.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QThread>

namespace nexora::library {

namespace {
const QString InMemory = QStringLiteral(":memory:");
} // namespace

// Produced on the worker thread, consumed on the owner's thread after the thread finished.
struct GameLibrary::ScanJob
{
    ScanResult scan;
    bool merged = false; // the worker attempted the merge
    std::optional<ApplyStats> stats; // set when the worker's merge succeeded
    QList<GameEntry> entries; // database content after the merge
    int missingCount = 0;
    QString error;
};

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
    m_databasePath = databasePath;
    reloadEntries();
    return true;
}

void GameLibrary::setScanOptions(const ScanOptions &options)
{
    m_options = options;
}

QString GameLibrary::optionsSignature() const
{
    QStringList parts = m_options.roots;
    parts.sort();
    QStringList exts;
    for (auto it = m_options.extensionMap.cbegin(); it != m_options.extensionMap.cend(); ++it)
        exts.append(it.key() + u'=' + it.value().join(u'|'));
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
    auto job = std::make_shared<ScanJob>();
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const ScanOptions options = m_options;
    const auto cancel = m_cancel;
    // Everything the worker needs is copied here; it never touches *this.
    const QString databasePath = m_db.isOpen() && m_databasePath != InMemory ? m_databasePath : QString();
    const QString signature = optionsSignature();
    const quint64 generation = m_writeGeneration;
    QThread *thread = QThread::create([options, job, cancel, databasePath, signature] {
        job->scan = LibraryScanner::scan(options, cancel.get());
        if (databasePath.isEmpty() || job->scan.cancelled)
            return;
        QElapsedTimer timer;
        timer.start();
        job->merged = true;
        GameDatabase db; // a separate connection, owned by this thread
        if (!db.open(databasePath, /*allowRecovery=*/false)) {
            job->error = db.lastError();
            return;
        }
        job->stats = db.applyScan(job->scan, options.roots);
        if (!job->stats) {
            job->error = db.lastError();
            return;
        }
        db.setMeta(QStringLiteral("scan-signature"), signature);
        db.setMeta(QStringLiteral("last-scan"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        job->entries = db.entries();
        job->missingCount = db.missingCount();
        qCDebug(lcLibrary) << "Merged" << job->scan.files.size() << "scanned file(s) on the scan thread in"
                           << timer.elapsed() << "ms";
    });
    thread->setObjectName(QStringLiteral("nexora-library-scan"));
    connect(thread, &QThread::finished, this, [this, thread, job, generation] {
        if (m_thread == thread)
            m_thread = nullptr;
        thread->deleteLater();
        onScanDone(*job, generation);
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

void GameLibrary::onScanDone(ScanJob &job, quint64 writeGeneration)
{
    m_lastScan = std::move(job.scan);
    const bool cancelled = m_lastScan.cancelled;
    bool stored = false;
    QString error;
    if (job.merged) {
        if (job.stats) {
            stored = true;
            m_lastStats = *job.stats;
            if (writeGeneration == m_writeGeneration) {
                m_entries = std::move(job.entries);
                m_missingCount = job.missingCount;
                emit entriesChanged();
            } else {
                reloadEntries(); // a favorite/launch was written meanwhile; the snapshot may lack it
            }
        } else {
            error = job.error;
            reloadEntries();
        }
    } else if (!cancelled) {
        // In-memory database (tests): merge here.
        if (const auto stats = m_db.applyScan(m_lastScan, m_options.roots)) {
            stored = true;
            m_lastStats = *stats;
            m_db.setMeta(QStringLiteral("scan-signature"), optionsSignature());
            m_db.setMeta(QStringLiteral("last-scan"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        } else {
            error = m_db.lastError();
        }
        reloadEntries();
    }
    emit scanningChanged(false);
    if (stored)
        emit scanFinished(m_lastStats, int(m_lastScan.issues.size()));
    else if (!cancelled)
        emit scanFailed(error);
    if (m_rerunQueued) {
        m_rerunQueued = false;
        refresh();
    }
}

void GameLibrary::reloadEntries()
{
    m_missingCount = m_db.missingCount();
    m_entries = m_db.entries();
    emit entriesChanged();
}

std::optional<GameEntry> GameLibrary::entry(qint64 id) const
{
    return m_db.entry(id);
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
    ++m_writeGeneration;
    if (!m_db.setFavorite(id, favorite))
        return false;
    updateCached(id, [favorite](GameEntry &e) { e.favorite = favorite; });
    return true;
}

bool GameLibrary::recordLaunch(qint64 id)
{
    ++m_writeGeneration;
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
    ++m_writeGeneration;
    if (!m_db.setEmulatorOverride(id, emulatorId))
        return false;
    updateCached(id, [&emulatorId](GameEntry &e) { e.emulatorOverride = emulatorId; });
    return true;
}

int GameLibrary::purgeMissing()
{
    ++m_writeGeneration;
    const int removed = m_db.purgeMissing();
    reloadEntries();
    return removed;
}

} // namespace nexora::library
