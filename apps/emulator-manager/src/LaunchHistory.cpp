#include "nexora/emulator/LaunchHistory.h"

#include "nexora/shared/AtomicFile.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace nexora::emulator {

namespace {

constexpr int FormatVersion = 1;
constexpr qint64 MaxFileBytes = 4 * 1024 * 1024;

QString toIso(const QDateTime &time)
{
    return time.isValid() ? time.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime fromIso(const QString &text)
{
    return text.isEmpty() ? QDateTime() : QDateTime::fromString(text, Qt::ISODateWithMs);
}

QJsonObject toJson(const LaunchRecord &r)
{
    QJsonObject o;
    o.insert(QStringLiteral("emulatorId"), r.emulatorId);
    o.insert(QStringLiteral("profileId"), r.profileId);
    o.insert(QStringLiteral("gameTitle"), r.gameTitle);
    o.insert(QStringLiteral("startedAt"), toIso(r.startedAt));
    o.insert(QStringLiteral("endedAt"), toIso(r.endedAt));
    o.insert(QStringLiteral("pid"), r.pid);
    o.insert(QStringLiteral("finalState"), r.finalState);
    o.insert(QStringLiteral("error"), r.error);
    o.insert(QStringLiteral("exitCode"), r.exitCode ? QJsonValue(*r.exitCode) : QJsonValue(QJsonValue::Null));
    o.insert(QStringLiteral("startupFailure"), r.startupFailure);
    o.insert(QStringLiteral("stderrTail"), r.stderrTail);
    return o;
}

LaunchRecord fromJson(const QJsonObject &o)
{
    LaunchRecord r;
    r.emulatorId = o.value(QStringLiteral("emulatorId")).toString();
    r.profileId = o.value(QStringLiteral("profileId")).toString();
    r.gameTitle = o.value(QStringLiteral("gameTitle")).toString();
    r.startedAt = fromIso(o.value(QStringLiteral("startedAt")).toString());
    r.endedAt = fromIso(o.value(QStringLiteral("endedAt")).toString());
    r.pid = o.value(QStringLiteral("pid")).toInteger();
    r.finalState = o.value(QStringLiteral("finalState")).toString();
    r.error = o.value(QStringLiteral("error")).toString();
    if (const QJsonValue code = o.value(QStringLiteral("exitCode")); code.isDouble())
        r.exitCode = code.toInt();
    r.startupFailure = o.value(QStringLiteral("startupFailure")).toBool();
    r.stderrTail = o.value(QStringLiteral("stderrTail")).toString();
    return r;
}

} // namespace

LaunchHistory::LaunchHistory(QString filePath, int maxRecords, shared::RedactionRules rules)
    : m_path(std::move(filePath))
    , m_maxRecords(std::max(1, maxRecords))
    , m_rules(std::move(rules))
{ }

QList<LaunchRecord> LaunchHistory::records() const
{
    if (!QFileInfo::exists(m_path))
        return {};
    QString error;
    const auto data = shared::readFileLimited(m_path, MaxFileBytes, &error);
    if (!data) {
        shared::moveAside(m_path);
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(*data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || doc.object().value(QStringLiteral("version")).toInt(-1) != FormatVersion
        || !doc.object().value(QStringLiteral("records")).isArray()) {
        shared::moveAside(m_path);
        return {};
    }
    QList<LaunchRecord> result;
    const QJsonArray array = doc.object().value(QStringLiteral("records")).toArray();
    for (const QJsonValue &value : array) {
        if (value.isObject())
            result.prepend(fromJson(value.toObject())); // newest first
    }
    return result;
}

bool LaunchHistory::append(const LaunchRecord &record, QString *error)
{
    LaunchRecord safe = record;
    safe.error = shared::redact(safe.error, m_rules);
    safe.stderrTail = shared::redact(safe.stderrTail, m_rules);
    if (safe.stderrTail.size() > MaxStderrTail)
        safe.stderrTail = safe.stderrTail.right(MaxStderrTail);

    QList<LaunchRecord> all = records(); // newest first
    all.prepend(safe);
    if (all.size() > m_maxRecords)
        all.resize(m_maxRecords);

    QJsonArray array;
    for (auto it = all.crbegin(); it != all.crend(); ++it) // oldest first on disk
        array.append(toJson(*it));
    QJsonObject root;
    root.insert(QStringLiteral("version"), FormatVersion);
    root.insert(QStringLiteral("records"), array);
    return shared::writeFileAtomically(m_path, QJsonDocument(root).toJson(QJsonDocument::Indented), error);
}

} // namespace nexora::emulator
