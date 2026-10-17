#include "NotificationService.h"

#include "nexora/shared/AtomicFile.h"
#include "nexora/shared/Logging.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>

namespace nexora::ui {

namespace {

constexpr int HistoryFormatVersion = 1;
constexpr qint64 MaxHistoryFileSize = 256 * 1024;

constexpr std::array<std::pair<NotificationService::Category, const char *>, 8> kCategories{{
    {NotificationService::Category::Controller, "controller"},
    {NotificationService::Category::Game, "game"},
    {NotificationService::Category::SaveState, "saveState"},
    {NotificationService::Category::Library, "library"},
    {NotificationService::Category::Storage, "storage"},
    {NotificationService::Category::Update, "update"},
    {NotificationService::Category::System, "system"},
    {NotificationService::Category::Recovery, "recovery"},
}};

std::optional<NotificationService::Severity> severityFromName(const QString &name)
{
    if (name == QLatin1String("info"))
        return NotificationService::Severity::Info;
    if (name == QLatin1String("warning"))
        return NotificationService::Severity::Warning;
    if (name == QLatin1String("critical"))
        return NotificationService::Severity::Critical;
    return std::nullopt;
}

} // namespace

NotificationService::NotificationService(const QString &historyFile, QObject *parent)
    : QObject(parent)
    , m_historyFile(historyFile)
{
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &NotificationService::dismiss);
}

QString NotificationService::categoryName(Category category)
{
    for (const auto &[c, name] : kCategories) {
        if (c == category)
            return QString::fromLatin1(name);
    }
    return QStringLiteral("system");
}

std::optional<NotificationService::Category> NotificationService::categoryFromName(const QString &name)
{
    for (const auto &[c, n] : kCategories) {
        if (name == QLatin1String(n))
            return c;
    }
    return std::nullopt;
}

QString NotificationService::severityName(Severity severity)
{
    switch (severity) {
    case Severity::Info: return QStringLiteral("info");
    case Severity::Warning: return QStringLiteral("warning");
    case Severity::Critical: return QStringLiteral("critical");
    }
    return QStringLiteral("info");
}

quint64 NotificationService::post(Category category, Severity severity, const QString &text, const QString &key)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return 0;
    const QString dedupKey = key.isEmpty() ? categoryName(category) + u':' + trimmed : key;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (Notification *recent = findRecent(dedupKey)) {
        const quint64 id = recent->id;
        // Every copy (current, queued, history) of the entry is updated together.
        auto bump = [&](Notification &n) {
            if (n.id != id)
                return;
            ++n.count;
            n.updatedAt = now;
            n.text = trimmed; // the latest wording wins
            n.severity = std::max(n.severity, severity);
        };
        bool currentBumped = false;
        if (m_current && m_current->id == id) {
            bump(*m_current);
            currentBumped = true;
            m_hideTimer.start(displayTime(m_current->severity));
        }
        for (Notification &n : m_queue)
            bump(n);
        for (Notification &n : m_history)
            bump(n);
        if (currentBumped)
            emit currentChanged();
        emit historyChanged();
        persist();
        return id;
    }

    Notification n;
    n.id = m_nextId++;
    n.category = category;
    n.severity = severity;
    n.text = trimmed;
    n.key = dedupKey;
    n.createdAt = now;
    n.updatedAt = now;
    addToHistory(n);
    if (shouldDisplay(n)) {
        if (n.severity == Severity::Critical) {
            // Criticals go before everything that is waiting; the current one finishes first
            // unless it is less important, in which case it goes back to the front.
            m_queue.push_front(n);
            if (m_current && m_current->severity != Severity::Critical) {
                m_queue.insert(m_queue.begin() + 1, *m_current);
                m_current.reset();
                m_hideTimer.stop();
            }
        } else {
            m_queue.push_back(n);
        }
        if (!m_current)
            showNext();
    }
    qCInfo(lcUi).noquote() << "Notification" << categoryName(category) << severityName(severity) << trimmed;
    return n.id;
}

void NotificationService::postMessage(const QString &category, const QString &text, bool error)
{
    post(categoryFromName(category).value_or(Category::System), error ? Severity::Warning : Severity::Info, text);
}

void NotificationService::dismiss()
{
    m_hideTimer.stop();
    const bool had = m_current.has_value();
    m_current.reset();
    if (!m_queue.empty())
        showNext();
    else if (had)
        emit currentChanged();
}

void NotificationService::clearHistory()
{
    m_history.clear();
    emit historyChanged();
    persist();
}

QVariantMap NotificationService::current() const
{
    return m_current ? toMap(*m_current) : QVariantMap{};
}

QVariantList NotificationService::history() const
{
    QVariantList list;
    list.reserve(m_history.size());
    for (const Notification &n : m_history)
        list.append(toMap(n));
    return list;
}

void NotificationService::setQuiet(bool quiet)
{
    if (m_quiet == quiet)
        return;
    m_quiet = quiet;
    emit settingsChanged();
}

void NotificationService::setHiddenCategories(const QStringList &categories)
{
    m_hidden.clear();
    for (const QString &name : categories) {
        if (const auto c = categoryFromName(name))
            m_hidden.append(*c);
    }
    emit settingsChanged();
}

