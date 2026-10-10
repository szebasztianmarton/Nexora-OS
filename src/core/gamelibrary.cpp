#include "gamelibrary.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

#include "appsettings.h"
#include "emulatorregistry.h"

namespace nexora {

namespace {

constexpr int MaxRecent = 20;

QString toIso(const QDateTime &dt)
{
    return dt.toUTC().toString(Qt::ISODateWithMs);
}

QDateTime fromIso(const QJsonValue &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

void scanFolder(const QString &folder, const QString &root, int depth, int maxDepth,
                const QSet<QString> &extensions, const QSet<QString> &hidden, QSet<QString> &seen,
                ScanResult &result)
{
    const QDir dir(folder);
    // Hidden entries are excluded because QDir::Hidden is not requested.
    const QFileInfoList infos = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &info : infos) {
        if (info.isDir()) {
            if (info.isSymLink() || depth >= maxDepth)
                continue;
            if (!info.isReadable() || !info.isExecutable()) {
                result.skipped.append(QStringLiteral("%1: permission denied").arg(info.absoluteFilePath()));
                continue;
            }
            scanFolder(info.absoluteFilePath(), root, depth + 1, maxDepth, extensions, hidden, seen, result);
            continue;
        }
        const QString extension = info.suffix().toLower();
        if (extension.isEmpty() || !extensions.contains(extension))
            continue;
        const QString path = info.absoluteFilePath();
        if (hidden.contains(path) || seen.contains(path))
            continue;
        seen.insert(path);
        GameEntry entry;
        entry.path = path;
        entry.fileName = info.fileName();
        entry.title = GameLibrary::titleFromFileName(entry.fileName);
        entry.extension = extension;
        entry.directory = root;
        entry.fileSize = info.size();
        entry.artworkPath = GameLibrary::findArtwork(path);
        result.entries.append(entry);
    }
}

} // namespace

GameLibrary::GameLibrary(const QString &configDirectory, AppSettings *settings, EmulatorRegistry *registry,
                         QObject *parent)
    : QObject(parent)
    , m_model(new GameModel(this))
    , m_settings(settings)
    , m_registry(registry)
    , m_store(QDir(configDirectory).filePath(QStringLiteral("library-state.json")))
{
    loadState();
    connect(&m_watcher, &QFutureWatcher<ScanResult>::finished, this, &GameLibrary::onScanDone);
    if (m_registry) {
        connect(m_registry, &EmulatorRegistry::reloaded, this, &GameLibrary::reresolveEmulators);
        connect(m_registry, &EmulatorRegistry::statusChanged, this, &GameLibrary::reresolveEmulators);
    }
    refreshEmulatorInfo();
}

GameLibrary::~GameLibrary()
{
    m_watcher.disconnect(this);
    m_watcher.waitForFinished();
}

bool GameLibrary::scanning() const { return m_scanning; }
bool GameLibrary::hasScanned() const { return m_hasScanned; }
QDateTime GameLibrary::lastScanTime() const { return m_lastScan; }
QStringList GameLibrary::skippedEntries() const { return m_skipped; }
int GameLibrary::hiddenCount() const { return int(m_hidden.size()); }
int GameLibrary::favoriteCount() const { return int(m_favorites.size()); }
int GameLibrary::recentCount() const { return int(m_recent.size()); }
QString GameLibrary::stateError() const { return m_stateError; }

void GameLibrary::refresh()
{
    if (m_scanning) {
        m_rescanQueued = true;
        return;
    }
    const QStringList directories = m_settings ? m_settings->libraryDirectories() : QStringList();
    const QStringList extensions = m_registry ? m_registry->supportedExtensions() : QStringList();
    const QStringList hidden(m_hidden.cbegin(), m_hidden.cend());
    m_scanning = true;
    emit scanningChanged();
    qInfo("Library: scanning %lld director%s", qint64(directories.size()), directories.size() == 1 ? "y" : "ies");
    m_watcher.setFuture(QtConcurrent::run(&GameLibrary::scanDirectories, directories, extensions, hidden, 4));
}

void GameLibrary::onScanDone()
{
    ScanResult result = m_watcher.result();
    const QDateTime now = QDateTime::currentDateTime();
    bool firstSeenChanged = false;
    for (GameEntry &entry : result.entries) {
        if (!m_firstSeen.contains(entry.path)) {
            m_firstSeen.insert(entry.path, now);
            firstSeenChanged = true;
        }
        applyState(entry);
    }
    m_model->resetEntries(result.entries);
    m_skipped = result.skipped;
    m_lastScan = now;
    m_hasScanned = true;
    m_scanning = false;
    if (firstSeenChanged)
        saveState();
    updateGameCounts();
    qInfo("Library: scan finished, %lld games, %lld skipped", qint64(result.entries.size()),
          qint64(result.skipped.size()));
    for (const QString &skipped : std::as_const(m_skipped))
        qWarning("Library: skipped %s", qPrintable(skipped));

    emit scanFinished();
    emit scanningChanged();
    if (m_rescanQueued) {
        m_rescanQueued = false;
        refresh();
    }
}

ScanResult GameLibrary::scanDirectories(const QStringList &directories, const QStringList &extensions,
                                        const QStringList &hiddenPaths, int maxDepth)
{
    ScanResult result;
    QSet<QString> extensionSet;
    for (const QString &ext : extensions)
        extensionSet.insert(ext.toLower());
    const QSet<QString> hidden(hiddenPaths.cbegin(), hiddenPaths.cend());
    QSet<QString> seen;
    if (extensionSet.isEmpty())
        return result;

    for (const QString &directory : directories) {
        const QString root = QDir::cleanPath(directory);
        const QFileInfo info(root);
        if (!info.exists())
            result.skipped.append(QStringLiteral("%1: directory not found").arg(root));
        else if (!info.isDir())
            result.skipped.append(QStringLiteral("%1: not a directory").arg(root));
        else if (!info.isReadable() || !info.isExecutable())
            result.skipped.append(QStringLiteral("%1: permission denied").arg(root));
        else
            scanFolder(root, root, 0, maxDepth, extensionSet, hidden, seen, result);
    }
    std::sort(result.entries.begin(), result.entries.end(),
              [](const GameEntry &a, const GameEntry &b) { return a.path < b.path; });
    return result;
}

QString GameLibrary::titleFromFileName(const QString &fileName)
{
    QString title = fileName;
    const qsizetype dot = title.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        title.truncate(dot);
    title.replace(QLatin1Char('_'), QLatin1Char(' '));
    title = title.simplified();
    return title.isEmpty() ? fileName : title;
}

QString GameLibrary::findArtwork(const QString &gamePath)
{
    static const QStringList imageExtensions{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                             QStringLiteral("webp")};
    const QFileInfo game(gamePath);
    const QString base = game.completeBaseName();
    const QString folder = game.absolutePath();
    const QStringList candidates{folder, folder + QStringLiteral("/covers"), folder + QStringLiteral("/artwork")};
    for (const QString &dir : candidates) {
        for (const QString &ext : imageExtensions) {
            for (const QString &suffix : {ext, ext.toUpper()}) {
                const QString path = QStringLiteral("%1/%2.%3").arg(dir, base, suffix);
                if (QFileInfo(path).isFile())
                    return path;
            }
        }
    }
    return {};
}

QString GameLibrary::resolveEmulator(const GameEntry &entry, bool *overridden) const
{
    *overridden = false;
    if (!m_registry)
        return {};
    const QString overrideId = m_overrides.value(entry.path);
    if (!overrideId.isEmpty() && m_registry->find(overrideId)) {
        *overridden = true;
        return overrideId;
    }
    return m_registry->emulatorForExtension(entry.extension);
}

void GameLibrary::applyState(GameEntry &entry) const
{
    entry.favorite = m_favorites.contains(entry.path);
    entry.lastPlayed = QDateTime();
    for (const auto &recent : m_recent) {
        if (recent.first == entry.path) {
            entry.lastPlayed = recent.second;
            break;
        }
    }
    entry.firstSeen = m_firstSeen.value(entry.path);
    entry.emulatorId = resolveEmulator(entry, &entry.emulatorOverridden);
}

void GameLibrary::setFavorite(const QString &path, bool favorite)
{
    if (path.isEmpty() || m_favorites.contains(path) == favorite)
        return;
    if (favorite)
        m_favorites.insert(path);
    else
        m_favorites.remove(path);
    if (const GameEntry *existing = m_model->entry(path)) {
        GameEntry updated = *existing;
        updated.favorite = favorite;
        m_model->updateEntry(updated);
    }
    saveState();
    emit favoriteChanged(path, favorite);
    emit stateChanged();
}

bool GameLibrary::toggleFavorite(const QString &path)
{
    setFavorite(path, !m_favorites.contains(path));
    return m_favorites.contains(path);
}

void GameLibrary::setEmulatorOverride(const QString &path, const QString &emulatorId)
{
    if (path.isEmpty())
        return;
    if (!emulatorId.isEmpty() && (!m_registry || !m_registry->find(emulatorId))) {
        qWarning("Library: cannot assign unknown emulator \"%s\"", qPrintable(emulatorId));
        return;
    }
    if (m_overrides.value(path) == emulatorId)
        return;
    if (emulatorId.isEmpty())
        m_overrides.remove(path);
    else
        m_overrides.insert(path, emulatorId);
    if (const GameEntry *existing = m_model->entry(path)) {
        GameEntry updated = *existing;
        updated.emulatorId = resolveEmulator(updated, &updated.emulatorOverridden);
        m_model->updateEntry(updated);
    }
    saveState();
    updateGameCounts();
    emit stateChanged();
}

void GameLibrary::removeFromLibrary(const QString &path)
{
    if (path.isEmpty() || m_hidden.contains(path))
        return;
    m_hidden.insert(path);
    m_model->removeEntry(path);
    saveState();
    updateGameCounts();
    emit stateChanged();
}

void GameLibrary::restoreHiddenEntries()
{
    if (!m_hidden.isEmpty()) {
        m_hidden.clear();
        saveState();
        emit stateChanged();
    }
    refresh();
}

QVariantMap GameLibrary::get(const QString &path) const
{
    const GameEntry *entry = m_model->entry(path);
    return entry ? m_model->toVariantMap(*entry) : QVariantMap();
}

void GameLibrary::recordLaunch(const QString &path)
{
    if (path.isEmpty())
        return;
    const QDateTime now = QDateTime::currentDateTime();
    QSet<QString> affected{path};
    m_recent.removeIf([&](const QPair<QString, QDateTime> &r) { return r.first == path; });
    m_recent.prepend({path, now});
    while (m_recent.size() > MaxRecent)
        affected.insert(m_recent.takeLast().first);
    for (const QString &p : std::as_const(affected)) {
        if (const GameEntry *existing = m_model->entry(p)) {
            GameEntry updated = *existing;
            applyState(updated);
            m_model->updateEntry(updated);
        }
    }
    saveState();
    emit stateChanged();
}

QString GameLibrary::mostRecentPath() const
{
    for (const auto &recent : m_recent) {
        if (m_model->indexOfPath(recent.first) >= 0)
            return recent.first;
    }
    return {};
}

void GameLibrary::loadState()
{
    const QJsonObject obj = m_store.load(&m_stateError);
    if (!m_stateError.isEmpty())
        qWarning("Library: %s", qPrintable(m_stateError));

    for (const QJsonValue &v : obj.value(QLatin1String("favorites")).toArray()) {
        if (v.isString() && !v.toString().isEmpty())
            m_favorites.insert(v.toString());
    }
    for (const QJsonValue &v : obj.value(QLatin1String("recent")).toArray()) {
        const QJsonObject r = v.toObject();
        const QString path = r.value(QLatin1String("path")).toString();
        const QDateTime played = fromIso(r.value(QLatin1String("lastPlayed")));
        const bool duplicate = std::any_of(m_recent.cbegin(), m_recent.cend(),
                                           [&](const QPair<QString, QDateTime> &e) { return e.first == path; });
        if (!path.isEmpty() && played.isValid() && !duplicate && m_recent.size() < MaxRecent)
            m_recent.append({path, played});
    }
    const QJsonObject overrides = obj.value(QLatin1String("overrides")).toObject();
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        if (it.value().isString() && !it.value().toString().isEmpty())
            m_overrides.insert(it.key(), it.value().toString());
    }
    for (const QJsonValue &v : obj.value(QLatin1String("hidden")).toArray()) {
        if (v.isString() && !v.toString().isEmpty())
            m_hidden.insert(v.toString());
    }
    const QJsonObject firstSeen = obj.value(QLatin1String("firstSeen")).toObject();
    for (auto it = firstSeen.constBegin(); it != firstSeen.constEnd(); ++it) {
        const QDateTime dt = fromIso(it.value());
        if (dt.isValid())
            m_firstSeen.insert(it.key(), dt);
    }
}

