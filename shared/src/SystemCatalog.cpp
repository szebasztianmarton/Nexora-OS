#include "nexora/shared/SystemCatalog.h"

#include "nexora/shared/AtomicFile.h"
#include "nexora/shared/Logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace nexora::shared {

bool SystemCatalog::isValidId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,31}$"));
    return pattern.match(id).hasMatch();
}

bool SystemCatalog::isValidExtension(const QString &ext)
{
    static const QRegularExpression pattern(QStringLiteral("^\\.[a-z0-9]{1,16}$"));
    return pattern.match(ext).hasMatch();
}

std::optional<SystemCatalog> SystemCatalog::load(const QString &file, QStringList *issues)
{
    QStringList local;
    auto fail = [&](const QString &message) -> std::optional<SystemCatalog> {
        local.append(QStringLiteral("%1: %2").arg(file, message));
        if (issues)
            issues->append(local);
        return std::nullopt;
    };

    QString readError;
    const auto data = readFileLimited(file, MaxFileSize, &readError);
    if (!data)
        return fail(readError);

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(*data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(
            QStringLiteral("invalid JSON at offset %1: %2").arg(parseError.offset).arg(parseError.errorString()));
    if (!doc.isObject())
        return fail(QStringLiteral("top-level value must be an object"));
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != FormatVersion)
        return fail(QStringLiteral("unsupported or missing \"version\" (expected %1)").arg(FormatVersion));
    if (!root.value(QStringLiteral("systems")).isArray())
        return fail(QStringLiteral("\"systems\" must be an array"));

    SystemCatalog catalog;
    const QJsonArray array = root.value(QStringLiteral("systems")).toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QString label = QStringLiteral("%1: systems[%2]").arg(file).arg(i);
        if (!array.at(i).isObject()) {
            local.append(label + QStringLiteral(" must be an object; entry dropped"));
            continue;
        }
        const QJsonObject o = array.at(i).toObject();
        GameSystem system;
        system.id = o.value(QStringLiteral("id")).toString();
        system.name = o.value(QStringLiteral("name")).toString().trimmed();
        system.shortName = o.value(QStringLiteral("shortName")).toString().trimmed();

        if (!isValidId(system.id)) {
            local.append(label + QStringLiteral(" has an invalid id \"%1\"; entry dropped").arg(system.id));
            continue;
        }
        if (catalog.find(system.id)) {
            local.append(label + QStringLiteral(" duplicates id \"%1\"; entry dropped").arg(system.id));
            continue;
        }
        if (system.name.isEmpty()) {
            local.append(label + QStringLiteral(" (\"%1\") needs a name; entry dropped").arg(system.id));
            continue;
        }
        if (system.shortName.isEmpty())
            system.shortName = system.name;

        bool extensionsOk = o.value(QStringLiteral("extensions")).isArray();
        const QJsonArray exts = o.value(QStringLiteral("extensions")).toArray();
        for (const QJsonValue &e : exts) {
            const QString ext = e.toString().toLower();
            if (!isValidExtension(ext)) {
                extensionsOk = false;
                local.append(label
                             + QStringLiteral(" (\"%1\") has an invalid extension \"%2\"; entry dropped")
                                   .arg(system.id, e.toString()));
                break;
            }
            if (!system.extensions.contains(ext))
                system.extensions.append(ext);
        }
        if (!extensionsOk) {
            if (!o.value(QStringLiteral("extensions")).isArray())
                local.append(label
                             + QStringLiteral(" (\"%1\") needs an \"extensions\" array; entry dropped").arg(system.id));
            continue;
        }
        catalog.m_systems.append(system);
    }

    for (const QString &issue : std::as_const(local))
        qCWarning(lcConfig).noquote() << issue;
    if (issues)
        issues->append(local);
    return catalog;
}

SystemCatalog SystemCatalog::builtIn()
{
    QStringList issues;
    if (auto catalog = load(QStringLiteral(":/nexora/systems.json"), &issues))
        return *catalog;
    return {};
}

const GameSystem *SystemCatalog::find(const QString &id) const
{
    for (const GameSystem &s : m_systems) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

QStringList SystemCatalog::systemsForExtension(const QString &ext) const
{
    const QString lower = ext.toLower();
    QStringList ids;
    for (const GameSystem &s : m_systems) {
        if (s.extensions.contains(lower))
            ids.append(s.id);
    }
    return ids;
}

} // namespace nexora::shared
