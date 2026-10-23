#include "GameSessionManager.h"

#include "AppController.h"

#include "nexora/shared/AtomicFile.h"
#include "nexora/shared/Logging.h"
#include "nexora/shared/Settings.h"

#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QUuid>

#include <QRegularExpression>

#include <algorithm>
#include <numeric>

namespace nexora::ui {

using emulator::ProcessState;
using shared::Settings;
using Category = NotificationService::Category;
using Severity = NotificationService::Severity;

namespace {

const QString MetaFile = QStringLiteral("meta.json");

QString iso(const QDateTime &t)
{
    return t.isValid() ? t.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime fromIso(const QJsonValue &v)
{
    QDateTime t = QDateTime::fromString(v.toString(), Qt::ISODateWithMs);
    if (t.isValid())
        t = t.toUTC();
    return t;
}

QJsonObject toJson(const GameSessionManager::Record &r)
{
    QJsonArray states;
    for (const auto &s : r.states) {
        states.append(QJsonObject{{QStringLiteral("file"), s.file},
                                  {QStringLiteral("bytes"), s.bytes},
                                  {QStringLiteral("sha256"), s.sha256},
                                  {QStringLiteral("savedAt"), iso(s.savedAt)}});
    }
    return QJsonObject{
        {QStringLiteral("version"), GameSessionManager::MetaVersion},
        {QStringLiteral("id"), r.id},
        {QStringLiteral("gameId"), QString::number(r.gameId)},
        {QStringLiteral("gameTitle"), r.gameTitle},
        {QStringLiteral("gamePath"), r.gamePath},
        {QStringLiteral("gameSize"), r.gameSize},
        {QStringLiteral("gameModified"), iso(r.gameModified)},
        {QStringLiteral("emulatorId"), r.emulatorId},
        {QStringLiteral("emulatorVersion"), r.emulatorVersion},
        {QStringLiteral("profileId"), r.profileId},
        {QStringLiteral("adapter"), r.adapter},
        {QStringLiteral("createdAt"), iso(r.createdAt)},
        {QStringLiteral("note"), r.note},
        {QStringLiteral("states"), states},
    };
}

// Strict: anything unexpected makes the whole file unreadable (it is then moved aside).
std::optional<GameSessionManager::Record> fromJson(const QJsonObject &o, const QString &dirName)
{
    static const QRegularExpression fileName(QStringLiteral("^[A-Za-z0-9._-]{1,128}$"));
    if (o.value(QStringLiteral("version")).toInt() != GameSessionManager::MetaVersion)
        return std::nullopt;
    GameSessionManager::Record r;
    r.id = o.value(QStringLiteral("id")).toString();
    if (r.id != dirName)
        return std::nullopt;
    bool ok = false;
    r.gameId = o.value(QStringLiteral("gameId")).toString().toLongLong(&ok);
    if (!ok || r.gameId <= 0)
        return std::nullopt;
    r.gameTitle = o.value(QStringLiteral("gameTitle")).toString().left(500);
    r.gamePath = o.value(QStringLiteral("gamePath")).toString();
    r.gameSize = qint64(o.value(QStringLiteral("gameSize")).toDouble(-1));
    r.gameModified = fromIso(o.value(QStringLiteral("gameModified")));
    r.emulatorId = o.value(QStringLiteral("emulatorId")).toString();
    r.emulatorVersion = o.value(QStringLiteral("emulatorVersion")).toString().left(64);
    r.profileId = o.value(QStringLiteral("profileId")).toString();
    r.adapter = o.value(QStringLiteral("adapter")).toString();
    r.createdAt = fromIso(o.value(QStringLiteral("createdAt")));
    r.note = o.value(QStringLiteral("note")).toString().left(500);
    if (r.gamePath.isEmpty() || !QDir::isAbsolutePath(r.gamePath) || r.emulatorId.isEmpty())
        return std::nullopt;
    for (const QJsonValue &v : o.value(QStringLiteral("states")).toArray()) {
        const QJsonObject s = v.toObject();
        GameSessionManager::SavedState state;
        state.file = s.value(QStringLiteral("file")).toString();
        state.bytes = qint64(s.value(QStringLiteral("bytes")).toDouble(-1));
        state.sha256 = s.value(QStringLiteral("sha256")).toString();
        state.savedAt = fromIso(s.value(QStringLiteral("savedAt")));
        if (!fileName.match(state.file).hasMatch() || state.file == MetaFile || state.bytes <= 0
            || state.sha256.size() != 64 || !state.savedAt.isValid())
            return std::nullopt;
        r.states.append(state);
    }
    if (r.states.isEmpty() || r.states.size() > GameSessionManager::MaxStatesPerSession)
        return std::nullopt;
    return r;
}

} // namespace

GameSessionManager::GameSessionManager(const Dependencies &deps, QObject *parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.app && m_deps.settings);
    auto *manager = m_deps.app->manager();
    connect(manager, &emulator::EmulatorManager::stateChanged, this,
            [this](ProcessState state) { onManagerState(state); });
    connect(manager, &emulator::EmulatorManager::processFinished, this,
            [this](const QString &, const emulator::ProcessOutcome &outcome) { onProcessFinished(outcome); });
    connect(manager, &emulator::EmulatorManager::pausedChanged, this, [this](bool paused) {
        if (!m_active)
            return;
        if (paused && m_active->state == State::Running)
            setState(State::SuspendedInMemory);
        else if (!paused && m_active->state == State::SuspendedInMemory)
            setState(State::Running);
    });
    connect(m_deps.settings, &Settings::valueChanged, this, [this](const QString &key) {
        if (key.startsWith(QLatin1String("sessions."))) {
            emit activeChanged();
            emit sessionsChanged();
        }
    });
    m_closeTimer.setSingleShot(true);
    connect(&m_closeTimer, &QTimer::timeout, this, &GameSessionManager::onCloseTimeout);
}

GameSessionManager::~GameSessionManager() = default;

QString GameSessionManager::stateName(State state)
{
    switch (state) {
    case State::Starting: return QStringLiteral("starting");
    case State::Running: return QStringLiteral("running");
    case State::SuspendedInMemory: return QStringLiteral("suspendedInMemory");
    case State::SavingState: return QStringLiteral("savingState");
    case State::Closing: return QStringLiteral("closing");
    case State::Resuming: return QStringLiteral("resuming");
    case State::Failed: return QStringLiteral("failed");
    }
    return {};
}

QString GameSessionManager::sha256Of(const QString &file, QString *error)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) {
        *error = QStringLiteral("read error");
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString GameSessionManager::sessionDirectory(const QString &sessionId) const
{
    return m_deps.statesDir + u'/' + sessionId;
}

void GameSessionManager::notify(Category category, Severity severity, const QString &text)
{
    if (m_deps.notifications)
        m_deps.notifications->post(category, severity, text);
    else
        qCInfo(lcUi).noquote() << text;
}

// --- Records -----------------------------------------------------------------------------

void GameSessionManager::load()
{
    m_records.clear();
    const QDir dir(m_deps.statesDir);
    const QStringList ids = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
    for (const QString &id : ids) {
        const QString metaPath = dir.filePath(id + u'/' + MetaFile);
        if (!QFileInfo::exists(metaPath))
            continue; // a session that never saved (or a foreign directory): left alone
        QString error;
        const auto bytes = shared::readFileLimited(metaPath, MaxMetaBytes, &error);
        const QJsonDocument doc = bytes ? QJsonDocument::fromJson(*bytes) : QJsonDocument();
        const auto record = doc.isObject() ? fromJson(doc.object(), id) : std::nullopt;
        if (!record) {
            const QString moved = shared::moveAside(metaPath);
            qCWarning(lcUi).noquote() << "Quick Resume: unreadable" << metaPath << "moved aside to" << moved;
            notify(Category::Recovery, Severity::Warning,
                   tr("A Quick Resume session could not be read; its files were kept in %1").arg(dir.filePath(id)));
            continue;
        }
        m_records.append(*record);
    }
    std::sort(m_records.begin(), m_records.end(),
              [](const Record &a, const Record &b) { return a.states.first().savedAt > b.states.first().savedAt; });
    qCInfo(lcUi) << "Quick Resume:" << m_records.size() << "saved session(s)";
    emit sessionsChanged();
}

GameSessionManager::Record *GameSessionManager::findRecord(const QString &sessionId)
{
    for (Record &r : m_records) {
        if (r.id == sessionId)
            return &r;
    }
    return nullptr;
}

GameSessionManager::Record *GameSessionManager::findRecordForGame(qint64 gameId, const QString &emulatorId)
{
    for (Record &r : m_records) {
        if (r.gameId == gameId && r.emulatorId == emulatorId)
            return &r;
    }
    return nullptr;
}

bool GameSessionManager::writeMeta(const Record &record, QString *error) const
{
    return shared::writeFileAtomically(sessionDirectory(record.id) + u'/' + MetaFile,
                                       QJsonDocument(toJson(record)).toJson(QJsonDocument::Indented), error);
}

void GameSessionManager::storeRecord(const Record &record)
{
    m_records.removeIf([&](const Record &r) { return r.id == record.id; });
    m_records.prepend(record);
    emit sessionsChanged();
}

qint64 GameSessionManager::totalStateBytes(const QString &exceptSessionId) const
{
    qint64 total = 0;
    for (const Record &r : m_records) {
        if (r.id == exceptSessionId)
            continue;
        for (const auto &s : r.states)
            total += s.bytes;
    }
    return total;
}

std::optional<shared::EmulatorConfig> GameSessionManager::emulator(const QString &emulatorId) const
{
    return m_deps.app->emulators()->find(emulatorId);
}

// Cheap check for the Home row (no hashing).
QString GameSessionManager::quickProblem(const Record &record) const
{
    const SavedState &latest = record.states.first();
    const QFileInfo state(sessionDirectory(record.id) + u'/' + latest.file);
    if (!state.isFile() || state.size() != latest.bytes)
        return tr("The saved state is missing or damaged");
    if (!QFileInfo(record.gamePath).isFile())
        return tr("The game file is missing");
    const auto config = emulator(record.emulatorId);
    if (!config)
        return tr("Emulator \"%1\" is not configured").arg(record.emulatorId);
    if (!config->saveState)
        return tr("%1 no longer declares save-state support").arg(config->name);
    return {};
}

QString GameSessionManager::validateForResume(const Record &record) const
{
    if (const QString problem = quickProblem(record); !problem.isEmpty())
        return problem;
    const SavedState &latest = record.states.first();
    QString error;
    if (sha256Of(sessionDirectory(record.id) + u'/' + latest.file, &error) != latest.sha256)
        return tr("The saved state does not match its checksum (damaged or changed)");
    const QFileInfo game(record.gamePath);
    if (game.size() != record.gameSize
        || (record.gameModified.isValid() && game.lastModified().toUTC() != record.gameModified))
        return tr("The game file changed after the state was saved");
    const auto config = emulator(record.emulatorId);
    if (config->version != record.emulatorVersion)
        return tr("The emulator version changed (%1 → %2); the state may not load")
            .arg(record.emulatorVersion.isEmpty() ? tr("unknown") : record.emulatorVersion,
                 config->version.isEmpty() ? tr("unknown") : config->version);
    if (emulator::SaveStateAdapter::adapterName(config->saveState->adapter) != record.adapter)
        return tr("%1 now uses a different save-state adapter").arg(config->name);
    if (const QString reason = m_deps.app->unavailableReason(record.emulatorId); !reason.isEmpty())
        return reason;
    return {};
}

QVariantList GameSessionManager::suspended() const
{
    const int maxAgeDays = m_deps.settings->intValue(Settings::SessionsMaxAgeDays);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVariantList list;
    for (const Record &r : m_records) {
        if (m_active && m_active->record.id == r.id)
            continue; // the running game is not offered for resume
        const SavedState &latest = r.states.first();
        const auto config = emulator(r.emulatorId);
        list.append(QVariantMap{
            {QStringLiteral("id"), r.id},
            {QStringLiteral("gameId"), double(r.gameId)},
            {QStringLiteral("title"), r.gameTitle},
            {QStringLiteral("emulatorName"), config ? config->name : r.emulatorId},
            {QStringLiteral("savedAt"), QLocale().toString(latest.savedAt.toLocalTime(), QLocale::ShortFormat)},
            {QStringLiteral("bytes"),
             double(std::accumulate(r.states.cbegin(), r.states.cend(), qint64(0),
                                    [](qint64 n, const SavedState &s) { return n + s.bytes; }))},
            {QStringLiteral("note"), r.note},
            {QStringLiteral("problem"), quickProblem(r)},
            {QStringLiteral("old"), maxAgeDays > 0 && latest.savedAt.daysTo(now) > maxAgeDays},
        });
    }
    return list;
}

// --- Active session ------------------------------------------------------------------------

std::optional<GameSessionManager::State> GameSessionManager::activeState() const
{
    return m_active ? std::optional<State>(m_active->state) : std::nullopt;
}

QVariantMap GameSessionManager::active() const
{
    if (!m_active)
        return {};
    return {{QStringLiteral("id"), m_active->record.id},
            {QStringLiteral("gameId"), double(m_active->record.gameId)},
            {QStringLiteral("title"), m_active->record.gameTitle},
            {QStringLiteral("state"), stateName(m_active->state)},
            {QStringLiteral("canSuspend"), canSuspend()},
            {QStringLiteral("reason"), suspendUnavailableReason()}};
}

bool GameSessionManager::working() const
{
    return m_active
        && (m_active->state == State::SavingState || m_active->state == State::Closing
            || m_active->state == State::Resuming || m_active->state == State::Starting);
}

bool GameSessionManager::removeSession(const QString &sessionId)
{
    const Record *record = findRecord(sessionId);
    if (!record)
        return false;
    if (m_active && m_active->record.id == sessionId) {
        notify(Category::SaveState, Severity::Warning, tr("Close the game before removing its Quick Resume state"));
        return false;
    }
    const QString title = record->gameTitle;
    // The directory is Nexora's own (<statesDir>/<uuid>), named after a loaded record.
    QDir dir(sessionDirectory(sessionId));
    if (!dir.removeRecursively()) {
        notify(Category::SaveState, Severity::Warning,
               tr("Could not remove every Quick Resume file of %1 in %2").arg(title, dir.path()));
        load(); // show what is left
        return false;
    }
    m_records.removeIf([&](const Record &r) { return r.id == sessionId; });
    qCInfo(lcUi).noquote() << "Quick Resume state removed by the user:" << title;
    notify(Category::SaveState, Severity::Info, tr("Quick Resume state of %1 removed").arg(title));
    emit sessionsChanged();
    return true;
}

bool GameSessionManager::isQuickResumeEnabled(qint64 gameId) const
{
    return !m_deps.settings->stringListValue(Settings::SessionsExcludedGames).contains(QString::number(gameId));
}

bool GameSessionManager::setQuickResumeEnabled(qint64 gameId, bool enabled)
{
    QStringList excluded = m_deps.settings->stringListValue(Settings::SessionsExcludedGames);
    const QString id = QString::number(gameId);
    excluded.removeAll(id);
    if (!enabled)
        excluded.append(id);
    QString error;
    if (!m_deps.settings->setValue(Settings::SessionsExcludedGames, excluded, &error)) {
        notify(Category::System, Severity::Warning, tr("Could not change Quick Resume: %1").arg(error));
        return false;
    }
    return true;
}

QString GameSessionManager::limitProblem() const
{
    const QString own = m_active ? m_active->record.id : QString();
    const bool hasOwnRecord
        = std::any_of(m_records.cbegin(), m_records.cend(), [&](const Record &r) { return r.id == own; });
    const int others = int(m_records.size()) - (hasOwnRecord ? 1 : 0);
    const int maxCount = m_deps.settings->intValue(Settings::SessionsMaxCount);
    if (others >= maxCount)
        return tr("Quick Resume is full (%1 of %2 games); remove one from Home first").arg(others).arg(maxCount);
    const qint64 maxBytes = qint64(m_deps.settings->intValue(Settings::SessionsMaxMegabytes)) * 1024 * 1024;
    if (totalStateBytes(own) >= maxBytes)
        return tr("Quick Resume storage is full (%1 MiB limit); remove a game from Home first")
            .arg(maxBytes / (1024 * 1024));
    return {};
}

QString GameSessionManager::suspendUnavailableReason() const
{
    if (!m_active)
        return {};
    if (!isQuickResumeEnabled(m_active->record.gameId))
        return tr("Quick Resume is off for this game");
    if (!m_active->adapter)
        return m_active->adapterError.isEmpty() ? tr("This emulator has no save-state support")
                                                : m_active->adapterError;
    if (m_active->state != State::Running && m_active->state != State::SuspendedInMemory)
        return tr("The game is not running yet");
    if (!m_active->adapter->isReady())
        return tr("The emulator has not connected to Nexora's save-state control yet");
    return limitProblem();
}

void GameSessionManager::setState(State state)
{
    if (!m_active || m_active->state == state)
        return;
    qCInfo(lcUi).noquote() << "Session" << m_active->record.id << stateName(m_active->state) << "->"
                           << stateName(state);
    m_active->state = state;
    emit activeChanged();
}

bool GameSessionManager::startGame(const library::GameEntry &entry, const QString &emulatorId, const QString &profileId,
                                   bool applyDefaultProfile, bool preferResume)
{
    if (m_active) {
        notify(Category::Game, Severity::Warning, tr("Another game is still running"));
        return false;
    }
    // A state belongs to the emulator that wrote it: the same game on another emulator gets
    // its own session, and the existing one is left untouched.
    const Record *existing = findRecordForGame(entry.id, emulatorId);
    if (preferResume && existing)
        return resume(existing->id);
    Record record;
    if (existing)
        record = *existing; // same session directory; its states are kept until a new one is saved
    else
        record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.gameId = entry.id;
    record.gameTitle = entry.displayTitle();
    record.gamePath = entry.path;
    record.emulatorId = emulatorId;
    record.profileId = profileId;
    return launch(record, {}, applyDefaultProfile);
}

bool GameSessionManager::resume(const QString &sessionId)
{
    const Record *record = findRecord(sessionId);
    if (!record)
        return false;
    if (m_active) {
        notify(Category::Game, Severity::Warning, tr("Close the running game before resuming another one"));
        return false;
    }
    if (const QString reason = validateForResume(*record); !reason.isEmpty()) {
        qCWarning(lcUi).noquote() << "Quick Resume refused for" << record->gameTitle << ":" << reason;
        emit resumeFailed(record->id, record->gameTitle, reason);
        return false;
    }
    return launch(*record, sessionDirectory(record->id) + u'/' + record->states.first().file,
                  record->profileId.isEmpty());
}

bool GameSessionManager::startFresh(const QString &sessionId)
{
    const Record *record = findRecord(sessionId);
    if (!record || m_active)
        return false;
    return launch(*record, {}, record->profileId.isEmpty());
}

bool GameSessionManager::launch(Record record, const QString &resumeFile, bool applyDefaultProfile)
{
    const auto config = emulator(record.emulatorId);
    Active active;
    active.fromState = !resumeFile.isEmpty();
    QStringList extra;
    if (!config || !config->saveState) {
        active.adapterError = tr("This emulator has no save-state support");
    } else if (!isQuickResumeEnabled(record.gameId)) {
        active.adapterError = tr("Quick Resume is off for this game");
    } else {
        const QString dir = sessionDirectory(record.id);
        if (!QDir().mkpath(dir)) {
            active.adapterError = tr("Could not create %1").arg(dir);
        } else {
            QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            auto adapter = emulator::SaveStateAdapter::create(*config->saveState);
            QString error;
            const auto args = adapter->prepareLaunch({dir, resumeFile, record.gamePath}, &error);
            if (args) {
                extra = *args;
                active.adapter = std::move(adapter);
                record.adapter = emulator::SaveStateAdapter::adapterName(config->saveState->adapter);
            } else {
                active.adapterError = tr("Quick Resume unavailable: %1").arg(error);
            }
        }
    }
    if (active.fromState && !active.adapter) {
        emit resumeFailed(record.id, record.gameTitle, active.adapterError);
        return false;
    }
    if (active.adapter) {
        connect(active.adapter.get(), &emulator::SaveStateAdapter::saveFinished, this,
                &GameSessionManager::onSaveFinished);
        connect(active.adapter.get(), &emulator::SaveStateAdapter::readyChanged, this,
                &GameSessionManager::activeChanged);
    }
    active.record = record;
    active.state = active.fromState ? State::Resuming : State::Starting;
    m_active = std::move(active);
    emit activeChanged();
    emit sessionsChanged();
    if (!m_deps.app->launchGame(record.emulatorId, record.gamePath, record.gameTitle, record.profileId,
                                applyDefaultProfile, extra)) {
        // Validation failures were already reported by AppController (processFinished).
        if (m_active) {
            const bool fromState = m_active->fromState;
            m_active.reset();
            emit activeChanged();
            emit sessionsChanged();
            if (fromState)
                emit resumeFailed(record.id, record.gameTitle, m_deps.app->statusMessage());
        }
        return false;
    }
    m_active->record.profileId = m_deps.app->activeProfileId();
    m_active->record.emulatorVersion = config ? config->version : QString();
    const QFileInfo game(record.gamePath);
    m_active->record.gameSize = game.size();
    m_active->record.gameModified = game.lastModified().toUTC();
    if (!m_active->record.createdAt.isValid())
        m_active->record.createdAt = QDateTime::currentDateTimeUtc();
    emit launchAccepted(record.gameId);
    return true;
}

void GameSessionManager::onManagerState(ProcessState state)
{
    if (!m_active)
        return;
    if (state == ProcessState::Running && (m_active->state == State::Starting || m_active->state == State::Resuming))
        setState(State::Running);
}

bool GameSessionManager::suspendToHome()
{
    if (!canSuspend()) {
        if (m_active)
            notify(Category::SaveState, Severity::Warning, suspendUnavailableReason());
        return false;
    }
    if (m_deps.app->manager()->isPaused())
        m_deps.app->resume(); // the emulator must run to answer the save request
    setState(State::SavingState);
    m_active->adapter->save(sessionDirectory(m_active->record.id));
    return true;
}

void GameSessionManager::onSaveFinished(bool ok, const QString &file, const QString &error)
{
    if (!m_active || m_active->state != State::SavingState)
        return;
    const QString title = m_active->record.gameTitle;
    const bool alive = m_deps.app->manager()->isActive();
    auto failed = [&](const QString &why) {
        setState(alive ? State::Running : State::Failed);
        notify(Category::SaveState, Severity::Warning,
               alive ? tr("Could not save %1 for Quick Resume: %2. The game is still running.").arg(title, why)
                     : tr("Could not save %1 for Quick Resume: %2").arg(title, why));
        finishPower(false);
    };
    if (!ok) {
        failed(error);
        return;
    }
    QString hashError;
    SavedState state;
    state.file = QFileInfo(file).fileName();
    // A verified state is never re-recorded: the emulator must have written a new file.
    if (std::any_of(m_active->record.states.cbegin(), m_active->record.states.cend(),
                    [&](const SavedState &s) { return s.file == state.file; })) {
        failed(tr("the emulator reported an already recorded state file (%1)").arg(state.file));
        return;
    }
    state.bytes = QFileInfo(file).size();
    state.sha256 = sha256Of(file, &hashError);
    state.savedAt = QDateTime::currentDateTimeUtc();
    if (state.sha256.isEmpty()) {
        failed(tr("the state file could not be read (%1)").arg(hashError));
        return;
    }
    Record record = m_active->record;
    record.note.clear();
    QList<SavedState> superseded;
    record.states.prepend(state);
    while (record.states.size() > MaxStatesPerSession)
        superseded.append(record.states.takeLast());
    QString metaError;
    if (!writeMeta(record, &metaError)) {
        // The new file stays on disk, unreferenced; the previous meta.json still describes
        // the previous (valid) state.
        failed(tr("its description could not be written (%1)").arg(metaError));
        return;
    }
    // Only now, with the new state verified and recorded, the oldest one goes.
    for (const SavedState &old : std::as_const(superseded)) {
        if (old.file != state.file)
            QFile::remove(sessionDirectory(record.id) + u'/' + old.file);
    }
    m_active->record = record;
    m_active->saved = true;
    storeRecord(record);
    const qint64 maxBytes = qint64(m_deps.settings->intValue(Settings::SessionsMaxMegabytes)) * 1024 * 1024;
    if (totalStateBytes({}) > maxBytes)
        notify(Category::Storage, Severity::Warning,
               tr("Quick Resume uses more than its %1 MiB limit; remove games from the Quick Resume row on Home")
                   .arg(maxBytes / (1024 * 1024)));

    setState(State::Closing);
    if (!alive) {
        onProcessFinished(m_deps.app->manager()->lastOutcome());
        return;
    }
    // Ask nicely first; stop() (SIGTERM, then SIGKILL after the emulator's stop timeout)
    // follows if it is still running when the timer fires.
    m_active->adapter->requestQuit();
    m_closeTimer.start(m_active->adapter->capability().timeoutMs);
}

void GameSessionManager::onCloseTimeout()
{
    if (m_active && m_active->state == State::Closing && m_deps.app->manager()->isActive()) {
        qCInfo(lcUi) << "Emulator did not quit after saving; stopping it";
        m_deps.app->stop();
    }
}

void GameSessionManager::onProcessFinished(const emulator::ProcessOutcome &outcome)
{
    if (!m_active) {
        // An emulator started without a session (Emulators screen) ended; a pending power
        // request waits for exactly that.
        finishPower(true);
        return;
    }
    m_closeTimer.stop();
    Active finished = std::move(*m_active);
    m_active.reset();
    if (finished.adapter) {
        finished.adapter->disconnect(this); // close() fails a pending request; handled below
        finished.adapter->close();
        // Deferred: this can run from inside the adapter's own saveFinished emission.
        finished.adapter.release()->deleteLater();
    }
    const QString title = finished.record.gameTitle;

    if (finished.state == State::SavingState) {
        notify(Category::SaveState, Severity::Warning,
               tr("%1 stopped while it was being saved for Quick Resume; no new state was written").arg(title));
    } else if (finished.state == State::Closing) {
        notify(Category::SaveState, Severity::Info, tr("%1 saved for Quick Resume").arg(title));
    } else if (finished.state == State::Resuming && outcome.finalState == ProcessState::Failed) {
        emit resumeFailed(finished.record.id, title,
                          tr("The emulator could not start from the saved state: %1").arg(outcome.message));
    } else if (Record *record = findRecord(finished.record.id)) {
        // The game ended on its own or was closed: its saved state is older than the play
        // that just happened. It stays resumable, with a note, until the user removes it.
        if (finished.state != State::Starting) {
            const QString when = QLocale().toString(record->states.first().savedAt.toLocalTime(), QLocale::ShortFormat);
            record->note = tr("Played after this state was saved; resuming returns to %1").arg(when);
            QString error;
            if (!writeMeta(*record, &error))
                qCWarning(lcUi).noquote() << "Could not update Quick Resume note:" << error;
        }
    } else if (!finished.saved) {
        // Never saved: drop the empty session directory (only if it is empty).
        QDir().rmdir(sessionDirectory(finished.record.id));
    }
    emit activeChanged();
    emit sessionsChanged();
    finishPower(true); // no emulator runs any more
}

void GameSessionManager::finishPower(bool ok)
{
    if (!m_powerDone)
        return;
    // The power request goes ahead only once no emulator runs any more.
    if (ok && m_deps.app->manager()->isActive())
        return;
    auto done = std::exchange(m_powerDone, nullptr);
    if (!ok)
        notify(Category::System, Severity::Critical,
               tr("Power request cancelled: the running game could not be saved"));
    done(ok);
}

void GameSessionManager::closeForPower(std::function<void(bool)> done)
{
    if (!m_deps.app->manager()->isActive()) {
        done(true);
        return;
    }
    m_powerDone = std::move(done);
    if (m_active && m_active->state == State::SavingState)
        return; // a suspend is already under way
    if (canSuspend()) {
        suspendToHome();
        return;
    }
    // No Quick Resume for this game: the UI warned that it will be closed.
    m_deps.app->stop();
}

void GameSessionManager::shutdown()
{
    if (m_shutDown)
        return;
    m_shutDown = true;
    if (!m_active || !m_deps.app->manager()->isActive())
        return;
    if (canSuspend() && suspendToHome()) {
        // Bounded: the save, the quit request and the emulator's stop timeout.
        const int budget = m_active->adapter->capability().timeoutMs * 2 + 5000;
        QDeadlineTimer deadline(budget);
        QEventLoop loop;
        auto stop = connect(this, &GameSessionManager::activeChanged, &loop, [&] {
            if (!m_active || (m_active->state != State::SavingState && m_active->state != State::Closing))
                loop.quit();
        });
        QTimer::singleShot(budget, &loop, &QEventLoop::quit);
        if (m_active && (m_active->state == State::SavingState || m_active->state == State::Closing))
            loop.exec();
        disconnect(stop);
        if (deadline.hasExpired())
            qCWarning(lcUi) << "Quick Resume: saving at shutdown timed out";
        if (m_active && m_active->state == State::Running)
            notify(Category::SaveState, Severity::Warning,
                   tr("%1 could not be saved for Quick Resume while shutting down").arg(m_active->record.gameTitle));
    }
}

} // namespace nexora::ui