void NotificationService::setDurationMs(int ms)
{
    m_durationMs = std::clamp(ms, 500, 60000);
}

bool NotificationService::shouldDisplay(const Notification &n) const
{
    if (n.severity == Severity::Critical)
        return true;
    if (m_hidden.contains(n.category))
        return false;
    return !(m_quiet && n.severity == Severity::Info);
}

NotificationService::Notification *NotificationService::findRecent(const QString &key)
{
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addMSecs(-m_dedupWindow.count());
    auto recent = [&](const Notification &n) { return n.key == key && n.updatedAt >= cutoff && !n.fromPreviousRun; };
    if (m_current && recent(*m_current))
        return &*m_current;
    for (Notification &n : m_queue) {
        if (recent(n))
            return &n;
    }
    for (Notification &n : m_history) {
        if (recent(n))
            return &n;
    }
    return nullptr;
}

void NotificationService::showNext()
{
    if (m_queue.empty())
        return;
    m_current = m_queue.front();
    m_queue.pop_front();
    m_hideTimer.start(displayTime(m_current->severity));
    emit currentChanged();
}

void NotificationService::addToHistory(const Notification &n)
{
    m_history.prepend(n);
    while (m_history.size() > MaxHistory)
        m_history.removeLast();
    emit historyChanged();
    if (n.severity != Severity::Info)
        persist();
}

std::chrono::milliseconds NotificationService::displayTime(Severity severity) const
{
    const int factor = severity == Severity::Critical ? 3 : severity == Severity::Warning ? 2 : 1;
    return std::chrono::milliseconds(m_durationMs * factor);
}

QVariantMap NotificationService::toMap(const Notification &n)
{
    return {
        {QStringLiteral("id"), n.id},
        {QStringLiteral("category"), categoryName(n.category)},
        {QStringLiteral("severity"), severityName(n.severity)},
        {QStringLiteral("text"), n.text},
        {QStringLiteral("count"), n.count},
        {QStringLiteral("createdAt"), n.createdAt},
        {QStringLiteral("updatedAt"), n.updatedAt},
        {QStringLiteral("fromPreviousRun"), n.fromPreviousRun},
    };
}

void NotificationService::persist() const
{
    if (m_historyFile.isEmpty())
        return;
    QJsonArray items;
    for (const Notification &n : m_history) {
        if (n.severity == Severity::Info)
            continue;
        items.append(QJsonObject{
            {QStringLiteral("category"), categoryName(n.category)},
            {QStringLiteral("severity"), severityName(n.severity)},
            {QStringLiteral("text"), n.text},
            {QStringLiteral("count"), n.count},
            {QStringLiteral("createdAt"), n.createdAt.toString(Qt::ISODateWithMs)},
            {QStringLiteral("updatedAt"), n.updatedAt.toString(Qt::ISODateWithMs)},
        });
        if (items.size() >= MaxPersisted)
            break;
    }
    const QJsonObject root{{QStringLiteral("version"), HistoryFormatVersion}, {QStringLiteral("items"), items}};
    QString error;
    if (!shared::writeFileAtomically(m_historyFile, QJsonDocument(root).toJson(QJsonDocument::Compact), &error))
        qCWarning(lcUi).noquote() << "Could not save the notification history:" << error;
}

void NotificationService::loadHistory()
{
    if (m_historyFile.isEmpty() || !QFileInfo::exists(m_historyFile))
        return;
    QString error;
    const auto bytes = shared::readFileLimited(m_historyFile, MaxHistoryFileSize, &error);
    const QJsonDocument doc = bytes ? QJsonDocument::fromJson(*bytes) : QJsonDocument();
    const QJsonObject root = doc.object();
    if (!bytes || !doc.isObject() || root.value(QStringLiteral("version")).toInt() != HistoryFormatVersion) {
        qCWarning(lcUi).noquote() << "Notification history unreadable; moved aside:"
                                  << shared::moveAside(m_historyFile);
        return;
    }
    QList<Notification> loaded;
    for (const QJsonValue &v : root.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        const auto category = categoryFromName(o.value(QStringLiteral("category")).toString());
        const auto severity = severityFromName(o.value(QStringLiteral("severity")).toString());
        const QString text = o.value(QStringLiteral("text")).toString().trimmed();
        if (!category || !severity || text.isEmpty())
            continue;
        Notification n;
        n.id = m_nextId++;
        n.category = *category;
        n.severity = *severity;
        n.text = text.left(1000);
        n.key = categoryName(n.category) + u':' + n.text;
        n.count = std::max(1, o.value(QStringLiteral("count")).toInt(1));
        n.createdAt = QDateTime::fromString(o.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
        n.updatedAt = QDateTime::fromString(o.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
        n.fromPreviousRun = true;
        loaded.append(n);
        if (loaded.size() >= MaxPersisted)
            break;
    }
    m_history = loaded + m_history;
    while (m_history.size() > MaxHistory)
        m_history.removeLast();
    emit historyChanged();
}

} // namespace nexora::ui