void GameLibrary::saveState()
{
    QStringList favorites(m_favorites.cbegin(), m_favorites.cend());
    favorites.sort();
    QStringList hidden(m_hidden.cbegin(), m_hidden.cend());
    hidden.sort();
    QJsonArray recent;
    for (const auto &r : std::as_const(m_recent))
        recent.append(QJsonObject{{QStringLiteral("path"), r.first}, {QStringLiteral("lastPlayed"), toIso(r.second)}});
    QJsonObject overrides;
    for (auto it = m_overrides.cbegin(); it != m_overrides.cend(); ++it)
        overrides.insert(it.key(), it.value());
    QJsonObject firstSeen;
    for (auto it = m_firstSeen.cbegin(); it != m_firstSeen.cend(); ++it)
        firstSeen.insert(it.key(), toIso(it.value()));

    const QJsonObject obj{{QStringLiteral("version"), 1},
                          {QStringLiteral("favorites"), QJsonArray::fromStringList(favorites)},
                          {QStringLiteral("recent"), recent},
                          {QStringLiteral("overrides"), overrides},
                          {QStringLiteral("hidden"), QJsonArray::fromStringList(hidden)},
                          {QStringLiteral("firstSeen"), firstSeen}};
    QString error;
    if (!m_store.save(obj, &error)) {
        qWarning("Library: %s", qPrintable(error));
        m_stateError = error;
        emit stateChanged();
    }
}

void GameLibrary::refreshEmulatorInfo()
{
    QHash<QString, QPair<QString, QString>> info;
    if (m_registry) {
        for (const EmulatorDefinition &def : m_registry->definitions())
            info.insert(def.id, {def.name, def.platforms.value(0)});
    }
    m_model->setEmulatorInfo(info);
}

void GameLibrary::reresolveEmulators()
{
    refreshEmulatorInfo();
    const QList<GameEntry> entries = m_model->entries(); // copy: updateEntry() mutates the model
    for (const GameEntry &entry : entries) {
        bool overridden = false;
        const QString id = resolveEmulator(entry, &overridden);
        if (id != entry.emulatorId || overridden != entry.emulatorOverridden) {
            GameEntry updated = entry;
            updated.emulatorId = id;
            updated.emulatorOverridden = overridden;
            m_model->updateEntry(updated);
        }
    }
    updateGameCounts();
}

void GameLibrary::updateGameCounts()
{
    if (!m_registry)
        return;
    QHash<QString, int> counts;
    for (const GameEntry &entry : m_model->entries()) {
        if (!entry.emulatorId.isEmpty())
            ++counts[entry.emulatorId];
    }
    m_registry->setGameCounts(counts);
}

} // namespace nexora
