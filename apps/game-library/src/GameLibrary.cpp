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
    auto result = std::make_shared<ScanResult>();
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const ScanOptions options = m_options;
    const auto cancel = m_cancel;
    QThread *thread
        = QThread::create([options, result, cancel] { *result = LibraryScanner::scan(options, cancel.get()); });
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

void GameLibrary::onScanDone(const ScanResult &result)
{
    m_lastScan = result;
    if (!result.cancelled) {
        if (const auto stats = m_db.applyScan(result, m_options.roots)) {
            m_lastStats = *stats;
            m_db.setMeta(QStringLiteral("scan-signature"), optionsSignature());
            m_db.setMeta(QStringLiteral("last-scan"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        }
        reloadEntries();
    }
    emit scanningChanged(false);
    emit scanFinished(m_lastStats, int(result.issues.size()));
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

bool GameLibrary::setFavorite(qint64 id, bool favorite)
{
    if (!m_db.setFavorite(id, favorite))
        return false;
    reloadEntries();
    return true;
}

bool GameLibrary::recordLaunch(qint64 id)
{
    if (!m_db.recordLaunch(id))
        return false;
    reloadEntries();
    return true;
}

bool GameLibrary::setEmulatorOverride(qint64 id, const QString &emulatorId)
{
    if (!m_db.setEmulatorOverride(id, emulatorId))
        return false;
    reloadEntries();
    return true;
}

int GameLibrary::purgeMissing()
{
    const int removed = m_db.purgeMissing();
    reloadEntries();
    return removed;
}

} // namespace nexora::library
